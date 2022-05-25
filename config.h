#ifndef _SQLITE_CONFIG_H_
#define _SQLITE_CONFIG_H_

#include <stdint.h>

/* config.h.  Generated from config.h.in by configure.  */
/* config.h.in.  Generated from configure.ac by autoheader.  */

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 0

/* Define to 1 if you have the `fdatasync' function. */
#define HAVE_FDATASYNC 0

/* Define to 1 if you have the `gmtime_r' function. */
#define HAVE_GMTIME_R 0

/* Define to 1 if the system has the type `int16_t'. */
#define HAVE_INT16_T 1

/* Define to 1 if the system has the type `int32_t'. */
#define HAVE_INT32_T 1

/* Define to 1 if the system has the type `int64_t'. */
#define HAVE_INT64_T 1

/* Define to 1 if the system has the type `int8_t'. */
#define HAVE_INT8_T 1

/* Define to 1 if the system has the type `intptr_t'. */
#define HAVE_INTPTR_T 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 0

/* Define to 1 if you have the `isnan' function. */
#define HAVE_ISNAN 0

/* Define to 1 if you have the `localtime_r' function. */
#define HAVE_LOCALTIME_R 0

/* Define to 1 if you have the `localtime_s' function. */
/* #undef HAVE_LOCALTIME_S */

/* Define to 1 if you have the <malloc.h> header file. */
#define HAVE_MALLOC_H 0

/* Define to 1 if you have the `malloc_usable_size' function. */
#define HAVE_MALLOC_USABLE_SIZE 0

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 0

/* Define to 1 if you have the pread() function. */
#define HAVE_PREAD 0

/* Define to 1 if you have the pread64() function. */
#define HAVE_PREAD64 0

/* Define to 1 if you have the pwrite() function. */
#define HAVE_PWRITE 0

/* Define to 1 if you have the pwrite64() function. */
#define HAVE_PWRITE64 0

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the strchrnul() function */
#define HAVE_STRCHRNUL 0

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 0

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 0

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 0

/* Define to 1 if the system has the type `uint16_t'. */
#define HAVE_UINT16_T 1

/* Define to 1 if the system has the type `uint32_t'. */
#define HAVE_UINT32_T 1

/* Define to 1 if the system has the type `uint64_t'. */
#define HAVE_UINT64_T 1

/* Define to 1 if the system has the type `uint8_t'. */
#define HAVE_UINT8_T 1

/* Define to 1 if the system has the type `uintptr_t'. */
#define HAVE_UINTPTR_T 1

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 0

/* Define to 1 if you have the `usleep' function. */
#define HAVE_USLEEP 0

/* Define to 1 if you have the utime() library function. */
#define HAVE_UTIME 0

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT ""

/* Define to the full name of this package. */
#define PACKAGE_NAME "sqlite"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "sqlite 3.38.2"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "sqlite"

/* Define to the version of this package. */
#define PACKAGE_VERSION "3.38.2"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */


#ifndef SQLITE_MINIMUM_FILE_DESCRIPTOR
/* 不操作文件描述符小于SQLITE_MINIMUM_FILE_DESCRIPTOR的文件，某些描述符表示 standard input, output, and error */
#define SQLITE_MINIMUM_FILE_DESCRIPTOR  3
#endif

/*This option omits the entire extension loading mechanism from SQLite, including sqlite3_enable_load_extension() and sqlite3_load_extension() interfaces.*/
#define SQLITE_OMIT_LOAD_EXTENSION 0

//This option omits the "write-ahead log" (a.k.a. "WAL") capability.
#define SQLITE_OMIT_WAL 1

//When built using SQLITE_OMIT_AUTOINIT,
//SQLite will not automatically initialize itself and the application is required to invoke sqlite3_initialize()
//directly prior to beginning use of the SQLite library.
#define SQLITE_OMIT_AUTOINIT 1



#ifndef SQLITE_TEMP_STORE
//This option controls whether temporary files are stored on disk or in memory. The meanings for various settings of this compile-time option are as follows:

//SQLITE_TEMP_STORE	Meaning
//0	Always use temporary files
//1	Use files by default but allow the PRAGMA temp_store command to override
//2	Use memory by default but allow the PRAGMA temp_store command to override
//3	Always use memory

#define SQLITE_TEMP_STORE 1
#endif

#ifndef SQLITE_TEMP_FILE_PREFIX
# define SQLITE_TEMP_FILE_PREFIX "etilqs_"
#endif

#ifndef SQLITE_DEFAULT_SECTOR_SIZE
# define SQLITE_DEFAULT_SECTOR_SIZE 4096
#endif


#ifndef SQLITE_THREADSAFE
#define SQLITE_THREADSAFE 1
#endif

#ifndef HAVE_READLINE
#define HAVE_READLINE 0
#endif

#ifndef NDEBUG
#define NDEBUG
#endif

#ifndef SQLITE_OS_OTHER
// 非Linux Windows系统
#define SQLITE_OS_OTHER 1
#endif

#if SQLITE_OS_OTHER


#if defined(_WIN32)
#ifndef SQLITE_OS_CUS_WINDOWS
#define SQLITE_OS_CUS_WINDOWS 1
#endif

#elif 0
//#ifndef SQLITE_OS_RTTHREAD
//#define SQLITE_OS_RTTHREAD 1
//#endif

#elif 1
#ifndef SQLITE_OS_QUEC_RTOS
#define SQLITE_OS_QUEC_RTOS 1
#endif
#else
#error "error config"
#endif

#endif // SQLITE_OS_OTHER

#if SQLITE_OS_CUS_WINDOWS
    #ifndef SQLITE_MUTEX_CUS_WINDOWS
    #    define SQLITE_MUTEX_CUS_WINDOWS 1
    #endif
#endif

#if SQLITE_OS_QUEC_RTOS
    #ifndef SQLITE_MUTEX_QUEC_RTOS
    #    define SQLITE_MUTEX_QUEC_RTOS 1
    #endif
#endif

#endif
