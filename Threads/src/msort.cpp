#include <iostream>
#include <vector>
#include <fstream>
#include <algorithm>
#include <Windows.h>
#include <memory.h>
#include <list>

using namespace std;

typedef struct task_info
{
    int left;
    int right;
} TASK_HANDLE;

CRITICAL_SECTION cs; // Критическая секция для защиты доступа к очереди задач
HANDLE sem;          // Семафор для сигнала о добавлении задачи
HANDLE *threads;     // Массив хендлеров на потоки

vector<TASK_HANDLE> tasks; // Очередь задач на выполнение
vector<vector<int>> merge_queue; // Очередь на последнее слияние отсортированных массивов

/* Основный переменные с данными */
vector<int> start_array;
vector<int> result_array;
int num_threads, num_elements;

/* Добавление новой задачи по разбиение массива на части */
void addNewTask(int id, int step)
{
    EnterCriticalSection(&cs); // Заходим в критическую секцию
    TASK_HANDLE task;
    task.left = id * step;
    task.right = (id + 1) * step - 1;
    if (id == num_threads - 1)
    {
        task.right = num_elements - 1;
    }
    tasks.push_back(task); // Добавляем подзадачи в очередь задач
    LeaveCriticalSection(&cs);      // Выходим из критической секцию
    ReleaseSemaphore(sem, 1, NULL); // Освобождаем два семафора для запуска потоков на выполнение задач
}

/* Реализация слияния в векторе */
void merge(vector<int> &arr, int l, int m, int r)
{
    int i, j, k;
    int n1 = m - l + 1;
    int n2 = r - m;

    vector<int> L(n1), R(n2);
    for (i = 0; i < n1; i++)
        L[i] = arr[l + i];
    for (j = 0; j < n2; j++)
        R[j] = arr[m + 1 + j];

    i = 0;
    j = 0;
    k = l;
    while (i < n1 && j < n2)
    {
        if (L[i] <= R[j])
        {
            arr[k] = L[i];
            i++;
        }
        else
        {
            arr[k] = R[j];
            j++;
        }
        k++;
    }

    while (i < n1)
    {
        arr[k] = L[i];
        i++;
        k++;
    }

    while (j < n2)
    {
        arr[k] = R[j];
        j++;
        k++;
    }
}

/* Рекусрвиная сортировка для одного потока */
void mergeSort(vector<int> &arr, int l, int r, TASK_HANDLE task)
{
    if (l >= r)
    {
        return;
    }
    int m = l + (r - l) / 2;
    mergeSort(arr, l, m, task);
    mergeSort(arr, m + 1, r, task);
    merge(arr, l, m, r);
}

/* Функция-метка. Берет задачу из вектора, выполняет сортировку подмассива. Полученный подмассив ожидает конечного слияния с другими. */
DWORD WINAPI mergeThread(LPVOID lpParam)
{
    int id = (int)lpParam;
    addNewTask(id, num_elements / num_threads);
    while (true)
    {
        if (tasks.empty())
        { // Проверяем, есть ли задачи на выполнение
            WaitForSingleObject(sem, INFINITE);
            break;
        }
        else
        {
            EnterCriticalSection(&cs);       // Заходим в критическую секцию
            TASK_HANDLE task = tasks.back(); // Берем последнюю задачу из очереди
            // cout << "Current Task: " << task.left << " " << task.right << endl;
            tasks.pop_back();
            LeaveCriticalSection(&cs);
            mergeSort(start_array, task.left, task.right, task);
            EnterCriticalSection(&cs); // Заходим в критическую секцию
            vector<int> merged_part(start_array.begin() + task.left, start_array.begin() + task.right + 1);
            merge_queue.push_back(merged_part);
            LeaveCriticalSection(&cs); // Выходим из критической секции
        }
    }
    return NULL;
}

/* Финальное слияние всех отсортированных подмассивов. Такую реализацию невозможно прогнать в несколько потоков. */
void finalMerge()
{
    vector<int> state;
    int state_size = num_elements / num_threads;
    while (merge_queue.size() != 1)
    {
        vector<int> left_part = merge_queue.front();
        vector<int> next_part = *(merge_queue.begin() + 1);
        state_size = state_size + next_part.size();
        state.resize(state_size);
        std::merge(left_part.begin(), left_part.end(), next_part.begin(), next_part.end(), state.begin());
        merge_queue[0] = state;
        merge_queue.erase(merge_queue.begin() + 1);
        // state.clear();
    }
    result_array.resize(num_elements);
    result_array.clear();
    result_array = merge_queue[0];
}

// Подготовка вывода в файлы.
void prepareOutput()
{
    unsigned long long start_time = GetTickCount64();
    WaitForMultipleObjects(num_threads, threads, true, INFINITE); // Ожидаем завершения всех потоков
    finalMerge();
    unsigned long long end_time = GetTickCount64();
    unsigned long long duration = end_time - start_time;

    std::ofstream out("output.txt", std::ios::out);
    out << num_threads << std::endl
        << num_elements << std::endl;
    int i = 0;
    // merge(start_array, 0, (num_elements - 1) / 2, num_elements - 1);
    for (; i < num_elements - 1; i++)
        out << result_array[i] << " ";
    out << result_array[i];
    out << endl;
    out.close();

    ofstream time_res("times.txt", ios::out);
    time_res << duration << endl;
    time_res.close();
}

int main()
{
    ifstream fin("input.txt");
    fin >> num_threads >> num_elements;
    int tmp = 0;
    start_array.resize(num_elements);
    start_array.clear();
    for (int i = 0; i < num_elements; i++)
    {
        fin >> tmp;
        start_array.push_back(tmp);
    }
    fin.close();

    InitializeCriticalSection(&cs);                    // Инициализируем критическую секцию
    sem = CreateSemaphore(NULL, 1, num_threads, NULL); // Инициализируем семафор с максимальным и текущим количеством потоков - 12

    threads = new HANDLE[num_threads];
    for (int i = 0; i < num_threads; i++)
    {
        threads[i] = CreateThread(NULL, 0, mergeThread, (LPVOID)i, 0, NULL);
    }
    prepareOutput();

    delete[] threads;
    DeleteCriticalSection(&cs); // Удаляем критическую секцию
    CloseHandle(sem);           // Закрываем дескриптор семафора
    return 0;
}