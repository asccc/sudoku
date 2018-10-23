
/** 
 * To the extent possible under law, the author(s) have dedicated 
 * all copyright and related and neighboring rights to this software
 * to the public domain worldwide. This software is distributed 
 * without any warranty.
 * 
 * You should have received a copy of the CC0 Public Domain Dedication 
 * along with this software. 
 * 
 * If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
 */

#include <stdlib.h> /* exit, malloc, free */
#include <stdio.h> /* stdin, feof, fgetc */
#include <stdbool.h> /* bool, true false */
#include <string.h> /* memcpy */
#include <pthread.h> /* pthread ... */

#define NOINDEX (9*9)+1
#define SFAILURE 0
#define SSUCCESS 1

/**
 * program options
 */
struct sopts {
  /* use threads */
  bool threads;
  /* use fancy output-format */
  bool fancyof;
  /* show help */
  bool help;
};

/**
 * error handler function ;)
 */
#define whops(...) do {                  \
  fprintf(stderr, __VA_ARGS__);          \
  fputs("\nprogram aborted!\n", stderr); \
  exit(1);                               \
} while (0)

/**
 * checks if the given number can be placed
 * in the given row and column
 *
 * @param  grid the sudoku grid
 * @param  num  the number to be placed
 * @param  row  the row index
 * @param  col  the column index
 * @return      true if the number can be placed, false otherwise
 */
static bool check_number (
  unsigned grid[], 
  unsigned num, 
  unsigned row, 
  unsigned col
) {
  if (!grid) {
    whops("NULL passed to %s", __func__);
  }
  /* calculate region */
  const unsigned rx = row / 3 * 3;
  const unsigned ry = col / 3 * 3;
  for (unsigned i = 0; i < 9; ++i) {
    /* check row and column */
    if (
      num == grid[(row * 9) + i] ||
      num == grid[col + (i * 9)]
    ) {
      /* number already set in row/column */
      return false;
    }
    /* check region */
    unsigned reg = 
      (rx + (i / 3)) * 9 + 
      (ry + (i % 3));
    if (num == grid[reg]) {
      /* number already set in 3*3 region */
      return false;
    }
  }
  return true;
}

/**
 * calculates a score based on seen numbers
 * in the given row and column (and group)
 *
 * @param  grid the sudoku grid
 * @param  row  the row
 * @param  col  column
 * @return      the score
 */
signed calc_score (
  unsigned grid[],
  unsigned row,
  unsigned col
) {
  unsigned seen[10] = {0};
  /* calculate region */
  const unsigned rx = row / 3 * 3;
  const unsigned ry = col / 3 * 3;
  for (unsigned i = 0; i < 9; ++i) {
    unsigned rnm = grid[(row * 9) + i];
    unsigned cnm = grid[col + (i * 9)];
    unsigned reg = 
      (rx + (i / 3)) * 9 +
      (ry + (i % 3));
    unsigned gnm = grid[reg];
    seen[rnm] = 1;
    seen[cnm] = 1;
    seen[gnm] = 1;
  }
  /* calculate score */
  signed scr = 0;
  for (unsigned i = 1; i < 10; ++i) {
    scr += seen[i];
  }
  return scr;
}

/**
 * returns a index in the grid with the least possibilities
 *
 * @param  grid the sudoku grid
 * @return      the index or 81 if no index was found
 */
unsigned find_best_index (
  unsigned grid[]
) {
  unsigned idx = NOINDEX;
  /* score benchmark, the higher the better */
  signed csc = 0;
  signed psc = -1;
  unsigned row;
  unsigned col;
  for (row = 0; row < 9; ++row) {
    for (col = 0; col < 9; ++col) {
      if (grid[row * 9 + col] == 0) {
        csc = calc_score(grid, row, col);
        if (csc > psc) {
          psc = csc;
          idx = row * 9 + col;
        }
      }
    }
  }
  return idx;
}

/**
 * tries to find a solution for the given puzzle.
 * nothing fancy, just a simple/stupid brute-force
 *
 * single threaded
 *
 * @param  grid the sudoku grid
 * @param  idx  the index in the grid to be checked
 * @return      true if a solution was found, false otherwise
 */
static bool find_solution_st (
  unsigned grid[]
) {
  if (!grid) {
    whops("NULL passed to %s", __func__);
  }

  /* find the first empty slot 
    with least possibilities */
  unsigned idx;
  idx = find_best_index(grid);

  if (idx == NOINDEX) {
    /* no empty slot found */
    return true;
  }

  /* slot is empty, start brute-force */
  const unsigned row = idx / 9;
  const unsigned col = idx % 9;
  /* this loop is manually unrolled */
  /* normally this would be a for-loop
    incrementing num from 1 to 9 */
  #define UNROLLED_CHECK(num)                \
    if (check_number(grid, num, row, col)) { \
      grid[idx] = num;                       \
      if (find_solution_st(grid)) { \
        return true;                         \
      }                                      \
      grid[idx] = 0;                         \
    }
  UNROLLED_CHECK(1);
  UNROLLED_CHECK(2);
  UNROLLED_CHECK(3);
  UNROLLED_CHECK(4);
  UNROLLED_CHECK(5);
  UNROLLED_CHECK(6);
  UNROLLED_CHECK(7);
  UNROLLED_CHECK(8);
  UNROLLED_CHECK(9);
  #undef UNROLLED_CHECK
  /* no solution found */
  return false;
}

/**
 * callback for pthread
 *
 * @param slot
 */
static void * find_solution_th (void *pass) 
{
  unsigned *slot = pass;
  unsigned *stat = slot; /* status slot */
  unsigned *grid = slot + 1;
  /* default status is FAILURE */
  *stat = SFAILURE;
  /* use the single-thread solver */
  if (find_solution_st(grid)) {
    /* solution was found */
    *stat = SSUCCESS;
  }
  /* pass everything back */
  pthread_exit(pass);
  return 0;
}

/**
 * starts the single-threaded solver in a thread
 *
 * @param pt   the thread handle
 * @param grid the grid memory for this thread
 * @param slot the memory slot for this thread
 * @param idx  the index in the grid we're at
 * @param num  the number to be tested
 * @return     the memory slot
 */
static void solv_fork (
  pthread_t *const pt,
  unsigned grid[],
  unsigned *slot,
  unsigned idx,
  unsigned num
) {
  /* first slot is for the success/failure status */
  slot[0] = SFAILURE;
  memcpy(slot + 1, grid, sizeof(unsigned)*9*9);

  /* fill in the number to test */
  slot[idx + 1] = num;

  /* fork off! */
  pthread_create(pt, 0, find_solution_th, slot);
}

/**
 * joins the given thread back
 *
 * @param  pt   the pthread handle
 * @param  grid the grid memory
 * @param  copy true if the result should be copied 
 * @return      true if a solution was found
 */
static bool solv_join (
  pthread_t *const pt,
  unsigned grid[],
  const bool copy
) {
  unsigned *slot;
  if (copy) {
    /* join thread */
    pthread_join(*pt, (void**)&slot);
    if (*slot == SSUCCESS) {
      /* copy solution */
      memcpy(grid, slot + 1, sizeof(unsigned)*9*9);
      return true;
    }
    /* no solution */
    return false;
  }
  /* cancel thread, result no longer needed */
  pthread_cancel(*pt);
  pthread_join(*pt, 0);
  return false;
}

/**
 * starts the brute-force in n-threads
 *
 * @see find_solution_st
 *
 * @param  grid the sudoku grid
 * @param  idx  the index in the grid to be checked
 * @return      true if a thread came back with a solution, false otherwise
 */
static bool find_solution_mt (
  unsigned grid[]
) {
  /* keep things simple, stupid */
  /* one thread for each possible number */
  pthread_t pool[9] = {0};
  unsigned *smem[9] = {0};
  unsigned pidx = 0;

  /* status flag */
  bool sfnd = false;

  /* find the first empty slot 
    with least possibilities */
  unsigned idx;
  idx = find_best_index(grid);

  if (idx == NOINDEX) {
    /* no empty slot found */
    return true;
  }

  /* we found a empty slot */
  const unsigned row = idx / 9;
  const unsigned col = idx % 9;
  /* start one thread for each possible number */
  for (unsigned num = 1; num <= 9; ++num) {
    if (check_number(grid, num, row, col)) {
      pthread_t *thrd = &pool[pidx];
      /* in order to keep valgrind happy ... 
        we use calloc instead of malloc */
      smem[pidx] = calloc(9*9+1, sizeof(unsigned));
      if (!smem[pidx]) {
        whops("out of memory in %s", __func__);
      }
      /* start thread */
      solv_fork(thrd, grid, smem[pidx], idx, num);
      /* next thread */
      pidx += 1;
    }
  }
  /* join threads and check if one came 
    back with a solution */
  for (unsigned pi = 0; pi < pidx; ++pi) {
    pthread_t *thrd = &pool[pi];
    if (solv_join(thrd, grid, !sfnd)) {
      /* solution found! */
      sfnd = true;
    }
    /* cleanup memory */
    free(smem[pi]);
  }
  /* stop here */
  return sfnd;
}

/**
 * sudoku solver entrypoint
 *
 * @param  grid the sudoku grid
 * @param  use_threads whenever to use threaded or not
 * @return      true if a complete solution was found, false otherwise
 */
static inline bool solve_puzzle (
  unsigned grid[],
  bool use_threads
) {
  if (!grid) {
    whops("NULL passed to %s", __func__);
  }
  /* start brute-force */
  if (use_threads) {
    /* multi-threaded */
    return find_solution_mt(grid);
  }
  /* single threaded */
  return find_solution_st(grid);
}

/**
 * fancy output
 *
 * @param grid the sudoku grid
 * @param out
 */
static void print_puzzle_fancy (
  unsigned grid[],
  FILE *out
) {
  #if defined(_WIN32)
    /* windows does not like UTF8 stuff in the console */
    /* UTF8 is (somewhat) supported, but ... linux is superior! */
    unsigned col = 0;
    fputs("+---+---+---+---+---+---+---+---+---+\n", out);
    for (unsigned idx = 0; idx < (9*9); ++idx) {
      if (!grid[idx]) {
        /* empty slot (0) */
        fputs("|   ", out);
      } else {
        /* filled slot */
        fprintf(out, "| %u ", grid[idx]);
      }
      if (col++ == 8) {
        /* next row */
        fputs("|\n+---+---+---+---+---+---+---+---+---+\n", out);
        col = 0;
      }
    }
  #else
    /* use fancy UTF8 "blocks" */
    unsigned row = 0;
    unsigned col = 0;
    fputs("┏━━━┯━━━┯━━━┳━━━┯━━━┯━━━┳━━━┯━━━┯━━━┓\n", out);
    for (unsigned idx = 0; idx < (9*9); ++idx) {
      if (!grid[idx]) {
        fprintf(out, "%s   ", col % 3 ? "│" : "┃");
      } else {
        fprintf(out, "%s %u ", col % 3 ? "│" : "┃", grid[idx]);
      }
      if (col++ > 7) {
        fputs("┃", stdout);
        if (row++ < 8) {
          if (row % 3) {
            fputs("\n┠───┼───┼───╂───┼───┼───╂───┼───┼───┨\n", out);
          } else {
            fputs("\n┣━━━┿━━━┿━━━╋━━━┿━━━┿━━━╋━━━┿━━━┿━━━┫\n", out);
          }
        } else {
          fputs("\n", out);
        }
        col = 0;
      }
    }
    fputs("┗━━━┷━━━┷━━━┻━━━┷━━━┷━━━┻━━━┷━━━┷━━━┛\n", out);
  #endif
}

/**
 * (pretty) prints the sudoku grid to the given
 * output file-handle
 *
 * @param grid the sudoku grid
 * @param fancy print the puzzle side-by-side
 * @param out  output-file
 */
static void print_puzzle (
  unsigned grid[], 
  FILE *out,
  bool fancy
) {
  if (!grid) {
    whops("NULL passed to %s", __func__);
  }

  if (!fancy) {
    /* simple output format used by @German */
    unsigned col = 0;
    for (unsigned idx = 0; idx < (9*9); ++idx) {
      fprintf(stdout, "%u", grid[idx]);
      if (col++ > 7) {
        fputs("\n", out);
        col = 0;
      }
    }
  } else {
    print_puzzle_fancy(grid, out);
  }

  fputs("\n", out);
}

/**
 * reads the input grid
 *
 * @param grid
 * @param inp
 */
static void read_puzzle_input (
  unsigned grid[],
  FILE *inp
) {
  /* for error reporting */
  unsigned rows[9][9] = {{0}};
  unsigned cols[9][9] = {{0}};
  bool grps[9][9] = {{false}};
  unsigned col = 0;
  unsigned row = 0;

  for (unsigned idx = 0; idx < (9 * 9); ++idx) {
    char chr = fgetc(inp);
    if (chr == EOF) {
      whops(
        "premature end of input in row %u and column %u",
        row + 1, col + 1
      );
    }
    if (chr != ' ') {
      if (chr < '1' || chr > '9') {
        /* out of bounds */
        whops(
          "invalid value `%c` (%i) in row %u and column %u",
          chr, chr, row + 1, col + 1
        );
      }
      /* get unsigned number from character */
      unsigned val = chr - '0';
      unsigned off = val - 1;
      /* check if value is unique in current row */
      if (rows[row][off]) {
        whops(
          "duplicate value %u in row %u (column %u)"
          " - value already seen in column %u",
          val, row + 1, col + 1,
          rows[row][off] + 1
        );
      }
      /* check if value is unique in current column */
      if (cols[col][off]) {
        whops(
          "duplicate value %u in column %u (row %u)"
          " - value already seen in row %u",
          val, col + 1, row + 1,
          cols[col][off] + 1
        );
      }
      /* check if value is unique in current 3*3 group */
      unsigned grp = row / 3 * 3 + col / 3;
      if (grps[grp][off]) {
        whops(
          "duplicate value %u in group %u "
          "(row %u and column %u)",
          val, grp + 1, row + 1, col + 1
        );
      }
      /* store given information */
      grid[idx] = val;
      rows[row][off] = col;
      cols[col][off] = row;
      grps[grp][off] = true;
    }
    if (col++ == 8) {
      /* line is complete */
      chr = fgetc(inp);
      if (chr != '\n') {
        whops(
          "unexpected input `%c` (%i) at index %u",
          chr, chr, idx
        );
      }
      col = 0;
      row += 1;
    }
  }
}

/**
 * parses program options
 *
 * @param opts option bucket
 * @param argc program argument count
 * @param argv program argument values
 */
void parse_sopts (
  struct sopts *opts,
  int argc,
  char *argv[]
) {
  opts->threads = true;
  opts->fancyof = false;
  opts->help = false;

  if (argc == 1) {
    /* no options passed */
    return;
  }

  for (unsigned i = 1; i < argc; ++i) {
    if (strcmp(argv[i], "-s") == 0) {
      opts->threads = false;
      continue;
    }
    if (strcmp(argv[i], "-f") == 0) {
      opts->fancyof = true;
      continue;
    }
    if (strcmp(argv[i], "-h") == 0 ||
        strcmp(argv[i], "-?") == 0) {
      opts->help = true;
      continue;
    }
  }
}

/**
 * prints the usage-help
 *
 */
static void print_usage ()
{
  puts("usage:");
  puts("\t./ssud [-s] [-f] [-h] input");
  puts("\noptions:");
  puts("\t-s\tenable single-threaded mode");
  puts("\t-f\tenable fancy output-format (UTF8 blocks on linux)");
  puts("\t-h\tshows this help");
  puts("");
}

/**
 * main entry point
 *
 */
int main (int argc, char *argv[])
{
  /* handle options */
  struct sopts opts;
  parse_sopts(&opts, argc, argv);

  /* help wanted! */
  if (opts.help) {
    print_usage();
    return 0;
  }

  /* read grid */
  unsigned grid[(9 * 9)] = {0};
  read_puzzle_input(grid, stdin);

  if (opts.fancyof) {
    /* print input grid */
    print_puzzle(grid, stdout, true);
  }

  if (solve_puzzle(grid, opts.threads)) {
    /* puzzle was solved, print output grid */
    print_puzzle(grid, stdout, opts.fancyof);
  } else {
    fputs("no solution\n", stdout);
  }

  return 0;
}
