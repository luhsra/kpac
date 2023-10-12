#ifndef LIBKPAC_PROC_H
#define LIBKPAC_PROC_H

#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>

#define PROC_PID_SELF		((pid_t) -1)
#define PROC_PATH_MAX		1024

struct proc_vma {
    uintptr_t vm_start;
    uintptr_t vm_end;

    unsigned r : 1;                 /* read */
    unsigned w : 1;                 /* write */
    unsigned x : 1;                 /* execute */
    unsigned p : 1;                 /* private */

    size_t offset;
    dev_t major, minor;
    ino_t inode;

    char pathname[PROC_PATH_MAX];
};

ssize_t proc_exe(pid_t pid, char *restrict buf, size_t bufsize);
ssize_t proc_maps(pid_t pid, struct proc_vma *vmas, size_t vma_count);

#endif                          /* LIBKPAC_PROC_H */
