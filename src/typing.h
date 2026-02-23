#pragma once
#include <string>
#include <vector>

enum class CharState {
    Untyped,
    Correct,
    Wrong,
};

struct CharInfo {
    char ch;
    CharState state = CharState::Untyped;
    float popTimer = 0.0f;
    float popStrength = 0.0f; // 0..1, grows with streak
    bool hadError = false;    // true if a wrong key was pressed at this position
};

struct TypingState {
    std::vector<CharInfo> chars;
    int cursor = 0;
    int totalKeystrokes = 0;
    int correctKeystrokes = 0;
    int streak = 0;
    int maxStreak = 0;
    float elapsedTime = 0.0f;
    bool finished = false;

    // WPM history for cashout chart
    std::vector<float> wpmSamples;
    float wpmSampleTimer   = 0.0f;
    bool  wpmFinalPushed   = false;

    void load(const std::string& paragraph);
    void processKey(int key);
    void update(float dt);

    float getWPM() const;
    float getAccuracy() const;
};
