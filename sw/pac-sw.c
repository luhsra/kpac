#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <signal.h>

#include "pac-sw.h"
#include "qarma.h"

#define NUM_ROUNDS 1

static volatile int done = 0;

void sigint_handler()
{
    done = 1;
}

int main(void)
{
    struct {
        unsigned int pac, aut;
    } counter = { 0, 0 };

    qkey_t w0 = 0, k0 = 0;

    int fd = open(pa_path, O_RDWR | O_CREAT, 0666);
    if (fd == -1) {
        perror("creat");
        return 1;
    }

    if (ftruncate(fd, PAGE_SIZE)) {
        perror("ftruncate");
        return 1;
    }

    uint64_t *area = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (area == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    signal(SIGINT, sigint_handler);

    while (!done) {
        uint64_t state;
        uint64_t plain, tweak, cipher, recipher;

        state = __atomic_load_n(area + PAC_STATE, __ATOMIC_ACQUIRE);
        switch (state) {
        case DEV_PAC:
            counter.pac++;
            area[PAC_CIPH] = area[PAC_PLAIN];
            break;

        case DEV_AUT:
            counter.aut++;
            area[PAC_PLAIN] = area[PAC_CIPH];
            break;

        default:
            continue;
        }

        __atomic_store_n(area + PAC_STATE, DEV_STANDBY, __ATOMIC_RELEASE);

#ifdef __aarch64__
        asm ("sev");
#endif
    }

    printf("pac: %u\naut: %u\n", counter.pac, counter.aut);

    return 0;
}
