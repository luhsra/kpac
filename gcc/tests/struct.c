#include <stdio.h>

struct bar {
    char baz[10];
};

struct foo {
    union {
        char dummy;
        struct bar bar;
    };
};

int f2(struct foo *foo)
{
    int x;
    sscanf(foo->bar.baz, "%d", &x);
    return x;
}

int f1(volatile int x)
{
    struct foo foo;
    snprintf(foo.bar.baz, sizeof(foo.bar.baz), "%d", x);
    printf("ret: %p\n", __builtin_return_address(0));
    return f2(&foo);
}

int main()
{
    int x = 42;
    x = f1(x);
    return !(x==42);
}
