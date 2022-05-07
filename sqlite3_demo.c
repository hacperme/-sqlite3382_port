/**  @file
sqlite3_demo.c

 @brief
 文件功能模块描述。
*/

/*=====================================================================================
Copyright (c) 2022 Quectel Wireless Solution, Co., Ltd.  All Rights Reserved.
Quectel Wireless Solution Proprietary and Confidential.
=====================================================================================*/

/*=====================================================================================

                        EDIT HISTORY FOR MODULE
This section contains comments describing changes made to the module.
Notice that changes are listed in reverse chronological order.
WHEN				WHO			WHAT, WHERE, WHY
------------		-------		-------------------------------------------------------
04/05/2022			xinqiang		create
=====================================================================================*/


#include "sqlite3.h"
#include <stdlib.h>
#include <stdio.h>

#ifndef _WIN32
#include "ql_api_osi.h"
#include "ql_log.h"
#include "ql_api_dev.h"
#include "ql_sdmmc.h"

#define QL_SQLITE_DEMO_LOG(msg, ...)			    QL_LOG(QL_LOG_LEVEL_INFO, "sqlite_demo", msg, ##__VA_ARGS__)

ql_task_t sqlite3_demo_task = NULL;
#define SDEMMC_TEST 1

#else
#define QL_SQLITE_DEMO_LOG(msg, ...)			    printf(msg, ##__VA_ARGS__)
#endif


/*
** Internal check:  Verify that the SQLite is uninitialized.  Print a
** error message if it is initialized.
*/
static void verify_uninitialized(void){
  if( sqlite3_config(-1)==SQLITE_MISUSE ){
    QL_SQLITE_DEMO_LOG("WARNING: attempt to configure SQLite after"
                        " initialization.\n");
  }
}

/*
** A callback for the sqlite3_log() interface.
*/
static void shellLog(void *pArg, int iErrCode, const char *zMsg)
{
    QL_SQLITE_DEMO_LOG("(%d) %s\n", iErrCode, zMsg);
}

static void sqlite3_init(void)
{
    verify_uninitialized();
    sqlite3_config(SQLITE_CONFIG_MULTITHREAD);
    sqlite3_config(SQLITE_CONFIG_LOG, shellLog, NULL);
    sqlite3_initialize();
    
}

static int callback(void *NotUsed, int argc, char **argv, char **azColName)
{
    int i;
    for (i = 0; i < argc; i++)
    {
        QL_SQLITE_DEMO_LOG("%s = %s\n", azColName[i], argv[i] ? argv[i] : "NULL");
    }
    return 0;
}

static void print_heap_info(void)
{
#ifndef _WIN32
    ql_memory_heap_state_t heap_info = {0};
    ql_dev_memory_size_query(&heap_info);
    QL_SQLITE_DEMO_LOG("total:%lu, free:%lu", heap_info.total_size, heap_info.avail_size);
#endif
    return;
}

static void _sqlite3_demo_task(void *arg)
{
    sqlite3 *db = NULL;
    char *zErrMsg = 0;
    sqlite3_init();
    int rc;

    print_heap_info();
#ifndef _WIN32
#if SDEMMC_TEST
    while (1)
    {
        if(ql_sdmmc_is_mount())
        {
            break;
        }
        ql_rtos_task_sleep_s(1);
    }
    
    rc = sqlite3_open("SD:123.db", &db);
#else

    rc = sqlite3_open("UFS:/123.db", &db);
#endif
#else
    rc = sqlite3_open("123.db", &db);
#endif
    if(rc)
    {
        QL_SQLITE_DEMO_LOG("sqlite3_open rc:%d", rc);
        sqlite3_close(db);
        goto exit;
    }

    print_heap_info();

    while (1)
    {

        rc = sqlite3_exec(db, "select name from sqlite_master where type='table' and name='tbl1';", callback, 0, &zErrMsg);
        if (rc != SQLITE_OK)
        {
            QL_SQLITE_DEMO_LOG("(%d)SQL error: %s", rc, zErrMsg);
            sqlite3_free(zErrMsg);
        }
        else
        {

            rc = sqlite3_exec(db, "drop table tbl1;", callback, 0, &zErrMsg);
            if (rc != SQLITE_OK)
            {
                QL_SQLITE_DEMO_LOG("(%d)SQL error: %s", rc, zErrMsg);
                sqlite3_free(zErrMsg);
            }
        }

        print_heap_info();

        rc = sqlite3_exec(db, "create table tbl1(one text, two int);", callback, 0, &zErrMsg);
        if (rc != SQLITE_OK)
        {
            QL_SQLITE_DEMO_LOG("(%d)SQL error: %s", rc,zErrMsg);
            sqlite3_free(zErrMsg);
            break;
        }

        print_heap_info();

        rc = sqlite3_exec(db, "insert into tbl1 values('hello!',10);", callback, 0, &zErrMsg);
        if (rc != SQLITE_OK)
        {
            QL_SQLITE_DEMO_LOG("(%d)SQL error: %s", rc,zErrMsg);
            sqlite3_free(zErrMsg);
            break;
        }

        print_heap_info();

        rc = sqlite3_exec(db, "select * from tbl1;", callback, 0, &zErrMsg);
        if (rc != SQLITE_OK)
        {
            QL_SQLITE_DEMO_LOG("(%d)SQL error: %s", rc,zErrMsg);
            sqlite3_free(zErrMsg);
        }

        print_heap_info();

        break;
    }

    sqlite3_close(db);

    print_heap_info();


exit:

#ifndef _WIN32
    ql_rtos_task_delete(NULL);
#endif
    return;
    
}

void sqlite3_demo_init(void)
{
#ifndef _WIN32
    int ret = ql_rtos_task_create(&sqlite3_demo_task, 8 * 1024, APP_PRIORITY_NORMAL, "sqlite3_demo", _sqlite3_demo_task, NULL, 0);
    if (ret != 0)
    {
        ql_assert();
    }
#else
    _sqlite3_demo_task(NULL);
#endif
}
