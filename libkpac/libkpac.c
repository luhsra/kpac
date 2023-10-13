#define _GNU_SOURCE
#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <stdbool.h>
#include <errno.h>
#include <stddef.h>
#include <unistd.h>
#include <time.h>
#include <assert.h>

#include "asm.h"
#include "proc.h"

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

#define ALIGN_MASK(x, mask)	(((x) + (mask)) & ~(mask))
#define ALIGN_UP(x, a)		ALIGN_MASK(x, (__typeof__(x))(a) - 1)
#define ALIGN_DOWN(x, a)	ALIGN_UP((x) - ((a) - 1), (a))

#define IN_RANGE(value,low,high) ((value >= low) && (value <= high))

#define MIBI			(1024*1024)
#define NR_VMAS			512

enum {
    MODE_KPACD_SVC,
    MODE_SVC_ONLY,
};

struct kpac_routine {
    void *pac;
    void *aut;

    struct kpac_routine *prev; /* last allocated */
};

struct kpac_stat {
    struct {
        long total, patched;
    } pac;
    struct {
        long total, patched;
    } aut;
};

#define INST_PER_TRAMPOLINE 3
extern char __start_text_kpac;
/* kpac_pac_{504..8} */
extern void kpac_pac_0(void);
/* kpac_aut_{504..8} */
extern void kpac_aut_0(void);
extern char __stop_text_kpac;

static struct kpac_routine routine_own = {
    .pac = kpac_pac_0,
    .aut = kpac_aut_0,
    .prev = NULL, /* Dynamically allocated routines start here */
};

/* Globals */
static pid_t pid;
static long page_size;

static unsigned mode = MODE_KPACD_SVC;

static struct proc_vma vmas[NR_VMAS];
static size_t nr_vmas = 0;

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

static void *routine_pac(struct kpac_routine *routine, long offset)
{
    if (offset == 0)
        return routine->pac;

    if (offset >= 8 && offset <= 504 && offset % 8 == 0) {
        long index = 1 + (offset - 8) / 8;
        return (inst_t *) routine->pac - index * INST_PER_TRAMPOLINE;
    }

    log("no pac trampoline for offset %ld", offset);

    return NULL;
}

static void *routine_aut(struct kpac_routine *routine, long offset)
{
    if (offset == 0)
        return routine->aut;

    if (offset >= 8 && offset <= 504 && offset % 8 == 0) {
        long index = 1 + (offset - 8) / 8;
        return (inst_t *) routine->aut - index * INST_PER_TRAMPOLINE;
    }

    log("no aut trampoline for offset %ld", offset);

    return NULL;
}

static intptr_t vma_find_hole(uintptr_t current, uintptr_t min, uintptr_t max)
{
    size_t vma_cur;
    uintptr_t hole;

    /* Find the nearest hole below the current VMA */
    size_t vma_hole_below = 0;
    for (vma_cur = 0; vma_cur < nr_vmas; vma_cur++) {
        if (vma_cur > 0 && vmas[vma_cur].vm_start > vmas[vma_cur-1].vm_end)
            vma_hole_below = vma_cur;

        if (IN_RANGE(current, vmas[vma_cur].vm_start, vmas[vma_cur].vm_end)) {
            /* This is current VMA. */
            break;
        }
    }

    assert(vma_cur < nr_vmas);

    hole = vmas[vma_hole_below].vm_start - page_size;
    if (IN_RANGE(hole, min, max))
        return hole;

    /* Hole below is not suitable, look above */
    size_t vma_hole_above = vma_cur;
    while (vma_hole_above + 1 < nr_vmas &&
           vmas[vma_hole_above].vm_end == vmas[vma_hole_above+1].vm_start)
        vma_hole_above++;

    hole = vmas[vma_hole_above].vm_end;
    if (IN_RANGE(hole, min, max))
        return hole;

    return -1;
}

static struct kpac_routine *allocate_routine(void *hole)
{
    size_t len = &__stop_text_kpac - &__start_text_kpac;

    hole = mmap(hole, len + sizeof(struct kpac_routine),
                PROT_READ | PROT_WRITE | PROT_EXEC,
                MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);

    if (hole == MAP_FAILED)
        die("mmap: %s", strerror(errno));

    memcpy(hole, &__start_text_kpac, len);

    /* Fill metadata */
    struct kpac_routine *rout = (void *) ((uintptr_t) hole + len);
    rout->pac = hole + ((char *) kpac_pac_0  - &__start_text_kpac);
    rout->aut = hole + ((char *) kpac_aut_0  - &__start_text_kpac);

    return rout;
}

static struct kpac_routine *find_routine(void *branch)
{
    size_t kpac_len = &__stop_text_kpac - &__start_text_kpac;
    size_t padding = ALIGN_UP(kpac_len + sizeof(struct kpac_routine),
                              page_size);

    size_t    range     = 128 * MIBI;
    uintptr_t range_min = (uintptr_t) branch - range + padding;
    uintptr_t range_max = (uintptr_t) branch + range - padding;

    /* The last one allocated is a likely candidate */
    struct kpac_routine *needle = NULL;
    for (needle = &routine_own; needle != NULL; needle = needle->prev) {
        uintptr_t addr = (uintptr_t) needle->pac;
        if (IN_RANGE(addr, range_min, range_max))
            return needle;
    }

    /* Find a suitable hole in the address space */
    intptr_t hole = vma_find_hole((uintptr_t) branch, range_min, range_max);
    if (hole == -1)
        return NULL;

    /* Allocate and copy text into it */
    struct kpac_routine *routine = allocate_routine((void *) hole);
    log("allocated routine at %p", routine);

    routine->prev = routine_own.prev;
    routine_own.prev = routine;

    return routine;
}

static bool patch_paciasp(inst_t *text, size_t len, size_t i)
{
    int rn, rd, rt1, rt2, off;

    if (mode == MODE_SVC_ONLY || i + 1 >= len)
        goto fallback;

    struct kpac_routine *routine = find_routine(&text[i]);

    /* paciasp
     * stp x29, x30, [sp, #-N]! */
    if (stp_pre(text[i+1], &rn, &rt1, &rt2) &&
        rn == REG_SP && (rt1 == REG_LR || rt2 == REG_LR)) {

        void *fn = routine_pac(routine, rt1 == REG_LR ? 0 : 8);

        text[i] = text[i+1];
        emit_bl(&text[i+1], fn);
        return true;
    }

    /* paciasp
     * str x30, [sp, #-N]! */
    if (str_pre(text[i+1], &rn, &rt1) &&
        rn == REG_SP && rt1 == REG_LR) {

        void *fn = routine_pac(routine, 0);

        text[i] = text[i+1];
        emit_bl(&text[i+1], fn);
        return true;
    }

    /* paciasp
     * sub sp, sp, #N
     * (stp/str) */
    if (i + 2 < len &&
        sub_imm(text[i+1], &rn, &rd) &&
        rn == REG_SP && rd == REG_SP) {

        /* stp x29, x30, [sp] */
        if (stp_off(text[i+2], &rn, &rt1, &rt2, &off) &&
            rn == REG_SP && (rt1 == REG_LR || rt2 == REG_LR)) {

            void *fn = routine_pac(routine, rt1 == REG_LR ? off : 8 + off);
            if (!fn)
                goto fallback;

            text[i] = text[i+1];
            text[i+1] = text[i+2];
            emit_bl(&text[i+2], fn);
            return true;
        }

        /* str x30, [sp] */
        if (str_off(text[i+2], &rn, &rt1, &off) &&
            rn == REG_SP && rt1 == REG_LR) {

            void *fn = routine_pac(routine, off);
            if (!fn)
                goto fallback;

            text[i] = text[i+1];
            text[i+1] = text[i+2];
            emit_bl(&text[i+2], fn);
            return true;
        }
    }

fallback:
    text[i] = INST_SVC_PAC;

    return false;
}

static bool patch_autiasp(inst_t *text, size_t len, size_t i)
{
    int rn, rd, rt1, rt2, off;

    if (mode == MODE_SVC_ONLY || i < 1)
        goto fallback;

    struct kpac_routine *routine = find_routine(&text[i]);

    /* ldp x29, x30, [sp], #N
     * autiasp */
    if (ldp_post(text[i-1], &rn, &rt1, &rt2) &&
        rn == REG_SP && (rt1 == REG_LR || rt2 == REG_LR)) {

        void *fn = routine_aut(routine, rt1 == REG_LR ? 0 : 8);

        text[i] = text[i-1];
        emit_bl(&text[i-1], fn);
        return true;
    }

    /* ldr x30, [sp], #N
     * autiasp */
    if (ldr_post(text[i-1], &rn, &rt1) &&
        rn == REG_SP && rt1 == REG_LR) {

        void *fn = routine_aut(routine, 0);

        text[i] = text[i-1];
        emit_bl(&text[i-1], fn);
        return true;
    }

    /* (ldp/ldr)
     * add sp, sp, #N
     * autiasp */
    if (i >= 2 &&
        add_imm(text[i-1], &rn, &rd) &&
        rn == REG_SP && rd == REG_SP) {

        /* ldp x29, x30, [sp, #N] */
        if (ldp_off(text[i-2], &rn, &rt1, &rt2, &off) &&
            rn == REG_SP && (rt1 == REG_LR || rt2 == REG_LR)) {

            void *fn = routine_aut(routine, rt1 == REG_LR ? off : 8 + off);
            if (!fn)
                goto fallback;

            text[i] = text[i-1];
            text[i-1] = text[i-2];
            emit_bl(&text[i-2], fn);
            return true;
        }

        /* ldr x30, [sp, #N] */
        if (ldr_off(text[i-2], &rn, &rt1, &off) &&
            rn == REG_SP && rt1 == REG_LR) {

            void *fn = routine_aut(routine, off);
            if (!fn)
                goto fallback;

            text[i] = text[i-1];
            text[i-1] = text[i-2];
            emit_bl(&text[i-2], fn);
            return true;
        }
    }

fallback:
    text[i] = INST_SVC_AUT;

    return false;
}

static void vma_patch(struct proc_vma *vma, struct kpac_stat *stat)
{
    char *filename = vma->pathname;
    inst_t *text = (inst_t *) vma->vm_start;
    size_t len = (vma->vm_end - vma->vm_start) / sizeof(inst_t);

    for (size_t i = 0; i < len; i++) {
        switch (text[i]) {
        case INST_PACIASP:
            // log("%s: %p found pac", filename, &text[i]);
            stat->pac.total++;

            if (patch_paciasp(text, len, i)) {
                stat->pac.patched++;
                // log("%s: %p patched pac", filename, &text[i]);
            }

            break;
        case INST_AUTIASP:
            // log("%s: %p found aut", filename, &text[i]);
            stat->aut.total++;

            if (patch_autiasp(text, len, i)) {
                stat->aut.patched++;
                // log("%s: %p patched aut", filename, &text[i]);
            }

            break;
        }
    }
}

__attribute__ ((constructor))
void libkpac_init()
{
    page_size = sysconf(_SC_PAGESIZE);
    pid = getpid();

    FILE *stat_file = NULL;
    char *stat_env = getenv("LIBKPAC_STAT");
    if (stat_env) {
        stat_file = fopen(stat_env, "a");
        if (!stat_file)
            die("fopen: %s", strerror(errno));
    }

    char *mode_env = getenv("LIBKPAC_MODE");
    if (mode_env) {
        if (!strcmp(mode_env, "svc-only"))
            mode = MODE_SVC_ONLY;
        else if (!strcmp(mode_env, "kpacd-svc"))
            mode = MODE_KPACD_SVC;
        else
            die("Invalid mode: %s", mode_env);
    }

    ssize_t ret = proc_maps(PROC_PID_SELF, vmas, NR_VMAS);
    if (ret == -1)
        die("proc_maps: %s", strerror(errno));
    nr_vmas = ret;

    log("Virtual memory areas:");
    for (size_t i = 0; i < nr_vmas; i++) {
        log("%016lx-%016lx (%c%c%c%c) %s",
            vmas[i].vm_start, vmas[i].vm_end,
            vmas[i].r ? 'r' : '-', vmas[i].w ? 'w' : '-',
            vmas[i].x ? 'x' : '-', vmas[i].p ? 'p' : '-',
            vmas[i].pathname);
    }

    for (size_t i = 0; i < (size_t) nr_vmas; i++) {
        struct kpac_stat stat = { 0 };
        struct timespec tp0, tp1, diff;

        struct proc_vma *vma = &vmas[i];
        size_t vm_size = vma->vm_end - vma->vm_start;

        clock_gettime(CLOCK_MONOTONIC_RAW, &tp0);

        /* We're interested only in executable areas */
        if (!vma->x)
            continue;

        /* Skip vdso and ourselves */
        if (strstr(vma->pathname, "[vdso]") ||
            strstr(vma->pathname, "libkpac.so")) {

            log("[%s] skipping", vma->pathname);
            continue;
        }

        log("[%s] patching segment %lx-%lx", vma->pathname, vma->vm_start, vma->vm_end);

        /* Need PROT_EXEC here to be able to execute mprotect in libc later */
        if (mprotect((void *) vma->vm_start, vm_size, PROT_READ | PROT_EXEC | PROT_WRITE))
            die("mprotect: %s", strerror(errno));

        /* Work on this VMA */
        vma_patch(vma, &stat);

        /* Restore security */
        if (mprotect((void *) vma->vm_start, vm_size, PROT_READ | PROT_EXEC))
            die("mprotect: %s", strerror(errno));

        clock_gettime(CLOCK_MONOTONIC_RAW, &tp1);
        timespec_diff(&tp1, &tp0, &diff);

        if (stat_file)
            fprintf(stat_file, "%s,%lld.%09lld,%ld,%ld,%ld,%ld\n",
                    vma->pathname,
                    (long long) diff.tv_sec, (long long) diff.tv_nsec,
                    stat.pac.total, stat.pac.patched,
                    stat.aut.total, stat.aut.patched);
    }

    for (struct kpac_routine *i = routine_own.prev; i != NULL; i = i->prev) {
        size_t len = &__stop_text_kpac - &__start_text_kpac;
        uintptr_t page = ALIGN_DOWN((uintptr_t) i, page_size);

        if (mprotect((void *) page, len, PROT_READ | PROT_EXEC))
            die("mprotect: %s", strerror(errno));
    }
}
