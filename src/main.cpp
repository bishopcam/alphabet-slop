#include "raylib.h"
#include "game.h"
#include "render.h"
#include <cstdlib>
#include <ctime>
#include <cstring>

int main() {
    std::srand((unsigned)std::time(nullptr));

    InitWindow(1000, 700, "Alphabet Soup - A Typing Roguelike");
    SetTargetFPS(60);

    GameState   game;
    RenderState render;
    render.init();
    render.initMenu();

    static const char* OPT_WORDS[] = {"start", "settings", "quit", nullptr};

    while (!WindowShouldClose() && !render.quitRequested) {
        float dt = GetFrameTime();

        game.update(dt);
        render.updateCursor(dt);

        switch (game.screen) {
            case GameScreen::Menu: {
                render.updateMenu(dt);

                int key = GetCharPressed();
                while (key > 0) {
                    if (key >= 32 && key <= 126 && render.menuInputLen < 30) {
                        // Normalise to lowercase
                        char lc = (char)key;
                        if (lc >= 'A' && lc <= 'Z') lc += 32;

                        // Build candidate input
                        char test[32];
                        std::memcpy(test, render.menuInput, render.menuInputLen);
                        test[render.menuInputLen]     = lc;
                        test[render.menuInputLen + 1] = '\0';
                        int testLen = render.menuInputLen + 1;

                        // Accept only if at least one option still matches
                        bool anyMatch = false;
                        for (int i = 0; OPT_WORDS[i]; i++) {
                            int wl = (int)std::strlen(OPT_WORDS[i]);
                            if (testLen <= wl &&
                                std::strncmp(test, OPT_WORDS[i], testLen) == 0) {
                                anyMatch = true;
                                break;
                            }
                        }

                        if (anyMatch) {
                            render.menuInput[render.menuInputLen] = lc;
                            render.menuInputLen++;
                            render.menuInput[render.menuInputLen] = '\0';

                            // Check for exact match → execute
                            for (int i = 0; OPT_WORDS[i]; i++) {
                                if (std::strcmp(render.menuInput, OPT_WORDS[i]) == 0) {
                                    if (i == 0) {           // start
                                        render.resetMenu();
                                        render.resetTypingCan(true);
                                        game.startNewParagraph();
                                    } else if (i == 1) {    // settings (placeholder)
                                        render.resetMenu();
                                    } else if (i == 2) {    // quit
                                        render.quitRequested = true;
                                    }
                                    break;
                                }
                            }
                        } else {
                            render.menuShake = 1.0f;
                        }
                    }
                    key = GetCharPressed();
                }
            } break;

            case GameScreen::Typing: {
                int key = GetCharPressed();
                while (key > 0) {
                    if (key >= 32 && key <= 126) {
                        int prevCursor = game.typing.cursor;
                        game.typing.processKey(key);
                        if (game.typing.cursor == prevCursor && !game.typing.finished)
                            game.screenShake = 1.0f;
                    }
                    key = GetCharPressed();
                }
                if (game.typing.cursor == 0 && game.typing.totalKeystrokes == 0)
                    game.typing.elapsedTime = 0.0f;
                // Transition to cashout only after the per-word bowl animation finishes
                if (game.typing.finished && render.readyToCashout) {
                    game.pendingCash = render.totalWordCash;
                    render.resetTypingCan(false); // preserve wordCashHistory for cashout chart
                    game.finishParagraph();
                }
            } break;

            case GameScreen::Cashout: {
                if (IsKeyPressed(KEY_ENTER)) {
                    if (game.paragraphsCompleted % 3 == 0)
                        game.screen = GameScreen::Store;
                    else {
                        render.resetTypingCan(true);
                        game.startNewParagraph();
                    }
                }
            } break;

            case GameScreen::Store: {
                if (IsKeyPressed(KEY_ENTER)) {
                    render.resetTypingCan(true);
                    game.startNewParagraph();
                }
            } break;
        }

        BeginDrawing();
        switch (game.screen) {
            case GameScreen::Menu:    render.drawMenu();              break;
            case GameScreen::Typing:  render.drawTyping(game, dt);    break;
            case GameScreen::Cashout: render.drawCashout(game, dt);    break;
            case GameScreen::Store:   render.drawStore(game);         break;
        }
        EndDrawing();
    }

    render.unload();
    CloseWindow();
    return 0;
}
