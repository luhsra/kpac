// Local Variables:
// rmsbolt-command: "gcc -O0 -fplugin=/srv/scratch/ill.ostapyshyn/pac-sw/gcc/pac_sw_plugin.so -fplugin-arg-pac_sw_plugin-sign-scope=std"
// rmsbolt-disassemble: nil
// End:

#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <stdlib.h>

int f2(int a)
{
    int arr[2];
    int b = a;
    b = b + arr[1];
    return b;
}

int f1()
{
    int arr[2];
    int a = 5;
    a = a + 5;
    return f2(a);
}

int main(int argc, char *argv[])
{
    printf("%s: %d\n", argv[0], f1());
    return 0;
}
