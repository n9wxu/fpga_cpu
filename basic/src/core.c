/* Interpreter state: names, tokenizer, program store, arrays, strings.
 *
 * Workspace layout (one arena from the HAL):
 *   [program lines][end marker][arrays ->]   free   [<- string heap]
 * Simple variables live in static tables indexed by name id, so a
 * variable reference in the token stream is a direct index.
 */
#include "basic.h"

uint8_t *arena, *arena_top, *prog_end, *arr_start, *arr_end, *str_bot;
uint8_t *ip, *lp;

uint16_t nnames;
static uint16_t npos;
static char npool[NPOOL];
static uint16_t noff[MAXV];
uint8_t vtype[MAXV];
var_t vars[MAXV];
uint8_t *arrs[MAXV];
uint8_t *fndef[MAXV];
uint16_t fnparam[MAXV];

val_t vs[VSMAX];
int vsp;

static int isdig(char c) { return c >= '0' && c <= '9'; }
static int isalph(char c) { return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z'); }
static char up(char c) { return (c >= 'a' && c <= 'z') ? (char)(c - 32) : c; }

/* ---- names ---- */

const char *var_name(uint16_t id, uint8_t *len)
{
    const char *p = npool + noff[id];
    *len = (uint8_t)p[0];
    return p + 1;
}

uint16_t var_intern(const char *s, uint8_t len)
{
    uint16_t i;
    for (i = 0; i < nnames; i++) {
        const char *p = npool + noff[i];
        if ((uint8_t)p[0] == len && !memcmp(p + 1, s, len))
            return i;
    }
    if (nnames >= MAXV || (uint32_t)npos + len + 1u > NPOOL)
        error(E_OM);
    noff[nnames] = npos;
    npool[npos] = (char)len;
    memcpy(npool + npos + 1, s, len);
    npos = (uint16_t)(npos + len + 1u);
    vtype[nnames] = s[len - 1] == '$' ? VT_STR : s[len - 1] == '%' ? VT_INT : VT_NUM;
    return nnames++;
}

/* ---- keywords ---- */

static const char *const kwnames[] = {
#define K(n, s) s,
    KEYWORDS
#undef K
};

const char *kw_name(uint8_t t) { return kwnames[t - KW_FIRST_ - 1]; }

static uint8_t kw_lookup(const char *w, int n)
{
    int i;
    for (i = 0; i < KW_LAST_ - KW_FIRST_ - 1; i++) {
        const char *k = kwnames[i];
        if ((int)strlen(k) == n && !memcmp(k, w, (size_t)n))
            return (uint8_t)(KW_FIRST_ + 1 + i);
    }
    return 0;
}

static int is_func_kw(uint8_t t) { return t >= KW_ABS && t <= KW_FN; }

/* ---- tokenizer ---- */

static uint8_t *emit_raw(uint8_t *o, const char *s, int n)
{
    if (n > 255)
        n = 255;
    *o++ = TK_RAW;
    *o++ = (uint8_t)n;
    memcpy(o, s, (size_t)n);
    return o + n;
}

static uint8_t *emit_num(uint8_t *o, float f, int isint)
{
    if (f > 3.4028235e38f)
        error(E_OV);
    if (isint) {
        int16_t i = (int16_t)f;
        *o++ = TK_INT;
        *o++ = (uint8_t)(i & 0xFF);
        *o++ = (uint8_t)((uint16_t)i >> 8);
    } else {
        union { float f; uint8_t b[4]; } u;
        u.f = f;
        *o++ = TK_FLT;
        memcpy(o, u.b, 4);
        o += 4;
    }
    return o;
}

/* Tokenize a line (without its line number). Returns the length,
 * including the terminating 0. Keywords must be separated from names,
 * except that a keyword may be followed directly by digits (GOTO10). */
int tokenize(const char *s, uint8_t *out, int max)
{
    uint8_t *o = out, *end = out + max - 8;
    char w[34];

    while (*s) {
        char c = *s;
        if (o >= end)
            error(E_LL);
        if (c == ' ') {
            s++;
            continue;
        }
        if (c == '"') {
            const char *q = ++s;
            int n;
            while (*s && *s != '"')
                s++;
            n = (int)(s - q);
            if (n > 255)
                error(E_LS);
            if (*s)
                s++;
            *o++ = TK_STR;
            *o++ = (uint8_t)n;
            memcpy(o, q, (size_t)n);
            o += n;
            continue;
        }
        if (isdig(c) || (c == '.' && isdig(s[1]))) {
            float f;
            int isint;
            s = parse_num(s, &f, &isint);
            o = emit_num(o, f, isint);
            continue;
        }
        if (c == '&' && up(s[1]) == 'H') {
            uint32_t v = 0;
            int n = 0;
            s += 2;
            for (;;) {
                char h = up(*s);
                if (isdig(h))
                    v = v * 16u + (uint32_t)(h - '0');
                else if (h >= 'A' && h <= 'F')
                    v = v * 16u + (uint32_t)(h - 'A' + 10);
                else
                    break;
                n++;
                s++;
            }
            if (!n || n > 8)
                error(E_SN);
            o = emit_num(o, (float)v, v <= 32767u);
            continue;
        }
        if (isalph(c)) {
            const char *st = s;
            int n = 0, k;
            uint8_t kw = 0;
            while (isalph(*s) || isdig(*s) || *s == '_') {
                if (n >= 31)
                    error(E_SN);
                w[n++] = up(*s++);
            }
            if (*s == '$') {
                w[n] = '$';
                kw = kw_lookup(w, n + 1);
                if (kw)
                    s++;
            }
            if (!kw)
                kw = kw_lookup(w, n);
            if (!kw) { /* keyword followed by digits: GOTO100 */
                for (k = 1; k < n && !isdig(w[k]); k++)
                    ;
                if (k < n) {
                    int j;
                    for (j = k; j < n && isdig(w[j]); j++)
                        ;
                    if (j == n && (kw = kw_lookup(w, k)) != 0)
                        s = st + k;
                }
            }
            if (kw) {
                *o++ = kw;
                if (kw == KW_REM) {
                    o = emit_raw(o, s, (int)strlen(s));
                    break;
                }
                if (kw == KW_DATA) {
                    const char *q = s;
                    int quoted = 0;
                    while (*s && (quoted || *s != ':')) {
                        if (*s == '"')
                            quoted = !quoted;
                        s++;
                    }
                    o = emit_raw(o, q, (int)(s - q));
                }
                continue;
            }
            if (n > 2 && w[0] == 'F' && w[1] == 'N') {
                *o++ = KW_FN;
                memmove(w, w + 2, (size_t)(n - 2));
                n -= 2;
            }
            if (*s == '$' || *s == '%')
                w[n++] = *s++;
            {
                uint16_t id = var_intern(w, (uint8_t)n);
                *o++ = TK_VAR;
                *o++ = (uint8_t)(id & 0xFF);
                *o++ = (uint8_t)(id >> 8);
            }
            continue;
        }
        if (c == '?') {
            *o++ = KW_PRINT;
            s++;
            continue;
        }
        if (c == '\'') {
            *o++ = KW_REM;
            o = emit_raw(o, s + 1, (int)strlen(s + 1));
            break;
        }
        if (c == '<') {
            s++;
            if (*s == '=') {
                s++;
                *o++ = TK_LE;
            } else if (*s == '>') {
                s++;
                *o++ = TK_NE;
            } else
                *o++ = '<';
            continue;
        }
        if (c == '>') {
            s++;
            if (*s == '=') {
                s++;
                *o++ = TK_GE;
            } else
                *o++ = '>';
            continue;
        }
        if ((uint8_t)c < 0x20 || (uint8_t)c >= 0x7F) {
            s++; /* drop control and non-ASCII characters outside strings */
            continue;
        }
        *o++ = (uint8_t)c;
        s++;
    }
    *o++ = 0;
    return (int)(o - out);
}

uint8_t *tok_skip(uint8_t *p)
{
    switch (*p) {
    case TK_INT:
    case TK_VAR: return p + 3;
    case TK_FLT: return p + 5;
    case TK_STR:
    case TK_RAW: return p + 2 + p[1];
    default: return p + 1;
    }
}

/* ---- detokenizer ---- */

int list_line(const uint8_t *l, char *out)
{
    char *o = out;
    const uint8_t *p = l + 4;
    int sp_after = 0, prev_opnd = 0;
    char nb[16];

    o += fmt_u32(LNUM(l), o);
    *o++ = ' ';
    while (*p) {
        uint8_t t = *p;
        if (t >= 0x80) {
            const char *k = kw_name(t);
            if ((sp_after || prev_opnd) && o[-1] != ' ')
                *o++ = ' ';
            while (*k)
                *o++ = *k++;
            p++;
            if ((t == KW_REM || t == KW_DATA) && *p == TK_RAW) {
                memcpy(o, p + 2, p[1]);
                o += p[1];
                p += 2 + p[1];
                sp_after = prev_opnd = 0;
                continue;
            }
            sp_after = !is_func_kw(t);
            prev_opnd = t == KW_TIME || t == KW_INKEYS;
            continue;
        }
        if (t == TK_INT || t == TK_FLT || t == TK_STR || t == TK_VAR) {
            if (sp_after)
                *o++ = ' ';
            if (t == TK_INT) {
                o += fmt_u32(RD16(p + 1), o);
            } else if (t == TK_FLT) {
                union { float f; uint8_t b[4]; } u;
                int n;
                memcpy(u.b, p + 1, 4);
                n = fmt_num(u.f, nb);
                memcpy(o, nb + 1, (size_t)(n - 1));
                o += n - 1;
            } else if (t == TK_STR) {
                *o++ = '"';
                memcpy(o, p + 2, p[1]);
                o += p[1];
                *o++ = '"';
            } else {
                uint8_t n;
                const char *nm = var_name(RD16(p + 1), &n);
                memcpy(o, nm, n);
                o += n;
            }
            p = tok_skip((uint8_t *)p);
            sp_after = 0;
            prev_opnd = 1;
            continue;
        }
        if (sp_after && t != ':')
            *o++ = ' ';
        if (t == TK_LE) {
            *o++ = '<';
            *o++ = '=';
        } else if (t == TK_GE) {
            *o++ = '>';
            *o++ = '=';
        } else if (t == TK_NE) {
            *o++ = '<';
            *o++ = '>';
        } else
            *o++ = (char)t;
        p++;
        sp_after = 0;
        prev_opnd = t == ')';
    }
    *o = 0;
    return (int)(o - out);
}

/* ---- program store ---- */

static uint8_t *align8(uint8_t *p)
{
    uintptr_t a = (uintptr_t)p;
    return (uint8_t *)((a + 7u) & ~(uintptr_t)7u);
}

void clear_vars(void)
{
    memset(vars, 0, sizeof vars);
    memset(arrs, 0, sizeof arrs);
    memset(fndef, 0, sizeof fndef);
    arr_start = arr_end = align8(prog_end + 2);
    str_bot = arena_top;
    vsp = 0;
}

void prog_new(void)
{
    arena[0] = arena[1] = 0;
    prog_end = arena;
    nnames = 0;
    npos = 0;
    clear_vars();
}

/* The line with this number (exact) or the first line at or after it. */
uint8_t *find_line(uint16_t num, int exact)
{
    uint8_t *p = (lp && LLEN(lp) && LNUM(lp) <= num) ? lp : arena;
    while (LLEN(p) && LNUM(p) < num)
        p += LLEN(p);
    if (exact && (!LLEN(p) || LNUM(p) != num))
        return NULL;
    return p;
}

void prog_edit(uint16_t num, const uint8_t *tok, uint16_t toklen)
{
    uint8_t *p;
    lp = NULL;
    clear_vars();
    p = find_line(num, 0);
    if (LLEN(p) && LNUM(p) == num) {
        uint16_t l = LLEN(p);
        memmove(p, p + l, (size_t)(prog_end + 2 - (p + l)));
        prog_end -= l;
    }
    if (toklen > 1) {
        uint16_t l = (uint16_t)(toklen + 4u);
        if (prog_end + 2 + l + 16 > arena_top)
            error(E_OM);
        memmove(p + l, p, (size_t)(prog_end + 2 - p));
        p[0] = (uint8_t)(l & 0xFF);
        p[1] = (uint8_t)(l >> 8);
        p[2] = (uint8_t)(num & 0xFF);
        p[3] = (uint8_t)(num >> 8);
        memcpy(p + 4, tok, toklen);
        prog_end += l;
    }
    clear_vars();
}

/* ---- string heap ----
 * Each block is [text, padded to even][owner pointer][uint16 length].
 * Blocks are allocated downward from arena_top. Every heap string has
 * exactly one owner descriptor (a variable, an array element or an
 * expression stack slot), so compaction can update it in place. The
 * trailer at the high end lets the collector walk from the top down.
 */
#define TRL ((uint32_t)(sizeof(str_t *) + 2u))
#define PADLEN(n) (((uint32_t)(n) + 1u) & ~1u)

char *str_alloc(uint16_t len)
{
    uint32_t need = PADLEN(len) + TRL;
    uint8_t *t;
    if ((uint32_t)(str_bot - arr_end) < need) {
        str_gc();
        if ((uint32_t)(str_bot - arr_end) < need)
            error(E_OM);
    }
    str_bot -= need;
    t = str_bot + PADLEN(len);
    memset(t, 0, TRL - 2u);
    t[TRL - 2u] = (uint8_t)(len & 0xFF);
    t[TRL - 1u] = (uint8_t)(len >> 8);
    return (char *)str_bot;
}

static void mark(str_t *d)
{
    uint8_t *p = (uint8_t *)d->p;
    if (!p || p < str_bot || p >= arena_top)
        return;
    memcpy(p + PADLEN(d->len), &d, sizeof d);
}

static uint32_t arr_count(const uint8_t *a)
{
    uint32_t n = 1;
    int i;
    for (i = 0; i < a[0]; i++)
        n *= RD16(a + 2 + 2 * i);
    return n;
}

#define ARR_HDR(nd) ((uint32_t)((2u + 2u * (unsigned)(nd) + 7u) & ~7u))

void str_gc(void)
{
    uint8_t *p = arena_top, *dest;
    str_t *nul = NULL;
    uint16_t i;
    int k;

    while (p > str_bot) {
        uint16_t len = RD16(p - 2);
        uint8_t *t = p - TRL;
        memcpy(t, &nul, sizeof nul);
        p = t - PADLEN(len);
    }
    for (i = 0; i < nnames; i++) {
        if (vtype[i] != VT_STR)
            continue;
        mark(&vars[i].s);
        if (arrs[i]) {
            uint8_t *a = arrs[i];
            str_t *e = (str_t *)(a + ARR_HDR(a[0]));
            uint32_t n = arr_count(a), j;
            for (j = 0; j < n; j++)
                mark(&e[j]);
        }
    }
    for (k = 0; k < vsp; k++)
        if (vs[k].t == T_STR)
            mark(&vs[k].u.s);

    dest = p = arena_top;
    while (p > str_bot) {
        uint16_t len = RD16(p - 2);
        uint32_t tot = PADLEN(len) + TRL;
        uint8_t *start = p - tot;
        str_t *own;
        memcpy(&own, p - TRL, sizeof own);
        if (own) {
            dest -= tot;
            if (dest != start)
                memmove(dest, start, tot);
            own->p = (char *)dest;
        }
        p = start;
    }
    str_bot = dest;
}

uint32_t mem_free(void)
{
    str_gc();
    return (uint32_t)(str_bot - arr_end);
}

/* ---- arrays ----
 * [ndims][type][uint16 size of each dimension]... padded to 8, then data.
 */
static uint16_t elsize(uint16_t id)
{
    return vtype[id] == VT_NUM ? 4 : vtype[id] == VT_INT ? 2 : (uint16_t)sizeof(str_t);
}

void arr_dim(uint16_t id, int nd, const uint16_t *dims)
{
    uint32_t n = 1, need;
    uint8_t *a;
    int i;
    if (arrs[id])
        error(E_DD);
    for (i = 0; i < nd; i++) {
        n *= dims[i];
        if (n > 0x7FFFFUL)
            error(E_OM);
    }
    need = ARR_HDR(nd) + n * elsize(id) + 8u;
    if ((uint32_t)(str_bot - arr_end) < need) {
        str_gc();
        if ((uint32_t)(str_bot - arr_end) < need)
            error(E_OM);
    }
    a = arr_end;
    memset(a, 0, ARR_HDR(nd) + n * elsize(id));
    a[0] = (uint8_t)nd;
    a[1] = vtype[id];
    for (i = 0; i < nd; i++) {
        a[2 + 2 * i] = (uint8_t)(dims[i] & 0xFF);
        a[3 + 2 * i] = (uint8_t)(dims[i] >> 8);
    }
    arrs[id] = a;
    arr_end = align8(a + ARR_HDR(nd) + n * elsize(id));
}

/* Parse "(i, j, ...)" at ip and return the element; DIMs 0..10 on first use. */
void *arr_ref(uint16_t id)
{
    uint16_t idx[8];
    int nd = 0, i;
    uint8_t *a;
    uint32_t off = 0;

    need('(');
    for (;;) {
        int32_t v;
        expr();
        v = pop_i32();
        if (v < 0 || v > 32767)
            error(E_BS);
        if (nd >= 8)
            error(E_BS);
        idx[nd++] = (uint16_t)v;
        if (*ip != ',')
            break;
        ip++;
    }
    need(')');
    a = arrs[id];
    if (!a) {
        uint16_t d[8];
        for (i = 0; i < nd; i++)
            d[i] = 11;
        arr_dim(id, nd, d);
        a = arrs[id];
    }
    if (a[0] != nd)
        error(E_BS);
    for (i = 0; i < nd; i++) {
        uint16_t d = RD16(a + 2 + 2 * i);
        if (idx[i] >= d)
            error(E_BS);
        off = off * d + idx[i];
    }
    return a + ARR_HDR(nd) + off * elsize(id);
}
