#include "analysis_window.h"
#include <SDL.h>
#include <atomic>
#include <thread>
#include <stdio.h>

static std::atomic<bool> running{false};
static std::thread windowThread;
static core_do_command_func coreCmd = nullptr;

static void window_loop()
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        fprintf(stderr, "Analysis window SDL init failed: %s\n", SDL_GetError());
        return;
    }
    SDL_Window* win = SDL_CreateWindow("Analysis Tool", SDL_WINDOWPOS_CENTERED,
        SDL_WINDOWPOS_CENTERED, 320, 240, SDL_WINDOW_SHOWN);
    if (!win)
    {
        fprintf(stderr, "Failed to create analysis window: %s\n", SDL_GetError());
        SDL_Quit();
        return;
    }

    while (running.load())
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            if (e.type == SDL_QUIT)
                running.store(false);
            else if (e.type == SDL_MOUSEBUTTONDOWN)
            {
                int x = e.button.x;
                int y = e.button.y;
                if (y < 50 && coreCmd)
                {
                    if (x < 150)
                        coreCmd(M64CMD_RESUME, 0, nullptr);
                    else if (x > 170)
                        coreCmd(M64CMD_PAUSE, 0, nullptr);
                }
            }
        }
        SDL_Surface* surf = SDL_GetWindowSurface(win);
        SDL_FillRect(surf, NULL, SDL_MapRGB(surf->format, 50, 50, 50));
        SDL_Rect resume = {20, 10, 100, 30};
        SDL_FillRect(surf, &resume, SDL_MapRGB(surf->format, 0, 128, 0));
        SDL_Rect pause = {200, 10, 100, 30};
        SDL_FillRect(surf, &pause, SDL_MapRGB(surf->format, 128, 0, 0));
        SDL_UpdateWindowSurface(win);
        SDL_Delay(16);
    }
    SDL_DestroyWindow(win);
    SDL_Quit();
}

extern "C" void analysis_window_start(core_do_command_func cmd)
{
    coreCmd = cmd;
    running = true;
    windowThread = std::thread(window_loop);
}

extern "C" void analysis_window_stop(void)
{
    running = false;
    if (windowThread.joinable())
        windowThread.join();
}
