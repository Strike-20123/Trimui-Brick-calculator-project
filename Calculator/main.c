/*
 * Brick Calculator
 * ----------------
 * A pocket calculator for the TrimUI Brick (and Smart Pro).
 *
 * Deliberately has ONE dependency: libSDL2. No SDL2_ttf, no SDL2_image,
 * no font files, no PNGs. Everything is drawn with filled rectangles and
 * a 5x7 bitmap font compiled into the binary. That keeps the build from
 * breaking and keeps the app from failing at runtime over a missing asset.
 *
 * Controls (joystick button numbers differ between firmwares -- every
 * press is logged to stdout so you can check the real numbers in log.txt
 * and adjust the BTN_* defines below if needed):
 *
 *   D-pad / left stick ... move selection
 *   A or B ............... press the highlighted key
 *   X .................... backspace
 *   Y .................... clear
 *   SELECT / START ....... quit
 *
 * Keyboard also works (handy for testing on a PC):
 *   arrows, enter/space, backspace, escape, 0-9 . + - * / % =
 */

#include <SDL2/SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* ---- logical screen; scaled to whatever the panel actually is ---------- */
#define LW 1024
#define LH 768

/* ---- joystick mapping (tweak if your firmware differs) ----------------- */
#define BTN_A       0
#define BTN_B       1
#define BTN_X       2
#define BTN_Y       3
#define BTN_SELECT  8
#define BTN_START   9

#define AXIS_DEADZONE 12000

/* ---- 5x7 font, ASCII 32..90, column-major, bit0 = top row -------------- */
static const unsigned char font5x7[59][5] = {
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
    {0x61,0x51,0x49,0x45,0x43}  /* Z */
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
        if (idx < 0 || idx > 58) idx = 0;
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

/* ---- calculator state -------------------------------------------------- */

#define ROWS 5
#define COLS 4
#define MAXDIGITS 14

static const char *labels[ROWS][COLS] = {
    { "C",  "<",  "%",  "/" },
    { "7",  "8",  "9",  "*" },
    { "4",  "5",  "6",  "-" },
    { "1",  "2",  "3",  "+" },
    { "+-", "0",  ".",  "=" }
};

static char entry[48] = "0";
static char memo[64]  = "";
static double acc     = 0.0;
static char  pending  = 0;
static int   newentry = 1;
static int   errflag  = 0;
static int   sel_r = 1, sel_c = 0;

static void fmt_num(double v, char *out, size_t n)
{
    char *p;
    if (!isfinite(v)) { snprintf(out, n, "ERROR"); return; }
    snprintf(out, n, "%.10g", v);
    /* %g can produce things like 1e+15 -- tidy the exponent a little */
    p = strstr(out, "e+");
    if (p) memmove(p + 1, p + 2, strlen(p + 2) + 1);
}

static void calc_reset(void)
{
    strcpy(entry, "0");
    memo[0] = '\0';
    acc = 0.0;
    pending = 0;
    newentry = 1;
    errflag = 0;
}

static double apply_op(double a, char op, double b, int *ok)
{
    *ok = 1;
    switch (op) {
        case '+': return a + b;
        case '-': return a - b;
        case '*': return a * b;
        case '/':
            if (b == 0.0) { *ok = 0; return 0.0; }
            return a / b;
        default: return b;
    }
}

static void push_digit(char d)
{
    size_t len;
    if (errflag) calc_reset();
    if (newentry) { strcpy(entry, "0"); newentry = 0; }
    len = strlen(entry);
    if (len >= MAXDIGITS) return;
    if (strcmp(entry, "0") == 0 && d != '.') {
        entry[0] = d;
        entry[1] = '\0';
    } else if (strcmp(entry, "-0") == 0 && d != '.') {
        entry[1] = d;
        entry[2] = '\0';
    } else {
        entry[len] = d;
        entry[len + 1] = '\0';
    }
}

static void push_dot(void)
{
    if (errflag) calc_reset();
    if (newentry) { strcpy(entry, "0"); newentry = 0; }
    if (strchr(entry, '.')) return;
    if (strlen(entry) >= MAXDIGITS) return;
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
}

static void negate(void)
{
    if (errflag) return;
    if (entry[0] == '-') memmove(entry, entry + 1, strlen(entry));
    else if (strcmp(entry, "0") != 0) {
        memmove(entry + 1, entry, strlen(entry) + 1);
        entry[0] = '-';
    }
    newentry = 0;
}

static void percent(void)
{
    double v;
    if (errflag) return;
    v = atof(entry) / 100.0;
    fmt_num(v, entry, sizeof(entry));
    newentry = 1;
}

static void set_op(char op)
{
    double cur;
    int ok;
    if (errflag) return;
    cur = atof(entry);
    if (pending && !newentry) {
        acc = apply_op(acc, pending, cur, &ok);
        if (!ok) { errflag = 1; strcpy(entry, "DIV BY 0"); memo[0] = '\0'; return; }
        fmt_num(acc, entry, sizeof(entry));
    } else {
        acc = cur;
    }
    pending = op;
    newentry = 1;
    snprintf(memo, sizeof(memo), "%.10g %c", acc, op);
}

static void equals(void)
{
    double cur;
    int ok;
    if (errflag) return;
    cur = atof(entry);
    if (pending) {
        acc = apply_op(acc, pending, cur, &ok);
        if (!ok) { errflag = 1; strcpy(entry, "DIV BY 0"); memo[0] = '\0'; pending = 0; return; }
    } else {
        acc = cur;
    }
    pending = 0;
    fmt_num(acc, entry, sizeof(entry));
    memo[0] = '\0';
    newentry = 1;
}

static void press(const char *lab)
{
    if (strcmp(lab, "C") == 0)        calc_reset();
    else if (strcmp(lab, "<") == 0)   backspace();
    else if (strcmp(lab, "%") == 0)   percent();
    else if (strcmp(lab, "+-") == 0)  negate();
    else if (strcmp(lab, "=") == 0)   equals();
    else if (strcmp(lab, ".") == 0)   push_dot();
    else if (lab[0] >= '0' && lab[0] <= '9') push_digit(lab[0]);
    else set_op(lab[0]);
}

static void press_sel(void) { press(labels[sel_r][sel_c]); }

static void move_sel(int dx, int dy)
{
    sel_c += dx;
    sel_r += dy;
    if (sel_c < 0) sel_c = COLS - 1;
    if (sel_c >= COLS) sel_c = 0;
    if (sel_r < 0) sel_r = ROWS - 1;
    if (sel_r >= ROWS) sel_r = 0;
}

/* ---- layout ------------------------------------------------------------ */

#define GX   48
#define GY   248
#define GAP  14
#define GW   (LW - 2 * GX)
#define GH   (LH - GY - 48)
#define CW   ((GW - (COLS - 1) * GAP) / COLS)
#define CH   ((GH - (ROWS - 1) * GAP) / ROWS)

static void render(SDL_Renderer *ren)
{
    int r, c, tw;

    fill(ren, 0, 0, LW, LH, 0x10131A);

    /* display panel */
    fill(ren, GX, 48, GW, 168, 0x1B202C);
    frame(ren, GX, 48, GW, 168, 3, 0x2E3648);

    if (memo[0]) {
        tw = text_w(memo, 3);
        draw_text(ren, memo, GX + GW - 24 - tw, 70, 3, 0x6B7689);
    }
    {
        int scale = 8;
        while (scale > 3 && text_w(entry, scale) > GW - 48) scale--;
        tw = text_w(entry, scale);
        draw_text(ren, entry, GX + GW - 24 - tw, 132,
                  scale, errflag ? 0xE06060 : 0xF2F5FA);
    }

    /* keypad */
    for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
            int x = GX + c * (CW + GAP);
            int y = GY + r * (CH + GAP);
            const char *lab = labels[r][c];
            int selected = (r == sel_r && c == sel_c);
            Uint32 bg, fg;

            if (strcmp(lab, "=") == 0)                bg = 0x2F6BD8;
            else if (c == COLS - 1)                   bg = 0x243247;
            else if (r == 0)                          bg = 0x2A2230;
            else                                      bg = 0x1E2330;
            fg = 0xE8ECF4;

            if (selected) {
                bg = (strcmp(lab, "=") == 0) ? 0x4F8BFF : 0x3A4460;
            }

            fill(ren, x, y, CW, CH, bg);
            if (selected) frame(ren, x - 4, y - 4, CW + 8, CH + 8, 4, 0xFFC24D);
            else          frame(ren, x, y, CW, CH, 2, 0x2B3243);

            {
                int s = 6;
                tw = text_w(lab, s);
                draw_text(ren, lab, x + (CW - tw) / 2, y + (CH - 7 * s) / 2, s, fg);
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
