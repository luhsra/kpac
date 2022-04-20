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
        uint64_t state, stdby = DEV_STANDBY;
        uint64_t plain, tweak, cipher, recipher;

        __atomic_load(area + PAC_STATE, &state, __ATOMIC_ACQUIRE);
        switch (state) {
        case DEV_PAC:
            plain = area[PAC_PLAIN] & PLAIN_MASK;
            tweak = area[PAC_TWEAK];

            cipher = 0x1111111111111111;// qarma64_enc(plain, tweak, w0, k0, NUM_ROUNDS);
            cipher = (plain & PLAIN_MASK) | (cipher & CIPHER_MASK);

            /* printf("PAC: (%016lX, %016lX) -> %016lX\n", plain, tweak, cipher); */
            counter.pac++;

            area[PAC_CIPH] = cipher;
            break;

        case DEV_AUT:
            cipher = area[PAC_CIPH];
            tweak = area[PAC_TWEAK];
            plain = cipher & PLAIN_MASK;

            recipher = 0x1111111111111111;//qarma64_enc(plain, tweak, w0, k0, NUM_ROUNDS);
            if ((cipher & CIPHER_MASK) != (recipher & CIPHER_MASK))
                plain |= (1UL << 63);

            /* printf("AUT: (%016lX, %016lX) -> %016lX\n", orig, tweak, plain); */
            counter.aut++;

            area[PAC_PLAIN] = plain;
            break;

        default:
            continue;
        }

        __atomic_store(area + PAC_STATE, &stdby, __ATOMIC_RELEASE);

#ifdef __aarch64__
        asm ("sev");
#endif
    }

    printf("pac: %u\naut: %u\n", counter.pac, counter.aut);

    return 0;
}
