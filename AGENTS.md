# N64 Real Time Analysis Tool

This project aims to build a high-performance and low latency analysis tool that is attached to Sky96 (a clone of the mupen64plus emulator with it's internal naming needing to be changed to "sky96"). When "my_original_rom1.z64" is launched in Sky96, the analysis tool opens in a separate window and displays live information about the game and emulator in real time. This analysis tool contains extensive and comprehensive menus and displays that can show the user everything that is going on inside the emulator and my_original_rom1.z64 in real time. The only limit is how deep the user wants to go. This analysis tool let's the user alter any emulator or ROM value in real time as the game is running to see what effects it will have. This analysis tool also includes arbitrary code execution and scripting, which allows the user to accomplish anything they can imagine.

**NEVER alter anything inside of mupen64plus-bundle-src-2.6.0. That emulator is for VIEWING AND STUDYING ONLY.**

## Current Contents
- **analysis_tool** – Real-time analysis plugin.
- **mupen64plus-bundle-src-2.6.0** – Official Mupen64Plus source bundle (2.6.0) with build scripts.
- **sky96** – A clone of the mupen64plus emulator with it's internal naming needing to be changed to "sky96".
- **my_original_rom1.z64** – ROM image for testing and experimentation.
- **AGENTS.md** – This guide.

## Software Stack Decisions
- **Language**: C++20 for core logic and plugin development.
- **Build System**: CMake for cross-platform builds on Ubuntu 24.04.2 LTS and Windows 11.
- **Graphics**: The emulator and analysis tool both render using **OpenGL 3.3**, a battle-tested and industry standard open source API.
- **GUI**: SDL2 combined with [Dear ImGui](https://github.com/ocornut/imgui) for a lightweight window that updates every frame.
  - The main Sky96 emulation window is an SDL2 window with an OpenGL context.
  - The analysis tool window also uses SDL2 with OpenGL and Dear ImGui.
- **Audio**: SDL2's audio subsystem for extremely reliable playback.
- **Input**: SDL2 event handling for keyboard and controller support.
- **Integration**: Use the sky96 plugin API to collect emulation state (registers, executed instructions, ROM values, etc.) and present them in the analysis window.
- **Optional Scripting**: Python scripts may be used for automation or quick prototyping, but performance-critical paths remain in C++.

These choices keep the tool portable and very low latency. SDL2 and Dear ImGui run with minimal overhead on both supported OSes. C++20 and CMake allow modern language features with straightforward compilation across platforms.

## Repository Layout
```
/                  – Project root
|-- AGENTS.md      – this document
|-- mupen64plus-bundle-src-2.6.0/ – reference emulator source tree
|-- analysis_tool/ – real‑time analysis plugin
|-- sky96/ – emulator that will be used with the analysis tool
|-- my_original_rom1.z64 – test ROM
```
All new analysis-tool code should live in a dedicated directory (e.g. `analysis_tool/`) so merges from upstream remain simple.

## Building the Emulator
Sky96 relies on the original Mupen64Plus build scripts. Install the required
packages on Ubuntu:
```bash
sudo apt update
sudo apt install build-essential libsdl2-dev libpng-dev libfreetype6-dev libz-dev
```
To build and test the emulator run:
```bash
cd sky96
./m64p_build.sh        # compile all modules into ./test
./m64p_test.sh         # run the bundled test ROM
./m64p_install.sh      # install locally into the project root
```
Use `./m64p_uninstall.sh` if you need to remove the installation.

## Analysis Tool Development
- Create a new CMake project in `analysis_tool/` that links to the Sky96 core.
- Use the plugin interface to subscribe to emulation events and gather runtime data.
- Render the data using Dear ImGui. Keep the window responsive by running the UI on its own loop synchronized with the emulator.
- Implement features such as frame advance, rewind, and state visualization. Display current and next instructions, register values, variable states, etc. The UI should update in real time with minimal impact on emulator performance.
- Consolidate all debug features here. ROM value displays, emulator state inspection, special emulation modes, and tool-assisted speedrun utilities should be placed in the analysis tool rather than the Sky96 core.

## Sky96 Development
- Keep its code isolated in the `sky96/` directory.
- The primary goal is to integrate extremely tightly with the analysis plugin while keeping in strict uniformity with the reference mupen64plus emulator.
- Any debug or special emulation features belong in the analysis tool. If a debugging feature exists in Sky96, move it to the plugin so the emulator core stays lean.

## Future Considerations
- Look into using shared memory or IPC if the analysis window is run as a separate process. This may help keep the emulator core isolated and fast.
- Experiment with GPU profiling or additional debugging data if it proves useful.
- Document all major decisions and rationale here so future contributors understand the direction taken.

## Contributor Guidelines
- Write detailed documentation of your work as you go. Implement exception handling and descriptive error messages anywhere you can. Keep notes of decisions and rationale alongside your code or in commit messages.
- Leave the Mupen64Plus source tree untouched; place all new analysis tool code under `analysis_tool/`.
- Place all Sky96 emulator code in the separate `sky96/` directory.
- When adding new features, update relevant READMEs and run `ctest`.
  Ensure the analysis tool builds and all existing tests pass. Add tests for
  every new unit of code and anywhere coverage is missing, and verify the `sky96` executable builds via CMake without errors.
- The analysis tool window stays open until closed by the user; there is no
  environment variable controlling its duration.
- Implement comprehensive exception handling throughout the project. We can never have too much of it.
  Every error must produce a clear and descriptive message printed to the terminal immediately.
- Prioritize thorough testing. Whenever you add or modify a feature, create a
  corresponding test and search the project for untested areas. Add tests everywhere you can;
  there is no such thing as too much coverage.
- Keep individual files small. If a source or documentation file grows beyond
  **400 lines**, refactor or split it into manageable pieces. This keeps the
  codebase easy to navigate. The limit does not apply to the untouched
  `mupen64plus-bundle-src-2.6.0` directory.
- When you find anything unnecessary, outdated, defunct, obsolete, deprecated,
  misleading or incorrect in any code, documentation, notes, code comments or
  README files, fix it immediately so the repository remains accurate. The only exception is the reference code in `mupen64plus-bundle-src-2.6.0`—the Mupen64Plus emulator is off limits and must never be altered or edited.
