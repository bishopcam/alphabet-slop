#pragma once
#include "typing.h"
#include <string>
#include <vector>

enum class GameScreen {
    Menu,
    Typing,
    Cashout,
    Store,
};

struct ParagraphResult {
    float wpm;
    float accuracy;
    int cash;
};

struct GameState {
    GameScreen screen = GameScreen::Menu;
    TypingState typing;

    int paragraphIndex = 0;
    int paragraphsCompleted = 0;
    int totalCash = 0;
    int lastCash = 0;
    float runningAvgWPM = 0.0f;
    bool beatAverage = false;

    std::vector<ParagraphResult> results;

    float screenShake = 0.0f;
    int   pendingCash = 0;  // set by render before finishParagraph (per-word sum)

    void startNewParagraph();
    void finishParagraph();
    float getAccuracyMultiplier(float accuracy) const;
    void update(float dt);
};
