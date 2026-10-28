# The Game of 16 (CLI 15-Puzzle)

A feature-complete, interactive Command Line Interface (CLI) implementation of the classic 15-puzzle (Game of 16) written in C.

## Features

- **Clean Minimalist CLI**: Uncluttered, modern terminal interface with solid colored square tiles and empty space. Tiles that are in their correct sorted position immediately turn **green**, while unsolved tiles remain **blue**.
- **Arrow-Key Controls**: Tiles are swapped into the empty space immediately on pressing arrow keys—**no Enter key press required**.
- **God Mode Auto-Solver**:
  - Invoked at any point by typing `GOD` and pressing **Enter**.
  - Automatically finds a valid solution from **any state**.
  - Executes moves at a rate of **1 tile per second** so the player can watch the board solve itself.
  - Automatically detects when the puzzle is solved and stops.
- **Dual Direction Modes**:
  - **Tile Slide Mode** (Default): Pressing an arrow key moves the tile in that direction into the empty space (e.g., pressing `↑` moves the tile below the empty space UP).
  - **Hole Move Mode**: Pressing an arrow key moves the empty hole cursor in that direction.
  - Toggle between modes anytime by pressing `T` or typing `TOGGLE` + `Enter`.
- **Cross-Platform**:
  - **Linux** (x86_64 & aarch64): Built with `ncurses`.
  - **Windows 64-bit**: Native Windows Console implementation with Virtual Terminal Processing—**zero external DLL dependencies** required!
- **Automated CI/CD**: Pre-configured GitHub Actions workflow automatically builds, tests, and publishes artifacts for Linux x86_64, Linux aarch64, and Windows 64-bit on every push.
- **Rock-Solid Solver**:
  - Combines **IDA*** (Manhattan distance + Linear Conflict heuristic) for fast, optimal solutions on shallow configurations.
  - Falls back to a deterministic **multi-stage BFS algorithm** that solves **any** solvable board in under 0.2 milliseconds.
  - 100% mathematically proven solvability verification via permutation parity and inversion counting.
- **Headless / Demo Mode**:
  - Run `--demo` to view auto-solving in standard terminal output without ncurses.
  - Run `--test` to verify internal algorithms against 50+ random permutations.

---

## Building and Running

### Linux (Native)
```bash
make
./game16
```

### Windows 64-bit

#### 1. On Windows (using MinGW/GCC)
```bash
gcc -Wall -Wextra -O3 -std=c11 main.c board.c solver.c ui.c -o game16.exe
.\game16.exe
```

#### 2. Cross-compiling for Windows from Linux
```bash
make windows
# Outputs a standalone game16.exe (run with wine or on Windows)
wine ./game16.exe
```

### Automated Multi-Platform GitHub Workflow
Pushing to GitHub triggers the `.github/workflows/build.yml` workflow, which automatically compiles and tests:
1. `game16-linux-x86_64`
2. `game16-linux-aarch64`
3. `game16-windows-x86_64.exe`

All binaries are verified via the automated test suite and published as downloadable artifacts (and GitHub releases on `v*` tags).

---

## In-Game Controls

| Key / Input | Action |
| :--- | :--- |
| `↑` `↓` `←` `→` | **Move tile immediately** into empty region (no Enter needed) |
| Type `GOD` + `Enter` | **Activate God Mode** (solves board automatically at 1 move/sec) |
| `T` / Type `TOGGLE` + `Enter` | **Toggle arrow direction mode** (Tile Slide ↔ Hole Move) |
| `R` / Type `RESET` + `Enter` | **New Game** (scrambles tiles into a fresh solvable board) |
| `Q` / `Esc` / Type `QUIT` + `Enter` | **Quit** game |

---

## Architecture Overview

- [board.h](file:///home/rishav/prog/16/board.h) / [board.c](file:///home/rishav/prog/16/board.c): 4×4 grid state, scramble generation, move mechanics, and mathematical solvability invariant check.
- [solver.h](file:///home/rishav/prog/16/solver.h) / [solver.c](file:///home/rishav/prog/16/solver.c): Dual-strategy solver (IDA* + Multi-stage BFS) and move path optimizer.
- [ui.h](file:///home/rishav/prog/16/ui.h) / [ui.c](file:///home/rishav/prog/16/ui.c): Terminal user interface, ncurses rendering, key event loop, 1-second God mode timer.
- [main.c](file:///home/rishav/prog/16/main.c): CLI arguments, self-test suite runner, entry point.
