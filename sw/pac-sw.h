#ifndef PAC_SW_H
#define PAC_SW_H

#define PAGE_SIZE 0x1000

#define PLAIN_MASK  0x0000FFFFFFFFFFFFUL
#define CIPHER_MASK 0xFFFF000000000000UL

enum {
    DEV_STANDBY = 0,
    DEV_PAC,
    DEV_AUT,
};

enum {
    PAC_STATE = 0,
    PAC_PLAIN,
    PAC_TWEAK,
    PAC_CIPH,
};

static const char *pa_path = "/dev/shm/pa";

#endif
