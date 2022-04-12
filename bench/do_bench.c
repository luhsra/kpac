#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/time.h>
#include <time.h>
#include <math.h>

static inline void timespec_diff(struct timespec *a, struct timespec *b,
                                 struct timespec *result) {
    result->tv_sec  = a->tv_sec  - b->tv_sec;
    result->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (result->tv_nsec < 0) {
        --result->tv_sec;
        result->tv_nsec += 1000000000L;
    }
}
int main(int argc, char *argv[])
{
    pid_t cpid;
    int wstatus;
    struct timespec tp0, tp1, diff;

    if (argc < 2) {
        fprintf(stderr, "Usage: %s benchmark [args]\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    clock_gettime(CLOCK_MONOTONIC_RAW, &tp0);
    cpid = fork();

    if (cpid == -1) {
        perror("fork");
        exit(EXIT_FAILURE);
    }

    if (cpid == 0) {
        execve(argv[1], argv+1, NULL);
        perror("execve");
        exit(127);
    }

    do {
        waitpid(cpid, &wstatus, 0);
    } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));

    clock_gettime(CLOCK_MONOTONIC_RAW, &tp1);

    if (WIFSIGNALED(wstatus)) {
        fprintf(stderr, "%s terminated due to signal\n", argv[1]);
        exit(EXIT_FAILURE);
    } else if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) != 0) {
        fprintf(stderr, "%s terminated with non-zero return code\n", argv[1]);
        exit(EXIT_FAILURE);
    }

    timespec_diff(&tp1, &tp0, &diff);

    printf("%ld.%09ld\n", diff.tv_sec, diff.tv_nsec);

    return EXIT_SUCCESS;
}
