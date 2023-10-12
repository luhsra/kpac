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
#include <unistd.h>
#include <time.h>

#include "asm.h"

#define ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#define ALIGN_UP(x, a)		ALIGN_MASK(x, (__typeof__(x))(a) - 1)
#define ALIGN_DOWN(x, a)	ALIGN_UP((x) - ((a) - 1), (a))

#define MIBI			(1024*1024)

#ifdef DEBUG
#define log(fmt, ...) fprintf(stderr, "libkpac: " fmt "\n", ##__VA_ARGS__)
#else
#define log(fmt, ...) ((void) 0)
#endif

#define die(fmt, ...)                                                   \
    do {                                                                \
        fprintf(stderr, "[%s:%d]: " fmt "\n",                           \
                __FILE__, __LINE__, ##__VA_ARGS__);                     \
        exit(EXIT_FAILURE);                                             \
    } while (0)

struct pac_procs {
    void *pac_0, *pac_8;
    void *aut_0, *aut_8;
};

extern char *program_invocation_name;

extern char __start_text_kpac;
extern void kpac_pac_8(void);
extern void kpac_pac_0(void);
extern void kpac_aut_8(void);
extern void kpac_aut_0(void);
extern char __stop_text_kpac;

static bool svc_mode = false;
static FILE *stat_file = NULL;

struct patch_stat {
    long total_pac, total_aut;
    long patched_pac, patched_aut;
};

static struct patch_stat total_stat = { 0 };

static char *run_id = "";
static pid_t pid;
static char exe_path[1024] = { 0 };

static long page_size = 0;

static inline void timespec_diff(struct timespec *a, struct timespec *b,
                                 struct timespec *result)
{
    result->tv_sec  = a->tv_sec  - b->tv_sec;
    result->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (result->tv_nsec < 0) {
        --result->tv_sec;
        result->tv_nsec += 1000000000L;
    }
}

static bool patch_paciasp(inst_t *text, size_t len, size_t i, struct pac_procs *procs)
{
    int rn, rd, rt1, rt2, off;

    if (svc_mode || i + 1 >= len)
        goto fallback;

    if ((uintptr_t) procs->pac_0 > (uintptr_t) text+128*MIBI ||
        (uintptr_t) procs->pac_8 < (uintptr_t) text-128*MIBI) {
        log("%p: cannot bl, out of range");
        goto fallback;
    }

    /* paciasp
     * stp x29, x30, [sp, #-N]! */
    if (stp_pre(text[i+1], &rn, &rt1, &rt2) &&
        rn == REG_SP && (rt1 == REG_LR || rt2 == REG_LR)) {

        text[i] = text[i+1];
        emit_bl(&text[i+1], rt1 == REG_LR ? procs->pac_0 : procs->pac_8);
        return true;
    }

    /* paciasp
     * str x30, [sp, #-N]! */
    if (str_pre(text[i+1], &rn, &rt1) &&
        rn == REG_SP && rt1 == REG_LR) {

        text[i] = text[i+1];
        emit_bl(&text[i+1], procs->pac_0);
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
            emit_bl(&text[i+2], rt1 == REG_LR ? procs->pac_0 : procs->pac_8);
            return true;
        }

        /* str x30, [sp] */
        if (str_off(text[i+2], &rn, &rt1, &off) && off == 0 &&
            rn == REG_SP && rt1 == REG_LR) {

            text[i] = text[i+1];
            text[i+1] = text[i+2];
            emit_bl(&text[i+2], procs->pac_0);
            return true;
        }
    }

fallback:
    text[i] = INST_SVC_PAC;
    return false;
}

static bool patch_autiasp(inst_t *text, size_t len, size_t i, struct pac_procs *procs)
{
    int rn, rd, rt1, rt2, off;

    if (svc_mode || i < 1)
        goto fallback;

    if ((uintptr_t) procs->aut_0 > (uintptr_t) text+128*MIBI ||
        (uintptr_t) procs->aut_8 < (uintptr_t) text-128*MIBI) {
        log("%p: cannot bl, out of range");
        goto fallback;
    }

    /* ldp x29, x30, [sp], #N
     * autiasp */
    if (ldp_post(text[i-1], &rn, &rt1, &rt2) &&
        rn == REG_SP && (rt1 == REG_LR || rt2 == REG_LR)) {

        text[i] = text[i-1];
        emit_bl(&text[i-1], rt1 == REG_LR ? procs->aut_0 : procs->aut_8);
        return true;
    }

    /* ldr x30, [sp], #N
     * autiasp */
    if (ldr_post(text[i-1], &rn, &rt1) &&
        rn == REG_SP && rt1 == REG_LR) {

        text[i] = text[i-1];
        emit_bl(&text[i-1], procs->aut_0);
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
            emit_bl(&text[i-2], rt1 == REG_LR ? procs->aut_0 : procs->aut_8);
            return true;
        }

        /* ldr x30, [sp], #N */
        if (ldr_off(text[i-2], &rn, &rt1, &off) && off == 0 &&
            rn == REG_SP && rt1 == REG_LR) {

            text[i] = text[i-1];
            text[i-1] = text[i-2];
            emit_bl(&text[i-2], procs->aut_0);
            return true;
        }
    }

fallback:
    text[i] = INST_SVC_AUT;
    return false;
}

static int phdr_patch(char *filename, inst_t *text, size_t len, struct pac_procs *procs,
                      struct patch_stat *stat)
{
    for (size_t i = 0; i < len; i++) {
        switch (text[i]) {
        case INST_PACIASP:
            log("%s: %p found pac", filename, &text[i]);
            stat->total_pac++;

            if (patch_paciasp(text, len, i, procs)) {
                stat->patched_pac++;
                log("%s: %p patched pac", filename, &text[i]);
            }

            break;
        case INST_AUTIASP:
            log("%s: %p found aut", filename, &text[i]);
            stat->total_aut++;

            if (patch_autiasp(text, len, i, procs)) {
                stat->patched_aut++;
                log("%s: %p patched aut", filename, &text[i]);
            }

            break;
        }
    }

    return 0;
}

static void allocate_procs(uintptr_t vaddr_end, struct pac_procs *procs)
{
    uintptr_t procs_start = (uintptr_t) &__start_text_kpac;
    uintptr_t procs_stop  = (uintptr_t) &__stop_text_kpac;

    size_t procs_len = procs_stop - procs_start;

    vaddr_end = ALIGN_UP(vaddr_end, page_size);

    /* Allocate a page for pac/aut routines within +-128 MiB of the segment.
     * They must be callable using BL instruction. */
    uintptr_t min = vaddr_end - 128 * MIBI;
    void *addr = mmap((void *) min, procs_len,
                      PROT_READ | PROT_WRITE,
                      MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (addr == MAP_FAILED)
        die("mmap: %s", strerror(errno));

    memcpy(addr, (void *) procs_start, procs_len);

    int ret = mprotect(addr, procs_len, PROT_READ | PROT_EXEC);
    if (ret)
        die("mprotect: %s", strerror(errno));

    procs->pac_0 = addr + ((uintptr_t) kpac_pac_0 - procs_start);
    procs->pac_8 = addr + ((uintptr_t) kpac_pac_8 - procs_start);
    procs->aut_0 = addr + ((uintptr_t) kpac_aut_0 - procs_start);
    procs->aut_8 = addr + ((uintptr_t) kpac_aut_8 - procs_start);
}

static int phdr_callback(struct dl_phdr_info *info, size_t _size, void *_data)
{
    static int cnt = 0;
    struct patch_stat stat = { 0 };

    struct timespec tp0, tp1, diff;
    clock_gettime(CLOCK_MONOTONIC_RAW, &tp0);

    page_size = sysconf(_SC_PAGESIZE);

    const char *filename = exe_path;
    if (info->dlpi_name[0] != '\0') {
        filename = info->dlpi_name;
        return 0;
    }

    if (!strcmp(filename, "linux-vdso.so.1") ||
        !strcmp(filename, "libkpac.so")) {
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

        struct pac_procs procs = { 0 };
        if (!svc_mode)
            allocate_procs((uintptr_t) vaddr_end, &procs);

        log("[%d:%s] patching segment %p-%p", cnt, filename, vaddr_start, vaddr_end);

        /* Need PROT_EXEC here to be able to execute mprotect in libc later */
        if (mprotect((void *)ALIGN_DOWN((uintptr_t) vaddr_start, page_size), size, PROT_READ | PROT_EXEC | PROT_WRITE))
            die("mprotect: %s", strerror(errno));

        phdr_patch(filename, vaddr_start, vaddr_end-vaddr_start, &procs, &stat);

        if (mprotect((void *)ALIGN_DOWN((uintptr_t) vaddr_start,page_size), size, PROT_READ | PROT_EXEC))
            die("mprotect: %s", strerror(errno));
    }

    cnt++;

    clock_gettime(CLOCK_MONOTONIC_RAW, &tp1);
    timespec_diff(&tp1, &tp0, &diff);

    if (stat_file)
        fprintf(stat_file, "%d,%s,%s,%lld.%09lld,%ld,%ld,%ld,%ld\n",
                pid, run_id, filename,
                (long long) diff.tv_sec, (long long) diff.tv_nsec,
                stat.total_pac, stat.patched_pac,
                stat.total_aut, stat.patched_aut);

    total_stat.total_pac += stat.total_pac;
    total_stat.total_aut += stat.total_aut;
    total_stat.patched_pac += stat.patched_pac;
    total_stat.patched_aut += stat.patched_aut;

    return 0;
}

__attribute__ ((constructor))
void libkpac_init()
{
    struct timespec tp0, tp1, diff;
    clock_gettime(CLOCK_MONOTONIC_RAW, &tp0);

    pid = getpid();
    if (readlink("/proc/self/exe", exe_path, sizeof(exe_path)) == -1)
        die("readlink: %s", strerror(errno));

    char *id_env = getenv("LIBKPAC_ID");
    if (id_env)
        run_id = id_env;

    char *stat_env = getenv("LIBKPAC_STAT");
    if (stat_env) {
        stat_file = fopen(stat_env, "a");
        if (!stat_file)
            die("fopen: %s", strerror(errno));
    }

    char *svc_env = getenv("LIBKPAC_SVC");
    if (svc_env && !strcmp(svc_env, "1"))
        svc_mode = true;

    if (dl_iterate_phdr(phdr_callback, NULL))
        die("dl_iterate_phdr");

    clock_gettime(CLOCK_MONOTONIC_RAW, &tp1);
    timespec_diff(&tp1, &tp0, &diff);

    if (stat_file) {
        fprintf(stat_file, "%d,%s,TOTAL,%lld.%09lld,%ld,%ld,%ld,%ld\n",
                pid, run_id,
                (long long) diff.tv_sec, (long long) diff.tv_nsec,
                total_stat.total_pac, total_stat.patched_pac,
                total_stat.total_aut, total_stat.patched_aut);
    }

}
