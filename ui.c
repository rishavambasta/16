#define _DEFAULT_SOURCE
#define _POSIX_C_SOURCE 200809L

#include "ui.h"
#include "solver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <stdbool.h>
#include <stdarg.h>

#define PAIR_TILE_NORMAL  1
#define PAIR_TILE_SOLVED  2
#define PAIR_TEXT_HEADER  3
#define PAIR_TEXT_DIM     4
#define PAIR_PROMPT       5
#define PAIR_STATUS_GOD   6
#define PAIR_STATUS_WIN   7

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <conio.h>
#include <io.h>

#ifndef strcasecmp
#define strcasecmp _stricmp
#endif

#define ERR (-1)
#ifndef KEY_BACKSPACE
#define KEY_BACKSPACE 0407
#endif
#ifndef KEY_RESIZE
#define KEY_RESIZE 0632
#endif

#define stdscr ((void*)0)
#define TRUE 1
#define FALSE 0
#define COLOR_PAIR(p) ((p) << 8)
#define A_BOLD  0x01
#define A_DIM   0x02
#define A_BLINK 0x04

static HANDLE win_hOut = INVALID_HANDLE_VALUE;
static HANDLE win_hIn = INVALID_HANDLE_VALUE;
static DWORD win_origOutMode = 0;
static DWORD win_origInMode = 0;

static void initscr(void) {
    win_hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    win_hIn = GetStdHandle(STD_INPUT_HANDLE);
    GetConsoleMode(win_hOut, &win_origOutMode);
    GetConsoleMode(win_hIn, &win_origInMode);
    SetConsoleMode(win_hOut, win_origOutMode | ENABLE_VIRTUAL_TERMINAL_PROCESSING | DISABLE_NEWLINE_AUTO_RETURN);
    SetConsoleMode(win_hIn, ENABLE_EXTENDED_FLAGS);
    printf("\033[?25l"); /* Hide cursor */
    fflush(stdout);
}

static void endwin(void) {
    if (win_hOut != INVALID_HANDLE_VALUE) {
        SetConsoleMode(win_hOut, win_origOutMode);
    }
    if (win_hIn != INVALID_HANDLE_VALUE) {
        SetConsoleMode(win_hIn, win_origInMode);
    }
    printf("\033[?25h\033[0m\n");
    fflush(stdout);
}

static inline void cbreak(void) {}
static inline void noecho(void) {}
static inline void keypad(void *w, int b) { (void)w; (void)b; }
static inline void curs_set(int s) { (void)s; }
static inline void timeout(int ms) { (void)ms; }
static inline bool has_colors(void) { return true; }
static inline void start_color(void) {}
static inline void use_default_colors(void) {}
static inline void init_pair(int p, int fg, int bg) { (void)p; (void)fg; (void)bg; }

#define getmaxyx(w, y, x) do { \
    CONSOLE_SCREEN_BUFFER_INFO csbi; \
    if (GetConsoleScreenBufferInfo(win_hOut, &csbi)) { \
        y = csbi.srWindow.Bottom - csbi.srWindow.Top + 1; \
        x = csbi.srWindow.Right - csbi.srWindow.Left + 1; \
    } else { \
        y = 24; x = 80; \
    } \
} while(0)

static inline void erase(void) {
    printf("\033[2J\033[H");
}

static inline void refresh(void) {
    fflush(stdout);
}

static inline void move(int y, int x) {
    printf("\033[%d;%dH", y + 1, x + 1);
}

static inline void printw(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

static inline void mvprintw(int y, int x, const char *fmt, ...) {
    printf("\033[%d;%dH", y + 1, x + 1);
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
}

static inline void attron(int attr) {
    int pair = (attr >> 8) & 0xFF;
    if (pair == PAIR_TILE_NORMAL) {
        printf("\033[44;97;1m"); /* Blue background, bold white text */
    } else if (pair == PAIR_TILE_SOLVED) {
        printf("\033[42;97;1m"); /* Green background, bold white text */
    } else if (pair == PAIR_TEXT_HEADER) {
        printf("\033[97;1m");    /* Bright white bold */
    } else if (pair == PAIR_TEXT_DIM) {
        printf("\033[90m");      /* Dim / gray */
    } else if (pair == PAIR_PROMPT) {
        printf("\033[93;1m");    /* Yellow */
    } else if (pair == PAIR_STATUS_GOD) {
        printf("\033[91;1m");    /* Red */
    } else if (pair == PAIR_STATUS_WIN) {
        printf("\033[92;1m");    /* Green */
    }
}

static inline void attroff(int attr) {
    (void)attr;
    printf("\033[0m");
}

#ifndef COLOR_BLACK
#define COLOR_BLACK   0
#define COLOR_RED     1
#define COLOR_GREEN   2
#define COLOR_YELLOW  3
#define COLOR_BLUE    4
#define COLOR_MAGENTA 5
#define COLOR_CYAN    6
#define COLOR_WHITE   7
#endif

static int win_getch(void) {
    /* 50ms non-blocking poll */
    DWORD start = GetTickCount();
    while (GetTickCount() - start < 50) {
        if (_kbhit()) {
            int c = _getch();
            if (c == 0 || c == 224) {
                int c2 = _getch();
                switch (c2) {
                    case 72: return KEY_UP;
                    case 80: return KEY_DOWN;
                    case 75: return KEY_LEFT;
                    case 77: return KEY_RIGHT;
                }
                return ERR;
            }
            if (c == 13) return '\n';
            if (c == 8)  return KEY_BACKSPACE;
            if (c == 27) return 27;
            return c;
        }
        Sleep(5);
    }
    return ERR;
}
#define getch win_getch

static long long get_time_ms(void) {
    return (long long)GetTickCount64();
}

static inline void sleep_ms(int ms) {
    Sleep(ms);
}

#else
/* Linux / POSIX with ncurses */
#include <unistd.h>
#include <strings.h>
#include <curses.h>

static long long get_time_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + (ts.tv_nsec / 1000000LL);
}

static inline void sleep_ms(int ms) {
    usleep((useconds_t)ms * 1000);
}
#endif

static void init_colors_ui(void) {
    if (has_colors()) {
        start_color();
        use_default_colors();
        init_pair(PAIR_TILE_NORMAL, COLOR_WHITE, COLOR_BLUE);
        init_pair(PAIR_TILE_SOLVED, COLOR_WHITE, COLOR_GREEN);
        init_pair(PAIR_TEXT_HEADER, COLOR_WHITE, -1);
        init_pair(PAIR_TEXT_DIM,    COLOR_WHITE, -1);
        init_pair(PAIR_PROMPT,      COLOR_YELLOW, -1);
        init_pair(PAIR_STATUS_GOD,  COLOR_RED,   -1);
        init_pair(PAIR_STATUS_WIN,  COLOR_GREEN, -1);
    }
}

static void render_clean(const Board *b, int arrow_mode, const char *cmd_buf,
                         const char *status_msg, bool in_god_mode, int god_step, int god_total,
                         time_t elapsed_sec, bool won) {
    erase();

    int max_y = 24, max_x = 80;
    getmaxyx(stdscr, max_y, max_x);

    /* Determine tile dimensions based on screen height */
    int tile_h = (max_y >= 21) ? 3 : ((max_y >= 17) ? 2 : 1);
    int row_gap = (max_y >= 21) ? 1 : 0;
    int tile_w = 6;
    int col_gap = 2;

    int board_w = 4 * tile_w + 3 * col_gap;
    int board_h = 4 * tile_h + 3 * row_gap;

    int total_needed_h = board_h + 6;
    int start_y = (max_y > total_needed_h) ? (max_y - total_needed_h) / 2 : 1;
    int start_x = (max_x > board_w) ? (max_x - board_w) / 2 : 2;

    /* 1. Minimal Header (Moves & Time) */
    int min = (int)(elapsed_sec / 60);
    int sec = (int)(elapsed_sec % 60);
    attron(COLOR_PAIR(PAIR_TEXT_HEADER) | A_BOLD);
    mvprintw(start_y, start_x + (board_w - 24) / 2, "Moves: %-4d     Time: %02d:%02d",
             b->move_count, min, sec);
    attroff(COLOR_PAIR(PAIR_TEXT_HEADER) | A_BOLD);
    start_y += 2;

    /* 2. 4x4 Grid of Simple Colored Tiles */
    for (int r = 0; r < BOARD_SIZE; r++) {
        int tile_top = start_y + r * (tile_h + row_gap);

        for (int line = 0; line < tile_h; line++) {
            move(tile_top + line, start_x);

            for (int c = 0; c < BOARD_SIZE; c++) {
                int val = b->tiles[r][c];

                if (val == 0) {
                    /* Empty tile position: clean empty space */
                    printw("%*s", tile_w, "");
                } else {
                    /* Tile done in ascending order: green; otherwise blue */
                    int target_val = r * BOARD_SIZE + c + 1;
                    int tile_pair = (val == target_val) ? PAIR_TILE_SOLVED : PAIR_TILE_NORMAL;

                    attron(COLOR_PAIR(tile_pair));
                    if (tile_h == 1) {
                        attron(A_BOLD);
                        printw("  %2d  ", val);
                        attroff(A_BOLD);
                    } else if (tile_h == 2) {
                        if (line == 0) {
                            attron(A_BOLD);
                            printw("  %2d  ", val);
                            attroff(A_BOLD);
                        } else {
                            printw("      ");
                        }
                    } else {
                        if (line == 1) {
                            attron(A_BOLD);
                            printw("  %2d  ", val);
                            attroff(A_BOLD);
                        } else {
                            printw("      ");
                        }
                    }
                    attroff(COLOR_PAIR(tile_pair));
                }

                if (c < BOARD_SIZE - 1) {
                    printw("%*s", col_gap, "");
                }
            }
        }
    }
    start_y += board_h + 1;

    /* 3. Minimal Status / Command Line */
    if (won) {
        attron(COLOR_PAIR(PAIR_STATUS_WIN) | A_BOLD);
        mvprintw(start_y, start_x, "● Solved in %d moves!  [R: restart  |  Q: quit]", b->move_count);
        attroff(COLOR_PAIR(PAIR_STATUS_WIN) | A_BOLD);
    } else if (in_god_mode) {
        attron(COLOR_PAIR(PAIR_STATUS_GOD) | A_BOLD);
        mvprintw(start_y, start_x, "● God Mode: step %d / %d [Esc to stop]",
                 god_step, god_total);
        attroff(COLOR_PAIR(PAIR_STATUS_GOD) | A_BOLD);
    } else {
        attron(COLOR_PAIR(PAIR_PROMPT) | A_BOLD);
        mvprintw(start_y, start_x, "> ");
        attroff(COLOR_PAIR(PAIR_PROMPT) | A_BOLD);
        printw("%s", cmd_buf);
        attron(A_BLINK);
        printw("▌");
        attroff(A_BLINK);

        if (status_msg && status_msg[0]) {
            attron(COLOR_PAIR(PAIR_TEXT_DIM));
            printw("  (%s)", status_msg);
            attroff(COLOR_PAIR(PAIR_TEXT_DIM));
        }
    }
    start_y++;

    /* 4. Subtle Controls Hint */
    attron(COLOR_PAIR(PAIR_TEXT_DIM) | A_DIM);
    if (!in_god_mode && !won) {
        if (arrow_mode == ARROW_MODE_SLIDE_TILE) {
            mvprintw(start_y, start_x, "Arrows: move  |  Type GOD: solve  |  T: flip  |  Q: quit");
        } else {
            mvprintw(start_y, start_x, "Arrows: move hole  |  Type GOD: solve  |  T: flip  |  Q: quit");
        }
    }
    attroff(COLOR_PAIR(PAIR_TEXT_DIM) | A_DIM);

    refresh();
}

void ui_run_game(int initial_scramble_moves, bool start_in_god_mode) {
    initscr();
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    curs_set(0);
    timeout(50);
    init_colors_ui();

    Board board;
    board_scramble(&board, initial_scramble_moves);

    int arrow_mode = ARROW_MODE_SLIDE_TILE;
    char cmd_buf[64] = {0};
    int cmd_len = 0;
    char status_msg[128] = "";

    bool in_god_mode = false;
    int god_moves[MAX_SOLUTION_MOVES];
    int god_move_count = 0;
    int god_step = 0;
    long long last_god_move_time = 0;

    time_t start_time = time(NULL);
    bool game_running = true;

    if (start_in_god_mode) {
        if (solver_solve(&board, god_moves, MAX_SOLUTION_MOVES, &god_move_count)) {
            in_god_mode = true;
            god_step = 0;
            last_god_move_time = get_time_ms();
        }
    }

    while (game_running) {
        bool won = board_is_solved(&board);
        time_t now_sec = time(NULL);
        time_t elapsed = (now_sec >= start_time) ? (now_sec - start_time) : 0;

        /* Handle God Mode tick: exactly 1 move per 1000ms */
        if (in_god_mode) {
            long long current_ms = get_time_ms();
            if (current_ms - last_god_move_time >= 500) {
                if (god_step < god_move_count) {
                    int blank_dir = god_moves[god_step];
                    board_move_blank(&board, blank_dir);
                    god_step++;
                    last_god_move_time = current_ms;

                    if (board_is_solved(&board) || god_step >= god_move_count) {
                        in_god_mode = false;
                        won = true;
                        status_msg[0] = '\0';
                    }
                } else {
                    in_god_mode = false;
                }
            }
        }

        render_clean(&board, arrow_mode, cmd_buf, status_msg, in_god_mode, god_step, god_move_count,
                     elapsed, won);

        int ch = getch();
        if (ch == ERR) {
            continue;
        }
        if (ch == KEY_RESIZE) {
            continue;
        }

        /* Escape key */
        if (ch == 27) {
            if (in_god_mode) {
                in_god_mode = false;
                snprintf(status_msg, sizeof(status_msg), "God Mode stopped");
            } else if (cmd_len > 0) {
                cmd_len = 0;
                cmd_buf[0] = '\0';
                status_msg[0] = '\0';
            } else {
                game_running = false;
                break;
            }
            continue;
        }

        /* Arrow Keys: immediate tile swap with NO Enter required */
        if (ch == KEY_UP || ch == KEY_DOWN || ch == KEY_LEFT || ch == KEY_RIGHT) {
            if (in_god_mode) {
                in_god_mode = false;
                snprintf(status_msg, sizeof(status_msg), "God Mode stopped");
            }

            int moved_tile = -1;
            if (board_handle_arrow(&board, ch, arrow_mode, &moved_tile)) {
                status_msg[0] = '\0';
            }
            continue;
        }

        /* Enter Key: execute command in buffer */
        if (ch == '\n' || ch == '\r' || ch == 10) {
            int start = 0;
            while (start < cmd_len && isspace((unsigned char)cmd_buf[start])) start++;
            int end = cmd_len - 1;
            while (end >= start && isspace((unsigned char)cmd_buf[end])) end--;

            char command[64] = {0};
            if (end >= start) {
                int len = end - start + 1;
                strncpy(command, cmd_buf + start, len);
                command[len] = '\0';
            }

            cmd_len = 0;
            cmd_buf[0] = '\0';

            if (strcasecmp(command, "GOD") == 0) {
                if (board_is_solved(&board)) {
                    snprintf(status_msg, sizeof(status_msg), "Already solved");
                } else {
                    snprintf(status_msg, sizeof(status_msg), "Solving...");
                    render_clean(&board, arrow_mode, cmd_buf, status_msg, false, 0, 0, elapsed, won);

                    if (solver_solve(&board, god_moves, MAX_SOLUTION_MOVES, &god_move_count)) {
                        in_god_mode = true;
                        god_step = 0;
                        last_god_move_time = get_time_ms();
                        status_msg[0] = '\0';
                    } else {
                        snprintf(status_msg, sizeof(status_msg), "No solution");
                    }
                }
            } else if (strcasecmp(command, "RESET") == 0 || strcasecmp(command, "R") == 0) {
                board_scramble(&board, initial_scramble_moves);
                in_god_mode = false;
                start_time = time(NULL);
                status_msg[0] = '\0';
            } else if (strcasecmp(command, "QUIT") == 0 || strcasecmp(command, "Q") == 0 ||
                       strcasecmp(command, "EXIT") == 0) {
                game_running = false;
                break;
            } else if (strcasecmp(command, "TOGGLE") == 0 || strcasecmp(command, "T") == 0) {
                arrow_mode = (arrow_mode == ARROW_MODE_SLIDE_TILE) ? ARROW_MODE_MOVE_BLANK : ARROW_MODE_SLIDE_TILE;
                snprintf(status_msg, sizeof(status_msg), "Arrow control toggled");
            } else if (command[0] != '\0') {
                snprintf(status_msg, sizeof(status_msg), "Unknown '%s'", command);
            }
            continue;
        }

        /* Backspace */
        if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            if (cmd_len > 0) {
                cmd_buf[--cmd_len] = '\0';
            }
            continue;
        }

        /* Direct shortcut keys when buffer is empty */
        if (cmd_len == 0) {
            if (ch == 'q' || ch == 'Q') {
                game_running = false;
                break;
            }
            if (ch == 'r' || ch == 'R') {
                board_scramble(&board, initial_scramble_moves);
                in_god_mode = false;
                start_time = time(NULL);
                status_msg[0] = '\0';
                continue;
            }
            if (ch == 't' || ch == 'T') {
                arrow_mode = (arrow_mode == ARROW_MODE_SLIDE_TILE) ? ARROW_MODE_MOVE_BLANK : ARROW_MODE_SLIDE_TILE;
                snprintf(status_msg, sizeof(status_msg), "Arrow control toggled");
                continue;
            }
        }

        /* Add printable character to command buffer */
        if (isprint(ch) && cmd_len < (int)sizeof(cmd_buf) - 2) {
            cmd_buf[cmd_len++] = (char)ch;
            cmd_buf[cmd_len] = '\0';
            status_msg[0] = '\0';
        }
    }

    endwin();
}

/* Plain terminal printer for demo mode */
static void print_board_plain(const Board *b) {
#ifdef _WIN32
    bool is_term = _isatty(_fileno(stdout));
#else
    bool is_term = isatty(STDOUT_FILENO);
#endif

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            int v = b->tiles[r][c];
            if (v == 0) {
                printf("      ");
            } else {
                int target_v = r * BOARD_SIZE + c + 1;
                bool is_done = (v == target_v);

                if (is_term) {
                    if (is_done) {
                        printf("\033[42;97;1m  %2d  \033[0m", v);
                    } else {
                        printf("\033[44;97;1m  %2d  \033[0m", v);
                    }
                    if (c < BOARD_SIZE - 1) printf("  ");
                } else {
                    if (is_done) {
                        printf("[%2d]* ", v);
                    } else {
                        printf("[%2d]  ", v);
                    }
                }
            }
        }
        printf("\n\n");
    }
}

void ui_run_demo(int scramble_moves, int delay_ms) {
    Board b;
    board_scramble(&b, scramble_moves);

    printf("\n--- Initial Board ---\n\n");
    print_board_plain(&b);

    int moves[MAX_SOLUTION_MOVES];
    int move_count = 0;

    if (!solver_solve(&b, moves, MAX_SOLUTION_MOVES, &move_count)) {
        printf("Error: Solver failed!\n");
        return;
    }

    printf("Solving (%d moves)...\n", move_count);

    for (int i = 0; i < move_count; i++) {
        int dir = moves[i];
        int tile = solver_get_moved_tile(&b, dir);
        board_move_blank(&b, dir);

        printf("Move %d/%d: Tile [%2d] -> %s\n",
               i + 1, move_count, tile, DIR_NAMES[opposite_dir(dir)]);

        if (delay_ms > 0) {
            print_board_plain(&b);
            sleep_ms(delay_ms);
        }
    }

    printf("\n--- Solved Board ---\n\n");
    print_board_plain(&b);

    if (board_is_solved(&b)) {
        printf("Goal reached.\n\n");
    }
}
