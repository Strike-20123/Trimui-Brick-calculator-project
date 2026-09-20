/*
 * TrimUI Brick Calculator
 * A D-pad / face-button driven calculator for devices with no touchscreen.
 * Renders a fixed 1024x768 UI (native Brick resolution) with SDL2 + SDL2_ttf.
 */

#define _DEFAULT_SOURCE
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <ctype.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#ifndef M_E
#define M_E 2.71828182845904523536
#endif

#define SCREEN_W 1024
#define SCREEN_H 768
#define COLS 4
#define MAX_ROWS 6
#define EXPR_MAX 128
#define GAMECONTROLLERDB_PATH "/usr/trimui/gamecontrollerdb.txt"

typedef enum {
    BTN_DIGIT,
    BTN_OP,
    BTN_FUNC,
    BTN_PARENS,
    BTN_CLEAR,
    BTN_BACK,
    BTN_EQUALS
} ButtonKind;

typedef struct {
    const char *label;
    const char *insert;
    ButtonKind kind;
} Button;

/* Standard mode: 5 rows x 4 cols */
static Button standardGrid[5][COLS] = {
    { {"C", "", BTN_CLEAR},   {"( )", "", BTN_PARENS}, {"%", "%", BTN_OP},  {"/", "/", BTN_OP} },
    { {"7", "7", BTN_DIGIT},  {"8", "8", BTN_DIGIT},   {"9", "9", BTN_DIGIT}, {"*", "*", BTN_OP} },
    { {"4", "4", BTN_DIGIT},  {"5", "5", BTN_DIGIT},   {"6", "6", BTN_DIGIT}, {"-", "-", BTN_OP} },
    { {"1", "1", BTN_DIGIT},  {"2", "2", BTN_DIGIT},   {"3", "3", BTN_DIGIT}, {"+", "+", BTN_OP} },
    { {"0", "0", BTN_DIGIT},  {".", ".", BTN_DIGIT},   {"<-", "", BTN_BACK}, {"=", "", BTN_EQUALS} },
};

/* Scientific mode: 6 rows x 4 cols (extra function row + power/const row on top) */
static Button sciGrid[6][COLS] = {
    { {"sin", "sin(", BTN_FUNC}, {"cos", "cos(", BTN_FUNC}, {"tan", "tan(", BTN_FUNC}, {"sqrt", "sqrt(", BTN_FUNC} },
    { {"^", "^", BTN_OP},        {"pi", "pi", BTN_DIGIT},   {"e", "e", BTN_DIGIT},     {"ln", "ln(", BTN_FUNC} },
    { {"C", "", BTN_CLEAR},      {"( )", "", BTN_PARENS},   {"%", "%", BTN_OP},        {"/", "/", BTN_OP} },
    { {"7", "7", BTN_DIGIT},     {"8", "8", BTN_DIGIT},     {"9", "9", BTN_DIGIT},     {"*", "*", BTN_OP} },
    { {"4", "4", BTN_DIGIT},     {"5", "5", BTN_DIGIT},     {"6", "6", BTN_DIGIT},     {"-", "-", BTN_OP} },
    { {"1", "1", BTN_DIGIT},     {"2", "2", BTN_DIGIT},     {"3", "3", BTN_DIGIT},     {"+", "+", BTN_OP} },
};
/* Note: scientific mode's final "0 . <- =" row is appended dynamically below
 * since the array above only fits 6 rows; see rowsFor()/labelFor() helpers. */

typedef struct {
    const char *s;
    int pos;
    int len;
    int error;
} Parser;

static double parseExpr(Parser *p);

static int p_match(Parser *p, char c) {
    if (p->pos < p->len && p->s[p->pos] == c) { p->pos++; return 1; }
    return 0;
}

static int p_starts(Parser *p, const char *kw) {
    int l = (int)strlen(kw);
    if (p->pos + l <= p->len && strncmp(p->s + p->pos, kw, l) == 0) {
        p->pos += l;
        return 1;
    }
    return 0;
}

static double parsePrimary(Parser *p) {
    if (p->error) return 0;
    if (p_match(p, '(')) {
        double v = parseExpr(p);
        if (!p_match(p, ')')) p->error = 1;
        return v;
    }
    if (p_starts(p, "sin(")) { double v = parseExpr(p); if (!p_match(p, ')')) p->error = 1; return sin(v * M_PI / 180.0); }
    if (p_starts(p, "cos(")) { double v = parseExpr(p); if (!p_match(p, ')')) p->error = 1; return cos(v * M_PI / 180.0); }
    if (p_starts(p, "tan(")) { double v = parseExpr(p); if (!p_match(p, ')')) p->error = 1; return tan(v * M_PI / 180.0); }
    if (p_starts(p, "sqrt(")) { double v = parseExpr(p); if (!p_match(p, ')')) p->error = 1; if (v < 0) { p->error = 1; return 0; } return sqrt(v); }
    if (p_starts(p, "ln(")) { double v = parseExpr(p); if (!p_match(p, ')')) p->error = 1; if (v <= 0) { p->error = 1; return 0; } return log(v); }
    if (p_starts(p, "log(")) { double v = parseExpr(p); if (!p_match(p, ')')) p->error = 1; if (v <= 0) { p->error = 1; return 0; } return log10(v); }
    if (p_starts(p, "pi")) return M_PI;
    if (p->pos < p->len && p->s[p->pos] == 'e' &&
        !(p->pos + 1 < p->len && (isalpha((unsigned char)p->s[p->pos + 1])))) {
        p->pos++;
        return M_E;
    }
    if (p->pos < p->len && (isdigit((unsigned char)p->s[p->pos]) || p->s[p->pos] == '.')) {
        char *end = NULL;
        double v = strtod(p->s + p->pos, &end);
        if (end == p->s + p->pos) { p->error = 1; return 0; }
        p->pos = (int)(end - p->s);
        return v;
    }
    p->error = 1;
    return 0;
}

static double parsePostfix(Parser *p) {
    double v = parsePrimary(p);
    while (p_match(p, '%')) v = v / 100.0;
    return v;
}

static double parseUnary(Parser *p) {
    if (p_match(p, '-')) return -parseUnary(p);
    if (p_match(p, '+')) return parseUnary(p);
    {
        double v = parsePostfix(p);
        if (p_match(p, '^')) {
            double e = parseUnary(p);
            v = pow(v, e);
        }
        return v;
    }
}

static double parseTerm(Parser *p) {
    double v = parseUnary(p);
    for (;;) {
        if (p_match(p, '*')) v *= parseUnary(p);
        else if (p_match(p, '/')) {
            double d = parseUnary(p);
            if (d == 0) { p->error = 1; return 0; }
            v /= d;
        } else break;
    }
    return v;
}

static double parseExpr(Parser *p) {
    double v = parseTerm(p);
    for (;;) {
        if (p_match(p, '+')) v += parseTerm(p);
        else if (p_match(p, '-')) v -= parseTerm(p);
        else break;
    }
    return v;
}

/* Returns 1 on success and writes result, 0 on parse error */
static int evaluate(const char *expr, double *result) {
    if (expr[0] == '\0') return 0;
    Parser p = { expr, 0, (int)strlen(expr), 0 };
    double v = parseExpr(&p);
    if (p.error || p.pos != p.len || !isfinite(v)) return 0;
    *result = v;
    return 1;
}

/* ---- App state ---- */

typedef struct {
    int mode;          /* 0 = standard, 1 = scientific */
    int cursorRow, cursorCol;
    char expr[EXPR_MAX];
    char display[EXPR_MAX];
    int justEvaluated;
    int hasError;
} AppState;

static int rowsFor(int mode) { return mode == 0 ? 5 : 7; }

/* Scientific mode row 6 (index 6, 0-based) is the shared bottom row that
 * didn't fit in the sciGrid array; synthesize it here. */
static Button sciBottomRow[COLS] = {
    {"0", "0", BTN_DIGIT}, {".", ".", BTN_DIGIT}, {"<-", "", BTN_BACK}, {"=", "", BTN_EQUALS}
};

static Button getButton(int mode, int row, int col) {
    if (mode == 0) return standardGrid[row][col];
    if (row < 6) return sciGrid[row][col];
    return sciBottomRow[col];
}

static void doClear(AppState *st) {
    st->expr[0] = '\0';
    st->display[0] = '\0';
    st->hasError = 0;
    st->justEvaluated = 0;
}

static void doBackspace(AppState *st) {
    size_t n = strlen(st->expr);
    if (n > 0) st->expr[n - 1] = '\0';
    st->hasError = 0;
    st->justEvaluated = 0;
}

static void doParens(AppState *st) {
    int open = 0, close = 0;
    for (size_t i = 0; i < strlen(st->expr); i++) {
        if (st->expr[i] == '(') open++;
        if (st->expr[i] == ')') close++;
    }
    char c = (open > close) ? ')' : '(';
    size_t n = strlen(st->expr);
    if (n + 1 < EXPR_MAX) { st->expr[n] = c; st->expr[n + 1] = '\0'; }
}

static void doInsert(AppState *st, const char *text) {
    if (st->justEvaluated) {
        /* Chain from previous result if an operator follows; otherwise start fresh */
        char c = text[0];
        int isOperatorStart = (c == '+' || c == '-' || c == '*' || c == '/' || c == '^' || c == '%');
        if (!isOperatorStart) {
            st->expr[0] = '\0';
        }
        st->justEvaluated = 0;
        st->hasError = 0;
    }
    size_t n = strlen(st->expr);
    size_t add = strlen(text);
    if (n + add + 1 < EXPR_MAX) {
        strcat(st->expr, text);
    }
}

static void doEquals(AppState *st) {
    double result;
    if (evaluate(st->expr, &result)) {
        snprintf(st->display, sizeof(st->display), "%.10g", result);
        strncpy(st->expr, st->display, EXPR_MAX - 1);
        st->expr[EXPR_MAX - 1] = '\0';
        st->hasError = 0;
    } else {
        strcpy(st->display, "Error");
        st->hasError = 1;
    }
    st->justEvaluated = 1;
}

static void pressButton(AppState *st, Button b) {
    switch (b.kind) {
        case BTN_CLEAR: doClear(st); break;
        case BTN_BACK: doBackspace(st); break;
        case BTN_PARENS: doParens(st); break;
        case BTN_EQUALS: doEquals(st); break;
        case BTN_DIGIT:
        case BTN_OP:
        case BTN_FUNC:
            doInsert(st, b.insert);
            break;
    }
}

/* ---- Rendering ---- */

typedef struct { Uint8 r, g, b; } Color;
static const Color COL_BG        = {20, 20, 15};
static const Color COL_DIGIT     = {42, 42, 32};
static const Color COL_DIGIT_TXT = {245, 243, 232};
static const Color COL_OP        = {58, 47, 28};
static const Color COL_OP_TXT    = {240, 153, 123};
static const Color COL_UTIL      = {35, 35, 25};
static const Color COL_UTIL_TXT  = {201, 200, 188};
static const Color COL_EQ        = {29, 95, 74};
static const Color COL_EQ_TXT    = {159, 225, 203};
static const Color COL_CURSOR    = {127, 119, 221};
static const Color COL_MUTED     = {138, 137, 125};
static const Color COL_TAB_BG    = {42, 42, 34};

static void drawText(SDL_Renderer *ren, TTF_Font *font, const char *text, int cx, int cy, Color col, int centerX, int centerY) {
    if (!text || text[0] == '\0') return;
    SDL_Color sc = { col.r, col.g, col.b, 255 };
    SDL_Surface *surf = TTF_RenderUTF8_Blended(font, text, sc);
    if (!surf) return;
    SDL_Texture *tex = SDL_CreateTextureFromSurface(ren, surf);
    SDL_Rect dst;
    dst.w = surf->w;
    dst.h = surf->h;
    dst.x = centerX ? cx - surf->w / 2 : cx;
    dst.y = centerY ? cy - surf->h / 2 : cy;
    SDL_FreeSurface(surf);
    SDL_RenderCopy(ren, tex, NULL, &dst);
    SDL_DestroyTexture(tex);
}

static void fillRect(SDL_Renderer *ren, SDL_Rect r, Color c) {
    SDL_SetRenderDrawColor(ren, c.r, c.g, c.b, 255);
    SDL_RenderFillRect(ren, &r);
}

static void render(SDL_Renderer *ren, TTF_Font *fontBig, TTF_Font *fontMid, TTF_Font *fontSmall, AppState *st) {
    SDL_SetRenderDrawColor(ren, COL_BG.r, COL_BG.g, COL_BG.b, 255);
    SDL_RenderClear(ren);

    int margin = 20;
    int gridTop = 150;
    int rows = rowsFor(st->mode);
    int gridH = SCREEN_H - gridTop - margin;
    int gridW = SCREEN_W - margin * 2;
    int cellW = gridW / COLS;
    int cellH = gridH / rows;
    int gap = 8;

    /* Mode tabs */
    SDL_Rect tab1 = { margin, 16, 160, 34 };
    SDL_Rect tab2 = { margin + 170, 16, 160, 34 };
    fillRect(ren, tab1, st->mode == 0 ? COL_TAB_BG : COL_BG);
    fillRect(ren, tab2, st->mode == 1 ? COL_TAB_BG : COL_BG);
    drawText(ren, fontSmall, "standard", tab1.x + tab1.w / 2, tab1.y + tab1.h / 2, st->mode == 0 ? COL_DIGIT_TXT : COL_MUTED, 1, 1);
    drawText(ren, fontSmall, "scientific", tab2.x + tab2.w / 2, tab2.y + tab2.h / 2, st->mode == 1 ? COL_DIGIT_TXT : COL_MUTED, 1, 1);
    drawText(ren, fontSmall, "L1/R1: mode", SCREEN_W - margin, 16 + 17, COL_MUTED, 0, 1);

    /* Display */
    const char *exprLine = st->justEvaluated ? "" : st->expr;
    const char *resultLine = st->justEvaluated ? st->display : (st->expr[0] ? st->expr : "0");
    drawText(ren, fontSmall, exprLine, SCREEN_W - margin, 66, COL_MUTED, 0, 0);
    drawText(ren, fontBig, resultLine, SCREEN_W - margin, 90, st->hasError ? (Color){224, 75, 74} : COL_DIGIT_TXT, 0, 0);

    SDL_SetRenderDrawColor(ren, 47, 47, 38, 255);
    SDL_RenderDrawLine(ren, margin, gridTop - 12, SCREEN_W - margin, gridTop - 12);

    for (int r = 0; r < rows; r++) {
        for (int c = 0; c < COLS; c++) {
            Button b = getButton(st->mode, r, c);
            SDL_Rect cell;
            cell.x = margin + c * cellW + gap / 2;
            cell.y = gridTop + r * cellH + gap / 2;
            cell.w = cellW - gap;
            cell.h = cellH - gap;

            Color bg, txt;
            switch (b.kind) {
                case BTN_OP:     bg = COL_OP; txt = COL_OP_TXT; break;
                case BTN_EQUALS: bg = COL_EQ; txt = COL_EQ_TXT; break;
                case BTN_CLEAR:
                case BTN_BACK:
                case BTN_PARENS: bg = COL_UTIL; txt = COL_UTIL_TXT; break;
                case BTN_FUNC:   bg = COL_UTIL; txt = COL_UTIL_TXT; break;
                default:         bg = COL_DIGIT; txt = COL_DIGIT_TXT; break;
            }
            fillRect(ren, cell, bg);

            if (r == st->cursorRow && c == st->cursorCol) {
                SDL_SetRenderDrawColor(ren, COL_CURSOR.r, COL_CURSOR.g, COL_CURSOR.b, 255);
                SDL_Rect outline = cell;
                for (int t = 0; t < 3; t++) {
                    SDL_Rect o = { outline.x - t, outline.y - t, outline.w + t * 2, outline.h + t * 2 };
                    SDL_RenderDrawRect(ren, &o);
                }
            }

            TTF_Font *f = (b.kind == BTN_FUNC || strlen(b.label) > 2) ? fontSmall : fontMid;
            drawText(ren, f, b.label, cell.x + cell.w / 2, cell.y + cell.h / 2, txt, 1, 1);
        }
    }

    SDL_RenderPresent(ren);
}

/* ---- Input handling ---- */

static void moveCursor(AppState *st, int dr, int dc) {
    int rows = rowsFor(st->mode);
    st->cursorRow = (st->cursorRow + dr + rows) % rows;
    st->cursorCol = (st->cursorCol + dc + COLS) % COLS;
}

static void toggleMode(AppState *st) {
    st->mode = 1 - st->mode;
    st->cursorRow = 0;
    st->cursorCol = 0;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_JOYSTICK) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        fprintf(stderr, "TTF_Init failed: %s\n", TTF_GetError());
        return 1;
    }

    /* Load device controller mappings if present (harmless if missing) */
    SDL_GameControllerAddMappingsFromFile(GAMECONTROLLERDB_PATH);
    for (int i = 0; i < SDL_NumJoysticks(); i++) {
        if (SDL_IsGameController(i)) SDL_GameControllerOpen(i);
    }

    SDL_Window *win = SDL_CreateWindow("Calculator", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
                                        SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN | SDL_WINDOW_FULLSCREEN);
    if (!win) {
        fprintf(stderr, "CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_Renderer *ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!ren) {
        fprintf(stderr, "CreateRenderer failed: %s\n", SDL_GetError());
        return 1;
    }

    TTF_Font *fontBig = TTF_OpenFont("res/font.ttf", 56);
    TTF_Font *fontMid = TTF_OpenFont("res/font.ttf", 32);
    TTF_Font *fontSmall = TTF_OpenFont("res/font.ttf", 20);
    if (!fontBig || !fontMid || !fontSmall) {
        fprintf(stderr, "Failed to load res/font.ttf: %s\n", TTF_GetError());
        return 1;
    }

    AppState st;
    memset(&st, 0, sizeof(st));

    int running = 1;
    Uint32 lastRepeat = 0;
    int heldDir = -1; /* 0=up 1=down 2=left 3=right */
    const Uint32 REPEAT_DELAY = 320;
    const Uint32 REPEAT_RATE = 130;

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {
                case SDL_QUIT:
                    running = 0;
                    break;
                case SDL_CONTROLLERDEVICEADDED:
                    SDL_GameControllerOpen(e.cdevice.which);
                    break;
                case SDL_CONTROLLERBUTTONDOWN: {
                    Uint8 btn = e.cbutton.button;
                    if (btn == SDL_CONTROLLER_BUTTON_DPAD_UP) { moveCursor(&st, -1, 0); heldDir = 0; lastRepeat = SDL_GetTicks(); }
                    else if (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN) { moveCursor(&st, 1, 0); heldDir = 1; lastRepeat = SDL_GetTicks(); }
                    else if (btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT) { moveCursor(&st, 0, -1); heldDir = 2; lastRepeat = SDL_GetTicks(); }
                    else if (btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT) { moveCursor(&st, 0, 1); heldDir = 3; lastRepeat = SDL_GetTicks(); }
                    else if (btn == SDL_CONTROLLER_BUTTON_A) pressButton(&st, getButton(st.mode, st.cursorRow, st.cursorCol));
                    else if (btn == SDL_CONTROLLER_BUTTON_B) doBackspace(&st);
                    else if (btn == SDL_CONTROLLER_BUTTON_X) doClear(&st);
                    else if (btn == SDL_CONTROLLER_BUTTON_Y) doParens(&st);
                    else if (btn == SDL_CONTROLLER_BUTTON_LEFTSHOULDER) toggleMode(&st);
                    else if (btn == SDL_CONTROLLER_BUTTON_RIGHTSHOULDER) toggleMode(&st);
                    else if (btn == SDL_CONTROLLER_BUTTON_START) running = 0;
                    break;
                }
                case SDL_CONTROLLERBUTTONUP: {
                    Uint8 btn = e.cbutton.button;
                    if ((btn == SDL_CONTROLLER_BUTTON_DPAD_UP && heldDir == 0) ||
                        (btn == SDL_CONTROLLER_BUTTON_DPAD_DOWN && heldDir == 1) ||
                        (btn == SDL_CONTROLLER_BUTTON_DPAD_LEFT && heldDir == 2) ||
                        (btn == SDL_CONTROLLER_BUTTON_DPAD_RIGHT && heldDir == 3)) {
                        heldDir = -1;
                    }
                    break;
                }
                case SDL_KEYDOWN: {
                    SDL_Keycode k = e.key.keysym.sym;
                    if (k == SDLK_ESCAPE) running = 0;
                    else if (k == SDLK_UP) { moveCursor(&st, -1, 0); heldDir = 0; lastRepeat = SDL_GetTicks(); }
                    else if (k == SDLK_DOWN) { moveCursor(&st, 1, 0); heldDir = 1; lastRepeat = SDL_GetTicks(); }
                    else if (k == SDLK_LEFT) { moveCursor(&st, 0, -1); heldDir = 2; lastRepeat = SDL_GetTicks(); }
                    else if (k == SDLK_RIGHT) { moveCursor(&st, 0, 1); heldDir = 3; lastRepeat = SDL_GetTicks(); }
                    else if (k == SDLK_z || k == SDLK_RETURN) pressButton(&st, getButton(st.mode, st.cursorRow, st.cursorCol));
                    else if (k == SDLK_x || k == SDLK_BACKSPACE) doBackspace(&st);
                    else if (k == SDLK_c) doClear(&st);
                    else if (k == SDLK_v) doParens(&st);
                    else if (k == SDLK_a || k == SDLK_s || k == SDLK_TAB) toggleMode(&st);
                    break;
                }
                case SDL_KEYUP: {
                    SDL_Keycode k = e.key.keysym.sym;
                    if ((k == SDLK_UP && heldDir == 0) || (k == SDLK_DOWN && heldDir == 1) ||
                        (k == SDLK_LEFT && heldDir == 2) || (k == SDLK_RIGHT && heldDir == 3)) {
                        heldDir = -1;
                    }
                    break;
                }
            }
        }

        if (heldDir != -1) {
            Uint32 now = SDL_GetTicks();
            if (now - lastRepeat > REPEAT_DELAY) {
                Uint32 sinceDelay = now - lastRepeat - REPEAT_DELAY;
                if (sinceDelay > REPEAT_RATE) {
                    if (heldDir == 0) moveCursor(&st, -1, 0);
                    else if (heldDir == 1) moveCursor(&st, 1, 0);
                    else if (heldDir == 2) moveCursor(&st, 0, -1);
                    else if (heldDir == 3) moveCursor(&st, 0, 1);
                    lastRepeat = now - REPEAT_DELAY;
                }
            }
        }

        render(ren, fontBig, fontMid, fontSmall, &st);
    }

    TTF_CloseFont(fontBig);
    TTF_CloseFont(fontMid);
    TTF_CloseFont(fontSmall);
    TTF_Quit();
    SDL_DestroyRenderer(ren);
    SDL_DestroyWindow(win);
    SDL_Quit();
    return 0;
}
