#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <time.h>
#include <stdbool.h>

#if defined(_win32) || defined(_WIN32) || defined(_WIN64) || defined(__MINGW32__) || defined(__CYGWIN__)
#error This program currently does NOT support windows (and very likely never wikk)! \
    if you are thinking otherwise, please delete this line or add `undef(_WIN32)`..
int main() {}
#else
#ifndef __linux__
#warning this program is not tested for your operating systems, some bugs might occur...
#endif //__linux__

#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>

#define die(str)                                                               \
  do {                                                                         \
    printf("An unexpected error occured at %s:%d (%s): %s [ERRNO: %d]\n",      \
           __FILE__, __LINE__, str, strerror(errno), errno);                   \
    exit(1);                                                                   \
  } while (0);                                                                 \

void print(const char *str) {
    write(STDOUT_FILENO, str, strlen(str));
}

typedef struct termios termios_t;
termios_t orig_term;

void restore_term() {
    print("\033[?25h");
    if (tcsetattr(0, TCSAFLUSH, &orig_term) < 0) {
        die("restore_term / tcsetattr");
    }
}

void raw_mode() {
    if (tcgetattr(1, &orig_term) < 0) die("raw_mode / tcgetattr");
    atexit(restore_term);

    termios_t raw = orig_term;
    raw.c_lflag &= ~(IXON | ICRNL);
    raw.c_lflag &= ~(ECHO | ICANON | ISIG | IEXTEN);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;

    print("\033[?25l");
    
    if (tcsetattr(0, TCSAFLUSH, &raw) < 0) die("raw_mode / tcsetattr");
}

int termsize(int *cols, int *rows) {
    struct winsize ws;
    if (ioctl(1, TIOCGWINSZ, &ws) == -1 || ws.ws_col == 0) {
        return -1;
    } else {
        *rows = ws.ws_row;
        *cols = ws.ws_col;
        return 0;
    }
}

typedef enum {
    EMPTY = 0,
    HEAD,
    TAIL,
    FOOD
} state;

typedef struct {
    state *cells;
    size_t size;
    size_t cols;
} grid;

#define ADD(buf, index, str)                                                   \
  do {                                                                         \
    memcpy(buf + index, str, strlen(str));                                     \
    index += strlen(str);                                                      \
  } while (0);

void render(grid grid, int x, int y) {
    char buf[grid.size * 8];
    size_t buf_pos = 0;
    ADD(buf, buf_pos, "\033[H\033[2J\033[3J");
    for (size_t i = 0; i < grid.size; i++) {
        switch (grid.cells[i]) {
        case EMPTY:
            ADD(buf, buf_pos, "\033[0m ");
            break;
        case HEAD:
            ADD(buf, buf_pos, "\033[43m ");
            break;
        case TAIL:
            ADD(buf, buf_pos, "\033[42m ");
            break;
        case FOOD:
            ADD(buf, buf_pos, "\033[41m ");
            break;
        }

        if (grid.cols == 0) {
            errno = EINVAL;
            die("render: cols should not be 0");
        }
    }

    // reset cursor
    ADD(buf, buf_pos, "\033[H");

    int xlen = snprintf(NULL, 0, "%d", x);
    int ylen = snprintf(NULL, 0, "%d", y);

    char *xstr = (char*)malloc((xlen+1) * sizeof(char));
    char *ystr = (char*)malloc((ylen+1) * sizeof(char));

    snprintf(xstr, xlen+1, "%d", x);
    snprintf(ystr, ylen+1, "%d", y);

    free(xstr);
    free(ystr);

    write(STDOUT_FILENO, buf, buf_pos);
}

bool *clone(bool *other, size_t len) {
    bool *new_arr = (bool*)malloc(len * sizeof(bool));
    for (size_t i = 0; i < len; i++) {
        new_arr[i] = other[i];
    }
    return new_arr;
}

typedef enum {
    UP,
    DOWN,
    LEFT,
    RIGHT
} direction_t;

typedef struct {
    int x, y;
} pos;

void step(grid *grid, pos *snake, size_t *snake_len, direction_t dir, pos *food) {
    for (size_t i = 0; i < *snake_len-1; i++) {
        snake[i] = snake[i+1];
    }

    switch (dir) {
    case UP:
        if (snake[*snake_len-1].y > 0) {
            snake[*snake_len-1].y--;
        } else {
            restore_term();
            print("\033[H\033[2J\033[3J");
            print("You lose!\n");
            exit(0);
        }
        break;
    case DOWN:
        if (snake[*snake_len-1].y < (int)(grid->size / grid->cols)) {
            snake[*snake_len-1].y++;
        } else {
            restore_term();
            print("\033[H\033[2J\033[3J");
            print("You lose!\n");
            exit(0);
        }
        break;
    case LEFT:
        if (snake[*snake_len-1].x > 0) {
            snake[*snake_len-1].x--;
        } else {
            restore_term();
            print("\033[H\033[2J\033[3J");
            print("You lose!\n");
            exit(0);
        }
        break;
    case RIGHT:
        if (snake[*snake_len-1].x < (int)grid->cols) {
            snake[*snake_len-1].x++;
        } else {
            restore_term();
            print("\033[H\033[2J\033[3J");
            print("You lose!\n");
            exit(0);
        }
        break;
    }

    pos head = snake[*snake_len-1];
    if (grid->cells[head.x + head.y * grid->cols] == TAIL) {
        restore_term();
        print("\033[H\033[2J\033[3J");
        print("You lose!\n");
        exit(0);
    }

    // clear grid
    memset(grid->cells, 0, grid->size * sizeof(state));

    for (size_t i = 0; i < *snake_len-1; i++) {
        pos p = snake[i];
        grid->cells[p.x + p.y * grid->cols] = TAIL;
    }
    pos h = snake[*snake_len-1];
    grid->cells[h.x + h.y * grid->cols] = HEAD;

    grid->cells[food->x + food->y * grid->cols] = FOOD;

    if (h.x == food->x && h.y == food->y) {
        *snake_len += 4;
        for (int i = *snake_len-5; i >= 0; i--) {
            snake[i+4] = snake[i];
        }
        snake[3] = snake[4];
        snake[2] = snake[3];
        snake[1] = snake[2];
        snake[0] = snake[1];


        *food = (pos) {
            (int)(random() % grid->cols),
            (int)(random() % (grid->size / grid->cols)),
        };
    }
}

#define DELAY (30 * 1000)

int main() {
    srand(time(NULL));
    raw_mode();

    bool esc_pre = false, esc = false;
    int x = 1, y = 1;

    int w, h;
    termsize(&w, &h);
    
    grid grid = {
        .cells = (state*)malloc((w*h)*sizeof(state)),
        .size = (size_t)(w*h),
        .cols = (size_t)w,
    };

    direction_t dir = RIGHT;

    pos food = {
        (int)(random() % grid.cols),
        (int)(random() % (grid.size / grid.cols))
    };
    
    size_t len = 4;
    pos snake[1000] = {0};
    snake[0] = (pos){0, 0};
    snake[1] = (pos){1, 0};
    snake[2] = (pos){2, 0};
    snake[3] = (pos){3, 0};

    for (;;) {
        step(&grid, snake, &len, dir, &food);
        render(grid, x, y);
        char c = '\0';
        if (read(1, &c, 1) < 0) die("main / read");
        if (c == 'q') {
            printf("\033[H\033[2J");
            exit(0);
            break;
        }

        switch (c) {
        case 27:
            esc_pre = true;
            break;
        case '[':
            esc = esc_pre;
            break;
        case 'A':
            if (esc) dir = UP;
            esc = false;
            break;
        case 'B':
            if (esc) dir = DOWN;
            esc = false;
            break;
        case 'C':
            if (esc) dir = RIGHT;
            esc = false;
            break;
        case 'D':
            if (esc) dir = LEFT;
            esc = false;
            break;
        default:
            esc = esc_pre = false;
            break;
        }
        usleep(DELAY);
    }
    return 0;
}

#endif //!defined(_WIN32)
