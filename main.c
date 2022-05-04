
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
#endif

extern int  wmain(int argc, wchar_t **wargv);
extern void sqlite3_demo_init(void);

int main(int argc, char **argv)
{
    char buff[128] = {0};
    getcwd(buff,128);
    printf("pwd:%s\n", buff);
    wmain(argc, (wchar_t **)argv);
//    sqlite3_demo_init();
    return 0;
}

