#include <windows.h>
#include <vector>
#include <string>
#include <iostream>

#define PHILLS_COUNT 5
using namespace std;

/*
 * Проблема структуры. Я не смог придумать нормальное решение только с одним хендлером allowEat вместо eatRequest и eatResponse
 * Суть в том, что это ивент будет реагировать сразу после SetEvent. Это решается установкой функции Sleep, но я боюсь терять драгоценное время
 */
typedef struct phill
{
    HANDLE eatRequest;  // Событие запроса на доступ к еде
    HANDLE eatResponse; // Событие ответа
    HANDLE endEat;      // Событие отправки сообщения об окончании трапезы
} PHILL_HANDLER;

/* Структуры потоков и самих философов*/
HANDLE threads[6];
PHILL_HANDLER philosophers[5];
HANDLE mutex;

/* Переменные времени для состоняний и работы программы */
DWORD stateTime = 0;
unsigned int start = 0;
int totalTime = 0;

/* Вектор с заранее заданной очередью философов */
vector<int> phills_queue;

void print_results(int phill, string step)
{
    WaitForSingleObject(mutex, INFINITE);
    cout << GetTickCount() - start << ":" << phill + 1 << step << endl;
    ReleaseMutex(mutex);
}

DWORD WINAPI control_thread(LPVOID param)
{
    /* Стартовое значение философов. Можно выбрать любую пару, которая не имеет общих вилок. */
    int i = 0, j = 2;
    while (true)
    {
        /* Вышли за рамки установленного времени*/
        if (GetTickCount() - start >= totalTime)
            break;

        /* Проверка на запросы доступа на трапезу от обоих выбранных философов */
        if (WaitForSingleObject(philosophers[i].eatRequest, INFINITE) == WAIT_OBJECT_0 && WaitForSingleObject(philosophers[j].eatRequest, INFINITE) == WAIT_OBJECT_0)
        {
            /* Разрешение на покушать :>*/
            SetEvent(philosophers[i].eatResponse);
            SetEvent(philosophers[j].eatResponse);
        }
        /* Ожидание окончания приема пищи */
        if (WaitForSingleObject(philosophers[i].endEat, INFINITE) == WAIT_OBJECT_0 && WaitForSingleObject(philosophers[j].endEat, INFINITE) == WAIT_OBJECT_0)
        {
            print_results(i, ":E->T");
            print_results(j, ":E->T");
        }

        /* Изменение философов */
        ++i %= PHILLS_COUNT;
        ++j %= PHILLS_COUNT;
    }
    return NULL;
}

DWORD WINAPI phill_eating(LPVOID param)
{
    int phill_id = (int)param;
    PHILL_HANDLER phil = philosophers[phill_id];
    while (true)
    {
        /* Запрос доступа */
        SetEvent(phil.eatRequest);
        /* Ожидание подтверждения */
        WaitForSingleObject(phil.eatResponse, INFINITE);
        if (GetTickCount() - start >= totalTime)
            break;

        /* Трапеза */
        print_results(phill_id, ":T->E");
        Sleep(stateTime);
        SetEvent(phil.endEat);
    }
    return NULL;
}

int main(int argc, char **argv)
{
    totalTime = atoi(argv[1]);
    stateTime = atoi(argv[2]);

    for (int i = 0; i < 5; i++)
    {
        philosophers[i].eatRequest = CreateEvent(NULL, false, false, NULL);
        philosophers[i].eatResponse = CreateEvent(NULL, false, false, NULL);
        philosophers[i].endEat = CreateEvent(NULL, false, false, NULL);
    }
    mutex = CreateMutex(NULL, false, NULL);

    for (int i = 0; phills_queue.size() != 5; i += 2)
    {
        phills_queue.push_back(i);
    }

    start = GetTickCount();
    for (int i = 0; i < 5; i++)
    {
        threads[i] = CreateThread(0, 0, phill_eating, (LPVOID)i, 0, 0);
    }
    threads[5] = CreateThread(0, 0, control_thread, 0, 0, 0);

    WaitForMultipleObjects(6, threads, false, INFINITE);

    for (int i = 0; i < 5; i++)
    {
        CloseHandle(philosophers[i].eatRequest);
        CloseHandle(philosophers[i].eatResponse);
        CloseHandle(philosophers[i].endEat);
    }
    CloseHandle(mutex);
    return 0;
}