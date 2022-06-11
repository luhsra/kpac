#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

#define NR_THREADS 8

int f2(char s[10])
{
    int x;
    sscanf(s, "%d", &x);
    return x;
}

int f1(volatile int x)
{
    char arr[10];
    snprintf(arr, sizeof(arr), "%d", x);
    return f2(arr);
}

void *worker()
{
    time_t t0 = time(NULL);
    while (time(NULL) <= t0 + 4) {
        int x = 42;
        x = f1(x);
        if (x != 42)
            exit(1);
    }

    return NULL;
}

int main()
{
    pthread_t threads[NR_THREADS];

    for (int i = 0; i < NR_THREADS; i++) {
        pthread_create(&threads[i], NULL, worker, NULL);
    }

    for (int i = 0; i < NR_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    return 0;
}
