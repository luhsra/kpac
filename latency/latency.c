#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/mman.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <fcntl.h>
#include <math.h>

#define STR1(x)  #x
#define STR(x)   STR1(x)

#define NR_RUNS			1000000

#define KPAC_BASE		0x9AC00000000
#define KPAC_OP_PAC		1
#define KPAC_PLAIN		8
#define KPAC_CIPHER		24

#define PAC_PL_BASE		0xA0000000UL
#define PAC_PL_LEN		0x2000UL
#define PAC_PL_PLAIN		0
#define PAC_PL_TWEAK		8
#define PAC_PL_CIPHER		16

#define die(fmt, ...)                                                   \
    do {                                                                \
        fprintf(stderr, "[%s:%d]: " fmt "\n",                           \
                __FILE__, __LINE__, ##__VA_ARGS__);                     \
        exit(EXIT_FAILURE);                                             \
    } while (0)

/* Enable in the kernel */
#define CNT_REG "PMCCNTR_EL0"
// #define CNT_REG "CNTVCT_EL0"

static void pac_pl_init()
{
    uintptr_t pac_pl_base = PAC_PL_BASE;
    uintptr_t pac_pl_target = PAC_PL_BASE - 0x1000;

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

static void summary(char *name, unsigned long *buf, unsigned long long sum)
{
    double avg = sum/NR_RUNS;

    double acc = 0;
    for (int i = 0; i < NR_RUNS; i++) {
        acc += (buf[i]-avg)*(buf[i]-avg);
    }
    double std = sqrt(acc / NR_RUNS);

    printf("%s avg = %lg ± %lg\n", name, avg, std/sqrt(NR_RUNS));
}

int main(int argc, char *argv[0])
{
    int opt;
    bool pacpl = false;
    bool svc = false;
    bool kpacd = false;

    while ((opt = getopt(argc, argv, "lsd")) != -1) {
        switch (opt) {
        case 'l': pacpl = true; break;
        case 's': svc = true; break;
        case 'd': kpacd = true; break;
        default:
            fprintf(stderr, "Usage: %s [-lsd]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    static unsigned long buf[NR_RUNS];

    if (pacpl) {
        pac_pl_init();

        unsigned long long acc = 0;
        for (int i = 0; i < NR_RUNS; i++) {
            unsigned long t0, t1;

            unsigned long pln = 0x0000DEADBEEFDEAD;
            unsigned long ctx = 0x0000BEEFDEADBEEF;

            asm volatile ("isb\n"
                          "mrs %0, " CNT_REG "\n"

                          "1: stp %2, %3, [%4, #" STR(PAC_PL_PLAIN) "]\n"
                          "ldr %2, [%4, #" STR(PAC_PL_CIPHER) "]\n"
                          "cbz %2, 1b\n"

                          "isb\n"
                          "mrs %1, " CNT_REG "\n"
                          : "=r" (t0), "=r" (t1), "+r" (pln)
                          : "r" (ctx),  "r" (PAC_PL_BASE));

            buf[i] = t1 - t0;
            acc += t1 - t0;
        }

        summary("pac-pl", buf, acc);
    }

    if (svc) {
        unsigned long long acc = 0;
        for (int i = 0; i < NR_RUNS; i++) {
            unsigned long t0, t1;
            asm volatile ("isb\n"
                          "mrs %0, " CNT_REG "\n"

                          "svc #0x9AC\n"

                          "isb\n"
                          "mrs %1, " CNT_REG "\n"
                          : "=r" (t0), "=r" (t1));

            buf[i] = t1 - t0;
            acc += t1 - t0;
        }

        asm volatile ("svc #0x9AD\n");

        summary("svc", buf, acc);
    }

    if (kpacd) {
        unsigned long long acc = 0;
        for (int i = 0; i < NR_RUNS; i++) {
            unsigned long t0, t1;

            unsigned long op = KPAC_OP_PAC;
            unsigned long pln = 0x0000DEADBEEFDEAD;
            unsigned long ctx = 0x0000BEEFDEADBEEF;

            asm volatile ("isb\n"
                          "mrs %0, " CNT_REG "\n"

                          "stp %3, %4, [%5, #" STR(KPAC_PLAIN) "]\n"
                          "stlr %2, [%5]\n"

                          "sevl\n"
                          "1: wfe\n"
                          "ldxr %2, [%5]\n"
                          "cbnz %2, 1b\n"
                          "ldr %3, [%5, #" STR(KPAC_CIPHER) "]\n"

                          "isb\n"
                          "mrs %1, " CNT_REG "\n"
                          : "=r" (t0), "=r" (t1), "+r" (op), "+r" (pln)
                          : "r" (ctx),  "r" (KPAC_BASE));

            buf[i] = t1 - t0;
            acc += t1 - t0;
        }

        summary("kpacd", buf, acc);
    }

    return 0;
}
