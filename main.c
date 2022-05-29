
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <assert.h>
#include <stdio.h>
#include <errno.h>
#include <dirent.h>
#include <unistd.h>
#ifdef WIN32
#include <windows.h>
#include <memory.h>
#include <process.h>
#include "heap_4.h"
#endif

extern int  wmain(int argc, wchar_t **wargv);
extern void sqlite3_demo_init(void);
extern int benchmark_main(int argc, char** argv);

void test_heap(void)
{
    printf("xPortGetFreeHeapSize:%d\n", xPortGetFreeHeapSize());
    char * a = pvPortMalloc(1000);
    printf("xPortGetFreeHeapSize:%d\n", xPortGetFreeHeapSize());
    printf("a:%p\n", a);
    vPortFree(a);
    printf("xPortGetFreeHeapSize:%d\n", xPortGetFreeHeapSize());

}

int main(int argc, char **argv)
{
    char buff[128] = {0};
    getcwd(buff,128);
    printf("pwd:%s\n", buff);
    test_heap();
//    wmain(argc, (wchar_t **)argv);

    sqlite3_demo_init();
//    benchmark_main(argc, argv);
    printf("xPortGetFreeHeapSize:%d\n", xPortGetFreeHeapSize());
    return 0;
}

