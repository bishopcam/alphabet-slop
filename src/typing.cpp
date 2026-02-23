#include "typing.h"
#include <algorithm>

void TypingState::load(const std::string& paragraph) {
    chars.clear();
    cursor = 0;
    totalKeystrokes = 0;
    correctKeystrokes = 0;
    streak = 0;
    maxStreak = 0;
    elapsedTime = 0.0f;
    finished = false;
    wpmSamples.clear();
    wpmSampleTimer  = 0.0f;
    wpmFinalPushed  = false;
    recentKeyTimes.clear();
    liveWPM = 0.0f;

    for (char c : paragraph) {
        CharInfo ci;
        ci.ch = c;
        chars.push_back(ci);
    }
}

void TypingState::processKey(int key) {
    if (finished || cursor >= (int)chars.size()) return;

    totalKeystrokes++;
    float now = elapsedTime;
    recentKeyTimes.push_back(now);

    char expected = chars[cursor].ch;
    if ((char)key == expected) {
        chars[cursor].state = CharState::Correct;
        chars[cursor].popTimer = 0.1f;
        correctKeystrokes++;
        streak++;
        if (streak > maxStreak) maxStreak = streak;
        // popStrength: near-zero at streak 1, grows to ~0.85 at streak 8+
        float norm = std::min((float)(streak - 1) / 7.0f, 1.0f);
        chars[cursor].popStrength = 0.05f + 0.80f * norm;
        cursor++;
        if (cursor >= (int)chars.size()) {
            finished = true;
        }
    } else {
        chars[cursor].state  = CharState::Wrong;
        chars[cursor].hadError = true;
        streak = 0;
    }
}

void TypingState::update(float dt) {
    if (!finished && cursor > 0) {
        elapsedTime += dt;
        wpmSampleTimer += dt;
        if (wpmSampleTimer >= 0.8f) {
            wpmSamples.push_back(getWPM());
            wpmSampleTimer -= 0.8f;
        }
    }

    // Live pace WPM: computed from the last ~2.2 seconds of key timings.
    float now = elapsedTime;
    while (!recentKeyTimes.empty() && now - recentKeyTimes.front() > 2.2f) {
        recentKeyTimes.erase(recentKeyTimes.begin());
    }
    if (recentKeyTimes.size() >= 2) {
        float span = std::max(0.08f, recentKeyTimes.back() - recentKeyTimes.front());
        float chars = (float)(recentKeyTimes.size() - 1);
        float rawLive = (chars / 5.0f) / (span / 60.0f);
        liveWPM += (rawLive - liveWPM) * std::min(1.0f, dt * 12.0f);
    } else if (!finished) {
        liveWPM = std::max(0.0f, liveWPM - dt * 42.0f);
    }

    // Push one final sample when the paragraph is completed
    if (finished && !wpmFinalPushed) {
        wpmSamples.push_back(getWPM());
        wpmFinalPushed = true;
    }

    for (auto& ci : chars) {
        if (ci.popTimer > 0.0f) {
            ci.popTimer -= dt;
            if (ci.popTimer < 0.0f) ci.popTimer = 0.0f;
        }
    }
}

float TypingState::getWPM() const {
    if (elapsedTime < 0.5f) return 0.0f;
    float minutes = elapsedTime / 60.0f;
    float words = (float)correctKeystrokes / 5.0f;
    return words / minutes;
}

float TypingState::getLiveWPM() const {
    return liveWPM;
}

float TypingState::getAccuracy() const {
    if (totalKeystrokes == 0) return 100.0f;
    return ((float)correctKeystrokes / (float)totalKeystrokes) * 100.0f;
}
