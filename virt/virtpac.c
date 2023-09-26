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

#define INST_PACIASP 0xD503233F
#define INST_SVC_PAC 0xD4013581 /* SVC #0x9AC */

#define INST_AUTIASP 0xD50323BF
#define INST_SVC_AUT 0xD40135A1 /* SVC #0x9AD */


#ifdef DEBUG
#define log(fmt, ...) fprintf(stderr, "virtpac: " fmt "\n", ##__VA_ARGS__)
#else
#define log(fmt, ...) ((void) 0)
#endif

#define die(fmt, ...)                                                   \
    do {                                                                \
        fprintf(stderr, "[%s:%d]: " fmt "\n",                           \
                __FILE__, __LINE__, ##__VA_ARGS__);                     \
        exit(EXIT_FAILURE);                                             \
    } while (0)

typedef uint32_t inst_t;

extern char *program_invocation_name;

static int phdr_patch(inst_t *start, inst_t *end)
{
    for (inst_t *p = start; p < end; p++) {
        switch (*p) {
        case INST_PACIASP:
            *p = INST_SVC_PAC;
            break;
        case INST_AUTIASP:
            *p = INST_SVC_AUT;
            break;
        }
    }

    return 0;
}

static int phdr_callback(struct dl_phdr_info *info, size_t _size, void *_data)
{
    static int cnt = 0;

    const char *filename = program_invocation_name;
    if (info->dlpi_name[0] != '\0')
        filename = info->dlpi_name;

    if (!strcmp(filename, "linux-vdso.so.1") ||
        !strcmp(filename, "virtpac.so")) {
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

        log("[%d:%s] patching segment %p-%p", cnt, filename, vaddr_start, vaddr_end);

        /* Need PROT_EXEC here to be able to execute mprotect in libc later */
        if (mprotect(vaddr_start, size, PROT_READ | PROT_EXEC | PROT_WRITE))
            die("mprotect: %s", strerror(errno));

        phdr_patch(vaddr_start, vaddr_end);

        if (mprotect(vaddr_start, size, PROT_READ | PROT_EXEC))
            die("mprotect: %s", strerror(errno));
    }

    cnt++;

    return 0;
}

__attribute__ ((constructor))
void virtpac_init()
{
    if (dl_iterate_phdr(phdr_callback, NULL))
        die("dl_iterate_phdr");
}
