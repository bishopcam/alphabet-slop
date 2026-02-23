#pragma once
#include "game.h"
#include "raylib.h"
#include <vector>

struct SoupLetter {
    char ch;
    float baseX, baseY;
    float orbitAngle, orbitSpeed;
    float orbitRx, orbitRy;
    float rot, rotSpeed;
    float size;
    unsigned char baseAlpha;
    // Pop-from-below intro animation
    float introProgress = 0.0f; // 0=below surface, 1=fully emerged
    float introDelay    = 0.0f; // seconds before this letter starts rising
    bool  introComplete = false;
    // Menu option identity (-1 = title letter)
    int optionIdx = -1;
    int charIdx   = 0;
};

struct VegPiece {
    int   type;          // 0=pea  1=carrot  2=celery  3=chicken
    float x, y;
    float orbitAngle, orbitSpeed;
    float orbitRx, orbitRy;
    float rot, rotSpeed;
    float szMul;         // size multiplier 0.8..1.5 for natural variation
    unsigned char alpha;
};

struct FlyingLetter {
    char  ch;
    float startX, startY;
    float elapsed;      // wall-clock time since word was queued
    float launchDelay;  // per-letter stagger offset within word
    float arcH;
    float spinDir;      // +1 or -1
    Color col;          // rating color (gold/green/blue-gray)
};

struct WordPopup {
    int   rating;      // 1=okay  2=great  3=perfect
    float elapsed;     // time since the word's letters were launched
    float showAt;      // elapsed when popup first becomes visible (last letter landing)
    float popX, popY;  // origin position (above the word's text)
    bool  triggered;   // true once elapsed >= showAt (used to fire canBounce)
};

struct Ripple {
    float x = 0, y = 0;
    float progress = 0.0f;
    float speed    = 0.35f;
    bool  active   = false;
};

struct RenderState {
    Font font;
    bool customFont = false;
    // Typing cursor
    float cursorX = 0, cursorY = 0;
    float targetCursorX = 0, targetCursorY = 0;
    float cursorBlink = 0;
    // Menu
    std::vector<SoupLetter> titleLetters;  // "ALPHABET" + "SOUP"
    std::vector<SoupLetter> optionLetters; // "START" / "SETTINGS" / "QUIT" (floating)
    std::vector<VegPiece>   vegPieces;    // floating vegetables & chicken
    std::vector<Ripple>     ripples;
    float menuTime    = 0;
    float rippleTimer = 0;
    char  menuInput[32] = {};
    int   menuInputLen  = 0;
    float menuShake     = 0;
    bool  quitRequested = false;

    // Per-word can animation (active during Typing screen)
    std::vector<FlyingLetter> flyingLetters;
    std::vector<WordPopup>    wordPopups;
    // Per-word cash log: (elapsedTime when typed, cash amount) — persists into Cashout
    std::vector<std::pair<float,int>> wordCashHistory;
    int   totalWordCash  = 0;    // accumulated this paragraph (read by main before reset)
    int   sentCharIdx    = 0;    // chars before this index have been launched
    float canFinishTimer = -1.0f; // -1=inactive; >=0=counting to cashout
    bool  readyToCashout = false;
    float canBounceTimer = 0.0f; // golden flash at bowl rim when word lands
    float chartProgress  = 0.0f; // 0→1: WPM chart animation on cashout screen
    float tachoWPM       = 0.0f; // smoothed WPM for tachometer needle

    void init();
    void unload();
    void initMenu();
    void updateMenu(float dt);
    void resetMenu();

    void drawMenu();
    void drawTyping(GameState& game, float dt);
    void drawCashout(const GameState& game, float dt);
    void drawStore(const GameState& game);
    void updateCursor(float dt);
    void resetTypingCan(bool newParagraph = false);
};
