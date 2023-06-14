#include <iostream>
#include <fstream>
#include <vector>
#include <algorithm>
#include <chrono>
#include <pthread.h>

using namespace std;

/* Структура задачи. Описывается, какие индексы в задаче будут рассматриваться */
typedef struct thread_info
{
    int64_t start; /* Стартовый индекс для сортировки (left) */
    int64_t end;   /* Конечный индекс для сортировки(right) */
} pthread_info_t;

/* 
 * Примитивы синхронизации. 
 * Замок для создания новых задач в процессе работы qsort. 
 * Монитор для работы с вектором задач 
*/
pthread_mutex_t cond_lock;
pthread_cond_t cond;

/* Массив всех потоков */
pthread_t *threads;
int64_t num_threads;

/* Вектор задач для сортировки */
vector<pthread_info_t> tasks;

/* Начальный массив и доп данные для работы с массивом*/
vector<int> start_array;
int64_t num_elements;

/* 
 * Функция создания задач. Основная идея - заменить двойную рекурсию.
 * Если массив слишком большой для одного потока, то он разбивается на подмассивы.
 * Так происходит до того момента, пока длина подмассива не станет не более 1000 элементов.
*/
void addNewTask(int64_t left, int64_t right, int64_t lindex, int64_t rindex)
{
    pthread_info_t new_task;
    for (uint8_t i = 0; i < 2; i++)
    {
        new_task.start = ((i == 0) ? (left) : (lindex));
        new_task.end = ((i == 0) ? (rindex) : (right));
        tasks.push_back(new_task);
        // Сигнализируем, что появилась новая задача.
        pthread_cond_signal(&cond);
    }
}

/* Функция быстрой сортировки. Обычная реализация для одного потока, нужно будет переделывать. */
void quickSort(vector<int> &arr, int64_t left, int64_t right)
{
    int pivot = arr[(left + right) / 2];
    int64_t i = left, j = right;
    if (left >= right) return;
    while (i <= j)
    {
        while (arr[i] < pivot) i++;
        while (arr[j] > pivot) j--;
        if (i <= j)
        {
            swap(arr[i], arr[j]);
            i++;
            j--;
        }
    }
    if (right - left > 1000 && num_threads < num_elements && num_threads > 1)
    {
        pthread_mutex_lock(&cond_lock);
        addNewTask(left, right, i, j);
        pthread_mutex_unlock(&cond_lock);
    }
    else
    {
        quickSort(arr, left, j);
        quickSort(arr, i, right);
    }
}

/* Функция-метка для создания потока. */
void *threadFunc(void *args)
{
    while (!tasks.empty())
    {
        pthread_mutex_lock(&cond_lock);
        // Ожидаем появления новых задач.
        while (tasks.empty()) {
            pthread_cond_wait(&cond, &cond_lock);
        }
        pthread_info_t current_task = tasks.back();
        tasks.pop_back();
        pthread_mutex_unlock(&cond_lock);
        quickSort(start_array, current_task.start, current_task.end);
    }
    return NULL;
}

/* Подготовка вывода в файлы. */
void prepareOutput()
{
    auto start_time = chrono::high_resolution_clock::now();
    for (size_t i = 0; i < num_threads; i++)
    {
        pthread_join(threads[i], NULL);
    }
    auto end_time = chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end_time - start_time).count();
    cout << "All Tasks were executed" << endl;

    std::ofstream out("output.txt", std::ios::out);
    out << num_threads << std::endl
        << num_elements << std::endl;
    int i = 0;
    for (; i < num_elements - 1; i++) out << start_array[i] << " ";
    out << start_array[i];
    out << endl;
    out.close();

    ofstream time_res("times.txt", ios::out);
    time_res << duration << endl;
    time_res.close();
}

int main()
{
    /* Чтение входных данных. */
    ifstream fin("input.txt");
    fin >> num_threads >> num_elements;
    int tmp = 0;
    for (int i = 0; i < num_elements; i++)
    {
        fin >> tmp;
        start_array.push_back(tmp);
    }
    fin.close();

    /* Инициализация новых переменных для потока. */
    threads = new pthread_t[num_threads];
    pthread_mutex_init(&cond_lock, NULL);
    pthread_cond_init(&cond, NULL);

    pthread_info_t main_task = { 0, num_elements - 1};
    tasks.push_back(main_task);

    /* Создания потоков */
    for (int i = 0; i < num_threads; i++) pthread_create(&threads[i], NULL, threadFunc, &tasks[i]);

    /* Подготовка выводов. Перевод потоков в режим ожидания других. */
    prepareOutput();

    /* Чистка. */
    delete[] threads;
    pthread_mutex_destroy(&cond_lock);
    pthread_cond_destroy(&cond);
    return 0;
}