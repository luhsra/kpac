#include <stdio.h>
#include <sys/mman.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdlib.h>

#include "pac-sw.h"

void protected(void);

void map_device(void)
{
    int fd = open(pa_path, O_RDWR);
    if (fd == -1) {
        perror("open");
        exit(1);
    }


    uint64_t *area = mmap((void *) 0xA000000UL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, fd, 0);
    if (area == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
}

int main(void) {
    map_device();
    protected();

    return 0;
}
