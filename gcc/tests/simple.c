#include <stdio.h>

int f2(char s[10])
{
    int x;
    sscanf(s, "%d", &x);
    return x;
}

int f1(volatile int x)
{
    char arr[10];
    snprintf(arr, sizeof(arr), "%d", x);
    printf("ret: %p\n", __builtin_return_address(0));
    return f2(arr);
}

int main()
{
    int x = 42;
    x = f1(x);
    return !(x==42);
}
