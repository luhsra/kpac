int __attribute__ ((noinline)) factorial(int n) {
    if (n == 0) {
        return 1;
    } else {
        return n * factorial(n-1);
    }
}

/*
 * This function compiled using GCC 10.2.1 with -O2 generates 4 exit paths:
 * - fast exit for a==0 without setting up/tearing down the stack frame
 * - 2x normal exit with epilogue
 * - sibling call to factorial() with epilogue
 */
int __attribute__ ((noinline)) fast_exit(int a)
{
    if (a==0)
        return 0;
    asm ("");
    volatile int arr1[10];
    arr1[0] = 0;
    if (a==1)
        return 1;
    volatile int arr2[10];
    arr2[0] = 0;
    if (a==2)
        return factorial(2);
    volatile int arr3[10];
    arr3[0] = 0;
    return 3;
}

int main()
{
    for (int i = 0; i <= 3; i++)
        if (fast_exit(i) != i)
            return 1;
    return 0;
}
