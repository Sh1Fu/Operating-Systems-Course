#include <iostream>
#include <fstream>
#include <chrono>
#include <vector>
#include <semaphore.h>
#include <pthread.h>

using namespace std;

typedef struct t_desc
{
    int id;
    int result;
    vector<int> task_start_values;
} thread_description;

sem_t main_sem;
thread_description *descr_arr;
pthread_t *threads;
int threads_count, descr_num, start_number;

void task_schedule()
{
    int step_per_task = start_number / threads_count;
    int insert_value = 1;
    for (int thread_index = 0; thread_index < threads_count; thread_index++)
    {
        for (int j = 0; j < step_per_task; j++)
        {
            descr_arr[thread_index].task_start_values.push_back(insert_value++);
        }
    }

    int final_step = 0;
    while (insert_value <= start_number)
    {
        descr_arr[final_step % threads_count].task_start_values.push_back(insert_value++);
    }
    if (threads_count <= start_number)
    {
        descr_num = threads_count;
    }
    else
    {
        descr_num = start_number;
    }
}

int num_decomposition(int n, int k) // https://shorturl.at/ahnF3 (ITMO wiki)
{
    if ((n != 0) && (k == 0))
    {
        return 0;
    }
    else if ((n == 0) && (k == 0))
    {
        return 1;
    }
    else if (k > n)
    {
        return (num_decomposition(n, n));
    }
    else if ((0 < k) && (k <= n))
    {
        return (num_decomposition(n, k - 1) + num_decomposition(n - k, k));
    }
}

void *thread_entry(void *param)
{
    thread_description tmp_job;
    int res, n, k;

    tmp_job = *(thread_description *)param;
    res = 0;

    sem_wait(&main_sem);
    for (int i = 0; i < tmp_job.task_start_values.size(); i++)
    {
        /* Take start values from thread descriptors */
        n = start_number - tmp_job.task_start_values[i];
        k = tmp_job.task_start_values[i];
        /* Run calculation */
        res += num_decomposition(n, k);
    }
    (*(thread_description *)param).result = res;

    sem_post(&main_sem);
    return NULL;
}

void prepare_output(void)
{
    int i = 0, res = 0;

    auto start_time = chrono::high_resolution_clock::now();
    for (int i = 0; i < descr_num; i++)
    {
        pthread_join(threads[i], 0);
    }
    auto end_time = chrono::high_resolution_clock::now();

    auto duration = std::chrono::duration<double, std::milli>(end_time - start_time).count();

    ofstream f("output.txt", ios::out);
    for (i = 0; i < descr_num; i++)
        res += descr_arr[i].result;
    res--;
    f << threads_count << endl
      << start_number << endl
      << res << endl;
    f.close();

    ofstream time_res("times.txt", ios::out);
    time_res << duration << endl;
    time_res.close();
}

int main(void)
{
    ifstream inputFile("input.txt", ios::in);
    inputFile >> threads_count >> start_number;
    inputFile.close();

    descr_arr = new thread_description[threads_count];
    sem_init(&main_sem, 0, 1);

    for (int i = 0; i < threads_count; i++)
    {
        descr_arr[i].id = i;
    }

    task_schedule();
    threads = new pthread_t[descr_num];

    for (int ind = 0; ind < descr_num; ind++)
    {
        pthread_create(&threads[ind], 0, thread_entry, &descr_arr[ind]);
    }
    prepare_output();

    delete[] descr_arr;
    delete[] threads;
    sem_destroy(&main_sem);
    return 0;
}
