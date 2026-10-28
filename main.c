#include "board.h"
#include "solver.h"
#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static void print_usage(const char *prog) {
    printf("THE GAME OF 16 - CLI Sliding 15-Puzzle\n");
    printf("======================================\n");
    printf("Usage:\n");
    printf("  %s                   Start interactive game\n", prog);
    printf("  %s --god             Start game directly in God Mode\n", prog);
    printf("  %s --scramble <N>    Scramble with N random moves (default: 60)\n", prog);
    printf("  %s --demo [N] [ms]   Run non-interactive CLI demo of God Mode\n", prog);
    printf("  %s --test            Run test suite (solvability, solver verification)\n", prog);
    printf("  %s --help            Display this help message\n", prog);
    printf("\nControls in Game:\n");
    printf("  [Arrow Keys]         Swap adjacent tile with empty space (immediate move)\n");
    printf("  Type GOD + [Enter]   Activate God Mode (solves game in 1 tile/second)\n");
    printf("  [T]                  Toggle arrow control mode (tile slide vs hole move)\n");
    printf("  [R]                  New game (scramble tiles)\n");
    printf("  [Q / Esc]            Quit game\n");
}

static int run_tests(void) {
    printf("=== RUNNING TEST SUITE FOR GAME OF 16 ===\n\n");

    /* 1. Test Solved Board */
    Board b;
    board_init(&b);
    if (!board_is_solved(&b)) {
        printf("[FAIL] Newly initialized board is not solved!\n");
        return 1;
    }
    if (!board_is_solvable(&b)) {
        printf("[FAIL] Solved board reported as unsolvable!\n");
        return 1;
    }
    printf("[PASS] Board initialization & solved state check.\n");

    /* 2. Test Arrow / Blank Moves */
    // In solved state, blank is at (3,3). Moving blank UP moves tile 12 (at 2,3) down into (3,3).
    if (!board_move_blank(&b, BLANK_UP)) {
        printf("[FAIL] Failed to move blank up from (3,3)!\n");
        return 1;
    }
    if (board_is_solved(&b)) {
        printf("[FAIL] Board still solved after moving blank!\n");
        return 1;
    }
    if (b.blank_r != 2 || b.blank_c != 3) {
        printf("[FAIL] Blank position incorrect after move!\n");
        return 1;
    }
    board_move_blank(&b, BLANK_DOWN); // restore
    if (!board_is_solved(&b)) {
        printf("[FAIL] Restored board is not solved!\n");
        return 1;
    }
    printf("[PASS] Move execution & coordinate tracking.\n");

    /* 3. Test Scramble Solvability */
    for (int i = 0; i < 20; i++) {
        board_scramble(&b, 50 + i * 5);
        if (!board_is_solvable(&b)) {
            printf("[FAIL] Scrambled board is unsolvable!\n");
            return 1;
        }
        if (board_is_solved(&b)) {
            printf("[FAIL] Scrambled board is already solved!\n");
            return 1;
        }
    }
    printf("[PASS] Scramble generator produces 100%% solvable jumbled boards.\n");

    /* 4. Test Solver on 50 Random Configurations */
    printf("Testing solver on 50 random boards...\n");
    clock_t start = clock();
    int total_moves = 0;
    int max_moves = 0;

    for (int t = 0; t < 50; t++) {
        board_scramble(&b, 200);
        Board test_copy = b;

        int moves[MAX_SOLUTION_MOVES];
        int move_count = 0;
        if (!solver_solve(&test_copy, moves, MAX_SOLUTION_MOVES, &move_count)) {
            printf("[FAIL] Solver returned false on test %d!\n", t);
            return 1;
        }

        /* Replay moves on test_copy */
        for (int m = 0; m < move_count; m++) {
            if (!board_move_blank(&test_copy, moves[m])) {
                printf("[FAIL] Move %d (%s) invalid on test %d!\n", m, DIR_NAMES[moves[m]], t);
                return 1;
            }
        }

        if (!board_is_solved(&test_copy)) {
            printf("[FAIL] Board not solved after replaying solver path on test %d!\n", t);
            return 1;
        }

        total_moves += move_count;
        if (move_count > max_moves) max_moves = move_count;
    }
    double elapsed = (double)(clock() - start) / CLOCKS_PER_SEC;

    printf("[PASS] 50/50 boards solved successfully in %.4f seconds (avg %.2f ms/solve)!\n",
           elapsed, (elapsed / 50.0) * 1000.0);
    printf("       Stats: avg moves = %d, max moves = %d.\n", total_moves / 50, max_moves);

    /* 5. Test Path Optimizer */
    int test_path[] = {BLANK_UP, BLANK_DOWN, BLANK_LEFT, BLANK_RIGHT, BLANK_UP};
    int opt_len = solver_optimize_path(test_path, 5);
    if (opt_len != 1 || test_path[0] != BLANK_UP) {
        printf("[FAIL] Path optimizer failed to reduce 5 moves to 1!\n");
        return 1;
    }
    printf("[PASS] Path optimizer cancels adjacent inverses correctly.\n");

    printf("\n>>> ALL TESTS PASSED! <<<\n");
    return 0;
}

int main(int argc, char *argv[]) {
    srand((unsigned int)time(NULL));

    int scramble_moves = 60;
    bool start_in_god_mode = false;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_usage(argv[0]);
            return 0;
        } else if (strcmp(argv[i], "--test") == 0) {
            return run_tests();
        } else if (strcmp(argv[i], "--god") == 0) {
            start_in_god_mode = true;
        } else if (strcmp(argv[i], "--scramble") == 0 && i + 1 < argc) {
            scramble_moves = atoi(argv[++i]);
            if (scramble_moves < 5) scramble_moves = 5;
        } else if (strcmp(argv[i], "--demo") == 0) {
            int moves = 30;
            int delay = 0;
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                moves = atoi(argv[++i]);
            }
            if (i + 1 < argc && argv[i + 1][0] != '-') {
                delay = atoi(argv[++i]);
            }
            ui_run_demo(moves, delay);
            return 0;
        } else {
            fprintf(stderr, "Unknown option: %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }

    ui_run_game(scramble_moves, start_in_god_mode);
    return 0;
}
