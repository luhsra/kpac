#ifndef PAC_SW_H
#define PAC_SW_H

#define PAGE_SIZE 0x1000

#define PLAIN_MASK 0x0000FFFFFFFFFFFF
#define CIPH_MASK  0xFFFF000000000000

enum {
    DEV_STANDBY = 0,
    DEV_PAC,
    DEV_AUT,
};

static const char *pa_path = "/dev/shm/pa";

#endif
