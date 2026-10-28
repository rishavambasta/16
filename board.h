#ifndef BOARD_H
#define BOARD_H

#include <stdbool.h>

#define BOARD_SIZE 4
#define TOTAL_TILES 16

/* Direction indices for blank movement */
#define BLANK_UP    0
#define BLANK_DOWN  1
#define BLANK_LEFT  2
#define BLANK_RIGHT 3

/* Arrow control modes */
#define ARROW_MODE_SLIDE_TILE 0  /* Arrow specifies direction tile moves into blank */
#define ARROW_MODE_MOVE_BLANK 1  /* Arrow specifies direction blank moves */

typedef struct {
    int tiles[BOARD_SIZE][BOARD_SIZE];
    int blank_r;
    int blank_c;
    int move_count;
} Board;

/* Initialize board to solved state */
void board_init(Board *b);

/* Check if the board is in solved state (1..15, blank 0 at bottom-right) */
bool board_is_solved(const Board *b);

/* Count inversions and verify if state is mathematically solvable */
bool board_is_solvable(const Board *b);

/* Scramble board with random valid moves, guaranteeing it's solvable and not already solved */
void board_scramble(Board *b, int scramble_moves);

/* Move blank in specified direction (BLANK_UP, BLANK_DOWN, BLANK_LEFT, BLANK_RIGHT) */
bool board_move_blank(Board *b, int dir);

/* Move tile using arrow key based on control mode.
   Returns true if valid move was made.
   Optionally sets *out_tile_val to the tile number that moved. */
bool board_handle_arrow(Board *b, int arrow_key, int mode, int *out_tile_val);

/* Find coordinates of value in board */
void board_find(const Board *b, int val, int *r, int *c);

/* Get opposite blank direction */
static inline int opposite_dir(int dir) {
    return dir ^ 1;
}

/* Direction delta tables */
extern const int DIR_DR[4];
extern const int DIR_DC[4];
extern const char *DIR_NAMES[4];

/* Standard arrow key codes */
#ifndef KEY_UP
#define KEY_UP    0403
#define KEY_DOWN  0402
#define KEY_LEFT  0404
#define KEY_RIGHT 0405
#endif

#endif /* BOARD_H */
