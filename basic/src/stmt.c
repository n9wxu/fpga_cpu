/* Statements, the run loop and the command prompt. */
#include "basic.h"

#define LINEMAX 162
#define TOKMAX  320
#define CTLMAX  48

static jmp_buf top;
static uint8_t imm[TOKMAX]; /* the tokenized immediate line */
static uint8_t zero_tok;    /* ip = &zero_tok ends the run */
static char linebuf[LINEMAX];
static uint8_t *stmt_ip;    /* start of the current statement */
static uint8_t *cont_ip, *cont_lp;
static int cont_ok, autorun_mode;
static uint16_t brk_count;
static int jumped; /* the statement moved ip: skip the end-of-statement check */

/* FOR / GOSUB stack */
enum { C_FOR, C_GOSUB };
typedef struct {
    uint8_t kind, isint;
    uint16_t var;
    float lim, step;
    uint8_t *lp, *ip;
} ctl_t;
static ctl_t ctl[CTLMAX];
static int csp;

/* READ position */
static uint8_t *data_lp, *data_scan; /* line and token where the search continues */
static const char *data_p, *data_e;  /* the current DATA text; data_p > data_e: used up */
static int data_ok;

static const char *const errmsg[] = {
    "SYNTAX", "RETURN WITHOUT GOSUB", "OUT OF DATA", "ILLEGAL QUANTITY", "OVERFLOW",
    "OUT OF MEMORY", "UNDEF'D STATEMENT", "BAD SUBSCRIPT", "REDIM'D ARRAY", "DIVISION BY ZERO",
    "ILLEGAL DIRECT", "TYPE MISMATCH", "STRING TOO LONG", "FORMULA TOO COMPLEX", "CAN'T CONTINUE",
    "UNDEF'D FUNCTION", "NEXT WITHOUT FOR", "FILE NOT FOUND", "FILE", "REDO FROM START", "LINE TOO LONG",
};

static void print_u16(uint16_t n)
{
    char b[8];
    fmt_u32(n, b);
    con_puts(b);
}

static void newline_if_needed(void)
{
    if (con_col())
        con_putc('\n');
}

void error(int e)
{
    newline_if_needed();
    con_putc('?');
    con_puts(errmsg[e]);
    con_puts(" ERROR");
    if (lp && LLEN(lp)) {
        con_puts(" IN ");
        print_u16(LNUM(lp));
    }
    con_putc('\n');
    cont_ok = 0;
    longjmp(top, 1);
}

void brk(void)
{
    newline_if_needed();
    con_puts("BREAK");
    if (lp && LLEN(lp)) {
        con_puts(" IN ");
        print_u16(LNUM(lp));
        cont_lp = lp;
        cont_ip = stmt_ip;
        cont_ok = 1;
    }
    con_putc('\n');
    longjmp(top, 1);
}

static void end_run(void)
{
    lp = NULL;
    ip = &zero_tok;
    jumped = 1;
}

static int at_end(void) { return *ip == 0 || *ip == ':' || *ip == KW_ELSE; }

static float arg_num(void)
{
    expr();
    return pop_num();
}

static int16_t arg_i16(void)
{
    expr();
    return pop_i16();
}

static int32_t arg_i32(void)
{
    expr();
    return pop_i32();
}

static uint16_t arg_line(void)
{
    int32_t n = arg_i32();
    if (n < 0 || n > 65535L)
        error(E_FC);
    return (uint16_t)n;
}

static void comma(void) { need(','); }

static void goto_line(uint16_t n)
{
    uint8_t *l = find_line(n, 1);
    if (!l)
        error(E_UL);
    lp = l;
    ip = l + 4;
    jumped = 1;
}

static void skip_line(void)
{
    while (*ip)
        ip = tok_skip(ip);
}

static void skip_stmt(void)
{
    while (*ip && *ip != ':')
        ip = tok_skip(ip);
}

/* ---- variables ---- */

typedef struct {
    uint8_t type;
    void *p;
    uint16_t id;
} lval_t;

static void lvalue(lval_t *lv)
{
    if (*ip != TK_VAR)
        error(E_SN);
    lv->id = RD16(ip + 1);
    ip += 3;
    lv->type = vtype[lv->id];
    lv->p = *ip == '(' ? arr_ref(lv->id) : (void *)&vars[lv->id];
}

/* Store the value on top of the stack (and pop it). */
static void store(const lval_t *lv)
{
    switch (lv->type) {
    case VT_NUM: *(float *)lv->p = pop_num(); break;
    case VT_INT: *(int16_t *)lv->p = pop_i16(); break;
    default: assign_str((str_t *)lv->p); break;
    }
}

static void do_let(void)
{
    lval_t lv;
    lvalue(&lv);
    need('=');
    expr();
    store(&lv);
}

/* ---- PRINT ---- */

static void print_val(void)
{
    val_t *v = &vs[vsp - 1];
    if (v->t == T_NUM || v->t == T_INT) {
        char b[20];
        int n = fmt_num(num_of(v), b);
        b[n++] = ' ';
        con_write(b, (uint16_t)n);
    } else {
        str_t s = sval(v);
        con_write(s.p, s.len);
    }
    vsp--;
}

static void do_print(void)
{
    int nl = 1;
    while (!at_end()) {
        uint8_t t = *ip;
        nl = 1;
        if (t == ';') {
            ip++;
            nl = 0;
        } else if (t == ',') {
            int c = con_col(), next = (c / 16 + 1) * 16;
            ip++;
            nl = 0;
            if (next >= COLS)
                con_putc('\n');
            else
                while (con_col() < next)
                    con_putc(' ');
        } else if (t == KW_TAB || t == KW_SPC) {
            int16_t n;
            ip++;
            need('(');
            n = arg_i16();
            need(')');
            if (n < 0 || n > 255)
                error(E_FC);
            if (t == KW_SPC)
                while (n-- > 0)
                    con_putc(' ');
            else
                while (con_col() < n && con_col() < COLS - 1)
                    con_putc(' ');
            nl = 0;
            if (*ip == ';')
                ip++;
        } else {
            expr();
            print_val();
        }
    }
    if (nl)
        con_putc('\n');
}

/* ---- INPUT, READ ---- */

static int read_line(char *buf, int max)
{
    int n;
    con_flush();
    n = hal_readline(buf, max);
    if (n == HAL_EOF)
        hal_exit(0);
    if (hal_break())
        brk();
    return n;
}

/* Next comma-separated field of s. Returns 0 at the end. */
static int field(const char **s, const char *e, const char **fp, uint16_t *fl)
{
    const char *p = *s;
    while (p < e && *p == ' ')
        p++;
    if (p < e && *p == '"') {
        const char *q = ++p;
        while (p < e && *p != '"')
            p++;
        *fp = q;
        *fl = (uint16_t)(p - q);
        if (p < e)
            p++;
        while (p < e && *p == ' ')
            p++;
    } else {
        const char *q = p;
        while (p < e && *p != ',')
            p++;
        *fp = q;
        *fl = (uint16_t)(p - q);
        while (*fl && q[*fl - 1] == ' ')
            (*fl)--;
    }
    if (p < e && *p == ',')
        p++;
    else if (p >= e)
        p = e + 1;
    *s = p;
    return 1;
}

/* Convert a text field into the variable's type. Returns 0 if a number is malformed. */
static int store_text(const lval_t *lv, const char *f, uint16_t fl)
{
    if (lv->type == VT_STR) {
        push_newstr(f, fl);
        store(lv);
        return 1;
    } else {
        char b[40];
        float v = 0;
        int isint, neg = 0;
        const char *e;
        uint16_t i = 0, n = fl < 39 ? fl : 39;
        memcpy(b, f, n);
        b[n] = 0;
        if (b[i] == '-' || b[i] == '+')
            neg = b[i++] == '-';
        if (i == n) {
            v = 0;
        } else {
            e = parse_num(b + i, &v, &isint);
            if (!e)
                return 0;
            while (*e == ' ')
                e++;
            if (*e)
                return 0;
        }
        push_num(neg ? -v : v);
        store(lv);
        return 1;
    }
}

static void do_input(void)
{
    uint8_t *start;
    int prompt = 0;
    if (*ip == TK_STR) {
        con_write((const char *)ip + 2, ip[1]);
        ip += 2 + ip[1];
        if (*ip == ';') {
            ip++;
            con_puts("? ");
        } else
            comma();
        prompt = 1;
    }
    if (!prompt)
        con_puts("? ");
    start = ip;
    for (;;) {
        int n = read_line(linebuf, LINEMAX), ok = 1;
        const char *s = linebuf, *e = linebuf + n;
        ip = start;
        for (;;) {
            lval_t lv;
            const char *f;
            uint16_t fl;
            lvalue(&lv);
            if (s > e) {
                con_puts("?? ");
                n = read_line(linebuf, LINEMAX);
                s = linebuf;
                e = linebuf + n;
            }
            field(&s, e, &f, &fl);
            if (!store_text(&lv, f, fl)) {
                ok = 0;
                break;
            }
            if (*ip != ',')
                break;
            ip++;
        }
        if (ok) {
            if (s <= e)
                con_puts("?EXTRA IGNORED\n");
            return;
        }
        con_puts("?REDO FROM START\n");
        con_puts("? ");
    }
}

static void data_restore(uint8_t *l)
{
    data_lp = l ? l : arena;
    data_scan = data_lp + 4;
    data_ok = 0;
}

static int data_find(void)
{
    uint8_t *q = data_scan, *l = data_lp;
    if (!LLEN(l))
        return 0;
    for (;;) {
        if (!*q) {
            l += LLEN(l);
            if (!LLEN(l)) {
                data_lp = l;
                data_scan = l;
                return 0;
            }
            q = l + 4;
            continue;
        }
        if (*q == KW_DATA && q[1] == TK_RAW) {
            data_p = (const char *)q + 3;
            data_e = data_p + q[2];
            data_lp = l;
            data_scan = q + 3 + q[2];
            return 1;
        }
        q = tok_skip(q);
    }
}

static void read_item(const char **f, uint16_t *fl)
{
    while (!data_ok || data_p > data_e) {
        if (!data_find())
            error(E_OD);
        data_ok = 1;
    }
    field(&data_p, data_e, f, fl);
}

static void do_read(void)
{
    for (;;) {
        lval_t lv;
        const char *f;
        uint16_t fl;
        lvalue(&lv);
        read_item(&f, &fl);
        if (!store_text(&lv, f, fl)) {
            lp = data_lp;
            error(E_SN);
        }
        if (*ip != ',')
            return;
        ip++;
    }
}

/* ---- control flow ---- */

static void do_if(void)
{
    expr();
    if (num_of(&vs[vsp - 1]) != 0) {
        vsp--;
        if (*ip == KW_GOTO) {
            ip++;
            goto_line(arg_line());
            return;
        }
        need(KW_THEN);
        if (*ip == TK_INT || *ip == TK_FLT)
            goto_line(arg_line());
        jumped = 1; /* the run loop executes the statements after THEN */
        return;
    }
    vsp--;
    {
        int depth = 0;
        while (*ip) {
            if (*ip == KW_IF)
                depth++;
            else if (*ip == KW_ELSE) {
                if (!depth) {
                    ip++;
                    if (*ip == TK_INT || *ip == TK_FLT)
                        goto_line(arg_line());
                    jumped = 1;
                    return;
                }
                depth--;
            }
            ip = tok_skip(ip);
        }
    }
}

static ctl_t *ctl_push(void)
{
    if (csp >= CTLMAX)
        error(E_OM);
    return &ctl[csp++];
}

static int count_next_vars(uint8_t *p)
{
    int n = 1;
    if (*p != TK_VAR)
        return 1;
    for (;;) {
        p += 3;
        if (*p != ',' || p[1] != TK_VAR)
            return n;
        p++;
        n++;
    }
}

static void do_next(void);

/* The loop runs zero times: continue after the matching NEXT. */
static void skip_for(void)
{
    int depth = 0;
    for (;;) {
        if (!*ip) {
            if (!lp)
                error(E_NF);
            lp += LLEN(lp);
            if (!LLEN(lp))
                error(E_NF);
            ip = lp + 4;
            continue;
        }
        if (*ip == KW_FOR)
            depth++;
        else if (*ip == KW_NEXT) {
            int n = count_next_vars(ip + 1);
            if (depth - n < 0) {
                int k;
                ip++;
                for (k = 0; k <= depth && *ip == TK_VAR; k++)
                    ip += k < depth ? 4 : 3; /* var, then ',' */
                jumped = 1;
                if (*ip == ',') {
                    ip++;
                    do_next();
                }
                return;
            }
            depth -= n;
        }
        ip = tok_skip(ip);
    }
}

static void do_for(void)
{
    lval_t lv;
    ctl_t *c;
    int i;
    float lim, step = 1.0f;
    lvalue(&lv);
    if (lv.p != (void *)&vars[lv.id] || lv.type == VT_STR)
        error(E_TM);
    need('=');
    expr();
    store(&lv);
    need(KW_TO);
    lim = arg_num();
    if (*ip == KW_STEP) {
        ip++;
        step = arg_num();
    }
    for (i = csp - 1; i >= 0; i--) /* reusing a loop variable drops the old loop */
        if (ctl[i].kind == C_FOR && ctl[i].var == lv.id) {
            csp = i;
            break;
        }
    {
        float v = lv.type == VT_INT ? (float)vars[lv.id].i : vars[lv.id].f;
        if (step >= 0 ? v > lim : v < lim) {
            skip_for();
            return;
        }
    }
    c = ctl_push();
    c->kind = C_FOR;
    c->var = lv.id;
    c->isint = lv.type == VT_INT;
    c->lim = lim;
    c->step = step;
    c->lp = lp;
    c->ip = ip;
}

static void do_next(void)
{
    for (;;) {
        int i = csp - 1;
        ctl_t *c;
        int done;
        if (*ip == TK_VAR) {
            uint16_t id = RD16(ip + 1);
            ip += 3;
            while (i >= 0 && !(ctl[i].kind == C_FOR && ctl[i].var == id))
                i--;
        } else {
            while (i >= 0 && ctl[i].kind != C_FOR)
                i--;
        }
        if (i < 0)
            error(E_NF);
        csp = i + 1;
        c = &ctl[i];
        if (c->isint) {
            int32_t v = (int32_t)vars[c->var].i + (int32_t)c->step;
            done = c->step >= 0 ? (float)v > c->lim : (float)v < c->lim;
            if (v < -32768L || v > 32767L)
                done = 1;
            else
                vars[c->var].i = (int16_t)v;
        } else {
            float v = vars[c->var].f + c->step;
            vars[c->var].f = v;
            done = c->step >= 0 ? v > c->lim : v < c->lim;
        }
        if (!done) {
            lp = c->lp;
            ip = c->ip;
            jumped = 1;
            return;
        }
        csp = i;
        if (*ip != ',')
            return;
        ip++;
    }
}

static void do_gosub_to(uint16_t n)
{
    ctl_t *c = ctl_push();
    c->kind = C_GOSUB;
    c->lp = lp;
    c->ip = ip;
    goto_line(n);
}

static void do_return(void)
{
    while (csp > 0 && ctl[csp - 1].kind != C_GOSUB)
        csp--;
    if (!csp)
        error(E_RG);
    csp--;
    lp = ctl[csp].lp;
    ip = ctl[csp].ip;
    skip_stmt();
}

static void do_on(void)
{
    int32_t n = arg_i32();
    uint8_t how = *ip;
    uint16_t lines[32];
    int cnt = 0;
    if (how != KW_GOTO && how != KW_GOSUB)
        error(E_SN);
    ip++;
    for (;;) {
        if (cnt >= 32)
            error(E_SN);
        lines[cnt++] = arg_line();
        if (*ip != ',')
            break;
        ip++;
    }
    if (n < 1 || n > cnt)
        return;
    if (how == KW_GOTO)
        goto_line(lines[n - 1]);
    else
        do_gosub_to(lines[n - 1]);
}

static void reset_run(void)
{
    clear_vars();
    csp = 0;
    data_restore(NULL);
    cont_ok = 0;
}

static void start_run(void)
{
    reset_run();
    if (!LLEN(arena)) {
        end_run();
        return;
    }
    lp = arena;
    ip = arena + 4;
    jumped = 1;
}

/* ---- DIM, DEF ---- */

static void do_dim(void)
{
    for (;;) {
        uint16_t id, dims[8];
        int nd = 0;
        if (*ip != TK_VAR)
            error(E_SN);
        id = RD16(ip + 1);
        ip += 3;
        need('(');
        for (;;) {
            int32_t d = arg_i32();
            if (d < 0 || d > 32766 || nd >= 8)
                error(E_FC);
            dims[nd++] = (uint16_t)(d + 1);
            if (*ip != ',')
                break;
            ip++;
        }
        need(')');
        arr_dim(id, nd, dims);
        if (*ip != ',')
            return;
        ip++;
    }
}

static void do_def(void)
{
    uint16_t id, pid;
    if (!lp)
        error(E_ID);
    need(KW_FN);
    if (*ip != TK_VAR)
        error(E_SN);
    id = RD16(ip + 1);
    ip += 3;
    need('(');
    if (*ip != TK_VAR)
        error(E_SN);
    pid = RD16(ip + 1);
    if (vtype[pid] == VT_STR)
        error(E_TM);
    ip += 3;
    need(')');
    need('=');
    fndef[id] = ip;
    fnparam[id] = pid;
    skip_stmt();
}

/* ---- files ---- */

static char fname[16];

static void get_fname(void)
{
    str_t s;
    uint16_t i, n;
    int dot = 0;
    expr();
    s = sval(&vs[vsp - 1]);
    vsp--;
    n = s.len;
    if (!n || n > 12)
        error(E_FC);
    for (i = 0; i < n; i++) {
        char c = s.p[i];
        if (c >= 'a' && c <= 'z')
            c = (char)(c - 32);
        if (c == '.')
            dot = 1;
        if (c == '/' || c == '\\' || c == ':' || c <= ' ')
            error(E_FC);
        fname[i] = c;
    }
    fname[n] = 0;
    if (!dot && n <= 8)
        memcpy(fname + n, ".BAS", 5);
}

static void do_save(void)
{
    int h;
    uint8_t *l;
    get_fname();
    h = hal_fopen(fname, 1);
    if (h < 0)
        error(E_FIO);
    for (l = arena; LLEN(l); l += LLEN(l)) {
        int n = list_line(l, linebuf);
        linebuf[n++] = '\n';
        if (hal_fwrite(h, linebuf, n) != n) {
            hal_fclose(h);
            error(E_FIO);
        }
    }
    hal_fclose(h);
}

static uint8_t tokbuf[TOKMAX];

/* Store a typed or loaded line. Returns 1 if it had a line number. */
static int enter_line(const char *s)
{
    uint32_t num = 0;
    int n;
    while (*s == ' ')
        s++;
    if (*s < '0' || *s > '9')
        return 0;
    while (*s >= '0' && *s <= '9') {
        num = num * 10u + (uint32_t)(*s++ - '0');
        if (num > 65535UL)
            error(E_SN);
    }
    n = tokenize(s, tokbuf, TOKMAX);
    prog_edit((uint16_t)num, tokbuf, (uint16_t)n);
    cont_ok = 0;
    return 1;
}

static void do_load(void)
{
    int h, n, len = 0, i;
    static char rb[128];
    get_fname();
    h = hal_fopen(fname, 0);
    if (h < 0)
        error(E_FNF);
    prog_new();
    reset_run();
    while ((n = hal_fread(h, rb, (int)sizeof rb)) > 0) {
        for (i = 0; i < n; i++) {
            char c = rb[i];
            if (c == '\r')
                continue;
            if (c == '\n') {
                linebuf[len] = 0;
                if (len)
                    enter_line(linebuf);
                len = 0;
            } else if (len < LINEMAX - 1)
                linebuf[len++] = c;
        }
    }
    if (len) {
        linebuf[len] = 0;
        enter_line(linebuf);
    }
    hal_fclose(h);
}

static void do_dir(void)
{
    char name[16];
    uint32_t size;
    if (hal_dir_open() < 0)
        error(E_FIO);
    while (hal_dir_next(name, &size) == 1) {
        char b[12];
        int n = (int)strlen(name);
        con_puts(name);
        while (n++ < 14)
            con_putc(' ');
        fmt_u32(size, b);
        con_puts(b);
        con_putc('\n');
        if (hal_break())
            brk();
    }
}

/* ---- LIST ---- */

static int tok_linenum(uint16_t *n)
{
    float f;
    if (*ip == TK_INT) {
        *n = RD16(ip + 1);
        ip += 3;
        return 1;
    }
    if (*ip == TK_FLT) {
        union { float f; uint8_t b[4]; } u;
        memcpy(u.b, ip + 1, 4);
        f = u.f;
        if (f < 0 || f > 65535.0f)
            error(E_FC);
        *n = (uint16_t)f;
        ip += 5;
        return 1;
    }
    return 0;
}

static void do_list(void)
{
    uint16_t a = 0, b = 65535u;
    uint8_t *l;
    if (tok_linenum(&a)) {
        b = a;
        if (*ip == '-') {
            ip++;
            b = 65535u;
            tok_linenum(&b);
        }
    } else if (*ip == '-') {
        ip++;
        tok_linenum(&b);
    }
    for (l = arena; LLEN(l); l += LLEN(l)) {
        if (LNUM(l) < a)
            continue;
        if (LNUM(l) > b)
            break;
        list_line(l, linebuf);
        con_puts(linebuf);
        con_putc('\n');
        if (hal_break())
            brk();
    }
}

/* ---- graphics and sound statements ---- */

static void two(int16_t *x, int16_t *y)
{
    *x = arg_i16();
    comma();
    *y = arg_i16();
}

static void do_line(void)
{
    int16_t x, y;
    if (*ip != KW_TO) {
        int16_t x0, y0;
        two(&x0, &y0);
        need(KW_TO);
        two(&x, &y);
        gfx_line(x0, y0, x, y);
    } else {
        ip++;
        two(&x, &y);
        gfx_line_to(x, y);
    }
    while (*ip == KW_TO) {
        ip++;
        two(&x, &y);
        gfx_line_to(x, y);
    }
}

static void do_sprite(void)
{
    int16_t n = arg_i16();
    uint8_t t = *ip++;
    if (t == KW_DEFINE) {
        int16_t w, h;
        two(&w, &h);
        spr_define(n, w, h);
        if (*ip == KW_READ) {
            int16_t r;
            ip++;
            for (r = 0; r < h; r++) {
                const char *f;
                uint16_t fl;
                read_item(&f, &fl);
                spr_pixels(n, f, fl);
            }
            return;
        }
        while (*ip == ',') {
            str_t s;
            ip++;
            expr();
            s = sval(&vs[vsp - 1]);
            spr_pixels(n, s.p, s.len);
            vsp--;
        }
    } else if (t == KW_AT) {
        int16_t x, y;
        two(&x, &y);
        spr_at(n, x, y);
    } else if (t == KW_OFF) {
        spr_off(n);
    } else
        error(E_SN);
}

static void do_pause(void)
{
    int32_t n = arg_i32();
    uint32_t end;
    if (n < 0)
        error(E_FC);
    end = hal_frame() + (uint32_t)n;
    while ((int32_t)(hal_frame() - end) < 0) {
        if (hal_break())
            brk();
        snd_poll();
    }
}

/* ---- statement dispatch ---- */

static void exec_stmt(uint8_t t)
{
    switch (t) {
    case TK_VAR:
        ip--;
        do_let();
        return;
    case KW_LET: do_let(); return;
    case KW_PRINT: do_print(); return;
    case KW_REM: skip_line(); return;
    case KW_DATA:
        if (*ip == TK_RAW)
            ip += 2 + ip[1];
        return;
    case KW_IF: do_if(); return;
    case KW_GOTO: goto_line(arg_line()); return;
    case KW_GOSUB: {
        uint16_t n = arg_line();
        do_gosub_to(n);
        return;
    }
    case KW_RETURN: do_return(); return;
    case KW_FOR: do_for(); return;
    case KW_NEXT: do_next(); return;
    case KW_ON: do_on(); return;
    case KW_END: end_run(); return;
    case KW_STOP:
        stmt_ip = ip;
        brk();
    case KW_INPUT: do_input(); return;
    case KW_READ: do_read(); return;
    case KW_RESTORE:
        if (at_end())
            data_restore(NULL);
        else {
            uint8_t *l = find_line(arg_line(), 1);
            if (!l)
                error(E_UL);
            data_restore(l);
        }
        return;
    case KW_DIM: do_dim(); return;
    case KW_DEF: do_def(); return;
    case KW_GET: {
        lval_t lv;
        int k;
        char c;
        lvalue(&lv);
        con_flush();
        k = hal_getkey(1);
        if (k == HAL_EOF)
            hal_exit(0);
        if (k == 3)
            brk();
        if (lv.type == VT_STR) {
            c = (char)k;
            push_newstr(&c, 1);
        } else
            push_int(k);
        store(&lv);
        return;
    }
    case KW_POKE:
    case KW_DOKE: {
        int32_t a = arg_i32(), v;
        comma();
        v = arg_i32();
        if (a < 0 || a > 0xFFFFFFL || v < -32768L || v > (t == KW_POKE ? 255L : 65535L))
            error(E_FC);
        if (t == KW_POKE)
            hal_poke8((uint32_t)a, (uint8_t)v);
        else {
            if (a & 1)
                error(E_FC);
            hal_poke16((uint32_t)a, (uint16_t)v);
        }
        return;
    }
    case KW_CALL: {
        int32_t a = arg_i32();
        if (a < 0 || a > 0xFFFFFFL || (a & 1))
            error(E_FC);
        hal_call((uint32_t)a);
        return;
    }
    case KW_RUN:
        if (*ip == TK_STR || *ip == TK_VAR) {
            do_load();
            start_run();
        } else if (at_end())
            start_run();
        else {
            uint16_t n = arg_line();
            reset_run();
            goto_line(n);
        }
        return;
    case KW_CONT:
        if (!cont_ok)
            error(E_CN);
        lp = cont_lp;
        ip = cont_ip;
        jumped = 1;
        return;
    case KW_LIST: do_list(); return;
    case KW_CLEAR: reset_run(); return;
    case KW_NEW:
        prog_new();
        reset_run();
        end_run();
        return;
    case KW_LOAD:
        do_load();
        end_run();
        return;
    case KW_SAVE: do_save(); return;
    case KW_DIR: do_dir(); return;
    case KW_DEL:
        get_fname();
        if (hal_fdelete(fname) < 0)
            error(E_FNF);
        return;
    case KW_CLS: con_cls(); return;
    case KW_LOCATE: {
        int16_t r, c;
        two(&r, &c);
        con_locate(r, c);
        return;
    }
    case KW_COLOR: {
        int16_t fg = arg_i16(), bg = 8;
        if (*ip == ',') {
            ip++;
            bg = arg_i16();
        }
        if (fg < 0 || fg > 7 || bg < 0 || bg > 8)
            error(E_FC);
        con_color(fg, bg == 8 ? -1 : bg);
        return;
    }
    case KW_TEXT: gfx_mode(0); return;
    case KW_BACKDROP: {
        int16_t c = arg_i16();
        if (c < 0 || c > 7)
            error(E_FC);
        gfx_backdrop(c);
        return;
    }
    case KW_GR: gfx_mode(1); return;
    case KW_HGR: gfx_mode(2); return;
    case KW_GCLS: gfx_clear(at_end() ? 8 : arg_i16()); return;
    case KW_PEN: gfx_pen(arg_i16()); return;
    case KW_PLOT: {
        int16_t x, y;
        two(&x, &y);
        gfx_plot(x, y);
        return;
    }
    case KW_LINE: do_line(); return;
    case KW_BOX: {
        int16_t x0, y0, x1, y1, f = 0;
        two(&x0, &y0);
        comma();
        two(&x1, &y1);
        if (*ip == ',') {
            ip++;
            f = arg_i16();
        }
        gfx_box(x0, y0, x1, y1, f);
        return;
    }
    case KW_CIRCLE: {
        int16_t x, y, r, f = 0;
        two(&x, &y);
        comma();
        r = arg_i16();
        if (*ip == ',') {
            ip++;
            f = arg_i16();
        }
        gfx_circle(x, y, r, f);
        return;
    }
    case KW_PALETTE: {
        int16_t i, r, g, b;
        two(&i, &r);
        comma();
        two(&g, &b);
        gfx_palette(i, r, g, b);
        return;
    }
    case KW_SCROLL: {
        int16_t x, y;
        two(&x, &y);
        gfx_scroll(x, y);
        return;
    }
    case KW_SPRITE: do_sprite(); return;
    case KW_VSYNC: vsync(); return;
    case KW_SOUND: {
        int16_t v, vol, d = 0;
        int32_t f;
        v = arg_i16();
        comma();
        f = arg_i32();
        comma();
        vol = arg_i16();
        if (*ip == ',') {
            ip++;
            d = arg_i16();
        }
        if (f < 0 || f > 65535L)
            error(E_FC);
        snd_set(v, (uint16_t)f, vol, d);
        return;
    }
    case KW_WAVE: {
        int16_t v, w;
        two(&v, &w);
        snd_wave(v, w);
        return;
    }
    case KW_BEEP: snd_set(0, 880, 12, 8); return;
    case KW_PAUSE: do_pause(); return;
    }
    error(E_SN);
}

static void run(void)
{
    for (;;) {
        uint8_t t = *ip;
        if (t == 0) {
            if (!lp)
                return;
            lp += LLEN(lp);
            if (!LLEN(lp)) {
                end_run();
                return;
            }
            ip = lp + 4;
            continue;
        }
        if (t == ':') {
            ip++;
            continue;
        }
        stmt_ip = ip;
        if (!(++brk_count & 31)) {
            if (hal_break())
                brk();
            snd_poll();
        }
        ip++;
        jumped = 0;
        exec_stmt(t);
        if (jumped)
            continue;
        if (*ip == KW_ELSE)
            skip_line();
        else if (*ip && *ip != ':')
            error(E_SN);
    }
}

/* ---- prompt ---- */

static void banner(void)
{
    char b[12];
    con_puts("FPGA BASIC 0.1\n");
    fmt_u32(mem_free(), b);
    con_puts(b);
    con_puts(" BYTES FREE\n\n");
}

void basic_main(const char *autorun, int quiet)
{
    uint32_t size;
    uint8_t *a = hal_arena(&size);
    volatile int ready = 1;
    uintptr_t al = ((uintptr_t)a + 7u) & ~(uintptr_t)7u;

    arena = (uint8_t *)al;
    arena_top = (uint8_t *)(((uintptr_t)a + size) & ~(uintptr_t)7u);
    rnd_seed(0x2545F491UL);
    gfx_init();
    con_init();
    prog_new();
    reset_run();
    if (!quiet)
        banner();

    if (setjmp(top)) {
        vsp = 0;
        lp = NULL;
        ready = 1;
        if (autorun_mode)
            hal_exit(1);
    } else if (autorun) {
        int n = (int)strlen(autorun);
        autorun_mode = 1;
        if (n > LINEMAX - 10)
            hal_exit(2);
        memcpy(linebuf, "RUN \"", 5);
        memcpy(linebuf + 5, autorun, (size_t)n);
        linebuf[5 + n] = '"';
        linebuf[6 + n] = 0;
        tokenize(linebuf, imm, TOKMAX);
        lp = NULL;
        ip = imm;
        run();
        spr_flush();
        hal_exit(0);
    }

    for (;;) {
        int n;
        spr_flush();
        if (ready)
            con_puts("READY.\n");
        ready = 0;
        n = read_line(linebuf, LINEMAX);
        if (enter_line(linebuf))
            continue;
        tokenize(linebuf, imm, TOKMAX);
        if (!imm[0])
            continue;
        lp = NULL;
        ip = imm;
        run();
        ready = 1;
        (void)n;
    }
}
