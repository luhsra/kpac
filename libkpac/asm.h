#ifndef LIBKPAC_ASM_H
#define LIBKPAC_ASM_H

#include <stdint.h>

typedef uint32_t inst_t;

#define REG_SP 31
#define REG_LR 30

#define mask_at(val, mask, shift) (((val) & ((mask) << (shift))) >> (shift))
#define emit_bl(addr, target)                                           \
    do {                                                                \
        inst_t *p = (inst_t *) addr;                                    \
        inst_t opcode = 0x25 << 26;                                     \
        ptrdiff_t ptrdiff = (intptr_t) (target) - (intptr_t) (addr);    \
        *p = (opcode | ((ptrdiff >> 2) & 0x3FFFFFF));                   \
    } while (0)

#define INST_PACIASP 0xD503233F
#define INST_SVC_PAC 0xD4013581 /* SVC #0x9AC */

#define INST_AUTIASP 0xD50323BF
#define INST_SVC_AUT 0xD40135A1 /* SVC #0x9AD */

/*
 * Stores
 */

static bool stp_pre(inst_t x, int *rn, int *rt1, int *rt2)
{
    /* pre-indexed store-pair,
     * See p. C6-1992 of the reference manual */

    if (mask_at(x, 0x3FF, 22) != 0b1010100110)
        return false;

    *rn = mask_at(x, 0x1F, 5);
    *rt1 = mask_at(x, 0x1F, 0);
    *rt2 = mask_at(x, 0x1F, 10);

    return true;
}

static bool stp_off(inst_t x, int *rn, int *rt1, int *rt2, int *imm)
{
    /* signed offset store-pair,
     * See p. C6-1992 of the reference manual */

    if (mask_at(x, 0x3FF, 22) != 0b1010100100)
        return false;

    *imm = mask_at(x, 0x7F, 15) * 8;

    *rn = mask_at(x, 0x1F, 5);
    *rt1 = mask_at(x, 0x1F, 0);
    *rt2 = mask_at(x, 0x1F, 10);

    return true;
}

static bool str_pre(inst_t x, int *rn, int *rt)
{
    /* pre-indexed store,
     * See p. C6-1995 of the reference manual */

    if (mask_at(x, 0x7FF, 21) != 0b11111000000 ||
        mask_at(x, 0x003, 10) != 0b11)
        return false;

    *rt = mask_at(x, 0x1F, 0);
    *rn = mask_at(x, 0x1F, 5);

    return true;
}

static bool str_off(inst_t x, int *rn, int *rt, int *imm)
{
    /* unsigned offset store,
     * See p. C6-1995 of the reference manual */

    if (mask_at(x, 0x3FF, 22) != 0b1111100100)
        return false;

    *imm = mask_at(x, 0xFFF, 10) * 8;
    *rt = mask_at(x, 0x1F, 0);
    *rn = mask_at(x, 0x1F, 5);

    return true;
}

/*
 * Loads
 */

static bool ldp_post(inst_t x, int *rn, int *rt1, int *rt2)
{
    /* post-indexed load-pair,
     * See p. C6-1666 of the reference manual */

    if (mask_at(x, 0x3FF, 22) != 0b1010100011)
        return false;

    *rn = mask_at(x, 0x1F, 5);
    *rt1 = mask_at(x, 0x1F, 0);
    *rt2 = mask_at(x, 0x1F, 10);

    return true;
}

static bool ldp_off(inst_t x, int *rn, int *rt1, int *rt2, int *imm)
{
    /* signed offset load-pair,
     * See p. C6-1666 of the reference manual */

    if (mask_at(x, 0x3FF, 22) != 0b1010100101)
        return false;

    *imm = mask_at(x, 0x7F, 15) * 8;

    *rn = mask_at(x, 0x1F, 5);
    *rt1 = mask_at(x, 0x1F, 0);
    *rt2 = mask_at(x, 0x1F, 10);

    return true;
}

static bool ldr_post(inst_t x, int *rn, int *rt)
{
    /* post-indexed load,
     * See p. C6-1673 of the reference manual */

    if (mask_at(x, 0x7FF, 21) != 0b11111000010 ||
        mask_at(x, 0x003, 10) != 0b01)
        return false;

    *rt = mask_at(x, 0x1F, 0);
    *rn = mask_at(x, 0x1F, 5);

    return true;
}

static bool ldr_off(inst_t x, int *rn, int *rt, int *imm)
{
    /* unsigned offset load,
     * See p. C6-1673 of the reference manual */

    if (mask_at(x, 0x3FF, 22) != 0b1111100101)
        return false;

    *imm = mask_at(x, 0xFFF, 10) * 8;
    *rt = mask_at(x, 0x1F, 0);
    *rn = mask_at(x, 0x1F, 5);

    return true;
}

/*
 * Arithmetic
 */

static bool sub_imm(inst_t x, int *rn, int *rd)
{
    /* immediate sub,
     * See p. C6-2068 of the reference manual */

    if (mask_at(x, 0x1FF, 23) != 0b110100010)
        return false;

    *rd = mask_at(x, 0x1F, 0);
    *rn = mask_at(x, 0x1F, 5);

    return true;
}


static bool add_imm(inst_t x, int *rn, int *rd)
{
    /* immediate add,
     * See p. C6-1257 of the reference manual */

    if (mask_at(x, 0x1FF, 23) != 0b100100010)
        return false;

    *rd = mask_at(x, 0x1F, 0);
    *rn = mask_at(x, 0x1F, 5);

    return true;
}

static bool add_reg(inst_t x, int *rn, int *rd, int *rm)
{
    /* extended/shifted register add,
     * See p. C6-1254 of the reference manual */

    if (mask_at(x, 0xFF, 24) != 0b10001011)
        return false;

    *rd = mask_at(x, 0x1F, 0);
    *rn = mask_at(x, 0x1F, 5);
    *rm = mask_at(x, 0x1F, 16);

    return true;
}

static bool sub_reg(inst_t x, int *rn, int *rd, int *rm)
{
    /* extended/shifted register sub,
     * See p. C6-1254 of the reference manual */

    if (mask_at(x, 0xFF, 24) != 0b11001011)
        return false;

    *rd = mask_at(x, 0x1F, 0);
    *rn = mask_at(x, 0x1F, 5);
    *rm = mask_at(x, 0x1F, 16);

    return true;
}


static bool mov_imm(inst_t x, int *rd)
{
    /* (inverted) wide immediate mov,
     * See p. C6-1791 of the reference manual */

    if (mask_at(x, 0x1FF, 23) != 0b110100101)
        return false;
    if (mask_at(x, 0x1FF, 23) != 0b100100101)
        return false;

    *rd = mask_at(x, 0x1F, 0);

    return true;
}

#endif                          /* LIBKPAC_ASM_H */
