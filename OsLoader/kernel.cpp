__asm("jmp kmain");

#define VIDEO_BUF_PTR (0xb8000)
#define IDT_TYPE_INTR (0x0E)
#define IDT_TYPE_TRAP (0x0F)
// Селектор секции кода, установленный загрузчиком ОС
#define GDT_CS (0x8)
#define PIC1_PORT (0x20)

#define NULL 0
/*Базовый порт управления курсором текстового экрана. Подходит для
большинства, но может отличаться в других BIOS и в общем случае адрес
должен быть прочитан из BIOS data area.*/
#define CURSOR_PORT (0x3D4)
#define VIDEO_WIDTH (80) // Ширина текстового экрана
#define ACPI_PORT 0x604
#define ACPI_CMD 0x2000
#define UINT_MAX 0xffffffff
#define HEIGHT 25
// Структура описывает данные об обработчике прерывания
struct idt_entry
{
    unsigned short base_lo;  // Младшие биты адреса обработчика
    unsigned short segm_sel; // Селектор сегмента кода
    unsigned char always0;   // Этот байт всегда 0
    unsigned char flags;     // Флаги тип. Флаги: P, DPL, Типы - это константы - IDT_TYPE...
    unsigned short base_hi;  // Старшие биты адреса обработчика
} __attribute__((packed));   // Выравнивание запрещено

// Структура, адрес которой передается как аргумент команды lidt

struct idt_ptr
{
    unsigned short limit;
    unsigned int base;
} __attribute__((packed)); // Выравнивание запрещено

typedef void (*intr_handler)();

struct idt_entry g_idt[256]; // Реальная таблица IDT
struct idt_ptr g_idtp;       // Описатель таблицы для команды lidt

// https://wiki.osdev.org/PS/2_Keyboard#Scan_Code_Set_1
unsigned char no_shift_codes[58] = {
    0, 0, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', 0, 0,
    'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', 0, 0,
    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`', 0,
    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0, 0, 0, ' '};
unsigned char shift_on_codes[58] = {
    0, 0, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', 0, 0,
    'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', 0, 0,
    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '\"', '~', 0,
    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0, 0, 0, ' '};

const char *welcome = "Wellcome to SolverOS";           // Welcome message
const char *default_err = "Error: Command not found!";  // Default Command message
const char *gcd_incorrect = "Error: Command Incorrect"; // GCD function error if integers less than 0
const char *gcd_overflow = "Error: Integer overflow";   // GCD function error if integer above than UINT_MAX
const char *equation_incorrect = "Error: Equation is incorrect";

int color;                        // Color value form bootsector
unsigned char shift = 0;          // Is shift pressed?
unsigned int current_string = 0;  // String position
unsigned int cursor_position = 0; // Cursor position
unsigned int string_len = 0;      // String lenght
char input_str[41];               // Input string buffer

// Interrupt functions
void intr_reg_handler(int num, unsigned short segm_sel, unsigned short flags, intr_handler hndlr);
void intr_init();
void intr_start();
void intr_enable();
void intr_disable();
void default_intr_handler();

// Keyboard functions
void keyb_handler();
void keyb_init();
static inline unsigned char inb(unsigned short port);
static inline void outb(unsigned short port, unsigned char data);
void keyb_process_keys();
void backspace_handler();
void on_key(unsigned char scan_code);

// Cursor position function
void cursor_moveto(unsigned int strnum, unsigned int pos);

// Command parsing functions
void command_parse();
void read_command();

// Convert functions
char *_itoa(int num, char *str, int base);
unsigned long long int _atoi(char *str);

// String and print functions
void out_str(int color, const char *ptr);
void out_chr(int color, unsigned char ptr);
void strrev(char str[], int length);
int strcmp(const char *a, const char *b);
int strlen(const char *a);
char *strtok(char *s, const char *delim);
char *strtok_r(char *s, const char *delim, char **last);
void swap(char &a, char &b);

// OS functinos
void solve(int a, int b, int c);
int gcd(int n1, int n2, char *str);
void shutdown();
void info();
void help();
void clear();

void intr_reg_handler(int num, unsigned short segm_sel, unsigned short flags, intr_handler hndlr)
{
    unsigned int hndlr_addr = (unsigned int)hndlr;
    g_idt[num].base_lo = (unsigned short)(hndlr_addr & 0xFFFF);
    g_idt[num].segm_sel = segm_sel;
    g_idt[num].always0 = 0;
    g_idt[num].flags = flags;
    g_idt[num].base_hi = (unsigned short)(hndlr_addr >> 16);
}

// Функция инициализации системы прерываний: заполнение массива с адресами обработчиков
void intr_init()
{
    int i;
    int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
    for (i = 0; i < idt_count; i++)
        intr_reg_handler(i, GDT_CS, 0x80 | IDT_TYPE_INTR, default_intr_handler); // segm_sel=0x8, P=1, DPL=0, Type=Intr
}

void intr_start()
{
    int idt_count = sizeof(g_idt) / sizeof(g_idt[0]);
    g_idtp.base = (unsigned int)(&g_idt[0]);
    g_idtp.limit = (sizeof(struct idt_entry) * idt_count) - 1;
    asm("lidt %0" ::"m"(g_idtp));
}

void intr_enable()
{
    asm("sti");
}

void intr_disable()
{
    asm("cli");
}

void keyb_handler()
{
    asm("pusha");
    // Обработка поступивших данных
    keyb_process_keys();
    // Отправка контроллеру 8259 нотификации о том, что прерывание обработано
    outb(PIC1_PORT, 0x20);
    asm("popa; leave; iret");
}

void default_intr_handler()
{
    asm("pusha");
    // ... (реализация обработки)
    asm("popa; leave; iret");
}

void keyb_init()
{
    // Регистрация обработчика прерывания
    intr_reg_handler(0x09, GDT_CS, 0x80 | IDT_TYPE_INTR, keyb_handler);
    // segm_sel=0x8, P=1, DPL=0, Type=Intr
    // Разрешение только прерываний клавиатуры от контроллера 8259
    outb(PIC1_PORT + 1, 0xFF ^ 0x02); // 0xFF - все прерывания, 0x02 -бит IRQ1 (клавиатура).
    // Разрешены будут только прерывания, чьи биты установлены в 0
}

static inline unsigned char inb(unsigned short port) // Чтение из порта
{
    unsigned char data;
    asm volatile("inb %w1, %b0"
                 : "=a"(data)
                 : "Nd"(port));
    return data;
}

static inline void outb(unsigned short port, unsigned char data) // Запись
{
    asm volatile("outb %b0, %w1" ::"a"(data), "Nd"(port));
}

void swap(char &a, char &b)
{
    char temp = a;
    a = b;
    b = temp;
}

int strlen(const char *str)
{
    const char *s;
    for (s = str; *s; ++s)
        ;
    return (s - str);
}

int strcmp(const char *s1, const char *s2)
{
    for (; *s1 == *s2; s1++, s2++)
        if (*s1 == '\0')
            return 0;
    return ((*(unsigned char *)s1 < *(unsigned char *)s2) ? -1 : +1);
}

char *strtok(char *s, const char *delim)
{
    static char *last;
    return strtok_r(s, delim, &last);
}

char *strtok_r(char *s, const char *delim, char **last)
{
    char *spanp;
    int c, sc;
    char *tok;
    if (s == NULL && (s = *last) == NULL)
        return (NULL);
cont:
    c = *s++;
    for (spanp = (char *)delim; (sc = *spanp++) != 0;)
    {
        if (c == sc)
            goto cont;
    }
    if (c == 0)
    { /* no non-delimiter characters */
        *last = NULL;
        return (NULL);
    }
    tok = s - 1;
    for (;;)
    {
        c = *s++;
        spanp = (char *)delim;
        do
        {
            if ((sc = *spanp++) == c)
            {
                if (c == 0)
                    s = NULL;
                else
                    s[-1] = 0;
                *last = s;
                return (tok);
            }
        } while (sc != 0);
    }
}

void strrev(char str[], int length)
{
    int start = 0;
    int end = length - 1;
    while (start < end)
    {
        swap(*(str + start), *(str + end));
        start++;
        end--;
    }
}

void cursor_moveto(unsigned int strnum, unsigned int pos)
{
    if (pos < 1 || pos > string_len || pos > 79)
        return;
    unsigned short new_pos = (strnum * VIDEO_WIDTH) + pos;
    outb(CURSOR_PORT, 0x0F);
    outb(CURSOR_PORT + 1, (unsigned char)(new_pos & 0xFF));
    outb(CURSOR_PORT, 0x0E);
    outb(CURSOR_PORT + 1, (unsigned char)((new_pos >> 8) & 0xFF));
    cursor_position = pos;
}

void out_str(int color, const char *ptr)
{
    unsigned char *video_buf = (unsigned char *)VIDEO_BUF_PTR;
    video_buf += 80 * 2 * current_string;
    while (*ptr)
    {
        video_buf[0] = (unsigned char)*ptr; // Символ (код)
        video_buf[1] = color;               // Цвет символа и фона
        video_buf += 2;
        ptr++;
    }
    if (current_string++ >= 25)
    { // Clear if we fill the window
        clear();
        out_chr(color, (unsigned char)'#');
    }
    string_len = 0;
    cursor_moveto(current_string, 0);
}

void out_chr(int color, unsigned char char_code)
{
    unsigned char *video_buf = (unsigned char *)VIDEO_BUF_PTR;
    video_buf += 80 * 2 * current_string + cursor_position * 2;
    video_buf[0] = char_code;
    video_buf[1] = color;
    if (string_len <= cursor_position)
        string_len++;
    cursor_moveto(current_string, cursor_position + 1);
}

void on_key(unsigned char scan_code)
{
    if (scan_code <= 58)
    {
        switch (scan_code)
        {
        case 42:
            shift = 1;
            break;
        case 54:
            shift = 1;
            break;

        default:
            if (shift)
            {
                if (shift_on_codes[scan_code])
                    out_chr(color, shift_on_codes[scan_code]);
            }
            else
            {
                if (no_shift_codes[scan_code])
                    out_chr(color, no_shift_codes[scan_code]);
            }
            break;
        }
    }
}

/* Обработка нажатия Backspace. Отчистка символов, которые стоят после курсора. */
void backspace_handler()
{
    unsigned char *video_buf = (unsigned char *)VIDEO_BUF_PTR;
    video_buf += 80 * 2 * current_string + 2 * cursor_position;
    if (cursor_position == 1)
        return;
    video_buf = video_buf - 2;
    video_buf[0] = 0;
    cursor_moveto(current_string, cursor_position - 1);
    string_len--;
    return;
}

/* Чтение команды из видеопамяти в переменную. */
void read_command()
{
    unsigned char *video_buf = (unsigned char *)VIDEO_BUF_PTR;
    video_buf += 80 * 2 * current_string + 2;
    int tmp = 0;
    while (tmp < string_len)
    {
        input_str[tmp] = video_buf[0];
        video_buf += 2;
        tmp++;
    }
    input_str[tmp] = '\0';
}

/* Вывод нужной ошибки */
void print_error(const char *error)
{
    out_str(color, error);
    out_chr(color, (unsigned char)'#');
}

/* Парсинг введенной команды и ее аргументов, если таковые есть */
void command_parse()
{
    char delim[] = " ";
    char *command_part = strtok(input_str, delim);
    if (strcmp(command_part, "info") == 0)
    {
        info();
    }
    else if (strcmp(command_part, "help") == 0)
    {
        help();
    }
    else if (strcmp(command_part, "clear") == 0)
    {
        clear();
        out_chr(color, (unsigned char)'#');
    }
    else if (strcmp(command_part, "shutdown") == 0)
    {
        shutdown();
    }
    else if (strcmp(command_part, "gcd") == 0)
    {
        char operation_number = 0;
        int n1, n2;
        int check_status = 0;
        while (command_part != NULL)
        {
            switch (operation_number)
            {
            case 0:
                if (strcmp(command_part, "gcd") != 0)
                    print_error(gcd_incorrect);
                else
                    check_status++;
                break;
            case 1:
                n1 = _atoi(command_part);
                if (n1 <= UINT_MAX && strlen(command_part) >= 10)
                {
                    print_error(gcd_overflow);
                    return;
                }
                else if (n1 <= 0)
                {
                    print_error(gcd_incorrect);
                    return;
                }
                else
                    check_status++;
                break;
            case 2:
                n2 = _atoi(command_part);
                if (n2 == 0 && strlen(command_part) >= 10)
                {
                    print_error(gcd_overflow);
                    return;
                }
                else if (n2 <= 0)
                {
                    print_error(gcd_incorrect);
                    return;
                }
                else
                    check_status++;
                break;
            default:
                break;
            }
            operation_number++;
            command_part = strtok(NULL, delim);
        }
        if (check_status == 3)
        {
            char result_str[110] = "Result: ";
            gcd(n1, n2, result_str);
            out_str(color, result_str);
            out_chr(color, (unsigned char)'#');
        }
    }
    else if (strcmp(command_part, "solve") == 0)
    {
        command_part = strtok(NULL, delim);
        int a, b, c;
        char parameter[50];
        char operation_number = 0;
        int i = 0, param_index = 0;
        if (strlen(command_part) < 5)
        {
            print_error(equation_incorrect);
            return;
        }
        if (command_part[0] == 'x')
        {
            a = 1;
            i++;
        }
        else
        {
            while (command_part[i] != 'x')
            {
                parameter[param_index] = command_part[i];
                param_index++;
                i++;
            }
            parameter[param_index] = '\0';
            if (param_index == 1 && parameter[0] == '-')
                a = -1;
            else
                a = _atoi(parameter);
        }
        i++;
        param_index = 0;
        while (command_part[i] != '=')
        {
            parameter[param_index] = command_part[i];
            param_index++;
            i++;
        }
        parameter[param_index] = '\0';
        b = _atoi(parameter);
        param_index = 0;
        i++;
        while (i != strlen(command_part))
        {
            parameter[param_index] = command_part[i];
            param_index++;
            i++;
        }
        parameter[param_index] = '\0';
        c = _atoi(parameter);
        param_index = 0;
        if (a != 0)
            solve(a, b, c);
        else
            print_error(equation_incorrect);
        a = 0;
        b = 0;
        c = 0;
    }
    else
    {
        out_str(color, default_err);
        out_chr(color, (unsigned char)'#');
    }
}

/* Обработка всех нажатий, проверка на нажатие клавиши enter или backspace */
void keyb_process_keys()
{
    // Проверка что буфер PS/2 клавиатуры не пуст (младший бит присутствует)
    if (inb(0x64) & 0x01)
    {
        unsigned char scan_code;
        unsigned char state;
        scan_code = inb(0x60); // Считывание символа с PS/2 клавиатуры
        if (scan_code < 128)
        {
            // Обработка backspace
            if (scan_code == 14)
            {
                backspace_handler();
            }
            if (scan_code == 28)
            {
                if (string_len == 1)
                    return;
                read_command();
                current_string++;
                cursor_position = 0;
                string_len = 0;
                command_parse();
            }
            on_key(scan_code);
        }
        else // Скан-коды выше 128 - это отпускание клавиши
        {
            if (scan_code == 170 || scan_code == 182) // Shift key up
                shift = 0;
        }
    }
}

/* Перевод числа в строку, учитывая знак числа */
char *_itoa(int num, char *str, int base)
{
    int i = 0;
    int is_negative = 0;

    if (num == 0)
    {
        str[i++] = '0';
        str[i] = '\0';
        return str;
    }

    if (num < 0 && base == 10)
    {
        is_negative = 1;
        num = -num;
    }
    while (num != 0)
    {
        int rem = num % base;
        str[i++] = (rem > 9) ? (rem - 10) + 'a' : rem + '0';
        num = num / base;
    }

    if (is_negative)
        str[i++] = '-';
    str[i] = '\0';
    strrev(str, i);
    return str;
}

/* Перевод строки в число с учетом знака */
unsigned long long int _atoi(char *str)
{
    long long int res = 0;
    long long int sign = 1;
    int i = 0;
    if (str[0] == '+')
    {
        sign = 1;
        i++;
    }
    else if (str[0] == '-')
    {
        sign = -1;
        i++;
    }
    for (; str[i] != '\0'; ++i)
        res = res * 10 + str[i] - '0';
    return sign * res;
}

/* Нахождение НОД двух положительных чисел */
int gcd(int n1, int n2, char *str)
{
    static int result;
    int temp;
    while (n2 != 0)
    {
        temp = n1 % n2;

        n1 = n2;
        n2 = temp;
    }
    result = n1;
    char solution[100];
    _itoa(n1, solution, 10);

    int num_index = 0;
    int res_index = strlen(str);

    while (num_index != strlen(solution))
    {
        str[res_index] = solution[num_index];
        num_index++;
        res_index++;
    }
    // result_str[res_index] = '\0';
    return result;
}

/* Решение уравнения вида ax+b=c */
void solve(int a, int b, int c)
{
    char result_str[110] = "Result: ";
    char solution[100] = "";
    char a_str[50] = "";
    char right_str[50] = "";
    int delim = 0;
    int sign = -1;
    int right_digit = c + (b * sign);
    int correct_result = 0;
    int append_str_index = 0;
    int result_str_index = strlen(result_str);
    if (right_digit % a == 0)
    {
        correct_result = right_digit / a;
        _itoa(correct_result, right_str, 10);
        while (append_str_index != strlen(right_str))
        {
            result_str[result_str_index] = right_str[append_str_index];
            append_str_index++;
            result_str_index++;
        }
    }
    else
    {
        delim = gcd(a, right_digit, NULL);
        while (delim != 1)
        {
            a /= delim;
            right_digit /= delim;
            delim = gcd(a, right_digit, NULL);
        }
        _itoa(a, a_str, 10);
        _itoa(right_digit, right_str, 10);

        while (append_str_index != strlen(right_str))
        {
            result_str[result_str_index] = right_str[append_str_index];
            append_str_index++;
            result_str_index++;
        }

        append_str_index = 0;
        result_str[result_str_index] = '/';
        result_str_index++;

        while (append_str_index != strlen(a_str))
        {
            result_str[result_str_index] = a_str[append_str_index];
            append_str_index++;
            result_str_index++;
        }
    }
    result_str[result_str_index] = '\0';
    out_str(color, result_str);
    out_chr(color, (unsigned char)'#');
}

/* Выключение компьютера на эмуляции QEMU последней версии */
void shutdown()
{
    asm volatile("outw %0, %1"
                 :
                 : "a"((unsigned short)ACPI_CMD), "d"((unsigned short)ACPI_PORT)); // отправляем команду ACPI на порт
}

/* Вывод общей информации */
void info()
{
    out_str(color, "SolverOS. Author: Galkin Klim, 4851003/10002");
    out_str(color, "OS Linux, Translator YASM, Intel syntax, C compiler - GCC");
    out_chr(color, (unsigned char)'#');
}

/* Вывод описания всех команд */
void help()
{
    out_str(color, "help - information about OS commands");
    out_str(color, "info - information about creator and used software");
    out_str(color, "solve equation - solve the equation ax+b=c");
    out_str(color, "gcd num1 num2- find the gcd of two nums");
    out_str(color, "shutdown - halt the OS");
    out_str(color, "clear - clean all strings");
    out_chr(color, (unsigned char)'#');
    return;
}

/* Чистка экрана */
void clear()
{
    /*asm volatile(
        "mov $0x03, %ah\n" // загрузка значения 3 в регистр ah
        "int $0x10\n"      // вызов прерывания 0x10
    );*/
    unsigned char *video_buf = (unsigned char *)VIDEO_BUF_PTR;
    for (int i = 0; i < VIDEO_WIDTH * HEIGHT; i++)
    {
        *(video_buf + i * 2) = '\0';
    }
    current_string = 0;
    cursor_position = 0;
    string_len = 0;
    return;
}

extern "C" int kmain()
{
    intr_init();
    keyb_init();
    intr_start();
    intr_enable();
    asm("\t movl %%ebx, %0"
        : "=r"(color));
    // Вывод строки
    clear();
    out_str(color, welcome);
    help();
    // Бесконечный цикл
    while (1)
    {
        asm("hlt");
    }
    return 0;
}