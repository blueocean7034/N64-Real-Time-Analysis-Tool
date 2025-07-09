#include "analysis_window.h"
#include <chrono>
#include <thread>

int main() {
    setenv("SDL_VIDEODRIVER", "dummy", 1);
    analysis_window_start(nullptr);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));
    analysis_window_stop();
    return 0;
}
