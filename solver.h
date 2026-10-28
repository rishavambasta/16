#ifndef SOLVER_H
#define SOLVER_H

#include "board.h"
#include <stdbool.h>

#define MAX_SOLUTION_MOVES 4096

/* Solve the puzzle from any state.
   out_moves: array to receive blank movement directions (BLANK_UP, BLANK_DOWN, etc.)
   max_moves: maximum capacity of out_moves array
   out_len: receives the final number of moves in solution
   Returns true on success, false if unsolvable or buffer exceeded. */
bool solver_solve(const Board *b, int out_moves[], int max_moves, int *out_len);

/* Helper to optimize move path by removing adjacent inverses */
int solver_optimize_path(int path[], int len);

/* Helper to get which tile number is moved by a given blank move */
int solver_get_moved_tile(const Board *b, int blank_dir);

#endif /* SOLVER_H */
