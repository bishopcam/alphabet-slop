#include "game.h"
#include "paragraphs.h"
#include <cstdlib>


void GameState::startNewParagraph() {
    paragraphIndex = std::rand() % (int)PARAGRAPHS.size();
    typing.load(PARAGRAPHS[paragraphIndex]);
    screen = GameScreen::Typing;
    beatAverage = false;
}

void GameState::finishParagraph() {
    float wpm = typing.getWPM();
    float acc = typing.getAccuracy();

    // Cash comes from per-word ratings accumulated in render (1/2/3 per word).
    // Fallback to 1 if for some reason nothing was tracked.
    int cash = (pendingCash > 0) ? pendingCash : 1;
    pendingCash = 0;

    beatAverage = (!results.empty() && wpm > runningAvgWPM);

    lastCash    = cash;
    totalCash  += cash;
    paragraphsCompleted++;

    if (results.empty())
        runningAvgWPM = wpm;
    else
        runningAvgWPM = (runningAvgWPM * (float)results.size() + wpm) / (float)(results.size() + 1);

    results.push_back({wpm, acc, cash});
    screen = GameScreen::Cashout;
}

float GameState::getAccuracyMultiplier(float accuracy) const {
    if (accuracy >= 100.0f) return 2.0f;
    if (accuracy >= 95.0f) return 1.5f;
    if (accuracy >= 90.0f) return 1.0f;
    return 0.5f;
}

void GameState::update(float dt) {
    if (screenShake > 0.0f) {
        screenShake -= dt * 15.0f;
        if (screenShake < 0.0f) screenShake = 0.0f;
    }

    if (screen == GameScreen::Typing) {
        typing.update(dt);
    }
}
