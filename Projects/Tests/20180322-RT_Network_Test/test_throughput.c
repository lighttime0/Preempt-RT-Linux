#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#include <sys/time.h>

#define THREAD_NUM 8                //We will create THREAD_NUM threads work at the same time
#define WORK_COUNT 10               //Each thread will do calc() WORK_COUNT times
#define LOOP_COUNT 1000000000       //Each calc() will calculate LOOP_COUNT times

double calc()
{
    srand(time(NULL));
    double a = rand() % 10 + 10;
    int i;
    double b = 3.157411, c = 657.147, res = 0;
    for (i = 0; i < LOOP_COUNT; ++i)
    {   
        switch(i % 4) {                 //Note, don't divided by res, because res may be 0.
            case 0:
                res = res + a - b;
                break;
            case 1:
                res = res / c * a;
                break;
            case 2:
                res = b * res - c;
                break;
            case 3:
                res = b * res / a + c;
                break;
        }
    }
    return res;
}

void *thread_func(void *data)
{
    pthread_t tid = pthread_self();
    int i;
    double res = 0;
    for (i = 0; i < WORK_COUNT; ++i)
        res += calc(); 
    //printf("tid: %lu \t result: %lf\n", (unsigned long)tid, res);
    return NULL;
}

int work()
{
    pthread_t thread[THREAD_NUM];
    int i, ret;

    for (i = 0; i < THREAD_NUM; ++i)
    {
        ret = pthread_create(&thread[i], NULL, thread_func, NULL);
        if (ret) {
            printf("create pthread failed\n");
            return ret;
        }
    }

    for (i = 0; i < THREAD_NUM; ++i)
    {
        ret = pthread_join(thread[i], NULL);
        if (ret)
            printf("join pthread failed: %m\n");
    }
    return ret;
}

int main(int argc, char *argv[])
{
    struct timeval t_start, t_end;
    gettimeofday(&t_start, NULL);

    work();

    gettimeofday(&t_end, NULL);
    double time_val = (t_end.tv_sec - t_start.tv_sec) * 1000000 + t_end.tv_usec - t_start.tv_usec;
    printf("test_throught cost: %.4lf s\n", time_val / 1000000.0);

    exit(0);
}