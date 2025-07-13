#include <ncurses.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH 80
#define HEIGHT 25

#define INITIAL_DELAY 200
#define MIN_DELAY 50
#define MAX_DELAY 1000
#define DELAY_STEP 50

#define CMD_NONE 0
#define CMD_FASTER 1
#define CMD_SLOWER 2
#define CMD_PAUSE 3
#define CMD_RESTART 4
#define CMD_EXIT 99

#define MUTATION_INTERVAL 50
#define MUTATION_RATE 5

#define CELL_DEAD 0
#define CELL_ALIVE 1

#define SYMBOL_ALIVE 'O'
#define SYMBOL_NEW '@'
#define SYMBOL_DEAD '.'

typedef struct {
  int alive;
  int age;
  int mutated;
} Cell;

static int count_alive_cells(Cell field[HEIGHT][WIDTH]);
static void generate_random_field(Cell field[HEIGHT][WIDTH],
                                  int density_percent);
static void update_field_with_age(Cell field[HEIGHT][WIDTH],
                                  Cell next_field[HEIGHT][WIDTH]);
static void draw_field_with_age(Cell field[HEIGHT][WIDTH], int delay_ms,
                                int generation, bool is_paused,
                                bool system_stabilized);
static bool fields_are_equal(Cell field1[HEIGHT][WIDTH],
                             Cell field2[HEIGHT][WIDTH]);
static void apply_mutations(Cell field[HEIGHT][WIDTH], int mutation_rate);

static void load_initial_state(FILE *input, Cell field[HEIGHT][WIDTH]) {
  char line[WIDTH + 2];

  for (int y = 0; y < HEIGHT; y++) {
    if (fgets(line, sizeof(line), input) != NULL) {
      for (int x = 0; x < WIDTH; x++) {
        if (line[x] == '1') {
          field[y][x].alive = CELL_ALIVE;
          field[y][x].age = 1;
          field[y][x].mutated = 0;
        } else {
          field[y][x].alive = CELL_DEAD;
          field[y][x].age = 0;
          field[y][x].mutated = 0;
        }
      }
    } else {

      for (int x = 0; x < WIDTH; x++) {
        field[y][x].alive = CELL_DEAD;
        field[y][x].age = 0;
        field[y][x].mutated = 0;
      }
    }
  }
}

static int count_neighbors(Cell field[HEIGHT][WIDTH], int y, int x) {
  int count = 0;

  for (int dy = -1; dy <= 1; dy++) {
    for (int dx = -1; dx <= 1; dx++) {

      if (dy == 0 && dx == 0) {
        continue;
      }

      int ny = y + dy;
      int nx = x + dx;

      if (ny >= 0 && ny < HEIGHT && nx >= 0 && nx < WIDTH) {
        if (field[ny][nx].alive == CELL_ALIVE) {
          count++;
        }
      }
    }
  }

  return count;
}

static void update_field_with_age(Cell field[HEIGHT][WIDTH],
                                  Cell next_field[HEIGHT][WIDTH]) {
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      int neighbors = count_neighbors(field, y, x);

      if (field[y][x].alive == CELL_ALIVE) {

        if (neighbors == 2 || neighbors == 3) {
          next_field[y][x].alive = CELL_ALIVE;
          next_field[y][x].age = field[y][x].age + 1;
          next_field[y][x].mutated = 0;
        } else {
          next_field[y][x].alive = CELL_DEAD;
          next_field[y][x].age = 0;
          next_field[y][x].mutated = 0;
        }
      } else {

        if (neighbors == 3) {
          next_field[y][x].alive = CELL_ALIVE;
          next_field[y][x].age = 0;
          next_field[y][x].mutated = 0;
        } else {
          next_field[y][x].alive = CELL_DEAD;
          next_field[y][x].age = 0;
          next_field[y][x].mutated = 0;
        }
      }
    }
  }

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      field[y][x].alive = next_field[y][x].alive;
      field[y][x].age = next_field[y][x].age;
      field[y][x].mutated = next_field[y][x].mutated;
    }
  }
}

static void init_screen() {
  initscr();
  start_color();
  use_default_colors();
  init_pair(1, COLOR_GREEN, -1);
  init_pair(2, COLOR_YELLOW, -1);
  init_pair(3, COLOR_BLUE, -1);
  init_pair(4, COLOR_RED, -1);
  cbreak();
  noecho();
  keypad(stdscr, TRUE);
  timeout(100);
}

static void draw_field_with_age(Cell field[HEIGHT][WIDTH], int delay_ms,
                                int generation, bool is_paused,
                                bool system_stabilized) {
  clear();
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      if (field[y][x].mutated == 1 && field[y][x].alive == CELL_ALIVE) {
        attron(COLOR_PAIR(4));
        mvaddch(y, x, '*');
        attroff(COLOR_PAIR(4));
      } else if (field[y][x].alive == CELL_ALIVE) {
        if (field[y][x].age == 0) {

          attron(COLOR_PAIR(2));
          mvaddch(y, x, SYMBOL_NEW);
          attroff(COLOR_PAIR(2));
        } else {

          attron(COLOR_PAIR(1));
          mvaddch(y, x, SYMBOL_ALIVE);
          attroff(COLOR_PAIR(1));
        }
      } else {

        attron(COLOR_PAIR(3));
        mvaddch(y, x, SYMBOL_DEAD);
        attroff(COLOR_PAIR(3));
      }
    }
  }

  mvprintw(HEIGHT, 0,
           "Controls: +/- - speed, P - pause, R - restart, Space - exit");
  mvprintw(HEIGHT + 1, 0, "Generation: %d | Alive: %d | Delay: %dms%s",
           generation, count_alive_cells(field), delay_ms,
           is_paused ? " | PAUSED" : "");
  if (system_stabilized) {
    mvprintw(HEIGHT + 2, 0, "СТОП: система стабилизировалась");
  }
  refresh();
}

static int process_input() {
  int ch = getch();
  if (ch == ERR) {
    return CMD_NONE;
  }
  if (ch == '+' || ch == '=') {
    return CMD_FASTER;
  } else if (ch == '-') {
    return CMD_SLOWER;
  } else if (ch == ' ') {
    return CMD_EXIT;
  } else if (ch == 'p' || ch == 'P') {
    return CMD_PAUSE;
  } else if (ch == 'r' || ch == 'R') {
    return CMD_RESTART;
  }

  return CMD_NONE;
}

static void close_screen() { endwin(); }

static void generate_random_field(Cell field[HEIGHT][WIDTH],
                                  int density_percent) {

  srand(time(NULL));

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {

      int random_value = rand() % 100;

      if (random_value < density_percent) {
        field[y][x].alive = CELL_ALIVE;
        field[y][x].age = 1;
        field[y][x].mutated = 0;
      } else {
        field[y][x].alive = CELL_DEAD;
        field[y][x].age = 0;
        field[y][x].mutated = 0;
      }
    }
  }
}

static int count_alive_cells(Cell field[HEIGHT][WIDTH]) {
  int count = 0;
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      if (field[y][x].alive == CELL_ALIVE) {
        count++;
      }
    }
  }
  return count;
}

static bool fields_are_equal(Cell field1[HEIGHT][WIDTH],
                             Cell field2[HEIGHT][WIDTH]) {
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      if (field1[y][x].alive != field2[y][x].alive) {
        return false;
      }
    }
  }
  return true;
}

static void apply_mutations(Cell field[HEIGHT][WIDTH], int mutation_rate) {
  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {

      if (rand() % 100 < mutation_rate) {

        field[y][x].alive =
            (field[y][x].alive == CELL_ALIVE) ? CELL_DEAD : CELL_ALIVE;
        field[y][x].age = 0;
        field[y][x].mutated = 1;
      }
    }
  }
}

int main(int argc, char *const argv[]) {
  Cell field[HEIGHT][WIDTH];
  Cell next_field[HEIGHT][WIDTH];
  Cell previous_field[HEIGHT][WIDTH];
  int delay_ms = INITIAL_DELAY;
  int generation = 0;
  bool is_paused = false;
  bool use_random = false;
  bool system_stabilized = false;
  int density_percent = 25;

  if (argc < 2) {
    use_random = true;
  } else {
    FILE *input = fopen(argv[1], "r");
    if (!input) {
      use_random = true;
    } else {
      load_initial_state(input, field);
      fclose(input);
    }
  }
  if (use_random) {
    if (argc >= 3) {
      density_percent = atoi(argv[2]);
      if (density_percent < 0 || density_percent > 100) {
        density_percent = 25;
      }
    }
    generate_random_field(field, density_percent);
  }

  init_screen();

  for (int y = 0; y < HEIGHT; y++) {
    for (int x = 0; x < WIDTH; x++) {
      previous_field[y][x].alive = field[y][x].alive;
      previous_field[y][x].age = field[y][x].age;
      previous_field[y][x].mutated = field[y][x].mutated;
    }
  }

  draw_field_with_age(field, delay_ms, generation, is_paused,
                      system_stabilized);

  while (1) {
    if (!is_paused && !system_stabilized) {
      napms(delay_ms);
    } else {
      napms(100);
    }

    int command = process_input();
    if (command == CMD_FASTER && delay_ms > MIN_DELAY) {
      delay_ms -= DELAY_STEP;
    } else if (command == CMD_SLOWER && delay_ms < MAX_DELAY) {
      delay_ms += DELAY_STEP;
    } else if (command == CMD_PAUSE) {
      is_paused = !is_paused;
    } else if (command == CMD_RESTART) {

      generate_random_field(field, density_percent);
      generation = 0;
      is_paused = false;
      system_stabilized = false;

      for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
          previous_field[y][x] = field[y][x];
    } else if (command == CMD_EXIT) {
      break;
    }

    if (!is_paused && !system_stabilized) {

      for (int y = 0; y < HEIGHT; y++)
        for (int x = 0; x < WIDTH; x++)
          previous_field[y][x] = field[y][x];

      update_field_with_age(field, next_field);
      generation++;

      if (generation > 0 && generation % MUTATION_INTERVAL == 0) {
        apply_mutations(field, MUTATION_RATE);
      }

      if (fields_are_equal(field, previous_field)) {
        system_stabilized = true;
      }
    }

    draw_field_with_age(field, delay_ms, generation, is_paused,
                        system_stabilized);
  }

  close_screen();

  return 0;
}