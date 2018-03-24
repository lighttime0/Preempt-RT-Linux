#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/time.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <sys/mman.h>

#define THREAD_NUM 8                //We will create THREAD_NUM threads work at the same time
#define WORK_COUNT 10               //Each thread will do calc() WORK_COUNT times
#define LOOP_COUNT 200000000       //Each calc() will calculate LOOP_COUNT times

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

int non_rt_work()
{
    pthread_t thread[THREAD_NUM];
    int i, ret;
 
    /* Create a pthread with specified attributes */
    for (i = 0; i < THREAD_NUM; ++i)
    {
        ret = pthread_create(&thread[i], NULL, thread_func, NULL);
        if (ret) {
            printf("create pthread failed\n");
            return ret;
        }
    }
 
    /* Join the thread and wait until it is done */
    for (i = 0; i < THREAD_NUM; ++i)
    {
        ret = pthread_join(thread[i], NULL);
        if (ret)
            printf("join pthread failed: %m\n");
    }

    return ret;
}

int rt_work()
{
    struct sched_param param;
    pthread_attr_t attr;
    pthread_t thread[THREAD_NUM];
    int i, ret;

    /* Lock memory */
    if(mlockall(MCL_CURRENT|MCL_FUTURE) == -1) {
            printf("mlockall failed: %m\n");
            exit(-2);
    }
 
    /* Initialize pthread attributes (default values) */
    ret = pthread_attr_init(&attr);
    if (ret) {
        printf("init pthread attributes failed\n");
        return ret;
    }
 
    /* Set a specific stack size  */
    ret = pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN);
    if (ret) {
        printf("pthread setstacksize failed\n");
        return ret;
    }

    /* Set scheduler policy and priority of pthread */
    ret = pthread_attr_setschedpolicy(&attr, SCHED_FIFO);
    if (ret) {
        printf("pthread setschedpolicy failed\n");
        return ret;
    }
    param.sched_priority = 80;
    ret = pthread_attr_setschedparam(&attr, &param);
    if (ret) {
        printf("pthread setschedparam failed\n");
        return ret;
    }
    /* Use scheduling parameters of attr */
    ret = pthread_attr_setinheritsched(&attr, PTHREAD_EXPLICIT_SCHED);
    if (ret) {
        printf("pthread setinheritsched failed\n");
        return ret;
    }
 
    /* Create a pthread with specified attributes */
    for (i = 0; i < THREAD_NUM; ++i)
    {
        ret = pthread_create(&thread[i], &attr, thread_func, NULL);
        if (ret) {
            printf("create pthread failed\n");
            return ret;
        }
    }
 
    /* Join the thread and wait until it is done */
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

    int ret;
    if ( argc == 2 && (strcmp(argv[1], "-n") == 0) )
        ret = non_rt_work();
    else if ( argc == 2 && (strcmp(argv[1], "-r") == 0) )
        ret = rt_work();
    else
    {
        printf("Usage: \n");
        printf("network_test <option>\n\n");
        printf("-n Non RealTime mode \t \t Run this Server program in Non-RealTime model.\n");
        printf("-r RealTime mode \t \t Run this Server program in RealTime model.\n");
        printf("-h Help \t \t Show help information");
    }

    gettimeofday(&t_end, NULL);
    double time_val = (t_end.tv_sec - t_start.tv_sec) * 1000000 + t_end.tv_usec - t_start.tv_usec;
    printf("test_latency cost: %.4lf s\n", time_val / 1000000.0);

    exit(0);
}