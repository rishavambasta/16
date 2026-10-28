#include "board.h"
#include <stdlib.h>
#include <string.h>
#include <time.h>

const int DIR_DR[4] = {-1, 1, 0, 0};
const int DIR_DC[4] = {0, 0, -1, 1};
const char *DIR_NAMES[4] = {"UP", "DOWN", "LEFT", "RIGHT"};

void board_init(Board *b) {
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            b->tiles[r][c] = r * BOARD_SIZE + c + 1;
        }
    }
    b->tiles[BOARD_SIZE - 1][BOARD_SIZE - 1] = 0;
    b->blank_r = BOARD_SIZE - 1;
    b->blank_c = BOARD_SIZE - 1;
    b->move_count = 0;
}

bool board_is_solved(const Board *b) {
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            if (r == BOARD_SIZE - 1 && c == BOARD_SIZE - 1) {
                if (b->tiles[r][c] != 0) return false;
            } else {
                if (b->tiles[r][c] != r * BOARD_SIZE + c + 1) return false;
            }
        }
    }
    return true;
}

bool board_is_solvable(const Board *b) {
    int arr[TOTAL_TILES];
    int len = 0;
    int blank_row = -1;

    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            int val = b->tiles[r][c];
            if (val == 0) {
                blank_row = r;
            } else {
                arr[len++] = val;
            }
        }
    }

    int inversions = 0;
    for (int i = 0; i < len; i++) {
        for (int j = i + 1; j < len; j++) {
            if (arr[i] > arr[j]) inversions++;
        }
    }

    /* Solvability formula for 4x4 grid:
       (inversions + blank_row_from_bottom) must be odd.
       blank_row_from_bottom = BOARD_SIZE - blank_row. */
    int blank_from_bottom = BOARD_SIZE - blank_row;
    return (inversions + blank_from_bottom) % 2 == 1;
}

void board_find(const Board *b, int val, int *r, int *c) {
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (b->tiles[i][j] == val) {
                *r = i;
                *c = j;
                return;
            }
        }
    }
    *r = -1;
    *c = -1;
}

bool board_move_blank(Board *b, int dir) {
    if (dir < 0 || dir >= 4) return false;
    int nr = b->blank_r + DIR_DR[dir];
    int nc = b->blank_c + DIR_DC[dir];

    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) {
        return false;
    }

    b->tiles[b->blank_r][b->blank_c] = b->tiles[nr][nc];
    b->tiles[nr][nc] = 0;
    b->blank_r = nr;
    b->blank_c = nc;
    b->move_count++;
    return true;
}

bool board_handle_arrow(Board *b, int arrow_key, int mode, int *out_tile_val) {
    int blank_dir = -1;

    if (mode == ARROW_MODE_SLIDE_TILE) {
        /* Arrow indicates which way the adjacent tile moves into the blank */
        switch (arrow_key) {
            case KEY_UP:    blank_dir = BLANK_DOWN;  break; /* Tile below moves UP */
            case KEY_DOWN:  blank_dir = BLANK_UP;    break; /* Tile above moves DOWN */
            case KEY_LEFT:  blank_dir = BLANK_RIGHT; break; /* Tile on right moves LEFT */
            case KEY_RIGHT: blank_dir = BLANK_LEFT;  break; /* Tile on left moves RIGHT */
            default: return false;
        }
    } else {
        /* Arrow indicates which way the blank space moves */
        switch (arrow_key) {
            case KEY_UP:    blank_dir = BLANK_UP;    break;
            case KEY_DOWN:  blank_dir = BLANK_DOWN;  break;
            case KEY_LEFT:  blank_dir = BLANK_LEFT;  break;
            case KEY_RIGHT: blank_dir = BLANK_RIGHT; break;
            default: return false;
        }
    }

    int nr = b->blank_r + DIR_DR[blank_dir];
    int nc = b->blank_c + DIR_DC[blank_dir];

    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) {
        return false;
    }

    if (out_tile_val) {
        *out_tile_val = b->tiles[nr][nc];
    }

    return board_move_blank(b, blank_dir);
}

void board_scramble(Board *b, int scramble_moves) {
    board_init(b);

    int prev_dir = -1;
    int valid_moves = 0;

    while (valid_moves < scramble_moves || board_is_solved(b)) {
        int dir = rand() % 4;
        /* Avoid immediately undoing the previous move */
        if (prev_dir != -1 && dir == opposite_dir(prev_dir)) {
            continue;
        }
        if (board_move_blank(b, dir)) {
            prev_dir = dir;
            valid_moves++;
        }
    }

    b->move_count = 0; /* Reset move counter for the user */
}
