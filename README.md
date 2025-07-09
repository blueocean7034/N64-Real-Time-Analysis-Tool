# N64 Real-Time Analysis Tool Quickstart

This repository contains the Sky96 emulator and a small analysis plugin.
Use the `run_sky96.sh` script to install everything, build the emulator
and plugin, and launch the test ROM with the analysis window enabled.

```bash
./run_sky96.sh
```

The script installs required build packages using `apt`, compiles all
Sky96 components (including video and audio plugins), builds the
`analysis_tool` project, runs its tests via `ctest`, and finally starts
the emulator with `my_original_rom1.z64`.
