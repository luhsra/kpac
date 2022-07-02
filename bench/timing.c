#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include <sys/wait.h>

static inline void timespec_diff(struct timespec *a, struct timespec *b,
                                 struct timespec *result) {
    result->tv_sec  = a->tv_sec  - b->tv_sec;
    result->tv_nsec = a->tv_nsec - b->tv_nsec;
    if (result->tv_nsec < 0) {
        --result->tv_sec;
        result->tv_nsec += 1000000000L;
    }
}

static int measure(char *const argv[], double *result)
{
    pid_t cpid;
    int wstatus;
    struct timespec tp0, tp1, diff;

    clock_gettime(CLOCK_MONOTONIC_RAW, &tp0);
    cpid = fork();

    if (cpid == -1) {
        PyErr_SetString(PyExc_OSError, strerror(errno));
        return 1;
    }

    if (cpid == 0) {
        execv(argv[0], argv);
        perror("execve");
        exit(127);
    }

    do {
        waitpid(cpid, &wstatus, 0);
    } while (!WIFEXITED(wstatus) && !WIFSIGNALED(wstatus));

    clock_gettime(CLOCK_MONOTONIC_RAW, &tp1);

    if (WIFSIGNALED(wstatus)) {
        PyErr_SetString(PyExc_RuntimeError, "program terminated due to signal.");
        return 1;
    } else if (WIFEXITED(wstatus) && WEXITSTATUS(wstatus) != 0) {
        PyErr_SetString(PyExc_RuntimeError, "terminated with non-zero return code.");
        return 1;
    }

    timespec_diff(&tp1, &tp0, &diff);
    *result = diff.tv_sec + diff.tv_nsec * 1e-9;

    return 0;
}

static char **pylist_as_argv(PyObject *list)
{
    int n = PyList_Size(list);
    char **argv = malloc((n+1) * sizeof(char *));
    if (!argv) {
        PyErr_SetString(PyExc_OSError, strerror(errno));
        return NULL;
    }

    for (int i = 0; i < n; i++) {
        PyObject *item = PyList_GetItem(list, i);
        if (PyUnicode_Check(item)) {
            argv[i] = (char *) PyUnicode_AsUTF8(item);
            continue;
        }
        if (PyBytes_Check(item)) {
            argv[i] = PyBytes_AsString(item);
            continue;
        }

        PyErr_SetString(PyExc_TypeError, "list items must be strings.");
        return NULL;

    }
    argv[n] = NULL;

    return argv;
}

static PyObject *timing_run(PyObject *self, PyObject *args)
{
    ((void) self);
    PyObject *list;

    if (!PyArg_ParseTuple(args, "O!", &PyList_Type, &list)) {
        PyErr_SetString(PyExc_TypeError, "parameter must be a list.");
        return NULL;
    }

    char **argv = pylist_as_argv(list);
    if (!argv)
        return NULL;

    double result;
    if (measure(argv, &result))
        return NULL;

    free(argv);

    return PyFloat_FromDouble(result);
}

static PyMethodDef TimingMethods[] = {
    {"run",  timing_run, METH_VARARGS, "Run program and measure time."},
    {NULL, NULL, 0, NULL}, /* Sentinel */
};

static struct PyModuleDef timingmodule = {
    PyModuleDef_HEAD_INIT,
    "timing",  /* name of module */
    NULL,      /* module documentation, may be NULL */
    -1,        /* size of per-interpreter state of the module,
                  or -1 if the module keeps state in global variables. */
    TimingMethods,
    NULL,
    NULL,
    NULL,
    NULL,
};

PyMODINIT_FUNC PyInit_timing(void)
{
    return PyModule_Create(&timingmodule);
}
