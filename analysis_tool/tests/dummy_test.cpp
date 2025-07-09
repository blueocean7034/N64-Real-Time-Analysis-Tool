#include "analysis_window.h"
#include <SDL.h>
#include <chrono>
#include <thread>

int main() {
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
        return 1;

    SDL_SetRelativeMouseMode(SDL_TRUE);

    analysis_window_start(nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    bool cursorVisible = SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE;
    bool relativeMode = SDL_GetRelativeMouseMode();

    analysis_window_stop();
    SDL_Quit();

    if (!cursorVisible || relativeMode)
        return 1;

    return 0;
}
