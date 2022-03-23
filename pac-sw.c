#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>

#include "pac-sw.h"
#include "qarma.h"

#define NUM_ROUNDS 1

int main(void)
{
    qkey_t w0 = 0, k0 = 0;

    int fd = open(pa_path, O_RDWR, 0666);
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

    while (1) {
        uint64_t cph, cph_orig, pln, twk, state, ok;

        __atomic_load(&area[0], &state, __ATOMIC_ACQUIRE);
        switch (state) {
        case DEV_PAC:
            printf("PAC start\n");

            pln = area[1] & PLAIN_MASK;
            twk = area[2];
            cph = qarma64_enc(pln, twk, w0, k0, NUM_ROUNDS);

            printf("PAC: (pln: %lx, twk: %lx) -> cph: %lx\n", pln, twk, cph);

            area[3] = (cph & CIPH_MASK) | pln;
            state = DEV_STANDBY;
            __atomic_store(&area[0], &state, __ATOMIC_RELEASE);
            break;

        case DEV_AUT:
            printf("AUT start\n");

            pln = area[3] & PLAIN_MASK;
            cph_orig = area[3] & CIPH_MASK;
            twk = area[2];
            cph = qarma64_enc(pln, twk, w0, k0, NUM_ROUNDS);

            ok = ((cph & CIPH_MASK) == cph_orig);
            printf("AUT: (pln: %lx, twk: %lx) -> cph: %lx -> %s\n", pln, twk, cph, ok ? "ok" : "fail");

            area[1] = ok ? pln : 0xFFFFFFFFDEADBEEF;
            state = DEV_STANDBY;
            __atomic_store(&area[0], &state, __ATOMIC_RELEASE);
            break;
        }

    }
}
