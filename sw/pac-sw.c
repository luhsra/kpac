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

    while (1) {
        uint64_t cipher, plain, tweak, orig;
        uint64_t state;

        __atomic_load(area + PAC_STATE, &state, __ATOMIC_ACQUIRE);
        switch (state) {
        case DEV_PAC:
            plain = area[PAC_PLAIN] & PLAIN_MASK;
            tweak = area[PAC_TWEAK];

            cipher = qarma64_enc(plain, tweak, w0, k0, NUM_ROUNDS);
            cipher = (plain & PLAIN_MASK) | (cipher & CIPHER_MASK);

            printf("PAC: (%016lX, %016lX) -> %016lX\n", plain, tweak, cipher);

            area[PAC_CIPH] = cipher;
            state = DEV_STANDBY;
            __atomic_store(area + PAC_STATE, &state, __ATOMIC_RELEASE);
            break;

        case DEV_AUT:
            orig = area[PAC_CIPH];
            tweak = area[PAC_TWEAK];

            plain = orig & PLAIN_MASK;
            cipher = qarma64_enc(plain, tweak, w0, k0, NUM_ROUNDS);
            if ((orig & CIPHER_MASK) != (cipher & CIPHER_MASK))
                plain |= (1UL << 63);

            printf("AUT: (%016lX, %016lX) -> %016lX\n", orig, tweak, plain);

            area[PAC_PLAIN] = plain;
            state = DEV_STANDBY;
            __atomic_store(area + PAC_STATE, &state, __ATOMIC_RELEASE);
            break;
        }

    }
}
