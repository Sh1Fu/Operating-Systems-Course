#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <chrono>
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    #include <Windows.h>
#elif __unix__
    #include <pthread.h>
#endif

using namespace std;

/* Структура задачи. Описывается, какие индексы в задаче будут рассматриваться */
typedef struct thread_info
{
    uint32_t start; /* Стартовый индекс для сортировки (left) */
    uint32_t end;   /* Конечный индекс для сортировки(right) */
} pthread_info_t;

/* 
 * Примитивы синхронизации. 
 * Замок для создания новых задач в процессе работы qsort. 
 * Монитор для работы с вектором задач 
*/
#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)
    HANDLE sem_handler;
    CRITICAL_SECTION cs_handler;
    HANDLE* threads;
#elif __unix__
    pthread_mutex_t cond_lock;
    pthread_cond_t cond;
    pthread_t *threads;
#endif

/* Массив всех потоков */
uint32_t num_threads;

/* Вектор задач для сортировки */
vector<pthread_info_t> tasks;

/* Начальный массив и доп данные для работы с массивом*/
vector<int> start_array;
uint32_t num_elements;