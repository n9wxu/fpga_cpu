/* Expression evaluator.
 *
 * Values live on an explicit stack (vs[]), so the string collector can
 * find every temporary. Precedence, lowest first:
 *   OR, AND, NOT, relations, + -, * / \ MOD, unary -, ^
 * Integer (%) operands stay integers through + - * \ MOD and
 * relations while the result fits in 16 bits; / always gives a float.
 */
#include <math.h>

#include "basic.h"

#define FLT_BIG 3.4028235e38f

static void e_or(void);
int expr_depth;
void expr(void)
{
    if (++expr_depth > EXPR_DEPTH_MAX)
        error(E_ST); /* runaway recursion (DEF FN calling itself, ((((...) */
    e_or();
    expr_depth--;
}

void need(uint8_t t)
{
    if (*ip != t)
        error(E_SN);
    ip++;
}

static val_t *push(void)
{
    if (vsp >= VSMAX)
        error(E_ST);
    return &vs[vsp++];
}

void push_num(float f)
{
    val_t *v;
    if (f > FLT_BIG || f < -FLT_BIG || f != f)
        error(E_OV);
    v = push();
    v->t = T_NUM;
    v->u.f = f;
}

void push_int(int32_t i)
{
    if (i < -32768L || i > 32767L) {
        push_num((float)i);
        return;
    }
    {
        val_t *v = push();
        v->t = T_INT;
        v->u.i = (int16_t)i;
    }
}

void push_newstr(const char *s, uint16_t len)
{
    val_t *v;
    char *p = NULL;
    if (len)
        p = str_alloc(len);
    if (len)
        memcpy(p, s, len);
    v = push();
    v->t = T_STR;
    v->u.s.p = p;
    v->u.s.len = len;
}

float num_of(const val_t *v)
{
    if (v->t == T_NUM)
        return v->u.f;
    if (v->t == T_INT)
        return (float)v->u.i;
    error(E_TM);
}

str_t sval(const val_t *v)
{
    if (v->t == T_STR)
        return v->u.s;
    if (v->t == T_SREF)
        return *v->u.r;
    error(E_TM);
}

float pop_num(void)
{
    vsp--;
    return num_of(&vs[vsp]);
}

int32_t to_i32(float f)
{
    if (f >= 2147483647.0f || f < -2147483647.0f)
        error(E_FC);
    return (int32_t)floorf(f);
}

int16_t to_i16(float f)
{
    if (f >= 32768.0f || f < -32768.0f)
        error(E_FC);
    return (int16_t)floorf(f);
}

int16_t pop_i16(void)
{
    vsp--;
    if (vs[vsp].t == T_INT)
        return vs[vsp].u.i;
    return to_i16(num_of(&vs[vsp]));
}

int32_t pop_i32(void)
{
    vsp--;
    if (vs[vsp].t == T_INT)
        return vs[vsp].u.i;
    return to_i32(num_of(&vs[vsp]));
}

/* 16-bit pattern for AND/OR/NOT: accepts -32768..65535. */
static uint16_t pop_bits(void)
{
    float f;
    vsp--;
    if (vs[vsp].t == T_INT)
        return (uint16_t)vs[vsp].u.i;
    f = num_of(&vs[vsp]);
    if (f >= 65536.0f || f < -32768.0f)
        error(E_FC);
    return (uint16_t)(int32_t)floorf(f);
}

/* Store the top of the stack in a string variable, then pop it. A heap
 * temporary is moved; anything else is copied. */
void assign_str(str_t *dst)
{
    val_t *v = &vs[vsp - 1];
    str_t s = sval(v);
    char *p;
    if (v->t == T_STR && s.p && (uint8_t *)s.p >= str_bot && (uint8_t *)s.p < arena_top) {
        *dst = s;
        vsp--;
        return;
    }
    if (!s.len) {
        dst->p = NULL;
        dst->len = 0;
        vsp--;
        return;
    }
    p = str_alloc(s.len); /* may move strings: fetch the source again */
    s = sval(v);
    memcpy(p, s.p, s.len);
    dst->p = p;
    dst->len = s.len;
    vsp--;
}

/* Replace the string on top of the stack with a copy of part of it. */
static void top_substr(uint16_t off, uint16_t n)
{
    val_t *v = &vs[vsp - 1];
    str_t s;
    char *p = NULL;
    if (n) {
        p = str_alloc(n);
        s = sval(v);
        memcpy(p, s.p + off, n);
    }
    v->t = T_STR;
    v->u.s.p = p;
    v->u.s.len = n;
}

static int is_num(const val_t *v) { return v->t == T_NUM || v->t == T_INT; }

static int cmp_str(void)
{
    str_t a = sval(&vs[vsp - 2]), b = sval(&vs[vsp - 1]);
    uint16_t n = a.len < b.len ? a.len : b.len;
    int c = n ? memcmp(a.p, b.p, n) : 0;
    if (!c)
        c = a.len < b.len ? -1 : a.len > b.len;
    return c < 0 ? -1 : c > 0;
}

static int rel_true(uint8_t op, int c)
{
    switch (op) {
    case '=': return c == 0;
    case '<': return c < 0;
    case '>': return c > 0;
    case TK_LE: return c <= 0;
    case TK_GE: return c >= 0;
    default: return c != 0; /* TK_NE */
    }
}

static int is_rel(uint8_t op)
{
    return op == '=' || op == '<' || op == '>' || op == TK_LE || op == TK_GE || op == TK_NE;
}

static float fpow(float x, float y)
{
    if (y == floorf(y) && y >= -64.0f && y <= 64.0f) {
        int n = (int)(y < 0 ? -y : y);
        float r = 1.0f, b = x;
        while (n) {
            if (n & 1)
                r *= b;
            b *= b;
            n >>= 1;
        }
        if (y < 0) {
            if (r == 0)
                error(E_DZ);
            r = 1.0f / r;
        }
        return r;
    }
    if (x < 0)
        error(E_FC);
    if (x == 0)
        return 0;
    return powf(x, y);
}

static void binop(uint8_t op)
{
    val_t *a = &vs[vsp - 2], *b = &vs[vsp - 1];

    if (!is_num(a) || !is_num(b)) {
        if (is_num(a) || is_num(b))
            error(E_TM);
        if (op == '+') {
            str_t sa = sval(a), sb = sval(b);
            uint16_t n = (uint16_t)(sa.len + sb.len);
            char *p;
            if (n > 255)
                error(E_LS);
            p = n ? str_alloc(n) : NULL;
            sa = sval(a);
            sb = sval(b);
            if (sa.len)
                memcpy(p, sa.p, sa.len);
            if (sb.len)
                memcpy(p + sa.len, sb.p, sb.len);
            vsp--;
            a->t = T_STR;
            a->u.s.p = p;
            a->u.s.len = n;
            return;
        }
        if (is_rel(op)) {
            int c = cmp_str();
            vsp -= 2;
            push_int(rel_true(op, c) ? -1 : 0);
            return;
        }
        error(E_TM);
    }

    if (a->t == T_INT && b->t == T_INT && op != '/' && op != '^') {
        int32_t x = a->u.i, y = b->u.i, r;
        vsp -= 2;
        switch (op) {
        case '+': r = x + y; break;
        case '-': r = x - y; break;
        case '*': r = x * y; break;
        case '\\':
            if (!y)
                error(E_DZ);
            r = x / y;
            break;
        case KW_MOD:
            if (!y)
                error(E_DZ);
            r = x % y;
            break;
        default: r = rel_true(op, x < y ? -1 : x > y) ? -1 : 0; break;
        }
        push_int(r);
        return;
    }

    {
        float x = num_of(a), y = num_of(b), r;
        vsp -= 2;
        switch (op) {
        case '+': r = x + y; break;
        case '-': r = x - y; break;
        case '*': r = x * y; break;
        case '/':
            if (y == 0)
                error(E_DZ);
            r = x / y;
            break;
        case '^': r = fpow(x, y); break;
        case '\\': {
            int32_t ix = to_i32(truncf(x)), iy = to_i32(truncf(y)); /* toward zero, like the integer path */
            if (!iy)
                error(E_DZ);
            push_int(ix / iy);
            return;
        }
        case KW_MOD:
            if (y == 0)
                error(E_DZ);
            r = fmodf(x, y);
            break;
        default: push_int(rel_true(op, x < y ? -1 : x > y) ? -1 : 0); return;
        }
        push_num(r);
    }
}

static void negate(void)
{
    val_t *v = &vs[vsp - 1];
    if (v->t == T_INT) {
        if (v->u.i == -32768) {
            v->t = T_NUM;
            v->u.f = 32768.0f;
        } else
            v->u.i = (int16_t)-v->u.i;
    } else if (v->t == T_NUM)
        v->u.f = -v->u.f;
    else
        error(E_TM);
}

/* ---- functions ---- */

static uint32_t rnd_state = 0x2545F491UL;
static float rnd_last = 0.5f;

void rnd_seed(uint32_t s) { rnd_state = s ? s : 0x2545F491UL; }

static float rnd(float x)
{
    if (x < 0) {
        union { float f; uint32_t u; } u;
        u.f = x;
        rnd_seed(u.u * 2654435761UL);
    } else if (x == 0)
        return rnd_last;
    rnd_state ^= rnd_state << 13;
    rnd_state ^= rnd_state >> 17;
    rnd_state ^= rnd_state << 5;
    rnd_last = (float)(rnd_state >> 8) * (1.0f / 16777216.0f);
    return rnd_last;
}

static void paren_expr(void)
{
    need('(');
    expr();
    need(')');
}

static float paren_num(void)
{
    paren_expr();
    return pop_num();
}

static void str_func(uint8_t t)
{
    char buf[16];
    switch (t) {
    case KW_LEFTS:
    case KW_RIGHTS: {
        int32_t n;
        str_t s;
        need('(');
        expr();
        need(',');
        expr();
        need(')');
        n = pop_i32();
        if (n < 0 || n > 255)
            error(E_FC);
        s = sval(&vs[vsp - 1]);
        if (n > s.len)
            n = s.len;
        top_substr(t == KW_LEFTS ? 0 : (uint16_t)(s.len - n), (uint16_t)n);
        return;
    }
    case KW_MIDS: {
        int32_t st, n = 255;
        str_t s;
        need('(');
        expr();
        need(',');
        expr();
        if (*ip == ',') {
            ip++;
            expr();
            n = pop_i32();
        }
        need(')');
        st = pop_i32();
        if (st < 1 || st > 255 || n < 0 || n > 255)
            error(E_FC);
        s = sval(&vs[vsp - 1]);
        if (st > s.len)
            n = 0;
        else if (st - 1 + n > s.len)
            n = s.len - (st - 1);
        top_substr((uint16_t)(st - 1), (uint16_t)n);
        return;
    }
    case KW_CHRS: {
        int32_t c = (int32_t)paren_num();
        if (c < 0 || c > 255)
            error(E_FC);
        buf[0] = (char)c;
        push_newstr(buf, 1);
        return;
    }
    case KW_STRS: {
        int n = fmt_num(paren_num(), buf);
        push_newstr(buf, (uint16_t)n);
        return;
    }
    case KW_HEXS: {
        float f = paren_num();
        uint32_t v;
        int n = 0, i;
        char t2[8];
        if (f < 0) {
            if (f < -32768.0f)
                error(E_FC);
            v = (uint16_t)(int16_t)f;
        } else {
            if (f >= 4294967296.0f)
                error(E_FC);
            v = (uint32_t)f;
        }
        do {
            t2[n++] = "0123456789ABCDEF"[v & 15u];
            v >>= 4;
        } while (v);
        for (i = 0; i < n; i++)
            buf[i] = t2[n - 1 - i];
        push_newstr(buf, (uint16_t)n);
        return;
    }
    case KW_INKEYS: {
        int k = hal_getkey(0);
        if (k == 3)
            brk();
        if (k < 0) {
            push_newstr(NULL, 0);
            return;
        }
        buf[0] = (char)k;
        push_newstr(buf, 1);
        return;
    }
    }
    error(E_SN);
}

static void call_fn(void)
{
    uint16_t id, pid;
    var_t save;
    uint8_t *ret;
    if (*ip != TK_VAR)
        error(E_SN);
    id = RD16(ip + 1);
    ip += 3;
    if (!fndef[id])
        error(E_UF);
    pid = fnparam[id];
    paren_expr();
    save = vars[pid];
    if (vtype[pid] == VT_INT)
        vars[pid].i = pop_i16();
    else
        vars[pid].f = pop_num();
    ret = ip;
    ip = fndef[id];
    expr();
    if (*ip && *ip != ':')
        error(E_SN);
    ip = ret;
    vars[pid] = save;
    if ((vtype[id] == VT_STR) != !is_num(&vs[vsp - 1]))
        error(E_TM);
}

static void e_primary(void)
{
    uint8_t t = *ip++;
    switch (t) {
    case TK_INT:
        push_int((int16_t)RD16(ip));
        ip += 2;
        return;
    case TK_FLT: {
        union { float f; uint8_t b[4]; } u;
        memcpy(u.b, ip, 4);
        ip += 4;
        push_num(u.f);
        return;
    }
    case TK_STR: {
        val_t *v = push();
        v->t = T_STR;
        v->u.s.len = *ip;
        v->u.s.p = *ip ? (char *)ip + 1 : NULL;
        ip += 1 + *ip;
        return;
    }
    case TK_VAR: {
        uint16_t id = RD16(ip);
        void *e;
        val_t *v;
        ip += 2;
        e = *ip == '(' ? arr_ref(id) : (void *)&vars[id];
        v = push();
        switch (vtype[id]) {
        case VT_NUM:
            v->t = T_NUM;
            v->u.f = *(float *)e;
            break;
        case VT_INT:
            v->t = T_INT;
            v->u.i = *(int16_t *)e;
            break;
        default:
            v->t = T_SREF;
            v->u.r = (str_t *)e;
            break;
        }
        return;
    }
    case '(':
        expr();
        need(')');
        return;
    case KW_FN: call_fn(); return;
    case KW_ABS:
        paren_expr();
        if (vs[vsp - 1].t == T_INT) {
            if (vs[vsp - 1].u.i < 0)
                negate();
        } else
            push_num(fabsf(pop_num()));
        return;
    case KW_INT:
        paren_expr();
        if (vs[vsp - 1].t != T_INT)
            push_num(floorf(pop_num()));
        return;
    case KW_SGN: {
        float f = paren_num();
        push_int(f > 0 ? 1 : f < 0 ? -1 : 0);
        return;
    }
    case KW_SQR: {
        float f = paren_num();
        if (f < 0)
            error(E_FC);
        push_num(sqrtf(f));
        return;
    }
    case KW_SIN: push_num(sinf(paren_num())); return;
    case KW_COS: push_num(cosf(paren_num())); return;
    case KW_TAN: push_num(tanf(paren_num())); return;
    case KW_ATN: push_num(atanf(paren_num())); return;
    case KW_EXP: {
        float f = paren_num();
        if (f > 88.72f)
            error(E_OV);
        push_num(expf(f));
        return;
    }
    case KW_LOG: {
        float f = paren_num();
        if (f <= 0)
            error(E_FC);
        push_num(logf(f));
        return;
    }
    case KW_RND: push_num(rnd(paren_num())); return;
    case KW_LEN:
    case KW_ASC: {
        str_t s;
        paren_expr();
        s = sval(&vs[vsp - 1]);
        vsp--;
        if (t == KW_LEN)
            push_int(s.len);
        else {
            if (!s.len)
                error(E_FC);
            push_int((uint8_t)s.p[0]);
        }
        return;
    }
    case KW_VAL: {
        str_t s;
        char buf[NUMTXT + 1];
        uint16_t n, i = 0;
        float f = 0;
        int isint, neg = 0;
        paren_expr();
        s = sval(&vs[vsp - 1]);
        if (s.len > NUMTXT)
            error(E_LS); /* too long to be a number: don't convert a prefix */
        n = s.len;
        if (n)
            memcpy(buf, s.p, n);
        buf[n] = 0;
        vsp--;
        while (buf[i] == ' ')
            i++;
        if (buf[i] == '-' || buf[i] == '+')
            neg = buf[i++] == '-';
        if (!parse_num(buf + i, &f, &isint))
            f = 0;
        push_num(neg ? -f : f);
        return;
    }
    case KW_INSTR: {
        str_t a, b;
        int32_t st = 1, i;
        int r = 0;
        need('(');
        expr();
        need(',');
        expr();
        if (*ip == ',') {
            ip++;
            expr();
            st = pop_i32();
            if (st < 1 || st > 255)
                error(E_FC);
        }
        need(')');
        a = sval(&vs[vsp - 2]);
        b = sval(&vs[vsp - 1]);
        for (i = st - 1; i + b.len <= a.len; i++)
            if (!b.len || !memcmp(a.p + i, b.p, b.len)) {
                r = (int)i + 1;
                break;
            }
        vsp -= 2;
        push_int(r);
        return;
    }
    case KW_FRE:
        paren_expr();
        vsp--;
        push_num((float)mem_free());
        return;
    case KW_PEEK:
    case KW_DEEK: {
        int32_t a = to_i32(paren_num());
        if (a < 0 || a > 0xFFFFFFL)
            error(E_FC);
        if (t == KW_PEEK)
            push_int(hal_peek8((uint32_t)a));
        else {
            if (a & 1)
                error(E_FC);
            push_num((float)hal_peek16((uint32_t)a));
        }
        return;
    }
    case KW_POS:
        if (*ip == '(') {
            paren_expr();
            vsp--;
        }
        push_int(con_col());
        return;
    case KW_TIME: push_num((float)hal_frame()); return;
    case KW_FRAMES:
        push_int(sheet_frames(to_i16(paren_num())));
        return;
    case KW_SPRX:
    case KW_SPRY:
        push_int(spr_pos(to_i16(paren_num()), t == KW_SPRY));
        return;
    case KW_HIT: { /* HIT(a, b): -1 if they touch; HIT(a): the lowest sprite touching a, or -1 */
        int16_t a, b = -1;
        need('(');
        expr();
        a = pop_i16();
        if (*ip == ',') {
            ip++;
            expr();
            b = pop_i16();
            if (b < 0)
                error(E_FC);
        }
        need(')');
        push_int(spr_hit(a, b));
        return;
    }
    case KW_PAL: { /* PAL(i, c): component c (0 red, 1 green, 2 blue) of palette entry i */
        int16_t i, c;
        need('(');
        expr();
        need(',');
        expr();
        need(')');
        c = pop_i16();
        i = pop_i16();
        push_int(gfx_pal_get(i, c));
        return;
    }
    case KW_POINT: {
        int16_t x, y;
        need('(');
        expr();
        need(',');
        expr();
        need(')');
        y = pop_i16();
        x = pop_i16();
        push_int(gfx_point(x, y));
        return;
    }
    case KW_LEFTS:
    case KW_RIGHTS:
    case KW_MIDS:
    case KW_CHRS:
    case KW_STRS:
    case KW_HEXS:
    case KW_INKEYS: str_func(t); return;
    }
    error(E_SN);
}

static void e_pow_operand(void)
{
    if (*ip == '-') {
        ip++;
        e_pow_operand();
        negate();
    } else if (*ip == '+') {
        ip++;
        e_pow_operand();
    } else
        e_primary();
}

static void e_pow(void)
{
    e_primary();
    while (*ip == '^') {
        ip++;
        e_pow_operand();
        binop('^');
    }
}

static void e_unary(void)
{
    if (*ip == '-') {
        ip++;
        e_unary();
        negate();
    } else if (*ip == '+') {
        ip++;
        e_unary();
        if (!is_num(&vs[vsp - 1]))
            error(E_TM);
    } else
        e_pow();
}

static void e_mul(void)
{
    e_unary();
    for (;;) {
        uint8_t op = *ip;
        if (op != '*' && op != '/' && op != '\\' && op != KW_MOD)
            return;
        ip++;
        e_unary();
        binop(op);
    }
}

static void e_add(void)
{
    e_mul();
    for (;;) {
        uint8_t op = *ip;
        if (op != '+' && op != '-')
            return;
        ip++;
        e_mul();
        binop(op);
    }
}

static void e_rel(void)
{
    e_add();
    while (is_rel(*ip)) {
        uint8_t op = *ip++;
        e_add();
        binop(op);
    }
}

static void e_not(void)
{
    if (*ip == KW_NOT) {
        ip++;
        e_not();
        push_int((int16_t)~pop_bits());
    } else
        e_rel();
}

static void e_and(void)
{
    e_not();
    while (*ip == KW_AND) {
        uint16_t a, b;
        ip++;
        e_not();
        b = pop_bits();
        a = pop_bits();
        push_int((int16_t)(a & b));
    }
}

static void e_or(void)
{
    e_and();
    while (*ip == KW_OR) {
        uint16_t a, b;
        ip++;
        e_and();
        b = pop_bits();
        a = pop_bits();
        push_int((int16_t)(a | b));
    }
}
