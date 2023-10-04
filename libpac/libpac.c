#define _GNU_SOURCE
#include <stdio.h>
#include <link.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <errno.h>
#include <stddef.h>

#include "asm.h"

#define PAGE_SIZE 4096

#define ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#define ALIGN_UP(x, a)		ALIGN_MASK(x, (__typeof__(x))(a) - 1)

#ifdef DEBUG
#define log(fmt, ...) fprintf(stderr, "libpac: " fmt "\n", ##__VA_ARGS__)
#else
#define log(fmt, ...) ((void) 0)
#endif

#define die(fmt, ...)                                                   \
    do {                                                                \
        fprintf(stderr, "[%s:%d]: " fmt "\n",                           \
                __FILE__, __LINE__, ##__VA_ARGS__);                     \
        exit(EXIT_FAILURE);                                             \
    } while (0)

extern char *program_invocation_name;

extern char __start_text_do_pac;
extern void do_pac_8(void);
extern void do_pac_0(void);
extern char __stop_text_do_pac;

extern char __start_text_do_aut;
extern void do_aut_8(void);
extern void do_aut_0(void);
extern char __stop_text_do_aut;

static bool pv_mode = false;

static bool patch_paciasp(inst_t *text, size_t len, size_t i, inst_t *pac[2])
{
    int rn, rd, rt1, rt2, off;

    if (!pv_mode || i + 1 >= len)
        goto fallback;

    /* paciasp
     * stp x29, x30, [sp, #-N]! */
    if (stp_pre(text[i+1], &rn, &rt1, &rt2) &&
        rn == REG_SP && (rt1 == REG_LR || rt2 == REG_LR)) {

        text[i] = text[i+1];
        emit_bl(&text[i+1], pac[rt1 == REG_LR ? 0 : 1]);
        return true;
    }

    /* paciasp
     * str x30, [sp, #-N]! */
    if (str_pre(text[i+1], &rn, &rt1) &&
        rn == REG_SP && rt1 == REG_LR) {

        text[i] = text[i+1];
        emit_bl(&text[i+1], pac[0]);
        return true;
    }

    /* paciasp
     * sub sp, sp, #N
     * (stp/str) */
    if (i + 2 < len &&
        sub_imm(text[i+1], &rn, &rd) &&
        rn == REG_SP && rd == REG_SP) {

        /* stp x29, x30, [sp] */
        if (stp_off(text[i+2], &rn, &rt1, &rt2, &off) && off == 0 &&
            rn == REG_SP && (rt1 == REG_LR || rt2 == REG_LR)) {

            text[i] = text[i+1];
            text[i+1] = text[i+2];
            emit_bl(&text[i+2], pac[rt1 == REG_LR ? 0 : 1]);
            return true;
        }

        /* str x30, [sp] */
        if (str_off(text[i+2], &rn, &rt1, &off) && off == 0 &&
            rn == REG_SP && rt1 == REG_LR) {

            text[i] = text[i+1];
            text[i+1] = text[i+2];
            emit_bl(&text[i+2], pac[0]);
            return true;
        }
    }

fallback:
    text[i] = INST_SVC_PAC;
    return false;
}

static bool patch_autiasp(inst_t *text, size_t len, size_t i, inst_t *aut[2])
{
    int rn, rd, rt1, rt2, off;

    if (!pv_mode || i < 1)
        goto fallback;

    /* ldp x29, x30, [sp], #N
     * autiasp */
    if (ldp_post(text[i-1], &rn, &rt1, &rt2) &&
        rn == REG_SP && (rt1 == REG_LR || rt2 == REG_LR)) {

        text[i] = text[i-1];
        emit_bl(&text[i-1], aut[rt1 == REG_LR ? 0 : 1]);
        return true;
    }

    /* ldr x30, [sp], #N
     * autiasp */
    if (ldr_post(text[i-1], &rn, &rt1) &&
        rn == REG_SP && rt1 == REG_LR) {

        text[i] = text[i-1];
        emit_bl(&text[i-1], aut[0]);
        return true;
    }

    /* (ldp/ldr)
     * add sp, sp, #N
     * autiasp */
    if (i < 2 &&
        add_imm(text[i-1], &rn, &rd) &&
        rn == REG_SP && rd == REG_SP) {

        /* ldp x29, x30, [sp], #N */
        if (ldp_off(text[i-2], &rn, &rt1, &rt2, &off) && off == 0 &&
            rn == REG_SP && (rt1 == REG_LR || rt2 == REG_LR)) {

            text[i] = text[i-1];
            text[i-1] = text[i-2];
            emit_bl(&text[i-2], aut[rt1 == REG_LR ? 0 : 1]);
            return true;
        }

        /* ldr x30, [sp], #N */
        if (ldr_off(text[i-2], &rn, &rt1, &off) && off == 0 &&
            rn == REG_SP && rt1 == REG_LR) {

            text[i] = text[i-1];
            text[i-1] = text[i-2];
            emit_bl(&text[i-2], aut[0]);
            return true;
        }
    }

fallback:
    text[i] = INST_SVC_AUT;
    return false;
}

static int phdr_patch(inst_t *text, size_t len, inst_t *pac[2], inst_t *aut[2])
{
    for (size_t i = 0; i < len; i++) {
        switch (text[i]) {
        case INST_PACIASP:
            if (patch_paciasp(text, len, i, pac))
                log("%p patched pac call", &text[i]);
            else
                log("%p patched pac svc", &text[i]);
            break;
        case INST_AUTIASP:
            if (patch_autiasp(text, len, i, aut))
                log("%p patched aut call", &text[i]);
            else
                log("%p patched aut svc", &text[i]);
            break;
        }
    }

    return 0;
}

static void allocate_routines(uintptr_t vaddr_end, inst_t *pac[2], inst_t *aut[2])
{
    uintptr_t pac_start = (uintptr_t) &__start_text_do_pac;
    uintptr_t pac_stop = (uintptr_t) &__stop_text_do_pac;
    uintptr_t aut_start = (uintptr_t) &__start_text_do_aut;
    uintptr_t aut_stop = (uintptr_t) &__stop_text_do_aut;

    size_t pac_len = pac_stop - pac_start;
    size_t aut_len = aut_stop - aut_start;

    vaddr_end = ALIGN_UP(vaddr_end, PAGE_SIZE);

    /* Allocate a page for pac/aut routines within +-128 MiB of the segment.
     * They must be callable using BL instruction. */
    uintptr_t min = vaddr_end - 128 * 1024 * 1024;
    void *addr = mmap((void *) min, pac_len+aut_len,
                          PROT_READ | PROT_WRITE,
                          MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED)
        die("mmap: %s", strerror(errno));

    pac[0] = addr + ((uintptr_t) do_pac_0 - pac_start);
    pac[1] = addr + ((uintptr_t) do_pac_8 - pac_start);

    aut[0] = addr + pac_len + ((uintptr_t) do_aut_0 - aut_start);
    aut[1] = addr + pac_len + ((uintptr_t) do_aut_8 - aut_start);

    memcpy(addr,         (void *) pac_start, pac_len);
    memcpy(addr+pac_len, (void *) aut_start, aut_len);

    int ret = mprotect(addr, pac_len+aut_len, PROT_READ | PROT_EXEC);
    if (ret)
        die("mprotect: %s", strerror(errno));
}

static int phdr_callback(struct dl_phdr_info *info, size_t _size, void *_data)
{
    static int cnt = 0;

    const char *filename = program_invocation_name;
    if (info->dlpi_name[0] != '\0')
        filename = info->dlpi_name;

    if (!strcmp(filename, "linux-vdso.so.1") ||
        !strcmp(filename, "libpac.so")) {
        log("[%d:%s] skipping", cnt, filename);
        return 0;
    }

    for (size_t i = 0; i < info->dlpi_phnum; i++) {
        const ElfW(Phdr) *phdr = &info->dlpi_phdr[i];

        if (phdr->p_type != PT_LOAD)
            continue;
        if (!(phdr->p_flags & PF_X))
            continue;

        inst_t *vaddr_start = (inst_t *) (info->dlpi_addr + phdr->p_vaddr);
        size_t size = phdr->p_memsz;
        inst_t *vaddr_end = (inst_t *) ((uintptr_t) vaddr_start + size);

        inst_t *pac[2], *aut[2];
        allocate_routines((uintptr_t) vaddr_end, pac, aut);

        log("[%d:%s] patching segment %p-%p", cnt, filename, vaddr_start, vaddr_end);

        /* Need PROT_EXEC here to be able to execute mprotect in libc later */
        if (mprotect(vaddr_start, size, PROT_READ | PROT_EXEC | PROT_WRITE))
            die("mprotect: %s", strerror(errno));

        phdr_patch(vaddr_start, vaddr_end-vaddr_start, pac, aut);

        if (mprotect(vaddr_start, size, PROT_READ | PROT_EXEC))
            die("mprotect: %s", strerror(errno));
    }

    cnt++;

    return 0;
}

__attribute__ ((constructor))
void libpac_init()
{
    char *pv_mode_env = getenv("LIBPAC_PV");
    if (pv_mode_env && !strcmp(pv_mode_env, "1"))
        pv_mode = true;

    if (dl_iterate_phdr(phdr_callback, NULL))
        die("dl_iterate_phdr");
}
