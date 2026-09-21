/*
 * Brick Calculator -- scientific
 * ------------------------------
 * A calculator for the TrimUI Brick (TG3040) and Smart Pro.
 *
 * One dependency: libSDL2. No SDL2_ttf, no SDL2_image, no font files,
 * no PNGs. Everything is drawn with filled rectangles and a 5x7 bitmap
 * font compiled into the binary.
 *
 * Three pages, cycled with the shoulder buttons:
 *   MAIN  -- the usual keypad
 *   FUNC  -- trig, logs, powers, memory
 *   BASE  -- hex/dec/oct/bin entry and bitwise operators
 *
 * Controls (button numbers vary by firmware -- every press is logged to
 * stdout, so check log.txt and adjust the BTN_* defines if needed):
 *
 *   D-pad / left stick ... move selection
 *   A or B ............... press the highlighted key
 *   X .................... backspace
 *   Y .................... clear
 *   L / R ................ previous / next page
 *   SELECT / START ....... quit  (on stock firmware MENU exits the app)
 *
 * Keyboard also works for testing on a PC: arrows, enter, backspace,
 * escape, tab (next page), 0-9 . + - * / = and a-f for hex digits.
 *
 * Note on bases: HEX, OCT and BIN work on 32-bit integers. Switching to
 * one of them truncates the current value. Scientific functions always
 * work in decimal and will switch the base back to DEC themselves.
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* ---- logical screen; scaled to whatever the panel actually is ---------- */
#define LW 1024
#define LH 768

/* ---- joystick mapping (tweak if your firmware differs) ----------------- */
#define BTN_A       0
#define BTN_B       1
#define BTN_X       2
#define BTN_Y       3
#define BTN_L       4
#define BTN_R       5
#define BTN_L2      6
#define BTN_R2      7
#define BTN_SELECT  8
#define BTN_START   9

#define AXIS_DEADZONE 12000

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

static void set_color(SDL_Renderer *r, Uint32 c)
{
    SDL_SetRenderDrawColor(r, (c >> 16) & 0xFF, (c >> 8) & 0xFF, c & 0xFF, 255);
}

static void fill(SDL_Renderer *r, int x, int y, int w, int h, Uint32 c)
{
    SDL_Rect rect = { x, y, w, h };
    set_color(r, c);
    SDL_RenderFillRect(r, &rect);
}

static void frame(SDL_Renderer *r, int x, int y, int w, int h, int t, Uint32 c)
{
    fill(r, x, y, w, t, c);
    fill(r, x, y + h - t, w, t, c);
    fill(r, x, y, t, h, c);
    fill(r, x + w - t, y, t, h, c);
}

static int text_w(const char *s, int scale)
{
    int n = (int)strlen(s);
    if (n == 0) return 0;
    return n * 6 * scale - scale;
}

static void draw_text(SDL_Renderer *r, const char *s, int x, int y, int scale, Uint32 c)
{
    int i, col, row;
    set_color(r, c);
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
                    SDL_RenderFillRect(r, &p);
                }
            }
        }
    }
}

/* ---- pages ------------------------------------------------------------- */

#define MAXR 5
#define MAXC 6
#define NPAGES 3

typedef struct {
    const char *name;
    int rows, cols;
    const char *k[MAXR][MAXC];
} Page;

static const Page pages[NPAGES] = {
    { "MAIN", 5, 4, {
        { "AC",   "<",    "%",    "/",    NULL,  NULL },
        { "7",    "8",    "9",    "*",    NULL,  NULL },
        { "4",    "5",    "6",    "-",    NULL,  NULL },
        { "1",    "2",    "3",    "+",    NULL,  NULL },
        { "+-",   "0",    ".",    "=",    NULL,  NULL } } },

    { "FUNC", 5, 6, {
        { "SIN",  "COS",  "TAN",  "LOG",  "LN",   "SQRT" },
        { "ASIN", "ACOS", "ATAN", "X^2",  "X^Y",  "CBRT" },
        { "SINH", "COSH", "TANH", "10^X", "E^X",  "1/X"  },
        { "PI",   "E",    "N!",   "ABS",  "MOD",  "DRG"  },
        { "MC",   "MR",   "M+",   "M-",   "MS",   "AC"   } } },

    { "BASE", 5, 6, {
        { "7",    "8",    "9",    "A",    "B",    "HEX" },
        { "4",    "5",    "6",    "C",    "D",    "DEC" },
        { "1",    "2",    "3",    "E",    "F",    "OCT" },
        { "0",    ".",    "<",    "AC",   "=",    "BIN" },
        { "AND",  "OR",   "XOR",  "NOT",  "SHL",  "SHR" } } }
};

/* ---- calculator state -------------------------------------------------- */

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

/* The display string is rounded to 10 significant figures. Keeping the
 * exact double alongside it stops precision leaking every time a result
 * is fed into the next operation (ln(E) used to give 0.9999999998). */
static double exactv     = 0.0;
static int    exact_ok   = 0;

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
        int neg = 0;
        char buf[40];
        int i = 0, j = 0;

        if (iv < 0) { neg = 1; iv = -iv; }
        u = (unsigned int)(iv & 0xFFFFFFFFLL);

        if (u == 0) {
            buf[i++] = '0';
        } else {
            while (u) {
                int d = (int)(u % (unsigned)base);
                buf[i++] = (char)(d < 10 ? '0' + d : 'A' + d - 10);
                u /= (unsigned)base;
            }
        }
        if (neg && j < (int)n - 1) out[j++] = '-';
        while (i > 0 && j < (int)n - 1) out[j++] = buf[--i];
        out[j] = '\0';
        return;
    }

    snprintf(out, n, "%.10g", v);
    /* %g can produce things like 1e+15 -- tidy the exponent a little */
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

/* ---- core operations --------------------------------------------------- */

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
        case '+': return "+";
        case '-': return "-";
        case '*': return "*";
        case '/': return "/";
        case '^': return "^";
        case 'm': return "MOD";
        case '&': return "AND";
        case '|': return "OR";
        case 'x': return "XOR";
        case 'l': return "SHL";
        case 'r': return "SHR";
        default:  return "?";
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
        case '/':
            if (b == 0.0) { *ok = 0; return 0.0; }
            return a / b;
        case '^': return pow(a, b);
        case 'm':
            if (b == 0.0) { *ok = 0; return 0.0; }
            return fmod(a, b);
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

    /* reject digits that don't exist in the current base */
    if (d >= '0' && d <= '9') val = d - '0';
    else if (d >= 'A' && d <= 'F') val = d - 'A' + 10;
    else return;
    if (val >= base) return;

    if (newentry) { strcpy(entry, "0"); newentry = 0; }
    len = strlen(entry);
    if ((int)len >= max_digits()) return;
    exact_ok = 0;

    if (strcmp(entry, "0") == 0) {
        entry[0] = d;
        entry[1] = '\0';
    } else if (strcmp(entry, "-0") == 0) {
        entry[1] = d;
        entry[2] = '\0';
    } else {
        entry[len] = d;
        entry[len + 1] = '\0';
    }
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
    if (entry[0] == '\0' || strcmp(entry, "-") == 0) strcpy(entry, "0");
    exact_ok = 0;
}

static void negate(void)
{
    if (errflag) return;
    if (entry[0] == '-') memmove(entry, entry + 1, strlen(entry));
    else if (strcmp(entry, "0") != 0) {
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
    } else {
        acc = cur;
    }
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
    } else {
        acc = cur;
    }
    pending = 0;
    set_entry(acc);
    memo[0] = '\0';
    newentry = 1;
}

/* unary functions all operate in decimal */
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

static double f_sqr(double x)  { return x * x; }
static double f_inv(double x)  { return 1.0 / x; }
static double f_p10(double x)  { return pow(10.0, x); }
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

/* ---- key dispatch ------------------------------------------------------ */

static void press(int pg, const char *lab)
{
    /* on the BASE page single letters A-F are hex digits, not functions */
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

    /* binary operators */
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

    /* percent: plain divide-by-100 on the current entry */
    else if (!strcmp(lab, "%")) {
        if (!errflag) { set_entry(entry_value() / 100.0); newentry = 1; }
    }

    /* unary functions */
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
        if (!errflag) {
            set_entry((double)(~(long long)entry_value()));
            newentry = 1;
        }
    }

    /* constants */
    else if (!strcmp(lab, "PI")) { base = 10; set_entry(M_PI); newentry = 1; }
    else if (!strcmp(lab, "E"))  { base = 10; set_entry(exp(1.0)); newentry = 1; }

    /* memory */
    else if (!strcmp(lab, "MC")) { memreg = 0.0; memheld = 0; }
    else if (!strcmp(lab, "MR")) { set_entry(memreg); newentry = 1; }
    else if (!strcmp(lab, "MS")) { memreg = entry_value(); memheld = 1; newentry = 1; }
    else if (!strcmp(lab, "M+")) { memreg += entry_value(); memheld = 1; newentry = 1; }
    else if (!strcmp(lab, "M-")) { memreg -= entry_value(); memheld = 1; newentry = 1; }

    /* modes */
    else if (!strcmp(lab, "DRG")) degrees = !degrees;
    else if (!strcmp(lab, "HEX")) set_base(16);
    else if (!strcmp(lab, "DEC")) set_base(10);
    else if (!strcmp(lab, "OCT")) set_base(8);
    else if (!strcmp(lab, "BIN")) set_base(2);
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
    sel_c += dx;
    sel_r += dy;
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

#define GX   48
#define GY   248
#define GAP  14
#define GW   (LW - 2 * GX)
#define GH   (LH - GY - 44)

static void render(SDL_Renderer *ren)
{
    const Page *p = &pages[page];
    int rows = p->rows, cols = p->cols;
    int cw = (GW - (cols - 1) * GAP) / cols;
    int ch = (GH - (rows - 1) * GAP) / rows;
    int r, c, tw;
    char status[64];

    fill(ren, 0, 0, LW, LH, 0x10131A);

    /* display panel */
    fill(ren, GX, 44, GW, 174, 0x1B202C);
    frame(ren, GX, 44, GW, 174, 3, 0x2E3648);

    /* status line: page, angle mode, base, memory flag */
    snprintf(status, sizeof(status), "%s  %s%s%s",
             p->name,
             degrees ? "DEG" : "RAD",
             base == 16 ? "  HEX" : base == 8 ? "  OCT" :
             base == 2  ? "  BIN" : "",
             memheld ? "  M" : "");
    draw_text(ren, status, GX + 20, 62, 3, 0x6B7689);

    if (memo[0]) {
        tw = text_w(memo, 3);
        draw_text(ren, memo, GX + GW - 20 - tw, 62, 3, 0x8E9AAF);
    }
    {
        int scale = 8;
        while (scale > 2 && text_w(entry, scale) > GW - 40) scale--;
        tw = text_w(entry, scale);
        draw_text(ren, entry, GX + GW - 20 - tw, 132,
                  scale, errflag ? 0xE06060 : 0xF2F5FA);
    }

    /* page indicator dots */
    for (c = 0; c < NPAGES; c++)
        fill(ren, GX + 20 + c * 22, 196, 14, 6,
             c == page ? 0xFFC24D : 0x39425A);

    /* keypad */
    for (r = 0; r < rows; r++) {
        for (c = 0; c < cols; c++) {
            const char *lab = p->k[r][c];
            int x, y, selected, s;
            Uint32 bg, fg;

            if (!lab) continue;

            x = GX + c * (cw + GAP);
            y = GY + r * (ch + GAP);
            selected = (r == sel_r && c == sel_c);

            if (!strcmp(lab, "="))                bg = 0x2F6BD8;
            else if (!strcmp(lab, "AC"))          bg = 0x3A2430;
            else if (c == cols - 1)               bg = 0x243247;
            else if (page == 0 && r == 0)         bg = 0x2A2230;
            else if (page > 0 && r == rows - 1)   bg = 0x232B3C;
            else                                  bg = 0x1E2330;
            fg = 0xE8ECF4;

            /* highlight whichever base is active */
            if ((!strcmp(lab, "HEX") && base == 16) ||
                (!strcmp(lab, "DEC") && base == 10) ||
                (!strcmp(lab, "OCT") && base == 8)  ||
                (!strcmp(lab, "BIN") && base == 2))
                bg = 0x2C5A46;

            if (selected) bg = (!strcmp(lab, "=")) ? 0x4F8BFF : 0x3A4460;

            fill(ren, x, y, cw, ch, bg);
            if (selected) frame(ren, x - 4, y - 4, cw + 8, ch + 8, 4, 0xFFC24D);
            else          frame(ren, x, y, cw, ch, 2, 0x2B3243);

            /* DRG shows the current mode rather than its own name */
            {
                const char *shown = !strcmp(lab, "DRG")
                                    ? (degrees ? "DEG" : "RAD") : lab;
                s = (cols <= 4) ? 6 : 5;
                while (s > 2 && text_w(shown, s) > cw - 12) s--;
                tw = text_w(shown, s);
                draw_text(ren, shown, x + (cw - tw) / 2,
                          y + (ch - 7 * s) / 2, s, fg);
            }
        }
    }

    SDL_RenderPresent(ren);
}

/* ---- main -------------------------------------------------------------- */

int main(int argc, char *argv[])
{
    SDL_Window *win;
    SDL_Renderer *ren;
    SDL_Joystick *joy = NULL;
    int running = 1;
    int axdir[2] = { 0, 0 };

    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    if (SDL_NumJoysticks() > 0) {
        joy = SDL_JoystickOpen(0);
        printf("joystick: %s (%d buttons, %d axes, %d hats)\n",
               joy ? SDL_JoystickName(joy) : "none",
               joy ? SDL_JoystickNumButtons(joy) : 0,
               joy ? SDL_JoystickNumAxes(joy) : 0,
               joy ? SDL_JoystickNumHats(joy) : 0);
    } else {
        printf("no joystick detected, keyboard only\n");
    }
    fflush(stdout);

    win = SDL_CreateWindow("Calculator",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           LW, LH, SDL_WINDOW_FULLSCREEN_DESKTOP);
    if (!win) {
        fprintf(stderr, "CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
    if (!ren) ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_SOFTWARE);
    if (!ren) {
        fprintf(stderr, "CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(win);
        SDL_Quit();
        return 1;
    }

    SDL_RenderSetLogicalSize(ren, LW, LH);
    SDL_ShowCursor(SDL_DISABLE);

    render(ren);

    while (running) {
        SDL_Event e;
        int dirty = 0;

        if (!SDL_WaitEventTimeout(&e, 250)) continue;

        do {
            switch (e.type) {
            case SDL_QUIT:
                running = 0;
                break;

            case SDL_KEYDOWN:
                dirty = 1;
                switch (e.key.keysym.sym) {
                case SDLK_ESCAPE: running = 0; break;
                case SDLK_LEFT:   move_sel(-1, 0); break;
                case SDLK_RIGHT:  move_sel(1, 0); break;
                case SDLK_UP:     move_sel(0, -1); break;
                case SDLK_DOWN:   move_sel(0, 1); break;
                case SDLK_TAB:      change_page(1); break;
                case SDLK_PAGEUP:   change_page(-1); break;
                case SDLK_PAGEDOWN: change_page(1); break;
                case SDLK_RETURN:
                case SDLK_KP_ENTER:
                case SDLK_SPACE:  press_sel(); break;
                case SDLK_BACKSPACE: backspace(); break;
                case SDLK_DELETE: calc_reset(); break;
                case SDLK_PLUS:
                case SDLK_KP_PLUS:     set_op('+'); break;
                case SDLK_MINUS:
                case SDLK_KP_MINUS:    set_op('-'); break;
                case SDLK_ASTERISK:
                case SDLK_KP_MULTIPLY: set_op('*'); break;
                case SDLK_SLASH:
                case SDLK_KP_DIVIDE:   set_op('/'); break;
                case SDLK_EQUALS:      equals(); break;
                case SDLK_PERIOD:
                case SDLK_KP_PERIOD:   push_dot(); break;
                default:
                    if (e.key.keysym.sym >= SDLK_0 && e.key.keysym.sym <= SDLK_9)
                        push_digit((char)('0' + (e.key.keysym.sym - SDLK_0)));
                    else if (e.key.keysym.sym >= SDLK_KP_1 && e.key.keysym.sym <= SDLK_KP_9)
                        push_digit((char)('1' + (e.key.keysym.sym - SDLK_KP_1)));
                    else if (e.key.keysym.sym == SDLK_KP_0)
                        push_digit('0');
                    else if (e.key.keysym.sym >= SDLK_a && e.key.keysym.sym <= SDLK_f)
                        push_digit((char)('A' + (e.key.keysym.sym - SDLK_a)));
                    break;
                }
                break;

            case SDL_JOYBUTTONDOWN:
                dirty = 1;
                printf("joy button %d\n", e.jbutton.button);
                fflush(stdout);
                if (e.jbutton.button == BTN_A || e.jbutton.button == BTN_B)
                    press_sel();
                else if (e.jbutton.button == BTN_X)
                    backspace();
                else if (e.jbutton.button == BTN_Y)
                    calc_reset();
                else if (e.jbutton.button == BTN_L || e.jbutton.button == BTN_L2)
                    change_page(-1);
                else if (e.jbutton.button == BTN_R || e.jbutton.button == BTN_R2)
                    change_page(1);
                else if (e.jbutton.button == BTN_SELECT || e.jbutton.button == BTN_START)
                    running = 0;
                break;

            case SDL_JOYHATMOTION:
                dirty = 1;
                if (e.jhat.value & SDL_HAT_LEFT)  move_sel(-1, 0);
                if (e.jhat.value & SDL_HAT_RIGHT) move_sel(1, 0);
                if (e.jhat.value & SDL_HAT_UP)    move_sel(0, -1);
                if (e.jhat.value & SDL_HAT_DOWN)  move_sel(0, 1);
                break;

            case SDL_JOYAXISMOTION:
                if (e.jaxis.axis < 2) {
                    int dir = 0;
                    if (e.jaxis.value < -AXIS_DEADZONE) dir = -1;
                    else if (e.jaxis.value > AXIS_DEADZONE) dir = 1;
                    if (dir != axdir[e.jaxis.axis]) {
                        axdir[e.jaxis.axis] = dir;
                        if (dir) {
                            if (e.jaxis.axis == 0) move_sel(dir, 0);
                            else                   move_sel(0, dir);
                            dirty = 1;
                        }
                    }
                }
                break;

            default:
                break;
            }
        } while (SDL_PollEvent(&e));

        if (dirty && running) render(ren);
    }

    if (joy) SDL_JoystickClose(joy);
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
