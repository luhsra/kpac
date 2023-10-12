#include <fcntl.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "proc.h"

#define COUNT_OF(x) ((sizeof(x)/sizeof(0[x])) / ((size_t)(!(sizeof(x) % sizeof(0[x])))))

static const char *pid_self = "self";

static char *stpecpy(char *dst, char *end, const char *restrict src)
{
    char *p;

    if (dst == NULL)
        return NULL;
    if (dst == end)
        return end;

    p = memccpy(dst, src, '\0', end - dst);
    if (p != NULL)
        return p - 1;

    /* truncation detected */
    end[-1] = '\0';
    return end;
}

ssize_t proc_exe(pid_t pid, char *restrict buf, size_t bufsize)
{
    char pathbuf[PROC_PATH_MAX] = { 0 };
    char *path = "/proc/self/exe";

    if (pid != PROC_PID_SELF) {
        snprintf(pathbuf, COUNT_OF(pathbuf), "/proc/%ld/exe", (long) pid);
        path = pathbuf;
    }

    return readlink(path, buf, bufsize);
}

ssize_t proc_maps(pid_t pid, struct proc_vma *vmas, size_t vma_count)
{
    char pathbuf[PROC_PATH_MAX] = { 0 };
    char *path = "/proc/self/maps";

    if (pid != PROC_PID_SELF) {
        snprintf(pathbuf, COUNT_OF(pathbuf), "/proc/%ld/maps", (long) pid);
        path = pathbuf;
    }

    FILE *fp = fopen(path, "r");
    if (!fp)
        return -1;

    char *line = NULL;
    size_t len = 0;
    size_t i = 0;

    ssize_t read;
    while (i < vma_count && (read = getline(&line, &len, fp)) != -1) {
        const char *space = " \r\n\t";
        char *saveptr = NULL;

        char *vm_start_s = strtok_r(line, "-", &saveptr);
        char *vm_end_s = strtok_r(NULL, space, &saveptr);
        char *flags_s = strtok_r(NULL, space, &saveptr);
        char *offset_s = strtok_r(NULL, space, &saveptr);
        char *major_s = strtok_r(NULL, ":", &saveptr);
        char *minor_s = strtok_r(NULL, space, &saveptr);
        char *inode_s = strtok_r(NULL, space, &saveptr);
        char *pathname = strtok_r(NULL, space, &saveptr);

        vmas[i].vm_start = strtoul(vm_start_s, NULL, 16);
        vmas[i].vm_end = strtoul(vm_end_s, NULL, 16);
        vmas[i].r = flags_s[0] == 'r';
        vmas[i].w = flags_s[1] == 'w';
        vmas[i].x = flags_s[2] == 'x';
        vmas[i].p = flags_s[3] == 'p';
        vmas[i].offset = strtoul(offset_s, NULL, 16);
        vmas[i].major = strtoul(major_s, NULL, 16);
        vmas[i].minor = strtoul(minor_s, NULL, 16);
        vmas[i].inode = strtoul(inode_s, NULL, 10);

        vmas[i].pathname[0] = '\0';
        if (pathname)
            stpecpy(vmas[i].pathname, vmas[i].pathname+PROC_PATH_MAX, pathname);

        i++;
    }

    fclose(fp);
    free(line);

    return i;
}
