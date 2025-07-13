#include <ncurses.h>
#include <stdio.h>
#include <stdbool.h>
#include <stdlib.h>
#include <time.h>

#define WIDTH 80
#define HEIGHT 25

#define INITIAL_DELAY 200
#define MIN_DELAY 50
#define MAX_DELAY 1000
#define DELAY_STEP 50

// Команды управления
#define CMD_NONE 0
#define CMD_FASTER 1
#define CMD_SLOWER 2
#define CMD_PAUSE 3
#define CMD_RESTART 4
#define CMD_EXIT 99

// Настройки мутаций и автоостановки
#define MUTATION_INTERVAL 50  // Мутации каждые N поколений
#define MUTATION_RATE 5       // Процент клеток для мутации (0-100)

// Состояния клеток
#define CELL_DEAD 0
#define CELL_ALIVE 1

// Символы для отображения
#define SYMBOL_ALIVE 'O'  // Живая клетка
#define SYMBOL_NEW '@'    // Новая клетка (только что ожила)
#define SYMBOL_DEAD '.'   // Мертвая клетка (точка вместо пробела)



// Структура для хранения состояния клетки с возрастом
typedef struct {
    int alive;
    int age;  // Возраст клетки (0 = только что родилась)
    int mutated; // 1 если клетка мутировала, 0 иначе
} Cell;

// Прототипы функций
int count_alive_cells(Cell field[HEIGHT][WIDTH]);
void generate_random_field(Cell field[HEIGHT][WIDTH], int density_percent);
void update_field_with_age(Cell field[HEIGHT][WIDTH], Cell next_field[HEIGHT][WIDTH]);
void draw_field_with_age(Cell field[HEIGHT][WIDTH], int delay_ms, int generation, bool is_paused, bool system_stabilized);
bool fields_are_equal(Cell field1[HEIGHT][WIDTH], Cell field2[HEIGHT][WIDTH]);
void apply_mutations(Cell field[HEIGHT][WIDTH], int mutation_rate);


/**
 * Загружает начальное состояние поля из файла
 * @param input - файл с паттерном (1 - живая клетка, 0 или другой символ - мертвая)
 * @param field - массив для хранения состояния поля
 */
void load_initial_state(FILE *input, Cell field[HEIGHT][WIDTH])
{
    char line[WIDTH + 2]; // строка + '\n' + '\0'

    for (int y = 0; y < HEIGHT; y++)
    {
        if (fgets(line, sizeof(line), input) != NULL)
        {
            for (int x = 0; x < WIDTH; x++)
            {
                if (line[x] == '1')
                {
                    field[y][x].alive = CELL_ALIVE;
                    field[y][x].age = 1; // Начальный возраст
                    field[y][x].mutated = 0; // Обнуляем mutated при загрузке
                }
                else
                {
                    field[y][x].alive = CELL_DEAD;
                    field[y][x].age = 0;
                    field[y][x].mutated = 0; // Обнуляем mutated при загрузке
                }
            }
        }
        else
        {
            // если файл кончился → добиваем строку мёртвыми клетками
            for (int x = 0; x < WIDTH; x++)
            {
                field[y][x].alive = CELL_DEAD;
                field[y][x].age = 0;
                field[y][x].mutated = 0; // Обнуляем mutated при загрузке
            }
        }
    }
}

/**
 * Подсчитывает количество живых соседей вокруг клетки (максимум 8)
 * @param field - текущее состояние поля
 * @param y, x - координаты клетки
 * @return количество живых соседей
 */
int count_neighbors(Cell field[HEIGHT][WIDTH], int y, int x)
{
    int count = 0;

    for (int dy = -1; dy <= 1; dy++)
    {
        for (int dx = -1; dx <= 1; dx++)
        {
            // пропускаем саму клетку
            if (dy == 0 && dx == 0)
            {
                continue;
            }

            int ny = y + dy;
            int nx = x + dx;

            // проверяем что коорды внутри поля
            if (ny >= 0 && ny < HEIGHT && nx >= 0 && nx < WIDTH)
            {
                if (field[ny][nx].alive == CELL_ALIVE)
                {
                    count++;
                }
            }
        }
    }

    return count;
}

/**
 * Обновляет состояние поля согласно правилам игры "Жизнь" с учетом возраста
 * @param field - текущее состояние поля
 * @param next_field - массив для следующего состояния
 */
void update_field_with_age(Cell field[HEIGHT][WIDTH], Cell next_field[HEIGHT][WIDTH])
{
    for (int y = 0; y < HEIGHT; y++)
    { // по каждой строчке
        for (int x = 0; x < WIDTH; x++)
        {
            int neighbors = count_neighbors(field, y, x);

            if (field[y][x].alive == CELL_ALIVE)
            {
                // Правила для живой клетки: 2 или 3 соседа = остается живой
                if (neighbors == 2 || neighbors == 3)
                {
                    next_field[y][x].alive = CELL_ALIVE;
                    next_field[y][x].age = field[y][x].age + 1; // Увеличиваем возраст
                    next_field[y][x].mutated = 0; // Обнуляем mutated при обновлении
                }
                else
                {
                    next_field[y][x].alive = CELL_DEAD;
                    next_field[y][x].age = 0;
                    next_field[y][x].mutated = 0; // Обнуляем mutated при обновлении
                }
            }
            else
            {
                // Правила для мертвой клетки: ровно 3 соседа = оживает
                if (neighbors == 3)
                {
                    next_field[y][x].alive = CELL_ALIVE;
                    next_field[y][x].age = 0; // Новая клетка
                    next_field[y][x].mutated = 0; // Обнуляем mutated при обновлении
                }
                else
                {
                    next_field[y][x].alive = CELL_DEAD;
                    next_field[y][x].age = 0;
                    next_field[y][x].mutated = 0; // Обнуляем mutated при обновлении
                }
            }
        }
    }

    // копируем в next_field обратно в field
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            field[y][x].alive = next_field[y][x].alive;
            field[y][x].age = next_field[y][x].age;
            field[y][x].mutated = next_field[y][x].mutated; // Копируем mutated
        }
    }
}

void init_screen()
{
    initscr();
    start_color();
    use_default_colors();
    init_pair(1, COLOR_GREEN, -1);      // Зеленый для старых клеток
    init_pair(2, COLOR_YELLOW, -1);     // Желтый для новых клеток
    init_pair(3, COLOR_BLUE, -1);       // Синий для мертвых клеток
    init_pair(4, COLOR_RED, -1); // Красный для мутированных клеток
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(100);
}



void draw_field_with_age(Cell field[HEIGHT][WIDTH], int delay_ms, int generation, bool is_paused, bool system_stabilized)
{
    clear();
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            if (field[y][x].mutated == 1 && field[y][x].alive == CELL_ALIVE) {
                attron(COLOR_PAIR(4));
                mvaddch(y, x, '*');
                attroff(COLOR_PAIR(4));
            } else if (field[y][x].alive == CELL_ALIVE)
            {
                if (field[y][x].age == 0)
                {
                    // Новая клетка - желтый цвет
                    attron(COLOR_PAIR(2));
                    mvaddch(y, x, SYMBOL_NEW);
                    attroff(COLOR_PAIR(2));
                }
                else
                {
                    // Старая клетка - зеленый цвет
                    attron(COLOR_PAIR(1));
                    mvaddch(y, x, SYMBOL_ALIVE);
                    attroff(COLOR_PAIR(1));
                }
            }
            else
            {
                // Мертвая клетка - синий цвет
                attron(COLOR_PAIR(3));
                mvaddch(y, x, SYMBOL_DEAD);
                attroff(COLOR_PAIR(3));
            }
        }
    }
    // Печать подсказки и статистики
    mvprintw(HEIGHT, 0, "Controls: +/- - speed, P - pause, R - restart, Space - exit");
    mvprintw(HEIGHT + 1, 0, "Generation: %d | Alive: %d | Delay: %dms%s", 
             generation, count_alive_cells(field), delay_ms, 
             is_paused ? " | PAUSED" : "");
    if (system_stabilized) {
        mvprintw(HEIGHT + 2, 0, "СТОП: система стабилизировалась");
    }
    refresh();
}

int process_input()
{
    int ch = getch();
    if (ch == ERR)
    {
        return CMD_NONE; // ничего не нажато - продолжаем
    }
    if (ch == '+' || ch == '=')
    {
        return CMD_FASTER; // ускорить
    }
    else if (ch == '-')
    {
        return CMD_SLOWER; // замедлить
    }
    else if (ch == ' ')
    {
        return CMD_EXIT; // выход
    }
    else if (ch == 'p' || ch == 'P') {
        return CMD_PAUSE; // пауза
    }
    else if (ch == 'r' || ch == 'R') {
        return CMD_RESTART; // рестарт
    }

    return CMD_NONE;
}

void close_screen() { 
    endwin(); 
}

/**
 * Генерирует случайное начальное состояние поля
 * @param field - массив для хранения состояния поля
 * @param density_percent - процент живых клеток (0-100)
 */
void generate_random_field(Cell field[HEIGHT][WIDTH], int density_percent)
{
    // Инициализируем генератор случайных чисел текущим временем
    srand(time(NULL));
    
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            // Генерируем случайное число от 0 до 99
            int random_value = rand() % 100;
            
            // Если случайное число меньше density_percent, то клетка живая
            if (random_value < density_percent)
            {
                field[y][x].alive = CELL_ALIVE;
                field[y][x].age = 1; // Начальный возраст
                field[y][x].mutated = 0; // Обнуляем mutated при генерации
            }
            else
            {
                field[y][x].alive = CELL_DEAD;
                field[y][x].age = 0;
                field[y][x].mutated = 0; // Обнуляем mutated при генерации
            }
        }
    }
}

// Функция для подсчета живых клеток
int count_alive_cells(Cell field[HEIGHT][WIDTH])
{
    int count = 0;
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            if (field[y][x].alive == CELL_ALIVE)
            {
                count++;
            }
        }
    }
    return count;
}

/**
 * Сравнивает два поля на идентичность
 * @param field1 - первое поле
 * @param field2 - второе поле
 * @return true если поля идентичны, false иначе
 */
bool fields_are_equal(Cell field1[HEIGHT][WIDTH], Cell field2[HEIGHT][WIDTH])
{
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            if (field1[y][x].alive != field2[y][x].alive)
            {
                return false;
            }
        }
    }
    return true;
}

/**
 * Применяет случайные мутации к полю
 * @param field - поле для мутаций
 * @param mutation_rate - процент клеток для мутации (0-100)
 */
void apply_mutations(Cell field[HEIGHT][WIDTH], int mutation_rate)
{
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            // Случайная мутация с вероятностью mutation_rate%
            if (rand() % 100 < mutation_rate)
            {
                // Инвертируем состояние клетки
                field[y][x].alive = (field[y][x].alive == CELL_ALIVE) ? CELL_DEAD : CELL_ALIVE;
                field[y][x].age = 0; // Сбрасываем возраст для новой клетки
                field[y][x].mutated = 1; // Устанавливаем mutated
            }
        }
    }
}


int main(int argc, char *argv[])
{
    Cell field[HEIGHT][WIDTH];
    Cell next_field[HEIGHT][WIDTH];
    Cell previous_field[HEIGHT][WIDTH]; // Для проверки стагнации
    int delay_ms = INITIAL_DELAY;
    int generation = 0;
    bool is_paused = false;
    bool use_random = false;
    bool system_stabilized = false;
    int density_percent = 25; // По умолчанию 25% живых клеток

    if (argc < 2)
    {
        use_random = true;
    }
    else
    {
        FILE *input = fopen(argv[1], "r");
        if (!input)
        {
            use_random = true;
        }
        else
        {
            load_initial_state(input, field);
            fclose(input);
        }
    }
    if (use_random)
    {
        if (argc >= 3)
        {
            density_percent = atoi(argv[2]);
            if (density_percent < 0 || density_percent > 100)
            {
                density_percent = 25;
            }
        }
        generate_random_field(field, density_percent);
    }
    // Инициализируем ncurses
    init_screen();
    // Копируем начальное поле для проверки стагнации
    for (int y = 0; y < HEIGHT; y++)
    {
        for (int x = 0; x < WIDTH; x++)
        {
            previous_field[y][x].alive = field[y][x].alive;
            previous_field[y][x].age = field[y][x].age;
            previous_field[y][x].mutated = field[y][x].mutated; // Копируем mutated
        }
    }
    
    // Отрисовываем начальное поле
    draw_field_with_age(field, delay_ms, generation, is_paused, system_stabilized);

    while (1)
    {
        if (!is_paused && !system_stabilized) {
            napms(delay_ms);
        } else {
            napms(100); // Короткая задержка в режиме паузы или стагнации
        }

        // Обработка нажатия клавиш
        int command = process_input();
        if (command == CMD_FASTER && delay_ms > MIN_DELAY)
        {
            delay_ms -= DELAY_STEP;
        }
        else if (command == CMD_SLOWER && delay_ms < MAX_DELAY)
        {
            delay_ms += DELAY_STEP;
        }
        else if (command == CMD_PAUSE)
        {
            is_paused = !is_paused; // Переключаем паузу
        }
        else if (command == CMD_RESTART)
        {
            // Рестарт с новой генерацией
            generate_random_field(field, density_percent);
            generation = 0;
            is_paused = false;
            system_stabilized = false;
            // Копируем поле для стагнации
            for (int y = 0; y < HEIGHT; y++)
                for (int x = 0; x < WIDTH; x++)
                    previous_field[y][x] = field[y][x];
        }
        else if (command == CMD_EXIT)
        {
            break;
        }

        // Обновляем поколение только если не на паузе и не стабилизировалось
        if (!is_paused && !system_stabilized) {
            // Копируем текущее поле в previous_field
            for (int y = 0; y < HEIGHT; y++)
                for (int x = 0; x < WIDTH; x++)
                    previous_field[y][x] = field[y][x];

            update_field_with_age(field, next_field);
            generation++;

            // Мутации раз в MUTATION_INTERVAL поколений
            if (generation > 0 && generation % MUTATION_INTERVAL == 0) {
                apply_mutations(field, MUTATION_RATE);
            }

            // Проверка стагнации
            if (fields_are_equal(field, previous_field)) {
                system_stabilized = true;
            }
        }

        // Рисуем новое поколение
        draw_field_with_age(field, delay_ms, generation, is_paused, system_stabilized);
    }

    // Завершаем ncurses
    close_screen();

    return 0;
}