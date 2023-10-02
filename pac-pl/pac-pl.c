#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <sys/mman.h>
#include <errno.h>
#include <fcntl.h>
#include <string.h>

#define PAC_PL_ADDR	0xA0000000UL
#define PAC_PL_TARGET	0x9FFFF000UL /* puts user part to 0xA0000000 */
#define PAC_PL_LEN	0x2000UL

#define die(fmt, ...)                                                   \
    do {                                                                \
        fprintf(stderr, "[%s:%d]: " fmt "\n",                           \
                __FILE__, __LINE__, ##__VA_ARGS__);                     \
        exit(EXIT_FAILURE);                                             \
    } while (0)

__attribute__ ((constructor))
void pac_pl_init()
{
    uintptr_t pac_pl_base = PAC_PL_ADDR;
    uintptr_t pac_pl_target = PAC_PL_TARGET;

    int fd = open("/dev/mem", O_RDWR | O_SYNC);
    if (fd == -1)
        die("open(/dev/mem): %s", strerror(errno));

    void *addr = mmap((void *) pac_pl_target, PAC_PL_LEN, PROT_READ | PROT_WRITE,
                      MAP_SHARED | MAP_POPULATE | MAP_FIXED, fd, pac_pl_base);
    if (addr == MAP_FAILED)
        die("mmap(/dev/mem): %s", strerror(errno));

    /* Test magic values */
    uint64_t *dev = addr;
    if (dev[3] != 0xDEADBEEFDEADBEEE ||
        dev[512+3] != 0xDEADBEEEDEADBEEF)
        die("device test failed");
}
