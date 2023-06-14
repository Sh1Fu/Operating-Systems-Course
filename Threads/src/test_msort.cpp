#include <iostream>
#include <vector>
#include <fstream>
#include <chrono>
#include <Windows.h>
#include <memory.h>

using namespace std;

typedef struct task_node
{
    uint64_t id;
    int32_t left;
    int32_t right;
    struct task_node *left_child;
    struct task_node *right_child;
} Node;

CRITICAL_SECTION cs; // Критическая секция для защиты доступа к очереди задач
HANDLE sem;          // Семафор для управления количеством выполняющихся задач
HANDLE *threads;
vector<Node *> tasks; // Очередь задач на выполнение
vector<Node *> merge_queue;
vector<int> start_array(10000);
int64_t num_threads, num_elements;

void addNewTask(Node *task, int32_t mid)
{
    EnterCriticalSection(&cs); // Заходим в критическую секцию
    Node *left_child = new Node{tasks.size(), task->left, mid, nullptr, nullptr};
    Node *right_child = new Node{tasks.size(), mid + 1, task->right, nullptr, nullptr};
    task->left_child = left_child;
    task->right_child = right_child;
    tasks.push_back(left_child); // Добавляем подзадачи в очередь задач
    tasks.push_back(right_child);
    merge_queue.push_back(task);
    cout << "Current merge size: " << merge_queue.size() << endl;
    // tasks.pop_back();
    LeaveCriticalSection(&cs); // Выходим из критической секцию
}

void merge(vector<int> &arr, int32_t l, int32_t m, int32_t r)
{
    int32_t i, j, k;
    int32_t n1 = m - l + 1;
    int32_t n2 = r - m;

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

void mergeSort(vector<int> &arr, int32_t l, int32_t r, Node *task)
{
    if (l >= r)
    {
        return;
    }
    int32_t m = l + (r - l) / 2;
    if (task->right - task->left > 1000 && num_threads < num_elements && num_threads > 1)
    {
        addNewTask(task, m);
        ReleaseSemaphore(sem, 2, NULL); // Освобождаем два семафора для запуска потоков на выполнение задач
    }
    else
    {
        mergeSort(arr, l, m, task);
        mergeSort(arr, m + 1, r, task);
        merge(arr, l, m, r);
        cout << task->left << " " << task->right << endl;
    }
}

DWORD WINAPI mergeThread(LPVOID lpParam)
{
    while (true)
    {
        if (!merge_queue.empty() && tasks.empty())
        {
            Node* merge_task = merge_queue.back();
            if (merge_task->left_child == nullptr && merge_task->right_child == nullptr)
            {
                EnterCriticalSection(&cs); // Заходим в критическую секцию
                int32_t m = merge_task->left + (merge_task->right - merge_task->left) / 2;
                merge(start_array, merge_task->left, m, merge_task->right); // Выполняем слияние для родительской задачи
                EnterCriticalSection(&cs);
                merge_queue.pop_back();    // Удаляем ее из очереди
                LeaveCriticalSection(&cs); // Выходим из критической секции
            }
        }
        else if (tasks.empty())
        {                                       // Проверяем, есть ли задачи на выполнение
            WaitForSingleObject(sem, INFINITE); // Ожидаем доступный семафор
            break;                              // Если задач нет, завершаем работу потока
        }
        EnterCriticalSection(&cs); // Заходим в критическую секцию
        Node *task = tasks.back(); // Берем последнюю задачу из очереди
        tasks.pop_back();
        LeaveCriticalSection(&cs); // Выходим из критической секции
        if (task->right - task->left <= 1000) mergeSort(start_array, task->left, task->right, task);
        delete task; 
    }
    return NULL;
}

/* Подготовка вывода в файлы. */
void prepareOutput()
{
    auto start_time = chrono::high_resolution_clock::now();
    WaitForMultipleObjects(num_threads, threads, true, INFINITE); // Ожидаем завершения всех потоков
    auto end_time = chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    std::ofstream out("output.txt", std::ios::out);
    out << num_threads << std::endl
        << num_elements << std::endl;
    int i = 0;
    for (; i < num_elements - 1; i++)
        out << start_array[i] << " ";
    out << start_array[i];
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
    for (int i = 0; i < num_elements; i++)
    {
        fin >> tmp;
        start_array.push_back(tmp);
    }
    fin.close();

    Node *main_task = (Node *)malloc(sizeof(Node));
    main_task->id = UINT64_MAX;
    main_task->left = 0;
    main_task->right = num_elements - 1;
    main_task->left_child = nullptr;
    main_task->right_child = nullptr;
    tasks.push_back(main_task);

    InitializeCriticalSection(&cs);                    // Инициализируем критическую секцию
    sem = CreateSemaphore(NULL, 1, num_threads, NULL); // Инициализируем семафор с максимальным и текущим количеством потоков - 12

    threads = new HANDLE[num_threads];

    for (int i = 0; i < num_threads; i++)
    {
        threads[i] = CreateThread(NULL, 0, mergeThread, NULL, 0, NULL);
    }
    prepareOutput();

    delete[] threads;
    DeleteCriticalSection(&cs); // Удаляем критическую секцию
    CloseHandle(sem);           // Закрываем дескриптор семафора
    return 0;
}