#include "a2_helper.h"
#include <stdlib.h>
#include <unistd.h>
#include <semaphore.h>
#include <pthread.h>
#include <sys/wait.h>


int nrThreads, done;

typedef struct
{
    int process;
    int thread;
    pthread_mutex_t *lock1, *lock2, *lock3, *lock4;
    sem_t *sem1, *sem2;

} THREAD_PARAM;


void *thread_sync(void *args)
{
    THREAD_PARAM *param = (THREAD_PARAM *)args;

    if (param->process == 5 && param->thread == 5)
    {
        pthread_mutex_lock(param->lock1);
    }

    if (param->process == 5 && param->thread == 2)
    {
        pthread_mutex_lock(param->lock2);
    }

    if (param->process == 3 && param->thread == 1)
    {
        pthread_mutex_lock(param->lock3);
    }

    info(BEGIN, param->process, param->thread);

    if (param->process == 5 && param->thread == 1)
    {
        pthread_mutex_unlock(param->lock1);
    }

    if (param->process == 5 && param->thread == 1)
    {
        pthread_mutex_lock(param->lock4);
    }

    info(END, param->process, param->thread);

    if (param->process == 3 && param->thread == 5)
    {
        pthread_mutex_unlock(param->lock2);
    }
    if (param->process == 5 && param->thread == 2)
    {
        pthread_mutex_unlock(param->lock3);
    }
    if (param->process == 5 && param->thread == 5)
    {
        pthread_mutex_unlock(param->lock4);
    }

    return NULL;
}

void *thread_barrier(void *args)
{
    THREAD_PARAM *param = (THREAD_PARAM *)args;

    sem_wait(param->sem1);

    if (param->thread != 14)
    {
        sem_wait(param->sem2);
        nrThreads++;
        sem_post(param->sem2);
        info(BEGIN, param->process, param->thread);
    }

    if (param->thread != 14)
    {
        sem_wait(param->sem2);
        if (nrThreads == 5 && done == 0)
        {
            info(BEGIN, 4, 14);
            info(END, 4, 14);
            done = 1;
        }
        sem_post(param->sem2);
        sem_wait(param->sem2);
        nrThreads--;
        sem_post(param->sem2);
        info(END, param->process, param->thread);
    }

    sem_post(param->sem1);

    return NULL;
}

int main()
{
    pthread_mutex_t lock1 = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t lock2 = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t lock3 = PTHREAD_MUTEX_INITIALIZER;
    pthread_mutex_t lock4 = PTHREAD_MUTEX_INITIALIZER;
    sem_t semaphore, count_semaphore;

    int pid[8];

    init();

    info(BEGIN, 1, 0);
    pid[2] = fork();
    if (pid[2] == 0)
    {
        info(BEGIN, 2, 0);
        pid[7] = fork();
        if (pid[7] == 0)
        {
            info(BEGIN, 7, 0);
            info(END, 7, 0);
            exit(2);
        }
        else
        {
            waitpid(pid[7], 0, 0);
            info(END, 2, 0);
            exit(2);
        }
    }
    pid[3] = fork();
    if (pid[3] == 0)
    {
        info(BEGIN, 3, 0);
        info(END, 3, 0);
        exit(2);
    }

    pid[4] = fork();
    if (pid[4] == 0)
    {
        info(BEGIN, 4, 0);

        pid[5] = fork();
        if (pid[5] == 0)
        {
            pthread_t tid[11];
            THREAD_PARAM parameters[11];

            info(BEGIN, 5, 0);

            for (int i = 0; i < 11; i++)
            {
                if (i <= 5)
                {
                    parameters[i].process = 3;
                    parameters[i].thread = i + 1;
                }
                else
                {
                    parameters[i].process = 5;
                    parameters[i].thread = i - 5;
                }

                parameters[i].lock1 = &lock1;
                parameters[i].lock2 = &lock2;
                parameters[i].lock3 = &lock3;
                parameters[i].lock4 = &lock4;
            }

            pthread_mutex_lock(&lock1);
            pthread_mutex_lock(&lock2);
            pthread_mutex_lock(&lock3);
            pthread_mutex_lock(&lock4);

            for (int i = 0; i < 11; i++)
                pthread_create(&tid[i], NULL, thread_sync, &parameters[i]);
            for (int i = 0; i < 11; i++)
                pthread_join(tid[i], NULL);

            info(END, 5, 0);
            exit(0);
        }

        else
        {
            waitpid(pid[5], 0, 0);
            pthread_t tid[35];

            THREAD_PARAM parameters[35];

            for (int i = 0; i < 35; i++)
            {
                parameters[i].process = 4;
                parameters[i].thread = i + 1;
                parameters[i].sem1 = &semaphore;
                parameters[i].sem2 = &count_semaphore;
            }
            sem_init(&semaphore, 0, 5);
            sem_init(&count_semaphore, 0, 1);

            for (int i = 0; i < 35; i++)
                pthread_create(&tid[i], NULL, thread_barrier, &parameters[i]);

            for (int i = 0; i < 35; i++)
                pthread_join(tid[i], NULL);

            info(END, 4, 0);
            exit(2);
        }
    }

    pid[6] = fork();
    if (pid[6] == 0)
    {
        info(BEGIN, 6, 0);
        info(END, 6, 0);
        exit(2);
    }

    waitpid(pid[2], 0, 0);
    waitpid(pid[3], 0, 0);
    waitpid(pid[4], 0, 0);
    waitpid(pid[6], 0, 0);

    info(END, 1, 0);

    sem_destroy(&semaphore);
    sem_destroy(&count_semaphore);

    pthread_mutex_destroy(&lock1);
    pthread_mutex_destroy(&lock2);
    pthread_mutex_destroy(&lock3);
    pthread_mutex_destroy(&lock4);

    return 0;
}
