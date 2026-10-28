#ifndef UI_H
#define UI_H

#include "board.h"
#include <stdbool.h>

/* Run the interactive CLI game */
void ui_run_game(int initial_scramble_moves, bool start_in_god_mode);

/* Run a headless demo of God Mode solving the puzzle */
void ui_run_demo(int scramble_moves, int delay_ms);

#endif /* UI_H */
