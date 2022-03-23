#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

#include "pac-sw.h"

void *pac(uint64_t *base, void *plain, uintptr_t twk) {
    uint64_t status;

    base[1] = (uintptr_t) plain;
    base[2] = twk;

    status = DEV_PAC;
    __atomic_store(&base[0], &status, __ATOMIC_RELEASE);

    do {
        __atomic_load(&base[0], &status, __ATOMIC_ACQUIRE);
    } while (status != DEV_STANDBY);

    return (void *) base[3];
}

void *aut(uint64_t *base, void *ciph, uintptr_t twk) {
    uint64_t status;

    base[3] = (uintptr_t) ciph;
    base[2] = twk;

    status = DEV_AUT;
    __atomic_store(&base[0], &status, __ATOMIC_RELEASE);

    do {
        __atomic_load(&base[0], &status, __ATOMIC_ACQUIRE);
    } while (status != DEV_STANDBY);

    return (void *) base[1];
}

int main(void) {
    int fd = open(pa_path, O_RDWR);
    if (fd == -1) {
        perror("open");
        return 1;
    }


    uint64_t *area = mmap(NULL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    if (area == MAP_FAILED) {
        perror("mmap");
        return 1;
    }

    int *tmp = pac(area, &fd, 0xFACADE);
    printf("tmp: %p\n", tmp);
    tmp = aut(area, tmp, 0xFACADE);

    printf("tmp: %p\n", tmp);
    printf("*tmp: %d\n", *tmp);

    return 0;
}
