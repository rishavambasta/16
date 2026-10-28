#include "solver.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Target coordinates for each tile (1..15, and 0 at bottom right) */
static int target_r[16];
static int target_c[16];
static bool targets_initialized = false;

static void init_targets(void) {
    if (targets_initialized) return;
    for (int i = 1; i <= 15; i++) {
        target_r[i] = (i - 1) / BOARD_SIZE;
        target_c[i] = (i - 1) % BOARD_SIZE;
    }
    target_r[0] = BOARD_SIZE - 1;
    target_c[0] = BOARD_SIZE - 1;
    targets_initialized = true;
}

/* Linear conflict + Manhattan distance heuristic for IDA* */
static int heuristic(int b[BOARD_SIZE][BOARD_SIZE]) {
    int h = 0;
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            int v = b[r][c];
            if (v != 0) {
                h += abs(r - target_r[v]) + abs(c - target_c[v]);
            }
        }
    }

    /* Linear conflicts in rows */
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c1 = 0; c1 < BOARD_SIZE; c1++) {
            int v1 = b[r][c1];
            if (v1 != 0 && target_r[v1] == r) {
                for (int c2 = c1 + 1; c2 < BOARD_SIZE; c2++) {
                    int v2 = b[r][c2];
                    if (v2 != 0 && target_r[v2] == r) {
                        if (target_c[v1] > target_c[v2]) h += 2;
                    }
                }
            }
        }
    }

    /* Linear conflicts in columns */
    for (int c = 0; c < BOARD_SIZE; c++) {
        for (int r1 = 0; r1 < BOARD_SIZE; r1++) {
            int v1 = b[r1][c];
            if (v1 != 0 && target_c[v1] == c) {
                for (int r2 = r1 + 1; r2 < BOARD_SIZE; r2++) {
                    int v2 = b[r2][c];
                    if (v2 != 0 && target_c[v2] == c) {
                        if (target_r[v1] > target_r[v2]) h += 2;
                    }
                }
            }
        }
    }
    return h;
}

/* IDA* state and search */
static int ida_path[128];
static int ida_nodes = 0;
static const int IDA_MAX_NODES = 80000;

static int ida_dfs(int b[BOARD_SIZE][BOARD_SIZE], int zr, int zc, int g, int threshold, int prev_move) {
    ida_nodes++;
    if (ida_nodes > IDA_MAX_NODES) return -2;

    int h = heuristic(b);
    int f = g + h;
    if (f > threshold) return f;
    if (h == 0) return -1; /* Solved */

    int min_over = 9999;
    for (int m = 0; m < 4; m++) {
        if (prev_move != -1 && (m ^ 1) == prev_move) continue; /* No immediate backtracking */
        int nr = zr + DIR_DR[m];
        int nc = zc + DIR_DC[m];
        if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) continue;

        b[zr][zc] = b[nr][nc];
        b[nr][nc] = 0;
        ida_path[g] = m;

        int t = ida_dfs(b, nr, nc, g + 1, threshold, m);
        b[nr][nc] = b[zr][zc];
        b[zr][zc] = 0;

        if (t == -1) return -1;
        if (t == -2) return -2;
        if (t < min_over) min_over = t;
    }
    return min_over;
}

static bool solve_ida(int start_b[BOARD_SIZE][BOARD_SIZE], int out_path[], int *out_len) {
    int b[BOARD_SIZE][BOARD_SIZE];
    memcpy(b, start_b, sizeof(b));
    int zr = -1, zc = -1;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (b[i][j] == 0) { zr = i; zc = j; }
        }
    }

    ida_nodes = 0;
    int threshold = heuristic(b);
    if (threshold == 0) {
        *out_len = 0;
        return true;
    }

    /* Limit IDA* to search depth 24 to keep response time < 5ms */
    while (threshold <= 24) {
        int t = ida_dfs(b, zr, zc, 0, threshold, -1);
        if (t == -1) {
            *out_len = threshold;
            memcpy(out_path, ida_path, threshold * sizeof(int));
            return true;
        }
        if (t == -2 || t > 24) break;
        threshold = t;
    }
    return false;
}

/* Deterministic multi-stage solver */
static int det_board[BOARD_SIZE][BOARD_SIZE];
static int det_locked[BOARD_SIZE][BOARD_SIZE];
static int det_history[MAX_SOLUTION_MOVES];
static int det_history_len = 0;

static void det_find_val(int val, int *r, int *c) {
    *r = 0;
    *c = 0;
    for (int i = 0; i < BOARD_SIZE; i++) {
        for (int j = 0; j < BOARD_SIZE; j++) {
            if (det_board[i][j] == val) {
                *r = i;
                *c = j;
                return;
            }
        }
    }
}

static bool det_do_move(int m) {
    int zr = 0, zc = 0;
    det_find_val(0, &zr, &zc);
    int nr = zr + DIR_DR[m];
    int nc = zc + DIR_DC[m];
    if (nr < 0 || nr >= BOARD_SIZE || nc < 0 || nc >= BOARD_SIZE) return false;
    det_board[zr][zc] = det_board[nr][nc];
    det_board[nr][nc] = 0;
    if (det_history_len < MAX_SOLUTION_MOVES) {
        det_history[det_history_len++] = m;
    }
    return true;
}

/* 1-Tile BFS solver */
static bool det_solve_1tile(int val, int tr, int tc) {
    int vr = 0, vc = 0, zr = 0, zc = 0;
    det_find_val(val, &vr, &vc);
    det_find_val(0, &zr, &zc);

    int start_t = vr * BOARD_SIZE + vc;
    int start_z = zr * BOARD_SIZE + zc;
    int target_t = tr * BOARD_SIZE + tc;

    if (start_t == target_t) return true;

    int parent_state[16][16];
    int parent_move[16][16];
    memset(parent_state, -1, sizeof(parent_state));

    int q_t[300], q_z[300];
    int head = 0, tail = 0;

    q_t[tail] = start_t;
    q_z[tail] = start_z;
    parent_state[start_t][start_z] = start_t * 16 + start_z;
    tail++;

    int end_t = -1, end_z = -1;

    while (head < tail) {
        int ct = q_t[head];
        int cz = q_z[head];
        head++;

        if (ct == target_t) {
            end_t = ct;
            end_z = cz;
            break;
        }

        int z_r = cz / BOARD_SIZE, z_c = cz % BOARD_SIZE;

        for (int m = 0; m < 4; m++) {
            int nz_r = z_r + DIR_DR[m];
            int nz_c = z_c + DIR_DC[m];
            if (nz_r < 0 || nz_r >= BOARD_SIZE || nz_c < 0 || nz_c >= BOARD_SIZE) continue;
            if (det_locked[nz_r][nz_c]) continue;

            int nz = nz_r * BOARD_SIZE + nz_c;
            int nt = ct;
            if (nz == ct) nt = cz;

            if (parent_state[nt][nz] == -1) {
                parent_state[nt][nz] = ct * 16 + cz;
                parent_move[nt][nz] = m;
                q_t[tail] = nt;
                q_z[tail] = nz;
                tail++;
            }
        }
    }

    if (end_t == -1) return false;

    int path[500];
    int plen = 0;
    int curr_t = end_t, curr_z = end_z;
    while (!(curr_t == start_t && curr_z == start_z)) {
        int p = parent_state[curr_t][curr_z];
        int m = parent_move[curr_t][curr_z];
        path[plen++] = m;
        curr_t = p / 16;
        curr_z = p % 16;
    }

    for (int i = plen - 1; i >= 0; i--) {
        det_do_move(path[i]);
    }
    return true;
}

/* 2-Tile BFS solver */
static bool det_solve_2tiles(int val1, int tr1, int tc1, int val2, int tr2, int tc2) {
    int v1r = 0, v1c = 0, v2r = 0, v2c = 0, zr = 0, zc = 0;
    det_find_val(val1, &v1r, &v1c);
    det_find_val(val2, &v2r, &v2c);
    det_find_val(0, &zr, &zc);

    int start_t1 = v1r * BOARD_SIZE + v1c;
    int start_t2 = v2r * BOARD_SIZE + v2c;
    int start_z = zr * BOARD_SIZE + zc;

    int target_t1 = tr1 * BOARD_SIZE + tc1;
    int target_t2 = tr2 * BOARD_SIZE + tc2;

    if (start_t1 == target_t1 && start_t2 == target_t2) return true;

    static int parent_state[16][16][16];
    static int parent_move[16][16][16];
    memset(parent_state, -1, sizeof(parent_state));

    static int q_t1[4500], q_t2[4500], q_z[4500];
    int head = 0, tail = 0;

    q_t1[tail] = start_t1;
    q_t2[tail] = start_t2;
    q_z[tail] = start_z;
    parent_state[start_t1][start_t2][start_z] = (start_t1 * 16 + start_t2) * 16 + start_z;
    tail++;

    int end_t1 = -1, end_t2 = -1, end_z = -1;

    while (head < tail) {
        int ct1 = q_t1[head];
        int ct2 = q_t2[head];
        int cz = q_z[head];
        head++;

        if (ct1 == target_t1 && ct2 == target_t2) {
            end_t1 = ct1;
            end_t2 = ct2;
            end_z = cz;
            break;
        }

        int z_r = cz / BOARD_SIZE, z_c = cz % BOARD_SIZE;

        for (int m = 0; m < 4; m++) {
            int nz_r = z_r + DIR_DR[m];
            int nz_c = z_c + DIR_DC[m];
            if (nz_r < 0 || nz_r >= BOARD_SIZE || nz_c < 0 || nz_c >= BOARD_SIZE) continue;
            if (det_locked[nz_r][nz_c]) continue;

            int nz = nz_r * BOARD_SIZE + nz_c;
            int nt1 = ct1;
            int nt2 = ct2;
            if (nz == ct1) nt1 = cz;
            else if (nz == ct2) nt2 = cz;

            if (parent_state[nt1][nt2][nz] == -1) {
                parent_state[nt1][nt2][nz] = (ct1 * 16 + ct2) * 16 + cz;
                parent_move[nt1][nt2][nz] = m;
                q_t1[tail] = nt1;
                q_t2[tail] = nt2;
                q_z[tail] = nz;
                tail++;
            }
        }
    }

    if (end_t1 == -1) return false;

    int path[4500];
    int plen = 0;
    int curr_t1 = end_t1, curr_t2 = end_t2, curr_z = end_z;
    while (!(curr_t1 == start_t1 && curr_t2 == start_t2 && curr_z == start_z)) {
        int p = parent_state[curr_t1][curr_t2][curr_z];
        int m = parent_move[curr_t1][curr_t2][curr_z];
        path[plen++] = m;
        curr_z = p % 16;
        p /= 16;
        curr_t2 = p % 16;
        curr_t1 = p / 16;
    }

    for (int i = plen - 1; i >= 0; i--) {
        det_do_move(path[i]);
    }
    return true;
}

/* 3-Tile BFS solver for final 2x2 grid */
static bool det_solve_final_2x2(void) {
    int v1r = 0, v1c = 0, v2r = 0, v2c = 0, v3r = 0, v3c = 0, zr = 0, zc = 0;
    det_find_val(11, &v1r, &v1c);
    det_find_val(12, &v2r, &v2c);
    det_find_val(15, &v3r, &v3c);
    det_find_val(0, &zr, &zc);

    int start_t1 = v1r * BOARD_SIZE + v1c;
    int start_t2 = v2r * BOARD_SIZE + v2c;
    int start_t3 = v3r * BOARD_SIZE + v3c;
    int start_z = zr * BOARD_SIZE + zc;

    int target_t1 = 2 * BOARD_SIZE + 2;
    int target_t2 = 2 * BOARD_SIZE + 3;
    int target_t3 = 3 * BOARD_SIZE + 2;
    int target_z = 3 * BOARD_SIZE + 3;

    if (start_t1 == target_t1 && start_t2 == target_t2 && start_t3 == target_t3 && start_z == target_z) {
        return true;
    }

    static int parent_state[16][16][16][16];
    static int parent_move[16][16][16][16];
    memset(parent_state, -1, sizeof(parent_state));

    int q_t1[100], q_t2[100], q_t3[100], q_z[100];
    int head = 0, tail = 0;

    q_t1[tail] = start_t1;
    q_t2[tail] = start_t2;
    q_t3[tail] = start_t3;
    q_z[tail] = start_z;
    parent_state[start_t1][start_t2][start_t3][start_z] = 1;
    tail++;

    bool found = false;

    while (head < tail) {
        int ct1 = q_t1[head];
        int ct2 = q_t2[head];
        int ct3 = q_t3[head];
        int cz = q_z[head];
        head++;

        if (ct1 == target_t1 && ct2 == target_t2 && ct3 == target_t3 && cz == target_z) {
            found = true;
            break;
        }

        int z_r = cz / BOARD_SIZE, z_c = cz % BOARD_SIZE;

        for (int m = 0; m < 4; m++) {
            int nz_r = z_r + DIR_DR[m];
            int nz_c = z_c + DIR_DC[m];
            if (nz_r < 2 || nz_r >= BOARD_SIZE || nz_c < 2 || nz_c >= BOARD_SIZE) continue;

            int nz = nz_r * BOARD_SIZE + nz_c;
            int nt1 = ct1, nt2 = ct2, nt3 = ct3;
            if (nz == ct1) nt1 = cz;
            else if (nz == ct2) nt2 = cz;
            else if (nz == ct3) nt3 = cz;

            if (parent_state[nt1][nt2][nt3][nz] == -1) {
                parent_state[nt1][nt2][nt3][nz] = (ct1 << 12) | (ct2 << 8) | (ct3 << 4) | cz;
                parent_move[nt1][nt2][nt3][nz] = m;
                q_t1[tail] = nt1;
                q_t2[tail] = nt2;
                q_t3[tail] = nt3;
                q_z[tail] = nz;
                tail++;
            }
        }
    }

    if (!found) return false;

    int path[100];
    int plen = 0;
    int curr_t1 = target_t1, curr_t2 = target_t2, curr_t3 = target_t3, curr_z = target_z;
    while (!(curr_t1 == start_t1 && curr_t2 == start_t2 && curr_t3 == start_t3 && curr_z == start_z)) {
        int p = parent_state[curr_t1][curr_t2][curr_t3][curr_z];
        int m = parent_move[curr_t1][curr_t2][curr_t3][curr_z];
        path[plen++] = m;
        curr_z = p & 0xF;
        curr_t3 = (p >> 4) & 0xF;
        curr_t2 = (p >> 8) & 0xF;
        curr_t1 = (p >> 12) & 0xF;
    }

    for (int i = plen - 1; i >= 0; i--) {
        det_do_move(path[i]);
    }
    return true;
}

static bool solve_deterministic(int in_board[BOARD_SIZE][BOARD_SIZE], int out_path[], int *out_len) {
    memcpy(det_board, in_board, sizeof(det_board));
    memset(det_locked, 0, sizeof(det_locked));
    det_history_len = 0;

    /* Phase 1: Row 0 */
    if (!det_solve_1tile(1, 0, 0)) return false;
    det_locked[0][0] = 1;
    if (!det_solve_1tile(2, 0, 1)) return false;
    det_locked[0][1] = 1;
    if (!det_solve_2tiles(3, 0, 2, 4, 0, 3)) return false;
    det_locked[0][2] = 1;
    det_locked[0][3] = 1;

    /* Phase 2: Row 1 */
    if (!det_solve_1tile(5, 1, 0)) return false;
    det_locked[1][0] = 1;
    if (!det_solve_1tile(6, 1, 1)) return false;
    det_locked[1][1] = 1;
    if (!det_solve_2tiles(7, 1, 2, 8, 1, 3)) return false;
    det_locked[1][2] = 1;
    det_locked[1][3] = 1;

    /* Phase 3: Column 0 */
    if (!det_solve_2tiles(9, 2, 0, 13, 3, 0)) return false;
    det_locked[2][0] = 1;
    det_locked[3][0] = 1;

    /* Phase 4: Column 1 */
    if (!det_solve_2tiles(10, 2, 1, 14, 3, 1)) return false;
    det_locked[2][1] = 1;
    det_locked[3][1] = 1;

    /* Phase 5: Final 2x2 */
    if (!det_solve_final_2x2()) return false;

    *out_len = det_history_len;
    memcpy(out_path, det_history, det_history_len * sizeof(int));
    return true;
}

int solver_optimize_path(int path[], int len) {
    if (len <= 0) return 0;
    int top = 0;
    int *stack = (int *)malloc((len + 1) * sizeof(int));
    if (!stack) return len;

    for (int i = 0; i < len; i++) {
        if (top > 0 && (stack[top - 1] ^ 1) == path[i]) {
            top--;
        } else {
            stack[top++] = path[i];
        }
    }
    for (int i = 0; i < top; i++) {
        path[i] = stack[i];
    }
    free(stack);
    return top;
}

int solver_get_moved_tile(const Board *b, int blank_dir) {
    int nr = b->blank_r + DIR_DR[blank_dir];
    int nc = b->blank_c + DIR_DC[blank_dir];
    if (nr >= 0 && nr < BOARD_SIZE && nc >= 0 && nc < BOARD_SIZE) {
        return b->tiles[nr][nc];
    }
    return -1;
}

bool solver_solve(const Board *b, int out_moves[], int max_moves, int *out_len) {
    init_targets();

    if (!board_is_solvable(b)) {
        return false;
    }

    if (board_is_solved(b)) {
        *out_len = 0;
        return true;
    }

    int raw_path[MAX_SOLUTION_MOVES];
    int raw_len = 0;

    int copy_b[BOARD_SIZE][BOARD_SIZE];
    for (int r = 0; r < BOARD_SIZE; r++) {
        for (int c = 0; c < BOARD_SIZE; c++) {
            copy_b[r][c] = b->tiles[r][c];
        }
    }

    /* 1. Try IDA* first for quick optimal path on shallow scrambles */
    bool solved = solve_ida(copy_b, raw_path, &raw_len);

    /* 2. Fall back to deterministic multi-stage solver */
    if (!solved) {
        solved = solve_deterministic(copy_b, raw_path, &raw_len);
    }

    if (!solved) return false;

    /* Optimize path */
    int opt_len = solver_optimize_path(raw_path, raw_len);
    if (opt_len > max_moves) return false;

    *out_len = opt_len;
    memcpy(out_moves, raw_path, opt_len * sizeof(int));
    return true;
}
