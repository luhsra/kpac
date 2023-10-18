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
#include <time.h>

#define STR1(x)  #x
#define STR(x)   STR1(x)

#define NR_RUNS			10000000

#define KPAC_BASE		0x9AC00000000
#define KPAC_OP_PAC		1
#define KPAC_OP_AUT		2
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
//#define CNT_REG "PMCCNTR_EL0"
#define CNT_REG "CNTVCT_EL0"

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

static inline unsigned long cntfrq(void)
{
    unsigned long ret;
    asm volatile ("mrs %0, CNTFRQ_EL0" : "=r" (ret));
    return ret;
}

int main(int argc, char *argv[0])
{
    int opt;
    bool pacpl = false;
    bool svc = false;
    bool kpacd = false;
    bool freq = false;

    while ((opt = getopt(argc, argv, "lsdf")) != -1) {
        switch (opt) {
        case 'l': pacpl = true; break;
        case 's': svc = true; break;
        case 'd': kpacd = true; break;
        case 'f': freq = true; break;
        default:
            fprintf(stderr, "Usage: %s [-lsdf]\n", argv[0]);
            exit(EXIT_FAILURE);
        }
    }

    unsigned long pln = 0x0000DEADBEEFDEAD;
    unsigned long ctx = 0x0000BEEFDEADBEEF;

    if (freq)
        printf("\\drefset{/cntfrq}{%lu}\n", cntfrq());

    if (pacpl) {
        pac_pl_init();

        double cma    = 0;
        double acc    = 0;
        double acc_sq = 0;
        for (int i = 0; i < NR_RUNS; i++) {
            unsigned long t0, t1;

            asm volatile ("isb\n"
                          "mrs %0, " CNT_REG "\n"

                          "1: stp %2, %3, [%4, #" STR(PAC_PL_PLAIN) "]\n"
                          "ldr %2, [%4, #" STR(PAC_PL_CIPHER) "]\n"
                          "cbz %2, 1b\n"

                          "isb\n"
                          "mrs %1, " CNT_REG "\n"
                          : "=&r" (t0), "=&r" (t1), "+&r" (pln)
                          : "r" (ctx),  "r" (PAC_PL_BASE));

            unsigned long diff = t1 - t0;

            cma = (diff + (i+1) * cma) / (i+2);
        }

        printf("\\drefset{/pac-pl}{%g}\n", cma);
    }

    if (svc) {
        double cma    = 0;
        double acc    = 0;
        double acc_sq = 0;
        for (int i = 0; i < NR_RUNS; i++) {
            unsigned long t0, t1;

            unsigned long sp_saved;
            unsigned long lr_saved;

            asm volatile ("mov %2, sp\n"
                          "mov %3, lr\n"
                          "mov lr, %4\n"
                          "mov sp, %5\n"

                          "isb\n"
                          "mrs %0, " CNT_REG "\n"

                          "svc #0x9AC\n"

                          "isb\n"
                          "mrs %1, " CNT_REG "\n"

                          "mov %4, lr\n"
                          "mov sp, %2\n"
                          "mov lr, %3\n"
                          : "=&r" (t0), "=&r" (t1), "=&r" (sp_saved), "=&r" (lr_saved), "+&r" (pln)
                          : "r" (ctx));

            unsigned long diff = t1 - t0;

            cma = (diff + (i+1) * cma) / (i+2);
        }

        printf("\\drefset{/svc}{%g}\n", cma);
    }

    if (kpacd) {
        double cma    = 0;
        for (int i = 0; i < NR_RUNS; i++) {
            unsigned long t0, t1;

            unsigned long op = KPAC_OP_PAC;

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
                          : "=&r" (t0), "=&r" (t1), "+&r" (op), "+&r" (pln)
                          : "r" (ctx),  "r" (KPAC_BASE));

            unsigned long diff = t1 - t0;

            cma = (diff + (i+1) * cma) / (i+2);
        }

        printf("\\drefset{/kpacd}{%g}\n", cma);
    }

    return 0;
}
