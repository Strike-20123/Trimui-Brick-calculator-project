/*
 * Brick Calculator -- graphing + scientific
 * -----------------------------------------
 * For the TrimUI Brick (TG3040) and Smart Pro (TG5040).
 *
 * One dependency: libSDL2. No SDL2_ttf, no SDL2_image, no font files,
 * no images. Everything is drawn with rectangles and lines using a 5x7
 * bitmap font compiled into the binary.
 *
 * FOUR PAGES, cycled with the shoulder buttons:
 *   MAIN   the usual keypad
 *   FUNC   trig, logs, powers, memory
 *   BASE   hex/dec/oct/bin entry and bitwise operators
 *   GRAPH  type an expression in X and plot it
 *
 * BUTTON MAPPING
 * There is no reliable published SDL button mapping for this hardware,
 * and it differs between firmware versions. So on first run the app asks
 * you to press each button in turn and writes the result to keys.cfg
 * next to the binary. Delete that file to run the wizard again, or press
 * the KEYS key on the FUNC page.
 *
 * The D-pad is read as hat, axes and buttons all at once, so it works
 * without configuration.
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#define LW 1024
#define LH 768

#define AXIS_DEADZONE 12000
#define CFG_FILE "keys.cfg"

/* ---- palette ----------------------------------------------------------- */
#define C_BG        0x0E1116
#define C_PANEL     0x171C26
#define C_PANEL_ED  0x28303F
#define C_KEY       0x1A2029
#define C_KEY_TOP   0x232B37
#define C_KEY_FN    0x1E2633
#define C_KEY_OP    0x21344D
#define C_KEY_EQ    0x2F6BD8
#define C_KEY_AC    0x3A2430
#define C_KEY_ON    0x2C5A46
#define C_SEL       0xFFC24D
#define C_SEL_BG    0x37425A
#define C_TEXT      0xEDF1F7
#define C_DIM       0x66718A
#define C_MID       0x93A0B8
#define C_ERR       0xE06A6A
#define C_GRID      0x232B3A
#define C_AXIS      0x4A5670
#define C_CURVE     0x5EC8F2
#define C_TRACE     0xFFC24D

/* ---- 5x7 font, ASCII 32..94, column-major, bit0 = top row -------------- */
static const unsigned char font5x7[63][5] = {
    {0x00,0x00,0x00,0x00,0x00}, /*   */
    {0x00,0x00,0x5F,0x00,0x00}, /* ! */
    {0x00,0x07,0x00,0x07,0x00}, /* " */
    {0x14,0x7F,0x14,0x7F,0x14}, /* # */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* $ */
    {0x23,0x13,0x08,0x64,0x62}, /* % */
    {0x36,0x49,0x55,0x22,0x50}, /* & */
    {0x00,0x05,0x03,0x00,0x00}, /* ' */
    {0x00,0x1C,0x22,0x41,0x00}, /* ( */
    {0x00,0x41,0x22,0x1C,0x00}, /* ) */
    {0x14,0x08,0x3E,0x08,0x14}, /* * */
    {0x08,0x08,0x3E,0x08,0x08}, /* + */
    {0x00,0x50,0x30,0x00,0x00}, /* , */
    {0x08,0x08,0x08,0x08,0x08}, /* - */
    {0x00,0x60,0x60,0x00,0x00}, /* . */
    {0x20,0x10,0x08,0x04,0x02}, /* / */
    {0x3E,0x51,0x49,0x45,0x3E}, /* 0 */
    {0x00,0x42,0x7F,0x40,0x00}, /* 1 */
    {0x42,0x61,0x51,0x49,0x46}, /* 2 */
    {0x21,0x41,0x45,0x4B,0x31}, /* 3 */
    {0x18,0x14,0x12,0x7F,0x10}, /* 4 */
    {0x27,0x45,0x45,0x45,0x39}, /* 5 */
    {0x3C,0x4A,0x49,0x49,0x30}, /* 6 */
    {0x01,0x71,0x09,0x05,0x03}, /* 7 */
    {0x36,0x49,0x49,0x49,0x36}, /* 8 */
    {0x06,0x49,0x49,0x29,0x1E}, /* 9 */
    {0x00,0x36,0x36,0x00,0x00}, /* : */
    {0x00,0x56,0x36,0x00,0x00}, /* ; */
    {0x00,0x08,0x14,0x22,0x41}, /* < */
    {0x14,0x14,0x14,0x14,0x14}, /* = */
    {0x41,0x22,0x14,0x08,0x00}, /* > */
    {0x02,0x01,0x51,0x09,0x06}, /* ? */
    {0x32,0x49,0x79,0x41,0x3E}, /* @ */
    {0x7E,0x11,0x11,0x11,0x7E}, /* A */
    {0x7F,0x49,0x49,0x49,0x36}, /* B */
    {0x3E,0x41,0x41,0x41,0x22}, /* C */
    {0x7F,0x41,0x41,0x22,0x1C}, /* D */
    {0x7F,0x49,0x49,0x49,0x41}, /* E */
    {0x7F,0x09,0x09,0x09,0x01}, /* F */
    {0x3E,0x41,0x49,0x49,0x7A}, /* G */
    {0x7F,0x08,0x08,0x08,0x7F}, /* H */
    {0x00,0x41,0x7F,0x41,0x00}, /* I */
    {0x20,0x40,0x41,0x3F,0x01}, /* J */
    {0x7F,0x08,0x14,0x22,0x41}, /* K */
    {0x7F,0x40,0x40,0x40,0x40}, /* L */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* M */
    {0x7F,0x04,0x08,0x10,0x7F}, /* N */
    {0x3E,0x41,0x41,0x41,0x3E}, /* O */
    {0x7F,0x09,0x09,0x09,0x06}, /* P */
    {0x3E,0x41,0x51,0x21,0x5E}, /* Q */
    {0x7F,0x09,0x19,0x29,0x46}, /* R */
    {0x46,0x49,0x49,0x49,0x31}, /* S */
    {0x01,0x01,0x7F,0x01,0x01}, /* T */
    {0x3F,0x40,0x40,0x40,0x3F}, /* U */
    {0x1F,0x20,0x40,0x20,0x1F}, /* V */
    {0x3F,0x40,0x38,0x40,0x3F}, /* W */
    {0x63,0x14,0x08,0x14,0x63}, /* X */
    {0x07,0x08,0x70,0x08,0x07}, /* Y */
    {0x61,0x51,0x49,0x45,0x43}, /* Z */
    {0x00,0x7F,0x41,0x41,0x00}, /* [ */
    {0x02,0x04,0x08,0x10,0x20}, /* \ */
    {0x00,0x41,0x41,0x7F,0x00}, /* ] */
    {0x04,0x02,0x01,0x02,0x04}  /* ^ */
};

/* ---- drawing helpers --------------------------------------------------- */

static SDL_Renderer *R;

static void set_color(Uint32 c)
{
    SDL_SetRenderDrawColor(R, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255);
}

static void fill(int x, int y, int w, int h, Uint32 c)
{
    SDL_Rect rect = { x, y, w, h };
    set_color(c);
    SDL_RenderFillRect(R, &rect);
}

static void frame_rect(int x, int y, int w, int h, int t, Uint32 c)
{
    fill(x, y, w, t, c);
    fill(x, y + h - t, w, t, c);
    fill(x, y, t, h, c);
    fill(x + w - t, y, t, h, c);
}

static void line(int x1, int y1, int x2, int y2, Uint32 c)
{
    set_color(c);
    SDL_RenderDrawLine(R, x1, y1, x2, y2);
}

static int text_w(const char *s, int scale)
{
    int n = (int)strlen(s);
    if (n == 0) return 0;
    return n * 6 * scale - scale;
}

static void draw_text(const char *s, int x, int y, int scale, Uint32 c)
{
    int i, col, row;
    set_color(c);
    for (i = 0; s[i]; i++) {
        char ch = s[i];
        int idx;
        if (ch >= 'a' && ch <= 'z') ch = (char)(ch - 32);
        idx = (int)((unsigned char)ch) - 32;
        if (idx < 0 || idx > 62) idx = 0;
        for (col = 0; col < 5; col++) {
            unsigned char bits = font5x7[idx][col];
            for (row = 0; row < 7; row++) {
                if (bits & (1 << row)) {
                    SDL_Rect p = { x + (i * 6 + col) * scale, y + row * scale,
                                   scale, scale };
                    SDL_RenderFillRect(R, &p);
                }
            }
        }
    }
}

static void draw_center(const char *s, int cx, int y, int scale, Uint32 c)
{
    draw_text(s, cx - text_w(s, scale) / 2, y, scale, c);
}

/* ---- key bindings ------------------------------------------------------ */

enum { ACT_CONFIRM, ACT_BACK, ACT_CLEAR, ACT_PREV, ACT_NEXT, ACT_QUIT, NACT };

static const char *act_name[NACT] = {
    "SELECT / CONFIRM", "DELETE", "CLEAR",
    "PREVIOUS PAGE", "NEXT PAGE", "QUIT"
};
static const char *act_key[NACT] = {
    "confirm", "back", "clear", "prev", "next", "quit"
};
static const char *act_hint[NACT] = {
    "OK", "DEL", "CLR", "PREV", "NEXT", "QUIT"
};

static int bind_btn[NACT] = { -1, -1, -1, -1, -1, -1 };

static void load_binds(int *ok)
{
    FILE *f = fopen(CFG_FILE, "r");
    char k[32];
    int v, i, found = 0;
    *ok = 0;
    if (!f) return;
    while (fscanf(f, "%31[^=]=%d\n", k, &v) == 2) {
        for (i = 0; i < NACT; i++)
            if (!strcmp(k, act_key[i])) { bind_btn[i] = v; found++; }
    }
    fclose(f);
    if (found >= NACT) *ok = 1;
}

static void save_binds(void)
{
    FILE *f = fopen(CFG_FILE, "w");
    int i;
    if (!f) return;
    for (i = 0; i < NACT; i++) fprintf(f, "%s=%d\n", act_key[i], bind_btn[i]);
    fclose(f);
}

static int action_for(int button)
{
    int i;
    for (i = 0; i < NACT; i++) if (bind_btn[i] == button) return i;
    return -1;
}

/* ---- expression parser (for the graph page) ---------------------------- */
/*
 * Recursive descent over:
 *   expr   := term (('+'|'-') term)*
 *   term   := power (('*'|'/') power | implicit-multiply)*
 *   power  := unary ('^' power)?          right associative
 *   unary  := '-' unary | primary
 *   primary:= number | 'X' | 'PI' | 'E' | FUNC '(' expr ')' | '(' expr ')'
 */

typedef struct {
    const char *s;
    int   pos;
    double x;
    int   err;
} PS;

static double p_expr(PS *p);

static void p_skip(PS *p) { while (p->s[p->pos] == ' ') p->pos++; }

static int p_eat(PS *p, const char *word)
{
    int n = (int)strlen(word);
    if (!strncmp(p->s + p->pos, word, (size_t)n)) { p->pos += n; return 1; }
    return 0;
}

static double p_primary(PS *p)
{
    double v;
    char c;

    p_skip(p);
    c = p->s[p->pos];

    if (c == '(') {
        p->pos++;
        v = p_expr(p);
        p_skip(p);
        if (p->s[p->pos] == ')') p->pos++;
        else p->err = 1;
        return v;
    }

    /* functions -- longest names first so SIN doesn't shadow SINH */
    {
        static const struct { const char *n; double (*f)(double); } fns[] = {
            { "ASINH", asinh }, { "ACOSH", acosh }, { "ATANH", atanh },
            { "SINH", sinh },   { "COSH", cosh },   { "TANH", tanh },
            { "ASIN", asin },   { "ACOS", acos },   { "ATAN", atan },
            { "SQRT", sqrt },   { "CBRT", cbrt },   { "LOG", log10 },
            { "SIN", sin },     { "COS", cos },     { "TAN", tan },
            { "LN", log },      { "ABS", fabs },    { "EXP", exp }
        };
        size_t i;
        for (i = 0; i < sizeof(fns) / sizeof(fns[0]); i++) {
            if (p_eat(p, fns[i].n)) {
                p_skip(p);
                if (p->s[p->pos] == '(') {
                    p->pos++;
                    v = p_expr(p);
                    p_skip(p);
                    if (p->s[p->pos] == ')') p->pos++;
                    else p->err = 1;
                } else {
                    v = p_primary(p);   /* allow SIN X without parentheses */
                }
                return fns[i].f(v);
            }
        }
    }

    if (p_eat(p, "PI")) return M_PI;
    if (p_eat(p, "X"))  return p->x;
    if (p_eat(p, "E"))  return exp(1.0);

    if ((c >= '0' && c <= '9') || c == '.') {
        char *end;
        v = strtod(p->s + p->pos, &end);
        p->pos += (int)(end - (p->s + p->pos));
        return v;
    }

    p->err = 1;
    return 0.0;
}

static double p_power(PS *p);

/* Unary minus binds LOOSER than '^', so -X^2 is -(X^2), not (-X)^2. */
static double p_unary(PS *p)
{
    p_skip(p);
    if (p->s[p->pos] == '-') { p->pos++; return -p_unary(p); }
    if (p->s[p->pos] == '+') { p->pos++; return  p_unary(p); }
    return p_power(p);
}

/* Right associative, and the exponent may itself be signed: 2^-1. */
static double p_power(PS *p)
{
    double a = p_primary(p);
    p_skip(p);
    if (p->s[p->pos] == '^') { p->pos++; return pow(a, p_unary(p)); }
    return a;
}

static int starts_primary(char c)
{
    return (c >= '0' && c <= '9') || c == '.' || c == '(' ||
           (c >= 'A' && c <= 'Z');
}

static double p_term(PS *p)
{
    double a = p_unary(p);
    for (;;) {
        char c;
        p_skip(p);
        c = p->s[p->pos];
        if (c == '*')      { p->pos++; a *= p_unary(p); }
        else if (c == '/') { p->pos++; a /= p_unary(p); }
        else if (starts_primary(c)) { a *= p_unary(p); }   /* 2X, 3SIN(X) */
        else break;
        if (p->err) break;
    }
    return a;
}

static double p_expr(PS *p)
{
    double a = p_term(p);
    for (;;) {
        char c;
        p_skip(p);
        c = p->s[p->pos];
        if (c == '+')      { p->pos++; a += p_term(p); }
        else if (c == '-') { p->pos++; a -= p_term(p); }
        else break;
        if (p->err) break;
    }
    return a;
}

static double eval_at(const char *expr, double x, int *ok)
{
    PS p;
    double v;
    p.s = expr; p.pos = 0; p.x = x; p.err = 0;
    v = p_expr(&p);
    p_skip(&p);
    if (p.s[p.pos] != '\0') p.err = 1;
    *ok = !p.err;
    return v;
}

/* ---- pages ------------------------------------------------------------- */

#define MAXR 5
#define MAXC 6
#define NPAGES 4

typedef struct {
    const char *name;
    int rows, cols;
    const char *k[MAXR][MAXC];
} Page;

static const Page pages[NPAGES] = {
    { "MAIN", 5, 4, {
        { "AC",   "<",    "%",    "/",    NULL,   NULL },
        { "7",    "8",    "9",    "*",    NULL,   NULL },
        { "4",    "5",    "6",    "-",    NULL,   NULL },
        { "1",    "2",    "3",    "+",    NULL,   NULL },
        { "+-",   "0",    ".",    "=",    NULL,   NULL } } },

    { "FUNC", 5, 6, {
        { "SIN",  "COS",  "TAN",  "LOG",  "LN",   "SQRT" },
        { "ASIN", "ACOS", "ATAN", "X^2",  "X^Y",  "CBRT" },
        { "SINH", "COSH", "TANH", "10^X", "E^X",  "1/X"  },
        { "PI",   "E",    "N!",   "ABS",  "MOD",  "DRG"  },
        { "MC",   "MR",   "M+",   "M-",   "MS",   "KEYS" } } },

    { "BASE", 5, 6, {
        { "7",    "8",    "9",    "A",    "B",    "HEX" },
        { "4",    "5",    "6",    "C",    "D",    "DEC" },
        { "1",    "2",    "3",    "E",    "F",    "OCT" },
        { "0",    ".",    "<",    "AC",   "=",    "BIN" },
        { "AND",  "OR",   "XOR",  "NOT",  "SHL",  "SHR" } } },

    { "GRAPH", 5, 6, {
        { "7",    "8",    "9",    "(",    ")",    "SIN"  },
        { "4",    "5",    "6",    "^",    "/",    "COS"  },
        { "1",    "2",    "3",    "*",    "-",    "TAN"  },
        { "0",    ".",    "X",    "+",    "<",    "LOG"  },
        { "PLOT", "AC",   "PI",   "E",    "SQRT", "LN"   } } }
};

/* ---- state ------------------------------------------------------------- */

enum { MODE_KEYS, MODE_PLOT, MODE_WIZARD };

static int mode = MODE_KEYS;

static char   entry[80]  = "0";
static char   memo[80]   = "";
static double acc        = 0.0;
static char   pending    = 0;
static int    newentry   = 1;
static int    errflag    = 0;
static double memreg     = 0.0;
static int    memheld    = 0;
static int    base       = 10;
static int    degrees    = 1;
static int    page       = 0;
static int    sel_r = 1, sel_c = 0;

/* full-precision shadow of the rounded display string */
static double exactv     = 0.0;
static int    exact_ok   = 0;

/* graph */
static char   eq[160]    = "";
static char   eqmsg[64]  = "";
static double vx0 = -10, vx1 = 10, vy0 = -7.5, vy1 = 7.5;
static int    trace_on   = 0;
static int    trace_px   = 0;

/* wizard */
static int    wiz_step   = 0;

static int max_digits(void)
{
    if (base == 2)  return 32;
    if (base == 8)  return 11;
    if (base == 16) return 8;
    return 14;
}

/* ---- number formatting ------------------------------------------------- */

static void fmt_num(double v, char *out, size_t n)
{
    char *p;

    if (!isfinite(v)) { snprintf(out, n, "ERROR"); return; }

    if (base != 10) {
        unsigned int u;
        long long iv = (long long)v;
        int neg = 0, i = 0, j = 0;
        char buf[40];

        if (iv < 0) { neg = 1; iv = -iv; }
        u = (unsigned int)(iv & 0xFFFFFFFFLL);
        if (u == 0) buf[i++] = '0';
        while (u) {
            int d = (int)(u % (unsigned)base);
            buf[i++] = (char)(d < 10 ? '0' + d : 'A' + d - 10);
            u /= (unsigned)base;
        }
        if (neg && j < (int)n - 1) out[j++] = '-';
        while (i > 0 && j < (int)n - 1) out[j++] = buf[--i];
        out[j] = '\0';
        return;
    }

    snprintf(out, n, "%.10g", v);
    p = strstr(out, "e+");
    if (p) memmove(p + 1, p + 2, strlen(p + 2) + 1);
}

static double entry_value(void)
{
    if (exact_ok) return exactv;
    if (base == 10) return atof(entry);
    return (double)strtoll(entry, NULL, base);
}

static void set_entry(double v)
{
    if (base != 10) v = (double)(long long)v;
    fmt_num(v, entry, sizeof(entry));
    exactv = v;
    exact_ok = 1;
}

/* ---- calculator ops ---------------------------------------------------- */

static void calc_reset(void)
{
    strcpy(entry, "0");
    memo[0] = '\0';
    acc = 0.0;
    pending = 0;
    newentry = 1;
    errflag = 0;
    exactv = 0.0;
    exact_ok = 0;
}

static const char *op_name(char op)
{
    switch (op) {
        case '+': return "+";     case '-': return "-";
        case '*': return "*";     case '/': return "/";
        case '^': return "^";     case 'm': return "MOD";
        case '&': return "AND";   case '|': return "OR";
        case 'x': return "XOR";   case 'l': return "SHL";
        case 'r': return "SHR";   default:  return "?";
    }
}

static double apply_op(double a, char op, double b, int *ok)
{
    long long ia = (long long)a, ib = (long long)b;
    *ok = 1;
    switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/': if (b == 0.0) { *ok = 0; return 0; } return a / b;
        case '^': return pow(a, b);
        case 'm': if (b == 0.0) { *ok = 0; return 0; } return fmod(a, b);
        case '&': return (double)(ia & ib);
        case '|': return (double)(ia | ib);
        case 'x': return (double)(ia ^ ib);
        case 'l': return (double)(ia << (ib & 31));
        case 'r': return (double)(ia >> (ib & 31));
        default:  return b;
    }
}

static void fail(const char *msg)
{
    errflag = 1;
    snprintf(entry, sizeof(entry), "%s", msg);
    memo[0] = '\0';
    pending = 0;
    exact_ok = 0;
}

static void push_digit(char d)
{
    size_t len;
    int val;

    if (errflag) calc_reset();
    if (d >= '0' && d <= '9') val = d - '0';
    else if (d >= 'A' && d <= 'F') val = d - 'A' + 10;
    else return;
    if (val >= base) return;

    if (newentry) { strcpy(entry, "0"); newentry = 0; }
    len = strlen(entry);
    if ((int)len >= max_digits()) return;
    exact_ok = 0;

    if (!strcmp(entry, "0"))       { entry[0] = d; entry[1] = '\0'; }
    else if (!strcmp(entry, "-0")) { entry[1] = d; entry[2] = '\0'; }
    else { entry[len] = d; entry[len + 1] = '\0'; }
}

static void push_dot(void)
{
    if (base != 10) return;
    if (errflag) calc_reset();
    if (newentry) { strcpy(entry, "0"); newentry = 0; }
    if (strchr(entry, '.')) return;
    if ((int)strlen(entry) >= max_digits()) return;
    exact_ok = 0;
    strcat(entry, ".");
}

static void backspace(void)
{
    size_t len;
    if (errflag) { calc_reset(); return; }
    if (newentry) return;
    len = strlen(entry);
    if (len > 0) entry[len - 1] = '\0';
    if (entry[0] == '\0' || !strcmp(entry, "-")) strcpy(entry, "0");
    exact_ok = 0;
}

static void negate(void)
{
    if (errflag) return;
    if (entry[0] == '-') memmove(entry, entry + 1, strlen(entry));
    else if (strcmp(entry, "0")) {
        memmove(entry + 1, entry, strlen(entry) + 1);
        entry[0] = '-';
    }
    if (exact_ok) exactv = -exactv;
    newentry = 0;
}

static void set_op(char op)
{
    double cur;
    int ok;
    if (errflag) return;
    cur = entry_value();
    if (pending && !newentry) {
        acc = apply_op(acc, pending, cur, &ok);
        if (!ok) { fail("ERROR"); return; }
        set_entry(acc);
    } else acc = cur;
    pending = op;
    newentry = 1;
    {
        char a[48];
        fmt_num(acc, a, sizeof(a));
        snprintf(memo, sizeof(memo), "%s %s", a, op_name(op));
    }
}

static void equals(void)
{
    double cur;
    int ok;
    if (errflag) return;
    cur = entry_value();
    if (pending) {
        acc = apply_op(acc, pending, cur, &ok);
        if (!ok) { fail("ERROR"); return; }
    } else acc = cur;
    pending = 0;
    set_entry(acc);
    memo[0] = '\0';
    newentry = 1;
}

static void unary(double (*fn)(double), int trig_in, int trig_out)
{
    double v;
    if (errflag) return;
    base = 10;
    v = entry_value();
    if (trig_in && degrees) v = v * M_PI / 180.0;
    v = fn(v);
    if (trig_out && degrees) v = v * 180.0 / M_PI;
    if (!isfinite(v)) { fail("ERROR"); return; }
    set_entry(v);
    newentry = 1;
}

static double f_sqr(double x) { return x * x; }
static double f_inv(double x) { return 1.0 / x; }
static double f_p10(double x) { return pow(10.0, x); }
static double f_fact(double x)
{
    double r = 1.0;
    long long i, n = (long long)x;
    if (x < 0 || x != floor(x) || n > 170) return NAN;
    for (i = 2; i <= n; i++) r *= (double)i;
    return r;
}

static void set_base(int b)
{
    double v;
    if (errflag) return;
    v = entry_value();
    base = b;
    set_entry(v);
    newentry = 1;
}

/* ---- graph ------------------------------------------------------------- */

static void eq_append(const char *t)
{
    if (strlen(eq) + strlen(t) < sizeof(eq) - 1) strcat(eq, t);
    eqmsg[0] = '\0';
}

static void eq_backspace(void)
{
    size_t n = strlen(eq);
    if (n) eq[n - 1] = '\0';
    eqmsg[0] = '\0';
}

static void view_reset(void)
{
    vx0 = -10; vx1 = 10; vy0 = -7.5; vy1 = 7.5;
}

static void do_plot(void)
{
    int ok;
    if (!eq[0]) { snprintf(eqmsg, sizeof(eqmsg), "NOTHING TO PLOT"); return; }
    eval_at(eq, 1.0, &ok);
    if (!ok) { snprintf(eqmsg, sizeof(eqmsg), "SYNTAX ERROR"); return; }
    eqmsg[0] = '\0';
    trace_on = 0;
    mode = MODE_PLOT;
}

static void view_zoom(double f)
{
    double cx = (vx0 + vx1) / 2, cy = (vy0 + vy1) / 2;
    double hw = (vx1 - vx0) / 2 * f, hh = (vy1 - vy0) / 2 * f;
    if (hw < 1e-6 || hw > 1e9) return;
    vx0 = cx - hw; vx1 = cx + hw;
    vy0 = cy - hh; vy1 = cy + hh;
}

static void view_pan(double dx, double dy)
{
    double w = vx1 - vx0, h = vy1 - vy0;
    vx0 += w * dx; vx1 += w * dx;
    vy0 += h * dy; vy1 += h * dy;
}

/* ---- key dispatch ------------------------------------------------------ */

static void start_wizard(void)
{
    int i;
    for (i = 0; i < NACT; i++) bind_btn[i] = -1;
    wiz_step = 0;
    mode = MODE_WIZARD;
}

static void press(int pg, const char *lab)
{
    /* GRAPH page builds a text expression instead of doing arithmetic */
    if (pg == 3) {
        if      (!strcmp(lab, "PLOT")) do_plot();
        else if (!strcmp(lab, "AC"))   { eq[0] = '\0'; eqmsg[0] = '\0'; }
        else if (!strcmp(lab, "<"))    eq_backspace();
        else if (!strcmp(lab, "SIN"))  eq_append("SIN(");
        else if (!strcmp(lab, "COS"))  eq_append("COS(");
        else if (!strcmp(lab, "TAN"))  eq_append("TAN(");
        else if (!strcmp(lab, "LOG"))  eq_append("LOG(");
        else if (!strcmp(lab, "LN"))   eq_append("LN(");
        else if (!strcmp(lab, "SQRT")) eq_append("SQRT(");
        else eq_append(lab);
        return;
    }

    /* BASE page: single letters A-F are hex digits, not functions */
    if (pg == 2 && strlen(lab) == 1 && lab[0] >= 'A' && lab[0] <= 'F') {
        push_digit(lab[0]);
        return;
    }

    if      (!strcmp(lab, "AC"))   calc_reset();
    else if (!strcmp(lab, "<"))    backspace();
    else if (!strcmp(lab, "+-"))   negate();
    else if (!strcmp(lab, "="))    equals();
    else if (!strcmp(lab, "."))    push_dot();
    else if (strlen(lab) == 1 && lab[0] >= '0' && lab[0] <= '9')
        push_digit(lab[0]);

    else if (!strcmp(lab, "+"))    set_op('+');
    else if (!strcmp(lab, "-"))    set_op('-');
    else if (!strcmp(lab, "*"))    set_op('*');
    else if (!strcmp(lab, "/"))    set_op('/');
    else if (!strcmp(lab, "X^Y"))  set_op('^');
    else if (!strcmp(lab, "MOD"))  set_op('m');
    else if (!strcmp(lab, "AND"))  set_op('&');
    else if (!strcmp(lab, "OR"))   set_op('|');
    else if (!strcmp(lab, "XOR"))  set_op('x');
    else if (!strcmp(lab, "SHL"))  set_op('l');
    else if (!strcmp(lab, "SHR"))  set_op('r');

    else if (!strcmp(lab, "%")) {
        if (!errflag) { set_entry(entry_value() / 100.0); newentry = 1; }
    }

    else if (!strcmp(lab, "SIN"))  unary(sin,    1, 0);
    else if (!strcmp(lab, "COS"))  unary(cos,    1, 0);
    else if (!strcmp(lab, "TAN"))  unary(tan,    1, 0);
    else if (!strcmp(lab, "ASIN")) unary(asin,   0, 1);
    else if (!strcmp(lab, "ACOS")) unary(acos,   0, 1);
    else if (!strcmp(lab, "ATAN")) unary(atan,   0, 1);
    else if (!strcmp(lab, "SINH")) unary(sinh,   0, 0);
    else if (!strcmp(lab, "COSH")) unary(cosh,   0, 0);
    else if (!strcmp(lab, "TANH")) unary(tanh,   0, 0);
    else if (!strcmp(lab, "LOG"))  unary(log10,  0, 0);
    else if (!strcmp(lab, "LN"))   unary(log,    0, 0);
    else if (!strcmp(lab, "SQRT")) unary(sqrt,   0, 0);
    else if (!strcmp(lab, "CBRT")) unary(cbrt,   0, 0);
    else if (!strcmp(lab, "X^2"))  unary(f_sqr,  0, 0);
    else if (!strcmp(lab, "1/X"))  unary(f_inv,  0, 0);
    else if (!strcmp(lab, "10^X")) unary(f_p10,  0, 0);
    else if (!strcmp(lab, "E^X"))  unary(exp,    0, 0);
    else if (!strcmp(lab, "ABS"))  unary(fabs,   0, 0);
    else if (!strcmp(lab, "N!"))   unary(f_fact, 0, 0);
    else if (!strcmp(lab, "NOT")) {
        if (!errflag) { set_entry((double)(~(long long)entry_value()));
                        newentry = 1; }
    }

    else if (!strcmp(lab, "PI")) { base = 10; set_entry(M_PI);      newentry = 1; }
    else if (!strcmp(lab, "E"))  { base = 10; set_entry(exp(1.0));  newentry = 1; }

    else if (!strcmp(lab, "MC")) { memreg = 0.0; memheld = 0; }
    else if (!strcmp(lab, "MR")) { set_entry(memreg); newentry = 1; }
    else if (!strcmp(lab, "MS")) { memreg = entry_value(); memheld = 1; newentry = 1; }
    else if (!strcmp(lab, "M+")) { memreg += entry_value(); memheld = 1; newentry = 1; }
    else if (!strcmp(lab, "M-")) { memreg -= entry_value(); memheld = 1; newentry = 1; }

    else if (!strcmp(lab, "DRG"))  degrees = !degrees;
    else if (!strcmp(lab, "HEX"))  set_base(16);
    else if (!strcmp(lab, "DEC"))  set_base(10);
    else if (!strcmp(lab, "OCT"))  set_base(8);
    else if (!strcmp(lab, "BIN"))  set_base(2);
    else if (!strcmp(lab, "KEYS")) start_wizard();
}

static void press_sel(void)
{
    const char *lab = pages[page].k[sel_r][sel_c];
    if (lab) press(page, lab);
}

static void clamp_sel(void)
{
    if (sel_r >= pages[page].rows) sel_r = pages[page].rows - 1;
    if (sel_c >= pages[page].cols) sel_c = pages[page].cols - 1;
    if (sel_r < 0) sel_r = 0;
    if (sel_c < 0) sel_c = 0;
}

static void move_sel(int dx, int dy)
{
    int rows = pages[page].rows, cols = pages[page].cols;
    sel_c += dx; sel_r += dy;
    if (sel_c < 0) sel_c = cols - 1;
    if (sel_c >= cols) sel_c = 0;
    if (sel_r < 0) sel_r = rows - 1;
    if (sel_r >= rows) sel_r = 0;
}

static void change_page(int d)
{
    page = (page + d + NPAGES) % NPAGES;
    clamp_sel();
}

/* ---- layout ------------------------------------------------------------ */

#define PAD    40
#define DISP_Y 32
#define DISP_H 150
#define TAB_Y  196
#define TAB_H  34
#define GRID_Y 248
#define GRID_H 452
#define HINT_Y 720
#define GAP    12
#define GW     (LW - 2 * PAD)

static void draw_hints(void)
{
    const int order[5] = { ACT_CONFIRM, ACT_BACK, ACT_CLEAR, ACT_NEXT, ACT_QUIT };
    int i, x = PAD;
    for (i = 0; i < 5; i++) {
        char buf[32];
        int a = order[i];
        if (bind_btn[a] < 0) continue;
        snprintf(buf, sizeof(buf), "B%d %s", bind_btn[a], act_hint[a]);
        draw_text(buf, x, HINT_Y, 2, C_DIM);
        x += text_w(buf, 2) + 26;
    }
    draw_text("DPAD MOVE", LW - PAD - text_w("DPAD MOVE", 2), HINT_Y, 2, C_DIM);
}

static void draw_tabs(void)
{
    int i, w = (GW - 3 * GAP) / NPAGES;
    for (i = 0; i < NPAGES; i++) {
        int x = PAD + i * (w + GAP);
        int on = (i == page);
        fill(x, TAB_Y, w, TAB_H, on ? C_KEY_OP : C_KEY);
        if (on) fill(x, TAB_Y, w, 3, C_SEL);
        draw_center(pages[i].name, x + w / 2, TAB_Y + (TAB_H - 14) / 2, 2,
                    on ? C_TEXT : C_DIM);
    }
}

static void render_keys(void)
{
    const Page *p = &pages[page];
    int rows = p->rows, cols = p->cols;
    int cw = (GW - (cols - 1) * GAP) / cols;
    int ch = (GRID_H - (rows - 1) * GAP) / rows;
    int r, c, tw;
    char status[80];

    fill(0, 0, LW, LH, C_BG);

    /* display panel */
    fill(PAD, DISP_Y, GW, DISP_H, C_PANEL);
    frame_rect(PAD, DISP_Y, GW, DISP_H, 2, C_PANEL_ED);

    if (page == 3) {
        draw_text("Y =", PAD + 18, DISP_Y + 18, 3, C_DIM);
        if (eqmsg[0])
            draw_text(eqmsg, PAD + GW - 18 - text_w(eqmsg, 2),
                      DISP_Y + 20, 2, C_ERR);
        {
            int s = 5;
            const char *show = eq[0] ? eq : "ENTER AN EXPRESSION IN X";
            while (s > 2 && text_w(show, s) > GW - 40) s--;
            draw_text(show, PAD + 20, DISP_Y + 62, s,
                      eq[0] ? C_TEXT : C_DIM);
        }
    } else {
        snprintf(status, sizeof(status), "%s%s%s",
                 degrees ? "DEG" : "RAD",
                 base == 16 ? "   HEX" : base == 8 ? "   OCT" :
                 base == 2  ? "   BIN" : "",
                 memheld ? "   M" : "");
        draw_text(status, PAD + 18, DISP_Y + 18, 2, C_DIM);
        if (memo[0])
            draw_text(memo, PAD + GW - 18 - text_w(memo, 2),
                      DISP_Y + 18, 2, C_MID);
        {
            int s = 8;
            while (s > 2 && text_w(entry, s) > GW - 40) s--;
            tw = text_w(entry, s);
            draw_text(entry, PAD + GW - 20 - tw, DISP_Y + 66, s,
                      errflag ? C_ERR : C_TEXT);
        }
    }

    draw_tabs();

    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            const char *lab = p->k[r][c];
            int x, y, sel, s;
            Uint32 bg;

            if (!lab) continue;
            x = PAD + c * (cw + GAP);
            y = GRID_Y + r * (ch + GAP);
            sel = (r == sel_r && c == sel_c);

            if      (!strcmp(lab, "="))      bg = C_KEY_EQ;
            else if (!strcmp(lab, "PLOT"))   bg = C_KEY_EQ;
            else if (!strcmp(lab, "AC"))     bg = C_KEY_AC;
            else if (c == cols - 1)          bg = C_KEY_OP;
            else if (page == 0 && r == 0)    bg = C_KEY_FN;
            else if (page > 0 && r == rows - 1) bg = C_KEY_FN;
            else                             bg = C_KEY;

            if ((!strcmp(lab, "HEX") && base == 16) ||
                (!strcmp(lab, "DEC") && base == 10) ||
                (!strcmp(lab, "OCT") && base == 8)  ||
                (!strcmp(lab, "BIN") && base == 2))
                bg = C_KEY_ON;

            if (sel) bg = (!strcmp(lab, "=") || !strcmp(lab, "PLOT"))
                          ? 0x4F8BFF : C_SEL_BG;

            fill(x, y, cw, ch, bg);
            fill(x, y, cw, 2, C_KEY_TOP);          /* subtle top bevel */
            if (sel) frame_rect(x - 3, y - 3, cw + 6, ch + 6, 3, C_SEL);

            {
                const char *shown = !strcmp(lab, "DRG")
                                    ? (degrees ? "DEG" : "RAD") : lab;
                s = (cols <= 4) ? 6 : 5;
                while (s > 2 && text_w(shown, s) > cw - 12) s--;
                draw_center(shown, x + cw / 2, y + (ch - 7 * s) / 2, s, C_TEXT);
            }
        }
    }

    draw_hints();
}

/* ---- plot rendering ---------------------------------------------------- */

#define PL_X 24
#define PL_Y 64
#define PL_W (LW - 2 * PL_X)
#define PL_H (LH - PL_Y - 56)

static int sx_of(double x)
{
    return PL_X + (int)((x - vx0) / (vx1 - vx0) * PL_W);
}
static int sy_of(double y)
{
    return PL_Y + (int)((vy1 - y) / (vy1 - vy0) * PL_H);
}
static double x_of(int px)
{
    return vx0 + (double)(px - PL_X) / PL_W * (vx1 - vx0);
}

/* choose a round-number grid step near 1/8th of the span */
static double nice_step(double span)
{
    double raw = span / 8.0;
    double mag = pow(10.0, floor(log10(raw)));
    double n = raw / mag;
    if (n < 1.5) n = 1;
    else if (n < 3) n = 2;
    else if (n < 7) n = 5;
    else n = 10;
    return n * mag;
}

static void render_plot(void)
{
    double stepx = nice_step(vx1 - vx0);
    double stepy = nice_step(vy1 - vy0);
    double t;
    int px, prevy = 0, have_prev = 0;
    char buf[80];

    fill(0, 0, LW, LH, C_BG);

    /* header */
    {
        int s = 3;
        while (s > 2 && text_w(eq, s) > LW - 220) s--;
        draw_text("Y =", PL_X, 22, s, C_DIM);
        draw_text(eq, PL_X + text_w("Y = ", s), 22, s, C_TEXT);
    }
    snprintf(buf, sizeof(buf), "X [%.4g, %.4g]   Y [%.4g, %.4g]",
             vx0, vx1, vy0, vy1);
    draw_text(buf, LW - PL_X - text_w(buf, 2), 26, 2, C_DIM);

    fill(PL_X, PL_Y, PL_W, PL_H, 0x121620);
    frame_rect(PL_X, PL_Y, PL_W, PL_H, 2, C_PANEL_ED);

    /* grid */
    for (t = ceil(vx0 / stepx) * stepx; t <= vx1; t += stepx) {
        int x = sx_of(t);
        if (x > PL_X + 1 && x < PL_X + PL_W - 1)
            line(x, PL_Y + 2, x, PL_Y + PL_H - 2, C_GRID);
    }
    for (t = ceil(vy0 / stepy) * stepy; t <= vy1; t += stepy) {
        int y = sy_of(t);
        if (y > PL_Y + 1 && y < PL_Y + PL_H - 1)
            line(PL_X + 2, y, PL_X + PL_W - 2, y, C_GRID);
    }

    /* axes */
    if (vy0 < 0 && vy1 > 0) {
        int y = sy_of(0);
        line(PL_X + 2, y, PL_X + PL_W - 2, y, C_AXIS);
    }
    if (vx0 < 0 && vx1 > 0) {
        int x = sx_of(0);
        line(x, PL_Y + 2, x, PL_Y + PL_H - 2, C_AXIS);
    }

    /* the curve */
    {
        int top = PL_Y + 2, bot = PL_Y + PL_H - 3;
        for (px = PL_X + 1; px < PL_X + PL_W - 1; px++) {
            double xv = x_of(px), yv;
            int ok, y;
            yv = eval_at(eq, xv, &ok);
            if (!ok || !isfinite(yv)) { have_prev = 0; continue; }
            y = sy_of(yv);
            if (have_prev) {
                int a = prevy, b = y;
                /* both ends off the same edge: nothing visible between them,
                 * so drawing a clamped segment would just smear along the
                 * border. Skip it. */
                int same_side = (a < top && b < top) || (a > bot && b > bot);
                /* a jump larger than the view is an asymptote, not a line */
                int asymptote = abs(y - prevy) > PL_H;
                if (!same_side && !asymptote) {
                    if (a < top) a = top;
                    if (a > bot) a = bot;
                    if (b < top) b = top;
                    if (b > bot) b = bot;
                    line(px - 1, a, px, b, C_CURVE);
                }
            }
            prevy = y;
            have_prev = 1;
        }
    }

    /* trace cursor */
    if (trace_on) {
        double xv = x_of(trace_px), yv;
        int ok, y;
        yv = eval_at(eq, xv, &ok);
        line(trace_px, PL_Y + 2, trace_px, PL_Y + PL_H - 2, C_TRACE);
        if (ok && isfinite(yv)) {
            y = sy_of(yv);
            if (y >= PL_Y && y <= PL_Y + PL_H)
                fill(trace_px - 3, y - 3, 7, 7, C_TRACE);
            snprintf(buf, sizeof(buf), "X %.6g    Y %.6g", xv, yv);
        } else {
            snprintf(buf, sizeof(buf), "X %.6g    Y UNDEFINED", xv);
        }
        draw_text(buf, PL_X + 10, PL_Y + PL_H - 24, 2, C_TRACE);
    }

    {
        const char *h = trace_on ? "DPAD TRACE   OK TRACE OFF   DEL BACK"
                                 : "DPAD PAN   PREV/NEXT ZOOM   OK TRACE   "
                                   "CLR RESET   DEL BACK";
        draw_text(h, PL_X, LH - 42, 2, C_DIM);
    }
}

/* ---- wizard rendering -------------------------------------------------- */

static void render_wizard(void)
{
    int i;
    char buf[80];

    fill(0, 0, LW, LH, C_BG);
    draw_center("BUTTON SETUP", LW / 2, 90, 5, C_TEXT);
    draw_center("PRESS THE BUTTON YOU WANT FOR EACH ACTION",
                LW / 2, 152, 2, C_DIM);

    for (i = 0; i < NACT; i++) {
        int y = 230 + i * 62;
        int done = (i < wiz_step);
        int cur  = (i == wiz_step);
        Uint32 col = cur ? C_SEL : done ? C_MID : C_GRID;

        fill(PAD + 60, y, GW - 120, 50, cur ? C_SEL_BG : C_PANEL);
        if (cur) frame_rect(PAD + 60, y, GW - 120, 50, 3, C_SEL);

        draw_text(act_name[i], PAD + 84, y + 18, 3, col);
        if (done) {
            snprintf(buf, sizeof(buf), "BUTTON %d", bind_btn[i]);
            draw_text(buf, PAD + GW - 84 - text_w(buf, 3), y + 18, 3, C_MID);
        } else if (cur) {
            draw_text("PRESS NOW", PAD + GW - 84 - text_w("PRESS NOW", 3),
                      y + 18, 3, C_SEL);
        }
    }

    draw_center("KEYBOARD: ESC SKIPS AND USES DEFAULTS",
                LW / 2, LH - 48, 2, C_DIM);
}

static void wizard_defaults(void)
{
    bind_btn[ACT_CONFIRM] = 0;
    bind_btn[ACT_BACK]    = 1;
    bind_btn[ACT_CLEAR]   = 2;
    bind_btn[ACT_PREV]    = 4;
    bind_btn[ACT_NEXT]    = 5;
    bind_btn[ACT_QUIT]    = 9;
}

static void wizard_button(int b)
{
    int i;
    for (i = 0; i < wiz_step; i++)
        if (bind_btn[i] == b) return;     /* already used, ignore */
    bind_btn[wiz_step++] = b;
    if (wiz_step >= NACT) {
        save_binds();
        mode = MODE_KEYS;
    }
}

/* ---- input ------------------------------------------------------------- */

static void nav(int dx, int dy)
{
    if (mode == MODE_KEYS) {
        move_sel(dx, dy);
    } else if (mode == MODE_PLOT) {
        if (trace_on) {
            if (dx) {
                trace_px += dx * 4;
                if (trace_px < PL_X + 1) trace_px = PL_X + 1;
                if (trace_px > PL_X + PL_W - 2) trace_px = PL_X + PL_W - 2;
            }
        } else {
            if (dx) view_pan(dx * 0.12, 0);
            if (dy) view_pan(0, -dy * 0.12);
        }
    }
}

static void do_action(int a)
{
    if (mode == MODE_PLOT) {
        switch (a) {
        case ACT_CONFIRM:
            trace_on = !trace_on;
            if (trace_on) trace_px = PL_X + PL_W / 2;
            break;
        case ACT_BACK:  mode = MODE_KEYS; break;
        case ACT_CLEAR: view_reset(); break;
        case ACT_PREV:  view_zoom(1.25); break;
        case ACT_NEXT:  view_zoom(0.8);  break;
        default: break;
        }
        return;
    }

    switch (a) {
    case ACT_CONFIRM: press_sel(); break;
    case ACT_BACK:
        if (page == 3) eq_backspace(); else backspace();
        break;
    case ACT_CLEAR:
        if (page == 3) { eq[0] = '\0'; eqmsg[0] = '\0'; } else calc_reset();
        break;
    case ACT_PREV: change_page(-1); break;
    case ACT_NEXT: change_page(1);  break;
    default: break;
    }
}

/* ---- main -------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    SDL_Window *win;
    SDL_Joystick *joy = NULL;
    int running = 1, have_cfg = 0;
    int axdir[2] = { 0, 0 };

    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (SDL_NumJoysticks() > 0) {
        char guid[64];
        joy = SDL_JoystickOpen(0);
        if (joy) {
            SDL_JoystickGetGUIDString(SDL_JoystickGetGUID(joy),
                                      guid, sizeof(guid));
            printf("joystick: %s\nguid: %s\nbuttons %d axes %d hats %d\n",
                   SDL_JoystickName(joy), guid,
                   SDL_JoystickNumButtons(joy),
                   SDL_JoystickNumAxes(joy),
                   SDL_JoystickNumHats(joy));
        }
    } else {
        printf("no joystick detected, keyboard only\n");
    }
    fflush(stdout);

    load_binds(&have_cfg);
    if (!have_cfg) {
        if (joy) start_wizard();
        else wizard_defaults();
    }

    win = SDL_CreateWindow("Calculator",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           LW, LH, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) {
        fprintf(stderr, "CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    R = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!R) R = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!R) {
        fprintf(stderr, "CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    SDL_RenderSetLogicalSize(R, LW, LH);
    SDL_ShowCursor(SDL_DISABLE);

    while (running) {
        SDL_Event e;
        int dirty = 1;

        if (mode == MODE_WIZARD)    render_wizard();
        else if (mode == MODE_PLOT) render_plot();
        else                        render_keys();
        SDL_RenderPresent(R);

        if (!SDL_WaitEvent(&e)) break;

        do {
            switch (e.type) {
            case SDL_QUIT:
                running = 0;
                break;

            case SDL_KEYDOWN:
                if (mode == MODE_WIZARD) {
                    if (e.key.keysym.sym == SDLK_ESCAPE) {
                        wizard_defaults();
                        save_binds();
                        mode = MODE_KEYS;
                    }
                    break;
                }
                switch (e.key.keysym.sym) {
                case SDLK_ESCAPE: running = 0; break;
                case SDLK_LEFT:   nav(-1, 0); break;
                case SDLK_RIGHT:  nav(1, 0);  break;
                case SDLK_UP:     nav(0, -1); break;
                case SDLK_DOWN:   nav(0, 1);  break;
                case SDLK_TAB:
                case SDLK_PAGEDOWN: do_action(ACT_NEXT); break;
                case SDLK_PAGEUP:   do_action(ACT_PREV); break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                case SDLK_SPACE:     do_action(ACT_CONFIRM); break;
                case SDLK_BACKSPACE: do_action(ACT_BACK);    break;
                case SDLK_DELETE:    do_action(ACT_CLEAR);   break;
                default: break;
                }
                break;

            case SDL_JOYBUTTONDOWN:
                printf("joy button %d\n", e.jbutton.button);
                fflush(stdout);
                if (mode == MODE_WIZARD) { wizard_button(e.jbutton.button); break; }
                {
                    int a = action_for(e.jbutton.button);
                    if (a == ACT_QUIT) running = 0;
                    else if (a >= 0) do_action(a);
                }
                break;

            case SDL_JOYHATMOTION:
                if (e.jhat.value & SDL_HAT_LEFT)  nav(-1, 0);
                if (e.jhat.value & SDL_HAT_RIGHT) nav(1, 0);
                if (e.jhat.value & SDL_HAT_UP)    nav(0, -1);
                if (e.jhat.value & SDL_HAT_DOWN)  nav(0, 1);
                break;

            case SDL_JOYAXISMOTION:
                if (e.jaxis.axis < 2) {
                    int dir = 0;
                    if (e.jaxis.value < -AXIS_DEADZONE) dir = -1;
                    else if (e.jaxis.value > AXIS_DEADZONE) dir = 1;
                    if (dir != axdir[e.jaxis.axis]) {
                        axdir[e.jaxis.axis] = dir;
                        if (dir) {
                            if (e.jaxis.axis == 0) nav(dir, 0);
                            else                   nav(0, dir);
                        }
                    }
                }
                break;

            default:
                break;
            }
        } while (running && SDL_PollEvent(&e));

        (void)dirty;
    }

    if (joy) SDL_JoystickClose(joy);
    SDL_DestroyRenderer(R);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
