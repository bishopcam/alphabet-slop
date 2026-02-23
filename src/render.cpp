#include "render.h"
#include <cmath>
#include <cstdio>
#include <cstring>
#include <algorithm>

static const int   SCREEN_W    = 1000;
static const int   SCREEN_H    = 700;
static const float FONT_SIZE   = 30.0f;
static const float LINE_SPACING = 44.0f;
static const float SPACING     = 1.0f;
static const int   TEXT_AREA_X = 60;
static const int   TEXT_AREA_W = SCREEN_W - 120;

// Per-word can animation timing
static const float WORD_STAGGER = 0.030f;  // delay between letters within a word
static const float FLIGHT_DUR   = 0.45f;   // flight time per letter
static const float CASH_PAUSE   = 1.20f;   // cash popup display time before cashout
static const float LINE_GROUP_STAGGER = 0.085f;

static const Color BG_COLOR      = {30, 30, 40, 255};
static const Color UNTYPED_COLOR = {180, 180, 190, 255};
static const Color CORRECT_COLOR = {80, 220, 100, 255};
static const Color WRONG_COLOR   = {220, 60, 60, 255};
static const Color CURSOR_COLOR  = {255, 220, 80, 255};
static const Color GHOST_COLOR   = {130, 140, 220, 200};
static const Color UI_COLOR      = {200, 200, 210, 255};
static const Color ACCENT_COLOR  = {255, 220, 80, 255};
static const Color DIM_COLOR     = {120, 120, 130, 255};

static const int COMBO_TIER_COUNT = 7;
static const int COMBO_THRESHOLDS[COMBO_TIER_COUNT] = {0, 2, 4, 7, 11, 16, 23};
static const Color COMBO_COLORS[COMBO_TIER_COUNT] = {
    {255, 84, 84, 255},   // red
    {255, 144, 72, 255},  // orange
    {255, 216, 76, 255},  // yellow
    {90, 220, 110, 255},  // green
    {72, 214, 220, 255},  // cyan
    {92, 148, 255, 255},  // blue
    {188, 112, 255, 255}, // violet
};

static float clamp01(float v) {
    if (v < 0.0f) return 0.0f;
    if (v > 1.0f) return 1.0f;
    return v;
}

static Color mixColor(Color a, Color b, float t) {
    t = clamp01(t);
    return {
        (unsigned char)((float)a.r + ((float)b.r - (float)a.r) * t),
        (unsigned char)((float)a.g + ((float)b.g - (float)a.g) * t),
        (unsigned char)((float)a.b + ((float)b.b - (float)a.b) * t),
        (unsigned char)((float)a.a + ((float)b.a - (float)a.a) * t),
    };
}

static int comboTierForStreak(int streak) {
    int tier = 0;
    for (int i = 1; i < COMBO_TIER_COUNT; i++) {
        if (streak >= COMBO_THRESHOLDS[i]) tier = i;
        else break;
    }
    return tier;
}

static float comboTierFill(int streak) {
    int tier = comboTierForStreak(streak);
    if (tier >= COMBO_TIER_COUNT - 1) return 1.0f;
    float lo = (float)COMBO_THRESHOLDS[tier];
    float hi = (float)COMBO_THRESHOLDS[tier + 1];
    float t  = (streak - lo) / std::max(1.0f, hi - lo);
    float segment = ((float)tier + clamp01(t)) / (float)(COMBO_TIER_COUNT - 1);
    return clamp01(segment);
}

static Color comboColorForStreak(int streak) {
    int tier = comboTierForStreak(streak);
    if (tier >= COMBO_TIER_COUNT - 1) return COMBO_COLORS[COMBO_TIER_COUNT - 1];
    float lo = (float)COMBO_THRESHOLDS[tier];
    float hi = (float)COMBO_THRESHOLDS[tier + 1];
    float t  = (streak - lo) / std::max(1.0f, hi - lo);
    return mixColor(COMBO_COLORS[tier], COMBO_COLORS[tier + 1], t);
}

static void drawComboHud(Font font, float x, float y, float w,
                         int comboStreakWords, int bestComboStreakWords,
                         float comboPulse, float comboBreakFlash,
                         float avgPressure, float avgPressurePulse,
                         bool hasRunningAverage)
{
    float h = hasRunningAverage ? 120.0f : 84.0f;
    DrawRectangleRounded({x, y, w, h}, 0.18f, 8, {16, 16, 26, 206});
    DrawRectangleRoundedLinesEx({x, y, w, h}, 0.18f, 8, 1.2f, {68, 68, 94, 190});

    Color comboColor = comboColorForStreak(comboStreakWords);
    float comboFill  = comboTierFill(comboStreakWords);
    float barX = x + 12.0f;
    float barY = y + 33.0f;
    float barW = w - 24.0f;
    float barH = 16.0f;

    DrawTextEx(font, "Rainbow Combo", {barX, y + 10.0f}, 15.0f, SPACING, {180, 180, 210, 210});

    char comboBuf[64];
    std::snprintf(comboBuf, sizeof(comboBuf), "x%d  best x%d", comboStreakWords, bestComboStreakWords);
    Vector2 comboSz = MeasureTextEx(font, comboBuf, 14.0f, SPACING);
    DrawTextEx(font, comboBuf, {x + w - comboSz.x - 12.0f, y + 10.0f}, 14.0f, SPACING, comboColor);

    DrawRectangleRounded({barX, barY, barW, barH}, 0.45f, 8, {22, 22, 34, 255});
    for (int i = 0; i < COMBO_TIER_COUNT - 1; i++) {
        float sx = barX + (float)i / (float)(COMBO_TIER_COUNT - 1) * barW;
        float sw = barW / (float)(COMBO_TIER_COUNT - 1);
        Color seg = COMBO_COLORS[i];
        seg.a = 95;
        DrawRectangleGradientH((int)sx, (int)barY, (int)sw + 1, (int)barH, seg, COMBO_COLORS[i + 1]);
    }
    DrawRectangleRounded({barX, barY, barW * comboFill, barH}, 0.45f, 8, {comboColor.r, comboColor.g, comboColor.b, 220});
    DrawRectangleRoundedLinesEx({barX, barY, barW, barH}, 0.45f, 8, 1.0f, {78, 78, 108, 220});

    if (comboPulse > 0.0f) {
        float a = comboPulse * comboPulse;
        DrawRectangleRounded({barX - 1.0f, barY - 1.0f, barW * comboFill + 2.0f, barH + 2.0f},
                             0.45f, 8, {comboColor.r, comboColor.g, comboColor.b, (unsigned char)(a * 170.0f)});
    }
    if (comboBreakFlash > 0.0f) {
        DrawRectangleRounded({barX - 1.0f, barY - 1.0f, barW + 2.0f, barH + 2.0f},
                             0.45f, 8, {255, 90, 90, (unsigned char)(comboBreakFlash * 120.0f)});
    }

    if (!hasRunningAverage) return;

    float pX = barX;
    float pY = y + 72.0f;
    float pW = barW;
    float pH = 14.0f;

    DrawTextEx(font, "Avg Pressure", {pX, pY - 18.0f}, 14.0f, SPACING, {160, 170, 200, 210});
    Color cool = {70, 130, 255, 205};
    Color hot  = {255, 110, 60, 230};
    Color pCol = mixColor(cool, hot, avgPressure);
    DrawRectangleRounded({pX, pY, pW, pH}, 0.45f, 8, {22, 22, 34, 255});
    DrawRectangleRounded({pX, pY, pW * clamp01(avgPressure), pH}, 0.45f, 8, pCol);
    DrawRectangleRoundedLinesEx({pX, pY, pW, pH}, 0.45f, 8, 1.0f, {72, 78, 110, 220});

    if (avgPressurePulse > 0.0f) {
        DrawRectangleRounded({pX - 2.0f, pY - 2.0f, pW + 4.0f, pH + 4.0f},
                             0.45f, 8, {pCol.r, pCol.g, pCol.b, (unsigned char)(avgPressurePulse * 120.0f)});
    }

    const char* mood = (avgPressure >= 0.92f) ? "HOT" :
                       (avgPressure >= 0.65f) ? "PUSHING" : "WARMUP";
    Vector2 moodSz = MeasureTextEx(font, mood, 13.0f, SPACING);
    DrawTextEx(font, mood, {x + w - moodSz.x - 12.0f, pY - 18.0f}, 13.0f, SPACING, pCol);
}

static void drawCentered(Font font, const char* text, float y, float size, Color color) {
    Vector2 sz = MeasureTextEx(font, text, size, SPACING);
    DrawTextEx(font, text, {(SCREEN_W - sz.x) / 2.0f, y}, size, SPACING, color);
}

void RenderState::init() {
    font = GetFontDefault();
    customFont = false;

    const char* paths[] = {
        "/usr/share/fonts/truetype/ubuntu/UbuntuMono-R.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        nullptr
    };
    for (int i = 0; paths[i]; i++) {
        if (FileExists(paths[i])) {
            font = LoadFontEx(paths[i], 64, nullptr, 0);
            SetTextureFilter(font.texture, TEXTURE_FILTER_BILINEAR);
            customFont = true;
            break;
        }
    }
}

void RenderState::unload() {
    if (customFont) UnloadFont(font);
}

void RenderState::updateCursor(float dt) {
    cursorBlink += dt;
    float lerp = dt * 7.0f;
    if (lerp > 1.0f) lerp = 1.0f;
    cursorX += (targetCursorX - cursorX) * lerp;
    cursorY += (targetCursorY - cursorY) * lerp;
}

// ---- Menu soup animation (top-down bowl view) ----

static float rf(float lo, float hi) {
    return lo + (float)(std::rand() % 10000) / 10000.0f * (hi - lo);
}

// Parametric style descriptor for soup letters
struct LetterStyle {
    float fontSize      = 100.0f;
    float orbitRxMin    =  3.0f, orbitRxMax    = 12.0f;
    float orbitRyMin    =  2.0f, orbitRyMax    =  8.0f;
    float orbitSpMin    =  0.05f, orbitSpMax   =  0.20f;
    float rotMin        = -14.0f, rotMax       =  14.0f;
    float rotSpMin      = -1.3f,  rotSpMax     =   1.3f;
    float flipChance    =  0.25f;   // probability of starting upside-down
    float introDelayMax =  3.0f;
    unsigned char baseAlpha = 255;
};

static const LetterStyle TITLE_STYLE; // uses defaults above

static const LetterStyle OPTION_STYLE = {
    36.0f,             // fontSize
    0.4f,  2.5f,       // orbitRx (subtle)
    0.3f,  1.5f,       // orbitRy
    0.03f, 0.09f,      // orbitSpeed (gentle)
    -4.0f, 4.0f,       // rot (slight tilt, no chaos)
    -0.20f, 0.20f,     // rotSpeed
    0.0f,              // flipChance (never backwards)
    2.2f,              // introDelayMax
    215                // baseAlpha
};

// Build one SoupLetter at (baseX, baseY) using the given style.
static SoupLetter makeSoupLetter(char ch, float baseX, float baseY,
                                  const LetterStyle& style, int optIdx = -1, int cIdx = 0)
{
    SoupLetter l = {};
    l.ch          = ch;
    l.baseX       = baseX;
    l.baseY       = baseY;
    l.orbitAngle  = rf(0.0f, 6.283f);
    l.orbitSpeed  = rf(style.orbitSpMin, style.orbitSpMax)
                    * (std::rand() & 1 ? 1.0f : -1.0f);
    l.orbitRx     = rf(style.orbitRxMin, style.orbitRxMax);
    l.orbitRy     = rf(style.orbitRyMin, style.orbitRyMax);
    bool flipped  = (style.flipChance > 0.0f &&
                     rf(0.0f, 1.0f) < style.flipChance);
    l.rot         = flipped ? 180.0f + rf(-8.0f, 8.0f)
                            : rf(style.rotMin, style.rotMax);
    l.rotSpeed    = rf(style.rotSpMin, style.rotSpMax);
    l.size        = style.fontSize;
    l.baseAlpha   = style.baseAlpha;
    l.introDelay  = rf(0.0f, style.introDelayMax);
    l.optionIdx   = optIdx;
    l.charIdx     = cIdx;
    return l;
}

// Add a centered line of soup letters into `out`.
static void addLetterLine(Font font, std::vector<SoupLetter>& out,
                           const char* text, float cy,
                           const LetterStyle& style,
                           int optIdx = -1)
{
    float fsz = style.fontSize;
    float totalW = 0.0f;
    for (const char* p = text; *p; p++) {
        char s[2] = {*p, '\0'};
        totalW += MeasureTextEx(font, s, fsz, SPACING).x + SPACING;
    }
    float x = 500.0f - totalW * 0.5f;
    int ci = 0;
    for (const char* p = text; *p; p++, ci++) {
        char s[2] = {*p, '\0'};
        float cw = MeasureTextEx(font, s, fsz, SPACING).x + SPACING;
        out.push_back(makeSoupLetter(*p, x + cw * 0.5f, cy, style, optIdx, ci));
        x += cw;
    }
}

// Draw one SoupLetter with a given base color (handles intro + orbit + shadow).
static void drawSoupLetter(Font font, const SoupLetter& l,
                            float shakeX, float shakeY, Color baseColor)
{
    bool started = l.introComplete || l.introProgress > 0.0f;
    if (!started) return;

    float lx   = l.baseX + std::sin(l.orbitAngle) * l.orbitRx + shakeX;
    float ly   = l.baseY + std::cos(l.orbitAngle) * l.orbitRy + shakeY;
    float lsz  = l.size;
    float alpF = (float)baseColor.a;
    float flash = 0.0f;

    if (!l.introComplete) {
        float p = l.introProgress;
        if (p < 0.42f) return; // hidden below surface
        if (p < 0.72f) {
            float t    = (p - 0.42f) / 0.30f;
            float ease = 1.0f - (1.0f-t)*(1.0f-t)*(1.0f-t);
            lsz   = l.size * ease;
            alpF  = ease * (float)baseColor.a;
            flash = std::sin(t * 3.14159f) * 0.7f;
        }
    }
    if (alpF < 4.0f) return;

    char str[2] = {l.ch, '\0'};
    Color col = {
        (unsigned char)std::min(255, (int)(baseColor.r + flash * 38)),
        (unsigned char)std::min(255, (int)(baseColor.g + flash * 20)),
        baseColor.b,
        (unsigned char)alpF
    };
    Vector2 sz     = MeasureTextEx(font, str, lsz, SPACING);
    Vector2 origin = {sz.x * 0.5f, sz.y * 0.5f};
    Color   sc     = {10, 5, 2, (unsigned char)(alpF * 0.35f)};
    DrawTextPro(font, str, {lx + 3, ly + 3}, origin, l.rot, lsz, SPACING, sc);
    DrawTextPro(font, str, {lx,     ly    }, origin, l.rot, lsz, SPACING, col);
}

void RenderState::initMenu() {
    titleLetters.clear();
    optionLetters.clear();
    vegPieces.clear();
    ripples.assign(6, Ripple{});
    menuTime    = 0.0f;
    rippleTimer = rf(0.2f, 0.7f);
    menuInputLen = 0;
    menuInput[0] = '\0';
    menuShake    = 0.0f;
    quitRequested = false;

    // Title letters — bigger (100 px), 25% upside-down, staggered intro
    addLetterLine(font, titleLetters, "ALPHABET", 210.0f, TITLE_STYLE);
    addLetterLine(font, titleLetters, "SOUP",     320.0f, TITLE_STYLE);

    // Option letters — gentle float, never flipped, smaller, staggered intro
    static const char* OPT_LABELS[] = {"START", "SETTINGS", "QUIT", nullptr};
    // Lay the three words out with fixed gaps, centered on screen
    const float OPT_Y   = 608.0f;
    const float OPT_GAP = 90.0f; // gap between words
    // Measure each word
    float wordWidths[3] = {};
    for (int i = 0; i < 3; i++) {
        for (const char* p = OPT_LABELS[i]; *p; p++) {
            char s[2] = {*p, '\0'};
            wordWidths[i] += MeasureTextEx(font, s, OPTION_STYLE.fontSize, SPACING).x + SPACING;
        }
    }
    float totalW = wordWidths[0] + wordWidths[1] + wordWidths[2] + OPT_GAP * 2;
    float startX = 500.0f - totalW * 0.5f;
    for (int i = 0; i < 3; i++) {
        // Build individual letters at centered positions
        float x  = startX;
        int  ci  = 0;
        for (const char* p = OPT_LABELS[i]; *p; p++, ci++) {
            char s[2] = {*p, '\0'};
            float cw = MeasureTextEx(font, s, OPTION_STYLE.fontSize, SPACING).x + SPACING;
            optionLetters.push_back(
                makeSoupLetter(*p, x + cw * 0.5f, OPT_Y, OPTION_STYLE, i, ci));
            x += cw;
        }
        startX += wordWidths[i] + OPT_GAP;
    }

    // Vegetables and chicken: rejection-sampled so pieces don't bunch up.
    // They can appear anywhere across the full bowl including edges.
    static const int VEG_COUNT  = 28;
    const float VEG_MIN_DIST_SQ = 72.0f * 72.0f;
    for (int i = 0; i < VEG_COUNT; i++) {
        VegPiece v;
        v.type = std::rand() % 4;
        // Try up to 30 candidate positions; keep first that clears min-distance
        float px = rf(22, 978), py = rf(22, 678);
        for (int attempt = 0; attempt < 30; attempt++) {
            float tx = rf(22, 978), ty = rf(22, 678);
            bool ok = true;
            for (const auto& e : vegPieces) {
                float dx = e.x - tx, dy = e.y - ty;
                if (dx*dx + dy*dy < VEG_MIN_DIST_SQ) { ok = false; break; }
            }
            if (ok) { px = tx; py = ty; break; }
        }
        v.x          = px;
        v.y          = py;
        v.orbitAngle = rf(0, 6.283f);
        v.orbitSpeed = rf(0.07f, 0.28f) * (std::rand() & 1 ? 1.0f : -1.0f);
        v.orbitRx    = rf(3, 16);
        v.orbitRy    = rf(2, 11);
        v.rot        = rf(0, 360);
        v.rotSpeed   = rf(-18.0f, 18.0f);
        v.szMul      = rf(0.85f, 1.45f);
        v.alpha      = (unsigned char)rf(160, 225);
        vegPieces.push_back(v);
    }
}

void RenderState::resetMenu() {
    menuInputLen = 0;
    menuInput[0] = '\0';
    menuShake    = 0.0f;
}

void RenderState::updateMenu(float dt) {
    menuTime += dt;
    if (menuShake > 0.0f) { menuShake -= dt * 10.0f; if (menuShake < 0.0f) menuShake = 0.0f; }

    // Title letters — orbit + intro pop-from-below
    for (auto& l : titleLetters) {
        l.orbitAngle += l.orbitSpeed * dt;
        l.rot        += l.rotSpeed   * dt;
        if (!l.introComplete) {
            if (l.introDelay > 0.0f) {
                l.introDelay -= dt;
            } else {
                l.introProgress += dt / 1.5f;
                if (l.introProgress >= 1.0f) {
                    l.introProgress = 1.0f;
                    l.introComplete = true;
                }
            }
        }
    }

    // Option letters — same intro/orbit logic, gentler params
    for (auto& l : optionLetters) {
        l.orbitAngle += l.orbitSpeed * dt;
        l.rot        += l.rotSpeed   * dt;
        if (!l.introComplete) {
            if (l.introDelay > 0.0f) {
                l.introDelay -= dt;
            } else {
                l.introProgress += dt / 1.5f;
                if (l.introProgress >= 1.0f) {
                    l.introProgress = 1.0f;
                    l.introComplete = true;
                }
            }
        }
    }

    // Veg / chicken pieces — orbit + slow spin
    for (auto& v : vegPieces) {
        v.orbitAngle += v.orbitSpeed * dt;
        v.rot        += v.rotSpeed   * dt;
    }

    // Ripples
    rippleTimer -= dt;
    if (rippleTimer <= 0.0f) {
        for (auto& r : ripples) {
            if (!r.active) {
                r.x        = rf(60, 940);
                r.y        = rf(60, 640);
                r.progress = 0.0f;
                r.speed    = rf(0.25f, 0.45f);
                r.active   = true;
                break;
            }
        }
        rippleTimer = rf(0.25f, 0.75f);
    }
    for (auto& r : ripples) {
        if (r.active) {
            r.progress += dt * r.speed;
            if (r.progress >= 1.0f) r.active = false;
        }
    }
}

// Draw one vegetable / chicken piece
static void drawVegPiece(const VegPiece& v, float sx, float sy) {
    float x = v.x + std::sin(v.orbitAngle) * v.orbitRx + sx;
    float y = v.y + std::cos(v.orbitAngle) * v.orbitRy + sy;
    float m = v.szMul;
    unsigned char a = v.alpha;
    switch (v.type) {
        case 0: { // Pea — bright-green circle with sheen dot
            int r1 = (int)(10.0f * m), r2 = (int)(4.0f * m);
            DrawCircle((int)x, (int)y, r1, {82, 158, 60, a});
            DrawCircle((int)x - (int)(2*m), (int)y - (int)(2*m), r2,
                       {148, 215, 105, (unsigned char)(a * 0.75f)});
            break;
        }
        case 1: { // Carrot slice — orange circle with inner rings
            int r1 = (int)(14.0f * m), r2 = (int)(8.0f * m), r3 = (int)(3.0f * m);
            DrawCircle((int)x, (int)y, r1, {208, 102, 28, a});
            DrawCircle((int)x, (int)y, r2, {235, 140, 55, (unsigned char)(a * 0.55f)});
            DrawCircle((int)x, (int)y, r3, {248, 182, 82, (unsigned char)(a * 0.85f)});
            break;
        }
        case 2: { // Celery — pale-green elongated strip, rotated
            float w = 28.0f * m, h = 11.0f * m;
            DrawRectanglePro({x, y, w, h}, {w*0.5f, h*0.5f}, v.rot, {138, 192, 118, a});
            float w2 = 20.0f * m, h2 = 5.0f * m;
            DrawRectanglePro({x, y, w2, h2}, {w2*0.5f, h2*0.5f}, v.rot,
                             {172, 220, 148, (unsigned char)(a * 0.5f)});
            break;
        }
        case 3: { // Chicken cube — tan/cream square, slightly irregular
            float w = 17.0f * m, h = 15.0f * m;
            DrawRectanglePro({x, y, w, h}, {w*0.5f, h*0.5f}, v.rot, {212, 188, 148, a});
            float w2 = 11.0f * m, h2 = 9.0f * m;
            DrawRectanglePro({x, y, w2, h2}, {w2*0.5f, h2*0.5f}, v.rot,
                             {238, 218, 182, (unsigned char)(a * 0.6f)});
            break;
        }
    }
}

// Shared word-wrap layout: fills charWidth[] and charPos[], returns textEnd position.
static void computeCharLayout(const TypingState& ts, Font font,
                               std::vector<float>& charWidth,
                               std::vector<Vector2>& charPos,
                               float& textEndX, float& textEndY)
{
    int n = (int)ts.chars.size();
    charWidth.assign(n, 0.0f);
    charPos.assign(n, {0.0f, 0.0f});

    for (int i = 0; i < n; i++) {
        char str[2] = {ts.chars[i].ch, '\0'};
        Vector2 sz = MeasureTextEx(font, str, FONT_SIZE, SPACING);
        charWidth[i] = (sz.x > 4.0f ? sz.x : FONT_SIZE * 0.45f) + SPACING;
    }

    float x = (float)TEXT_AREA_X;
    float y = 110.0f;
    textEndX = x; textEndY = y;

    int i = 0;
    while (i < n) {
        if (ts.chars[i].ch == ' ') {
            charPos[i] = {x, y};
            x += charWidth[i];
            i++;
        } else {
            int wordEnd = i;
            float wordW = 0.0f;
            while (wordEnd < n && ts.chars[wordEnd].ch != ' ') {
                wordW += charWidth[wordEnd];
                wordEnd++;
            }
            if (x > TEXT_AREA_X && x + wordW > TEXT_AREA_X + TEXT_AREA_W) {
                x = (float)TEXT_AREA_X;
                y += LINE_SPACING;
            }
            for (int k = i; k < wordEnd; k++) {
                charPos[k] = {x, y};
                x += charWidth[k];
            }
            i = wordEnd;
        }
    }
    textEndX = x;
    textEndY = y;
}

void RenderState::drawMenu() {
    float sx = 0, sy = 0;
    if (menuShake > 0.0f) {
        sx = rf(-4, 4) * menuShake;
        sy = rf(-2, 2) * menuShake;
    }

    // ---- Soup surface (top-down bowl) ----
    ClearBackground({42, 24, 12, 255});
    DrawCircle(500, 350, 520, {52, 30, 14, 255});
    DrawCircle(500, 350, 380, {62, 36, 17, 255});
    DrawCircle(500, 350, 220, {70, 41, 19, 255});

    // Circular ripples
    for (const auto& r : ripples) {
        if (!r.active) continue;
        float t   = r.progress;
        float rad = t * 75.0f;
        float a   = std::sin(t * 3.14159f) * 55.0f;
        Color rc  = {105, 72, 42, (unsigned char)a};
        DrawCircleLines((int)(r.x + sx), (int)(r.y + sy), rad, rc);
        if (rad > 18.0f)
            DrawCircleLines((int)(r.x + sx), (int)(r.y + sy), rad * 0.55f,
                            {105, 72, 42, (unsigned char)(a * 0.45f)});
    }

    // ---- Vegetables & chicken pieces ----
    for (const auto& v : vegPieces)
        drawVegPiece(v, sx, sy);

    // ---- Title letters: pop-from-below intro then normal orbit ----
    static const Color CREAM = {222, 210, 186, 255};
    for (const auto& l : titleLetters)
        drawSoupLetter(font, l, sx, sy, CREAM);

    // ---- Gradient overlay at bottom for option legibility ----
    DrawRectangleGradientV(0, 480, SCREEN_W, 220, {42, 24, 12, 0}, {20, 10, 5, 210});

    // "type to navigate" hint
    drawCentered(font, "type to navigate", 566, 17, {130, 115, 95, 180});

    // ---- Option letters floating with match-state coloring ----
    static const char* OPT_WORDS_LOWER[] = {"start", "settings", "quit", nullptr};
    for (const auto& l : optionLetters) {
        int optIdx  = l.optionIdx;
        int ci      = l.charIdx;
        const char* word    = OPT_WORDS_LOWER[optIdx];
        int         wordLen = (int)std::strlen(word);

        bool isMatch = (menuInputLen == 0) ||
                       (menuInputLen <= wordLen &&
                        std::strncmp(menuInput, word, menuInputLen) == 0);

        Color col;
        if (!isMatch) {
            col = {65, 55, 42, 100};
        } else if (menuInputLen > 0 && ci < menuInputLen) {
            col = CORRECT_COLOR;
        } else if (menuInputLen > 0 && ci == menuInputLen) {
            float pulse = (std::sin(menuTime * 5.5f) + 1.0f) * 0.5f;
            col = {255, 220, 80, (unsigned char)(140 + (int)(pulse * 115.0f))};
        } else {
            col = {175, 160, 135, 200};
        }

        drawSoupLetter(font, l, sx, sy, col);
    }
}

static void drawBowl(float cx, float topY, float halfTopW, float bowlH); // defined below
static void drawTachometer(Font font, float cx, float cy, float outerR, float innerR,
                            float tachoWPM, float avgWPM);              // defined below

void RenderState::resetTypingCan(bool newParagraph) {
    flyingLetters.clear();
    wordPopups.clear();
    sentCharIdx    = 0;
    canFinishTimer = -1.0f;
    readyToCashout = false;
    canBounceTimer = 0.0f;
    canBounceColor = {255, 220, 80, 255};
    canBounceStrength = 1.0f;
    chartProgress  = 0.0f;
    totalWordCash  = 0;
    if (newParagraph) {
        wordCashHistory.clear();
        tachoWPM = 0.0f;
        comboStreakWords = 0;
        bestComboStreakWords = 0;
        comboPulse = 0.0f;
        comboBreakFlash = 0.0f;
        avgPressure = 0.0f;
        avgPressurePulse = 0.0f;
        comboBonusCash = 0;
        pressureBonusCash = 0;
    }
}

void RenderState::drawTyping(GameState& game, float dt) {
    TypingState& ts = game.typing;

    float shakeX = 0, shakeY = 0;
    if (game.screenShake > 0.0f) {
        shakeX = (float)(std::rand() % 5 - 2) * game.screenShake;
        shakeY = (float)(std::rand() % 5 - 2) * game.screenShake;
    }

    ClearBackground(BG_COLOR);

    // Smooth tachometer WPM
    float rawWPM = ts.getWPM();
    tachoWPM += (rawWPM - tachoWPM) * std::min(1.0f, dt * 2.5f);

    bool hasRunningAverage = game.runningAvgWPM > 0.0f;
    float speedRatio = hasRunningAverage ? rawWPM / std::max(1.0f, game.runningAvgWPM) : 1.0f;
    if (hasRunningAverage && ts.elapsedTime > 0.15f) {
        float prevPressure = avgPressure;
        if (speedRatio >= 1.0f)
            avgPressure += dt * (0.22f + (speedRatio - 1.0f) * 0.85f);
        else
            avgPressure -= dt * (0.35f + (1.0f - speedRatio) * 0.90f);
        avgPressure = clamp01(avgPressure);
        if (prevPressure < 0.95f && avgPressure >= 0.95f) avgPressurePulse = 1.0f;
    } else {
        avgPressure = std::max(0.0f, avgPressure - dt * 0.25f);
    }
    if (comboPulse > 0.0f) comboPulse = std::max(0.0f, comboPulse - dt * 2.6f);
    if (comboBreakFlash > 0.0f) comboBreakFlash = std::max(0.0f, comboBreakFlash - dt * 2.2f);
    if (avgPressurePulse > 0.0f) avgPressurePulse = std::max(0.0f, avgPressurePulse - dt * 2.8f);

    // UI bar (no WPM text — tachometer is the focal WPM display)
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Accuracy: %.1f%%", ts.getAccuracy());
    DrawTextEx(font, buf, {(float)(TEXT_AREA_X) + shakeX, 20.0f + shakeY}, 22.0f, SPACING, UI_COLOR);

    if (hasRunningAverage) {
        std::snprintf(buf, sizeof(buf), "Avg: %.0f WPM", game.runningAvgWPM);
        DrawTextEx(font, buf, {(float)(TEXT_AREA_X + 500) + shakeX, 20.0f + shakeY}, 22.0f, SPACING, GHOST_COLOR);
    }

    std::snprintf(buf, sizeof(buf), "Cash: $%d", game.totalCash);
    Vector2 cashSz = MeasureTextEx(font, buf, 22.0f, SPACING);
    DrawTextEx(font, buf, {SCREEN_W - cashSz.x - 20.0f + shakeX, 20.0f + shakeY}, 22.0f, SPACING, ACCENT_COLOR);

    int n = (int)ts.chars.size();

    // Ghost cursor: fractional character position at average WPM
    float ghostCharPos = -1.0f;
    if (hasRunningAverage && ts.elapsedTime > 0.0f) {
        ghostCharPos = ts.elapsedTime * game.runningAvgWPM * 5.0f / 60.0f;
        if (ghostCharPos > (float)n - 0.001f) ghostCharPos = (float)n - 0.001f;
    }

    // --- Passes 1 & 2: word-wrap layout ---
    std::vector<float> charWidth;
    std::vector<Vector2> charPos;
    float textEndX, textEndY;
    computeCharLayout(ts, font, charWidth, charPos, textEndX, textEndY);

    // Per-character line index from wrapped layout
    std::vector<int> charLine(n, 0);
    int maxLine = 0;
    if (n > 0) {
        int line = 0;
        float prevY = charPos[0].y;
        for (int i = 0; i < n; i++) {
            if (i > 0 && std::fabs(charPos[i].y - prevY) > 1.0f) line++;
            charLine[i] = line;
            prevY = charPos[i].y;
        }
        maxLine = line;
    }

    int cursorLine = 0;
    if (n > 0) {
        if (ts.cursor < n) cursorLine = charLine[std::max(0, ts.cursor)];
        else cursorLine = maxLine;
    }
    int flushBeforeLine = ts.finished ? (maxLine + 1) : cursorLine;

    // --- Can animation constants ---
    const float CAN_CX   = 500.0f;
    const float CAN_TOP  = 552.0f;

    // --- Update flying letters ---
    // 1. Advance elapsed
    for (auto& fl : flyingLetters) fl.elapsed += dt;
    // 2. Remove landed letters
    flyingLetters.erase(
        std::remove_if(flyingLetters.begin(), flyingLetters.end(),
            [](const FlyingLetter& fl){ return fl.elapsed >= fl.launchDelay + FLIGHT_DUR; }),
        flyingLetters.end());

    // 3. Launch completed words in line groups:
    // words stay in text until you advance to the next wrapped line.
    int activeLaunchLine = -1;
    int lineWordIdx = 0;
    while (sentCharIdx < n) {
        // Skip spaces silently (they don't fly)
        if (ts.chars[sentCharIdx].ch == ' ') { sentCharIdx++; continue; }

        // Find end of current word
        int wordStart = sentCharIdx;
        int wordEnd   = sentCharIdx;
        while (wordEnd < n && ts.chars[wordEnd].ch != ' ') wordEnd++;

        // Word is done when cursor has passed the trailing space (or end of text)
        int triggerAt = (wordEnd < n) ? wordEnd + 1 : wordEnd;
        if (ts.cursor < triggerAt && !ts.finished) break;

        int wordLine = charLine[wordStart];
        if (wordLine >= flushBeforeLine) break;

        if (wordLine != activeLaunchLine) {
            activeLaunchLine = wordLine;
            lineWordIdx = 0;
        }
        float lineDelay = (float)lineWordIdx * LINE_GROUP_STAGGER;
        lineWordIdx++;

        // Compute per-word rating before launching letters
        bool wordClean = true;
        for (int k = wordStart; k < wordEnd; k++)
            if (ts.chars[k].hadError) { wordClean = false; break; }

        float currentWPM = ts.getWPM();
        float ratio = (game.runningAvgWPM > 0.0f)
                      ? currentWPM / game.runningAvgWPM : 1.2f;

        int rating = 0;
        if (wordClean) {
            if (ratio >= 1.0f)       rating = 3; // PERFECT: clean + on/above avg
            else if (ratio >= 0.75f) rating = 2; // GREAT:   clean + near avg
            else                     rating = 1; // OKAY:    clean but slow
        } else if (ratio >= 0.85f) {
            rating = 1; // OKAY: has errors but roughly on pace
        }

        // Word-combo streak: increments on clean words, resets on imperfect words.
        if (wordClean) {
            comboStreakWords++;
            if (comboStreakWords > bestComboStreakWords)
                bestComboStreakWords = comboStreakWords;
            comboPulse = 1.0f;
        } else {
            if (comboStreakWords > 0) comboBreakFlash = 1.0f;
            comboStreakWords = 0;
        }

        int comboTier = comboTierForStreak(comboStreakWords);
        Color comboCol = comboColorForStreak(comboStreakWords);

        int comboBonus = 0;
        if (rating > 0 && wordClean) {
            if (comboTier >= 2) comboBonus += 1;
            if (comboTier >= 4) comboBonus += 1;
        }

        int pressureBonus = 0;
        if (rating > 0 && hasRunningAverage) {
            if (avgPressure >= 0.70f) pressureBonus += 1;
            if (avgPressure >= 0.93f) pressureBonus += 1;
        }

        int wordCash = 0;
        if (rating > 0) {
            wordCash = std::min(7, rating + comboBonus + pressureBonus);
            wordCashHistory.push_back({ts.elapsedTime, wordCash});
            totalWordCash += wordCash;
            comboBonusCash += comboBonus;
            pressureBonusCash += pressureBonus;
        }

        // Rating color for flying letters
        Color ratingCol;
        if      (rating == 3) ratingCol = {255, 210,  60, 220}; // PERFECT: gold
        else if (rating == 2) ratingCol = { 80, 220, 100, 220}; // GREAT:   green
        else if (rating == 1) ratingCol = {130, 140, 220, 220}; // OKAY:    blue-gray
        else                  ratingCol = {180, 180, 190, 220}; // unrated: dim

        if (wordClean && comboStreakWords >= 2) {
            ratingCol = comboCol;
            ratingCol.a = 230;
        }

        // Per-word bowl impact trigger
        if (rating > 0) {
            float wx = 0.0f;
            for (int k = wordStart; k < wordEnd; k++)
                wx += charPos[k].x + charWidth[k] * 0.5f;
            wx /= (float)(wordEnd - wordStart);

            WordPopup wp;
            wp.rating    = rating;
            wp.elapsed   = 0.0f;
            wp.showAt    = lineDelay + (float)(wordEnd - wordStart - 1) * WORD_STAGGER
                           + FLIGHT_DUR * 0.75f;
            wp.popX      = wx;
            wp.popY      = charPos[wordStart].y;
            wp.triggered = false;
            wp.impact    = 1.0f + 0.08f * (float)wordCash + 0.10f * (float)comboTier;
            wp.flashColor = ratingCol;
            if (pressureBonus > 0)
                wp.flashColor = mixColor(wp.flashColor, {255, 122, 58, 235},
                                         pressureBonus == 2 ? 0.55f : 0.35f);
            wordPopups.push_back(wp);
        }

        // Launch each letter in the word with stagger
        for (int k = wordStart; k < wordEnd; k++) {
            FlyingLetter fl;
            fl.ch          = ts.chars[k].ch;
            fl.startX      = charPos[k].x + charWidth[k] * 0.5f;
            fl.startY      = charPos[k].y + FONT_SIZE * 0.5f;
            fl.launchDelay = lineDelay + (float)(k - wordStart) * WORD_STAGGER;
            fl.elapsed     = 0.0f;
            float vd       = std::max(10.0f, CAN_TOP - fl.startY);
            fl.arcH        = 45.0f + vd * 0.28f + (float)((k * 17 + 13) % 50) + lineDelay * 95.0f;
            fl.spinDir     = (k % 2 == 0) ? 1.0f : -1.0f;
            fl.col         = ratingCol;
            flyingLetters.push_back(fl);
        }
        sentCharIdx = (wordEnd < n) ? wordEnd + 1 : wordEnd;
    }

    // 4. Once all letters sent and landed, start finish timer
    if (ts.finished && sentCharIdx >= n && flyingLetters.empty() && canFinishTimer < 0.0f)
        canFinishTimer = 0.0f;
    if (canFinishTimer >= 0.0f) {
        canFinishTimer += dt;
        if (canFinishTimer >= CASH_PAUSE) readyToCashout = true;
    }

    // 5. Update word popups and bowl bounce timer
    if (canBounceTimer > 0.0f) {
        canBounceTimer -= dt;
        if (canBounceTimer < 0.0f) {
            canBounceTimer = 0.0f;
            canBounceColor = {255, 220, 80, 255};
            canBounceStrength = 1.0f;
        }
    }
    for (auto& wp : wordPopups) {
        wp.elapsed += dt;
        if (!wp.triggered && wp.elapsed >= wp.showAt) {
            wp.triggered    = true;
            canBounceTimer  = 0.42f;
            canBounceStrength = std::max(canBounceStrength, wp.impact);
            canBounceColor = wp.flashColor;
        }
    }
    // Remove expired popups
    wordPopups.erase(
        std::remove_if(wordPopups.begin(), wordPopups.end(),
            [](const WordPopup& wp){ return wp.elapsed >= wp.showAt + 1.4f; }),
        wordPopups.end());

    // --- Pass 3: render characters ---
    for (int i = 0; i < n; i++) {
        if (i < sentCharIdx) continue; // already launched (in-flight or landed)
        const CharInfo& ci = ts.chars[i];
        char str[2] = {ci.ch, '\0'};

        Color color = UNTYPED_COLOR;
        if (ci.state == CharState::Correct) color = CORRECT_COLOR;
        else if (ci.state == CharState::Wrong) color = WRONG_COLOR;

        float drawX = charPos[i].x + shakeX;
        float drawY = charPos[i].y + shakeY;

        // Pop: subtle upward bounce scaled by streak strength
        float offsetY = 0.0f;
        if (ci.popTimer > 0.0f) {
            float t = ci.popTimer / 0.1f;
            offsetY = -3.5f * ci.popStrength * std::sin(t * 3.14159f);
        }

        DrawTextEx(font, str, {drawX, drawY + offsetY}, FONT_SIZE, SPACING, color);
    }

    // Main cursor target (no shake — shake is applied in updateCursor side indirectly via targetCursor)
    if (ts.cursor < n) {
        targetCursorX = charPos[ts.cursor].x + shakeX;
        targetCursorY = charPos[ts.cursor].y + shakeY;
    } else {
        targetCursorX = textEndX + shakeX;
        targetCursorY = textEndY + shakeY;
    }

    // Ghost cursor: interpolate between ghostIdx and ghostIdx+1 on same line
    float ghostBaseX = -1.0f, ghostBaseY = -1.0f;
    if (ghostCharPos >= 0.0f) {
        int ghostIdx = (int)ghostCharPos;
        float ghostFrac = ghostCharPos - (float)ghostIdx;
        if (ghostIdx < n) {
            ghostBaseX = charPos[ghostIdx].x;
            ghostBaseY = charPos[ghostIdx].y;
            if (ghostIdx + 1 < n) {
                float nx = charPos[ghostIdx + 1].x;
                float ny = charPos[ghostIdx + 1].y;
                if (std::fabs(ny - ghostBaseY) < 1.0f) {
                    ghostBaseX += (nx - ghostBaseX) * ghostFrac;
                }
            }
        } else {
            ghostBaseX = textEndX;
            ghostBaseY = textEndY;
        }
    }

    if (ghostBaseX >= 0.0f) {
        float gDrawX = ghostBaseX + shakeX;
        float gDrawY = ghostBaseY + shakeY;
        DrawRectangleRec({gDrawX, gDrawY, 2.0f, FONT_SIZE + 4.0f}, GHOST_COLOR);
        DrawTextEx(font, "avg", {gDrawX - 4.0f, gDrawY - 18.0f}, 14.0f, SPACING, GHOST_COLOR);
    }

    // Draw main cursor
    float blinkAlpha = (std::sin(cursorBlink * 5.0f) + 1.0f) * 0.5f;
    unsigned char cAlpha = (unsigned char)(100 + blinkAlpha * 155);
    DrawRectangleRec({cursorX, cursorY, 2.0f, FONT_SIZE + 4.0f}, {CURSOR_COLOR.r, CURSOR_COLOR.g, CURSOR_COLOR.b, cAlpha});

    // Progress bar + ghost marker
    float progress = n > 0 ? (float)ts.cursor / (float)n : 0.0f;
    DrawRectangle(0, SCREEN_H - 6, (int)(SCREEN_W * progress), 6, ACCENT_COLOR);

    if (ghostCharPos >= 0.0f && n > 0) {
        float gp = std::min(ghostCharPos / (float)n, 1.0f);
        DrawRectangle((int)(SCREEN_W * gp) - 2, SCREEN_H - 10, 4, 10, GHOST_COLOR);
    }

    // --- Flying letters ---
    for (const FlyingLetter& fl : flyingLetters) {
        float t = fl.elapsed - fl.launchDelay;
        if (t <= 0.0f) continue;
        float fp = t / FLIGHT_DUR;
        if (fp > 1.0f) fp = 1.0f;

        float bx = fl.startX + (CAN_CX - fl.startX) * fp;
        float by = fl.startY + (CAN_TOP - fl.startY) * fp
                   - fl.arcH * 4.0f * fp * (1.0f - fp);
        float spin  = fp * 480.0f * fl.spinDir;
        float alpha = (fp > 0.72f) ? 1.0f - (fp - 0.72f) / 0.28f : 1.0f;

        char str[2] = {fl.ch, '\0'};
        Color col = fl.col;
        col.a = (unsigned char)(alpha * (fl.col.a / 220.0f) * 220.0f);
        Vector2 sz     = MeasureTextEx(font, str, FONT_SIZE, SPACING);
        Vector2 origin = {sz.x * 0.5f, sz.y * 0.5f};
        DrawTextPro(font, str, {bx, by}, origin, spin, FONT_SIZE, SPACING, col);
    }

    // --- Tachometer (focal point between text and bowl) ---
    drawTachometer(font, 155.0f, 510.0f, 92.0f, 62.0f, tachoWPM, game.runningAvgWPM);

    // Combo/pace HUD near tachometer
    drawComboHud(font, 248.0f, 466.0f, 250.0f,
                 comboStreakWords, bestComboStreakWords,
                 comboPulse, comboBreakFlash,
                 avgPressure, avgPressurePulse,
                 hasRunningAverage);

    if (comboPulse > 0.02f && comboStreakWords >= 2) {
        Color cc = comboColorForStreak(comboStreakWords);
        char comboPop[48];
        std::snprintf(comboPop, sizeof(comboPop), "combo x%d", comboStreakWords);
        float sz = 20.0f + comboPulse * 5.5f;
        Vector2 psz = MeasureTextEx(font, comboPop, sz, SPACING);
        DrawTextEx(font, comboPop,
                   {CAN_CX - psz.x * 0.5f, CAN_TOP - 64.0f - comboPulse * 8.0f},
                   sz, SPACING, {cc.r, cc.g, cc.b, (unsigned char)(120 + comboPulse * 110.0f)});
    }

    // --- Soup bowl (drawn on top so letters vanish into the soup) ---
    drawBowl(CAN_CX, CAN_TOP, 110.0f, 105.0f);

    // --- Bowl rim bounce glow when a word lands ---
    if (canBounceTimer > 0.0f) {
        float bt     = canBounceTimer / 0.42f;
        float strength = std::max(1.0f, canBounceStrength);
        float expand = (1.0f - bt) * (16.0f + 9.0f * strength);
        unsigned char ba = (unsigned char)(bt * bt * std::min(250.0f, 160.0f + 24.0f * strength));
        Color glow = canBounceColor;
        glow.a = ba;
        DrawEllipse((int)CAN_CX, (int)CAN_TOP,
                    (int)(110.0f * 0.90f + expand), (int)(13 + expand * 0.3f), glow);
        Color inner = glow;
        inner.a = (unsigned char)(ba * 0.55f);
        DrawEllipse((int)CAN_CX, (int)CAN_TOP,
                    (int)(110.0f * 0.78f + expand * 0.72f), (int)(9 + expand * 0.22f), inner);
    }
}

// Front-facing soup bowl with rounded spherical bottom.
// topY is the soup surface (letters land here).
// Uses circle-arc cross-section: R = (W²+D²)/(2D), scanlines taper naturally.
static void drawBowl(float cx, float topY, float halfTopW, float bowlH) {
    // Profile: (1-t)^2.5 power curve — stays wide near rim, tapers quickly toward base.
    // Matches reference image (wide mouth, walls curve in, small foot).
    const float halfBotW = halfTopW * 0.36f;
    const float botY     = topY + bowlH;

    for (int y = (int)topY; y <= (int)botY && y < SCREEN_H; y++) {
        float t  = ((float)y - topY) / bowlH;
        float hw = halfTopW - (halfTopW - halfBotW) * t * t * std::sqrt(t);    // t^2.5 — stays wide, tapers fast near base

        unsigned char rc = (unsigned char)(218 - t * 60);
        unsigned char gc = (unsigned char)(204 - t * 56);
        unsigned char bc = (unsigned char)(178 - t * 48);

        DrawLineEx({cx - hw, (float)y}, {cx + hw, (float)y}, 2.2f, {rc, gc, bc, 255});
        DrawLineEx({cx - hw, (float)y}, {cx - hw + 2.5f, (float)y}, 2.2f,
                   {(unsigned char)std::min(255,(int)(rc+42)), (unsigned char)std::min(255,(int)(gc+36)),
                    (unsigned char)std::min(255,(int)(bc+28)), 255});
        DrawLineEx({cx + hw - 2.5f, (float)y}, {cx + hw, (float)y}, 2.2f,
                   {(unsigned char)(rc*0.56f), (unsigned char)(gc*0.56f), (unsigned char)(bc*0.56f), 255});
    }

    // Small foot / base
    if ((int)botY < SCREEN_H) {
        DrawEllipse((int)cx, (int)botY, (int)halfBotW, 8, {172, 156, 138, 255});
        DrawEllipseLines((int)cx, (int)botY, (int)halfBotW, 8, {128, 114, 96, 215});
    }

    // Soup surface inside the rim opening
    float soupRx = halfTopW * 0.88f;
    DrawEllipse((int)cx, (int)topY, (int)soupRx, 13, {108, 74, 42, 255});
    DrawEllipse((int)(cx - halfTopW * 0.22f), (int)topY,
                (int)(soupRx * 0.32f), 6, {140, 102, 58, 180});

    // Outer rim — thick lip (like reference image)
    DrawEllipse((int)cx, (int)topY, (int)halfTopW, 16, {220, 207, 186, 255});
    DrawEllipseLines((int)cx, (int)topY, (int)halfTopW, 16, {158, 144, 124, 230});
    // Inner rim line — shows bowl wall thickness (second line in reference image)
    DrawEllipseLines((int)cx, (int)(topY + 5), (int)(halfTopW * 0.83f), 10,
                     {145, 132, 112, 190});
}

// Standard RPM-style WPM tachometer.
// Arc: 7-o'clock (120°) → 5-o'clock (420°=60°), sweep 300°.
// Needle catches fire above average WPM and cools as speed drops.
static void drawTachometer(Font font, float cx, float cy,
                            float outerR, float innerR,
                            float tachoWPM, float avgWPM)
{
    float time        = (float)GetTime();
    const float MAX_WPM   = std::max(80.0f, avgWPM > 0.0f ? avgWPM * 1.75f : 100.0f);
    const float ARC_START = 120.0f;
    const float ARC_SWEEP = 300.0f;
    const int   SEGS      = 72;
    const float avgRatio  = (avgWPM > 0.0f) ? std::min(avgWPM / MAX_WPM, 1.0f) : 0.55f;

    // Background track
    DrawRing({cx, cy}, innerR - 5, outerR + 5, ARC_START, ARC_START + ARC_SWEEP,
             SEGS, {16, 16, 26, 245});

    // Major tick marks (6 divisions)
    for (int i = 0; i <= 5; i++) {
        float t         = (float)i / 5.0f;
        float tickAngle = (ARC_START + t * ARC_SWEEP) * DEG2RAD;
        float ti = innerR - 8, to = outerR + 9;
        DrawLineEx({cx + ti * std::cos(tickAngle), cy + ti * std::sin(tickAngle)},
                   {cx + to * std::cos(tickAngle), cy + to * std::sin(tickAngle)},
                   2.5f, {205, 200, 188, 195});

        float wpmTick = t * MAX_WPM;
        char tickBuf[8];
        std::snprintf(tickBuf, sizeof(tickBuf), "%.0f", wpmTick);
        float labelR = outerR + 22;
        Vector2 tsz  = MeasureTextEx(font, tickBuf, 12.0f, 1.0f);
        DrawTextEx(font, tickBuf,
                   {cx + labelR * std::cos(tickAngle) - tsz.x * 0.5f,
                    cy + labelR * std::sin(tickAngle) - tsz.y * 0.5f},
                   12.0f, 1.0f, {175, 170, 158, 170});
    }

    // Minor tick marks
    for (int i = 1; i < 20; i++) {
        if (i % 4 == 0) continue; // skip major positions
        float t         = (float)i / 20.0f;
        float tickAngle = (ARC_START + t * ARC_SWEEP) * DEG2RAD;
        DrawLineEx({cx + (innerR - 3) * std::cos(tickAngle),
                    cy + (innerR - 3) * std::sin(tickAngle)},
                   {cx + (outerR + 3) * std::cos(tickAngle),
                    cy + (outerR + 3) * std::sin(tickAngle)},
                   1.5f, {140, 135, 125, 145});
    }

    // Average marker — prominent yellow tick
    if (avgWPM > 0.0f) {
        float avgAngle = (ARC_START + avgRatio * ARC_SWEEP) * DEG2RAD;
        DrawLineEx({cx + (innerR - 13) * std::cos(avgAngle),
                    cy + (innerR - 13) * std::sin(avgAngle)},
                   {cx + (outerR + 14) * std::cos(avgAngle),
                    cy + (outerR + 14) * std::sin(avgAngle)},
                   4.0f, {255, 220, 80, 230});
    }

    // Fire intensity: ramps up above average WPM, recedes below
    float fireIntensity = 0.0f;
    if (avgWPM > 0.0f && tachoWPM > avgWPM * 0.90f) {
        float startFire = avgWPM * 0.90f;
        float maxFire   = avgWPM + (MAX_WPM - avgWPM) * 0.65f;
        fireIntensity = std::min((tachoWPM - startFire) / std::max(1.0f, maxFire - startFire), 1.0f);
    } else if (avgWPM <= 0.0f && tachoWPM > 20.0f) {
        fireIntensity = std::min((tachoWPM - 20.0f) / 50.0f, 0.75f);
    }

    // Needle
    float needleT     = std::min(tachoWPM, MAX_WPM) / MAX_WPM;
    float needleAngle = (ARC_START + needleT * ARC_SWEEP) * DEG2RAD;
    float nx = cx + (outerR - 5) * std::cos(needleAngle);
    float ny = cy + (outerR - 5) * std::sin(needleAngle);

    // Triangular flame tongues aligned along the needle direction
    if (fireIntensity > 0.01f) {
        float f1 = std::sin(time * 13.0f);
        float f2 = std::sin(time * 21.0f + 0.7f);
        float f3 = std::sin(time * 8.5f  + 1.9f);
        float f4 = std::sin(time * 18.0f + 3.1f);
        float fi = fireIntensity;
        float bh = fi * 55.0f;

        // Needle direction vectors
        float fwd_x  =  std::sin(needleAngle);   // 90° left of needle
        float fwd_y  = -std::cos(needleAngle);
        float perp_x =  std::cos(needleAngle);   // perpendicular to fire direction
        float perp_y =  std::sin(needleAngle);

        // tongue: flames extend in needle direction from tip
        //   po = perp offset of base, bw = base half-width, h = length along needle,
        //   lean = tip perp-lean (flutter), col = color
        auto tongue = [&](float po, float bw, float h, float lean, Color col) {
            DrawTriangle(
                {nx + fwd_x*h  + perp_x*(po+lean), ny + fwd_y*h  + perp_y*(po+lean)}, // tip
                {nx - fwd_x*3  + perp_x*(po-bw),   ny - fwd_y*3  + perp_y*(po-bw)  }, // base L
                {nx - fwd_x*3  + perp_x*(po+bw),   ny - fwd_y*3  + perp_y*(po+bw)  }, // base R
                col);
        };

        // Outer dark-red wisps: long, lean wide
        tongue(-9.0f + f1*3.0f,  11.0f, bh*0.62f, -9.0f + f2*10.0f, {150, 18,  3, (unsigned char)(105.0f*fi)});
        tongue( 9.0f + f2*3.0f,  11.0f, bh*0.58f,  9.0f + f1*10.0f, {150, 18,  3, (unsigned char)(105.0f*fi)});

        // Mid orange
        tongue(-5.0f + f3*3.0f,   8.0f, bh*0.82f, -5.0f + f4*7.0f,  {235, 75,  8, (unsigned char)(155.0f*fi)});
        tongue( 5.0f + f4*3.0f,   8.0f, bh*0.78f,  5.0f + f3*7.0f,  {235, 75,  8, (unsigned char)(155.0f*fi)});

        // Central tall tongue: yellow-orange
        tongue(f1*2.0f,            7.0f, bh*1.00f,  f2*5.0f,         {255, 148, 16, (unsigned char)(188.0f*fi)});

        // Inner yellow
        tongue(-3.0f + f3*1.5f,   5.0f, bh*0.60f, -2.0f + f4*3.0f,  {255, 218, 50, (unsigned char)(212.0f*fi)});
        tongue( 3.0f + f4*1.5f,   5.0f, bh*0.55f,  2.0f + f3*3.0f,  {255, 218, 50, (unsigned char)(212.0f*fi)});

        // White-yellow core
        tongue(f1*0.8f,            4.0f, bh*0.35f,  f2*1.5f,         {255, 252, 150, (unsigned char)(238.0f*fi)});

        // Ball at the needle tip — prominent glowing orb
        DrawCircle((int)nx, (int)ny, (int)(12.0f * fi), {255, 230, 130, (unsigned char)(180.0f*fi)});
        DrawCircle((int)nx, (int)ny, (int)( 7.0f * fi), {255, 255, 210, (unsigned char)(230.0f*fi)});
    }

    // Needle shadow then needle
    DrawLineEx({cx + 2, cy + 2}, {nx + 2, ny + 2}, 3.5f, {0, 0, 0, 65});
    Color needleCol;
    if (fireIntensity > 0.0f) {
        // Color shifts fast: white → orange-red
        float f = std::min(fireIntensity * 1.8f, 1.0f);
        needleCol = {255,
                     (unsigned char)(255 - f * 210),
                     (unsigned char)(255 - f * 255),
                     255};
    } else {
        needleCol = WHITE;
    }
    DrawLineEx({cx, cy}, {nx, ny}, 3.5f, needleCol);

    // Pivot
    DrawCircle((int)cx, (int)cy, 9, {45, 45, 58, 255});
    DrawCircle((int)cx, (int)cy, 6, {220, 55, 40, 255});

    // WPM readout inside
    char wpmBuf[16];
    std::snprintf(wpmBuf, sizeof(wpmBuf), "%.0f", tachoWPM);
    Vector2 wsz = MeasureTextEx(font, wpmBuf, 24.0f, SPACING);
    DrawTextEx(font, wpmBuf, {cx - wsz.x * 0.5f, cy + innerR * 0.28f}, 24.0f, SPACING, WHITE);
    Vector2 lsz = MeasureTextEx(font, "WPM", 13.0f, SPACING);
    DrawTextEx(font, "WPM", {cx - lsz.x * 0.5f, cy + innerR * 0.28f + 28.0f},
               13.0f, SPACING, DIM_COLOR);
}

void RenderState::drawCashout(const GameState& game, float dt) {
    ClearBackground({22, 22, 26, 255});

    // Can body — metallic horizontal gradient
    int canX = 35, canY = 20, canW = SCREEN_W - 70, canH = SCREEN_H - 40;
    DrawRectangleGradientH(canX, canY, canW / 2, canH, {68, 72, 76, 255}, {205, 210, 215, 255});
    DrawRectangleGradientH(canX + canW / 2, canY, canW / 2, canH, {205, 210, 215, 255}, {68, 72, 76, 255});
    for (int sy = canY; sy < canY + canH; sy += 3)
        DrawLine(canX, sy, canX + canW, sy, {45, 45, 50, 28});
    DrawRectangleGradientH(canX, canY, 16, canH, {16, 16, 20, 200}, {0, 0, 0, 0});
    DrawRectangleGradientH(canX + canW - 16, canY, 16, canH, {0, 0, 0, 0}, {16, 16, 20, 200});

    // Advance chart/print animation
    if (chartProgress < 1.0f) {
        chartProgress += dt / 3.5f;
        if (chartProgress > 1.0f) chartProgress = 1.0f;
    }

    const auto& last    = game.results.back();
    int prevTotal       = game.totalCash - last.cash;
    float totalTime     = game.typing.elapsedTime;
    float playTime      = chartProgress * totalTime;

    // Compute how much cash has been "revealed" so far as chart plays back
    int earnedSoFar = 0;
    for (const auto& entry : wordCashHistory) {
        if (entry.first <= playTime) earnedSoFar += entry.second;
        else break; // entries sorted by time
    }
    earnedSoFar = std::min(earnedSoFar, last.cash);
    int displayTotal = prevTotal + earnedSoFar;

    char buf[128];
    const Color PAPER  = {240, 235, 218, 255};
    const Color INK    = {12,  12,  12,  255};
    const Color INKDIM = {80,  75,  68,  255};

    // =========================================================
    // LEFT PANEL — Nutrition Facts label
    // =========================================================
    const float PX = 52.0f, PY = 60.0f, PW = 415.0f, PH = 560.0f;
    DrawRectangleRec({PX, PY, PW, PH}, PAPER);
    DrawRectangleLinesEx({PX, PY, PW, PH}, 3.5f, INK);

    // Label edges slightly darker — sells the wrap-around-cylinder illusion
    DrawRectangleGradientH((int)PX, (int)PY, 22, (int)PH, {0, 0, 0, 60}, {0, 0, 0, 0});
    DrawRectangleGradientH((int)(PX + PW - 22), (int)PY, 22, (int)PH, {0, 0, 0, 0}, {0, 0, 0, 60});

    // Ink flash: brief green tint when a section is freshly stamped
    auto inkFlash = [&](float thr, float y, float h) {
        float age = chartProgress - thr;
        if (age >= 0.0f && age < 0.14f) {
            float a = 1.0f - age / 0.14f;
            DrawRectangle((int)PX + 3, (int)y, (int)PW - 6, std::max(1, (int)h),
                          {80, 200, 120, (unsigned char)(a * 100)});
        }
    };

    float rx = PX + 12, ry2 = PY + 10;

    // Per-section reveal thresholds — each stamps in independently
    const float T0=0.00f, T1=0.12f, T2=0.22f, T3=0.32f, T4=0.41f,
                T5=0.50f, T6=0.58f, T7=0.66f, T8=0.74f, T9=0.82f, T10=0.90f;

    // Section 0: title + serving size
    if (chartProgress >= T0) {
        inkFlash(T0, ry2, 54.0f);
        DrawTextEx(font, "Nutrition Facts", {rx, ry2}, 30.0f, SPACING, INK);
        DrawTextEx(font, "Serving size  1 paragraph", {rx, ry2 + 34}, 15.0f, SPACING, INK);
    }
    ry2 += 54;

    // Section 1: thick rule + WPM number
    if (chartProgress >= T1) {
        inkFlash(T1, ry2, 55.0f);
        DrawRectangle((int)PX, (int)ry2, (int)PW, 9, INK);
        DrawTextEx(font, "WPM", {rx, ry2 + 13 + 8}, 17.0f, SPACING, INK);
        std::snprintf(buf, sizeof(buf), "%.0f", last.wpm);
        Vector2 wpmSz = MeasureTextEx(font, buf, 46.0f, SPACING);
        DrawTextEx(font, buf, {PX + PW - wpmSz.x - 14, ry2 + 13 - 6}, 46.0f, SPACING, INK);
    }
    ry2 += 55;

    // Section 2: second thick rule + % Daily Best header
    if (chartProgress >= T2) {
        inkFlash(T2, ry2, 30.0f);
        DrawRectangle((int)PX, (int)ry2, (int)PW, 5, INK);
        Vector2 dhsz = MeasureTextEx(font, "% of Daily Best", 13.0f, SPACING);
        DrawTextEx(font, "% of Daily Best", {PX + PW - dhsz.x - 10, ry2 + 7}, 13.0f, SPACING, INKDIM);
    }
    ry2 += 25;

    // Stat row: stamps in at its threshold, always advances ry2 by 32
    auto statRow = [&](const char* label, const char* val, bool major, float thr) {
        if (chartProgress >= thr) {
            inkFlash(thr, ry2, 32.0f);
            DrawRectangle((int)PX, (int)ry2, (int)PW, 1, {140, 135, 125, 200});
            float fsz = major ? 17.0f : 15.0f;
            float indent = major ? 0.0f : 12.0f;
            DrawTextEx(font, label, {rx + indent, ry2 + 4}, fsz, SPACING, INK);
            if (val) {
                Vector2 vsz = MeasureTextEx(font, val, 17.0f, SPACING);
                DrawTextEx(font, val, {PX + PW - vsz.x - 12, ry2 + 4}, 17.0f, SPACING, INK);
            }
        }
        ry2 += 32;
    };

    std::snprintf(buf, sizeof(buf), "%.1f%%", last.accuracy);
    statRow("Accuracy", buf, true, T3);

    std::snprintf(buf, sizeof(buf), "%d chars", game.typing.maxStreak);
    statRow("Best Streak", buf, false, T4);

    std::snprintf(buf, sizeof(buf), "%d words", bestComboStreakWords);
    statRow("Best Word Combo", buf, false, T5);

    int errors = game.typing.totalKeystrokes - game.typing.correctKeystrokes;
    std::snprintf(buf, sizeof(buf), "%d", errors);
    statRow("Errors", buf, false, T6);

    std::snprintf(buf, sizeof(buf), "%.1f sec", game.typing.elapsedTime);
    statRow("Time Taken", buf, false, T7);

    // Thin rule + running average stamp together
    if (chartProgress >= T8) {
        inkFlash(T8, ry2, 7.0f);
        DrawRectangle((int)PX, (int)ry2, (int)PW, 3, INK);
    }
    ry2 += 7;
    if (game.runningAvgWPM > 0.0f) {
        std::snprintf(buf, sizeof(buf), "%.0f WPM", game.runningAvgWPM);
        statRow("Running Average", buf, false, T8);
    }

    if (game.beatAverage) {
        if (chartProgress >= T9) {
            inkFlash(T9, ry2, 28.0f);
            DrawRectangle((int)PX, (int)ry2, (int)PW, 1, {140, 135, 125, 200});
            DrawTextEx(font, "  Beat your average!", {rx + 10, ry2 + 4}, 14.0f, SPACING, {55, 140, 55, 255});
        }
        ry2 += 28;
    }

    // Section 9: thick rule + total cash
    if (chartProgress >= T10) {
        inkFlash(T10, ry2, 40.0f);
        DrawRectangle((int)PX, (int)ry2, (int)PW, 6, INK);
        DrawTextEx(font, "Total Cash", {rx, ry2 + 9}, 20.0f, SPACING, INK);
        std::snprintf(buf, sizeof(buf), "$%d", displayTotal);
        Color cashColor = (chartProgress >= 1.0f) ? (Color){40, 140, 55, 255} : (Color){60, 160, 70, 255};
        Vector2 csz = MeasureTextEx(font, buf, 26.0f, SPACING);
        DrawTextEx(font, buf, {PX + PW - csz.x - 12, ry2 + 6}, 26.0f, SPACING, cashColor);
    }

    // =========================================================
    // RIGHT PANEL — WPM chart + cash counter
    // =========================================================
    const float RX = 492.0f, RW = (float)(SCREEN_W - 492 - 52), RH = PH;
    DrawRectangleRec({RX, PY, RW, RH}, {20, 20, 30, 215});
    DrawRectangleLinesEx({RX, PY, RW, RH}, 1.5f, {55, 55, 75, 180});

    const auto& samples = game.typing.wpmSamples;
    if (samples.size() >= 2) {
        const float CX   = RX + 16;
        const float CW   = RW - 32;
        const float CY_T = PY + 48;
        const float CH   = 240.0f;
        const float CY_B = CY_T + CH;

        // Chart title
        Vector2 ctSz = MeasureTextEx(font, "WPM over time", 15.0f, SPACING);
        DrawTextEx(font, "WPM over time",
                   {RX + (RW - ctSz.x) * 0.5f, PY + 16}, 15.0f, SPACING, {140, 135, 160, 200});

        DrawRectangleRec({CX, CY_T, CW, CH}, {12, 12, 20, 220});
        DrawRectangleLinesEx({CX, CY_T, CW, CH}, 1.0f, {48, 48, 68, 200});

        // Y range with padding
        float minW = samples[0], maxW = samples[0];
        for (float s : samples) { if (s < minW) minW = s; if (s > maxW) maxW = s; }
        float wRange = maxW - minW;
        if (wRange < 12.0f) {
            float mid = (minW + maxW) * 0.5f;
            minW = mid - 10.0f; maxW = mid + 10.0f; wRange = 20.0f;
        }
        minW -= wRange * 0.12f; maxW += wRange * 0.12f; wRange = maxW - minW;

        // Running average dashed reference line
        if (game.runningAvgWPM > 0.0f) {
            float an = std::max(0.0f, std::min(1.0f, (game.runningAvgWPM - minW) / wRange));
            float ay = CY_B - an * CH;
            for (float x = CX + 2; x < CX + CW - 2; x += 13.0f)
                DrawLineEx({x, ay}, {std::min(x + 8.0f, CX + CW - 2), ay}, 1.5f, GHOST_COLOR);
            std::snprintf(buf, sizeof(buf), "avg %.0f", game.runningAvgWPM);
            DrawTextEx(font, buf, {CX + 4, ay - 16}, 12.0f, SPACING, GHOST_COLOR);
        }

        // Animated WPM line (reveals left→right with chartProgress)
        int ns       = (int)samples.size();
        int visCount = std::max(2, (int)(chartProgress * (float)ns));
        if (visCount > ns) visCount = ns;

        for (int i = 1; i < visCount; i++) {
            float x0 = CX + (float)(i-1) / (float)(ns-1) * CW;
            float x1 = CX + (float)(i)   / (float)(ns-1) * CW;
            float n0 = std::max(0.0f, std::min(1.0f, (samples[i-1] - minW) / wRange));
            float n1 = std::max(0.0f, std::min(1.0f, (samples[i]   - minW) / wRange));
            DrawLineEx({x0, CY_B - n0*CH}, {x1, CY_B - n1*CH}, 2.5f, ACCENT_COLOR);
        }
        // Leading dot
        if (visCount > 0) {
            int   di = visCount - 1;
            float dx = CX + (float)di / (float)(ns-1) * CW;
            float dn = std::max(0.0f, std::min(1.0f, (samples[di] - minW) / wRange));
            DrawCircle((int)dx, (int)(CY_B - dn*CH), 5, ACCENT_COLOR);
        }

        // --- Cash earned counter (animates with chart) ---
        float cashLabelY = CY_B + 22;
        Vector2 elSz = MeasureTextEx(font, "Earned this round:", 14.0f, SPACING);
        DrawTextEx(font, "Earned this round:",
                   {RX + (RW - elSz.x)*0.5f, cashLabelY}, 14.0f, SPACING,
                   {140, 135, 155, 190});

        std::snprintf(buf, sizeof(buf), "$%d", earnedSoFar);
        Color earnCol = (earnedSoFar == 0)        ? (Color){70, 70, 85, 160}  :
                        (earnedSoFar == last.cash) ? (Color){80, 220, 100, 255} :
                                                     (Color){255, 220, 80,  255};
        // Scale pop when cash changes
        float earnSz = (chartProgress < 1.0f && earnedSoFar > 0) ? 42.0f : 38.0f;
        Vector2 eSz  = MeasureTextEx(font, buf, earnSz, SPACING);
        DrawTextEx(font, buf, {RX + (RW - eSz.x)*0.5f, cashLabelY + 20},
                   earnSz, SPACING, earnCol);

        float bonusY = cashLabelY + 82.0f;
        std::snprintf(buf, sizeof(buf), "+$%d combo bonus", comboBonusCash);
        Color comboCol = comboColorForStreak(bestComboStreakWords);
        comboCol.a = 215;
        Vector2 cbSz = MeasureTextEx(font, buf, 14.0f, SPACING);
        DrawTextEx(font, buf, {RX + (RW - cbSz.x)*0.5f, bonusY}, 14.0f, SPACING, comboCol);

        std::snprintf(buf, sizeof(buf), "+$%d pace bonus", pressureBonusCash);
        Color paceCol = mixColor({90, 145, 255, 210}, {255, 122, 58, 225}, avgPressure);
        Vector2 pbSz = MeasureTextEx(font, buf, 14.0f, SPACING);
        DrawTextEx(font, buf, {RX + (RW - pbSz.x)*0.5f, bonusY + 24.0f}, 14.0f, SPACING, paceCol);

        // Paragraph counter at bottom of panel
        std::snprintf(buf, sizeof(buf), "Paragraph %d complete", game.paragraphsCompleted);
        Vector2 pcSz = MeasureTextEx(font, buf, 13.0f, SPACING);
        DrawTextEx(font, buf, {RX + (RW - pcSz.x)*0.5f, PY + PH - 55},
                   13.0f, SPACING, DIM_COLOR);
    }

    // ENTER prompt
    float pulse      = (std::sin(cursorBlink * 3.0f) + 1.0f) * 0.5f;
    unsigned char al = (unsigned char)(140 + pulse * 115);
    drawCentered(font, "Press ENTER to continue", SCREEN_H - 38, 20, {255, 220, 80, al});
}

void RenderState::drawStore(const GameState& game) {
    ClearBackground(BG_COLOR);

    drawCentered(font, "THE STORE", 100, 40, ACCENT_COLOR);
    drawCentered(font, "Coming Soon...", 300, 30, DIM_COLOR);

    char buf[64];
    std::snprintf(buf, sizeof(buf), "Your Cash: $%d", game.totalCash);
    drawCentered(font, buf, 370, 26, ACCENT_COLOR);

    float pulse = (std::sin(cursorBlink * 3.0f) + 1.0f) * 0.5f;
    unsigned char alpha = (unsigned char)(140 + pulse * 115);
    drawCentered(font, "Press ENTER to continue", SCREEN_H - 100, 22, {255, 220, 80, alpha});
}
