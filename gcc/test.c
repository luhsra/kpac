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

void map_device(void);

int main(int argc, char *argv[])
{
    printf("%s\n", argv[0]);
    f1();
    return 1;
}
