#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>

#define PAGE_SIZE 0x1000

void f2()
{
    int b = 6;
    b = b + 6;
    return;
}

void f1()
{
    int a = 5;
    a = a + 5;
    int ar[40];
    f2();
    ar[2] += 3;
    return;
}

void map_device(void)
{
    int fd = open("/dev/shm/pa", O_RDWR);
    if (fd == -1) {
        perror("open");
        exit(1);
    }


    unsigned long *area = mmap((void *) 0xA000000UL, PAGE_SIZE, PROT_READ|PROT_WRITE, MAP_SHARED|MAP_FIXED, fd, 0);
    if (area == MAP_FAILED) {
        perror("mmap");
        exit(1);
    }
}

__attribute((pac_exclude)) int main()
{
    map_device();
    f1();
    return 1;
}
