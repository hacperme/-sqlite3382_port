

## sqlite 移植代码说明

移植的sqlite 版本是 3.38.2

目录结构：

```bat
E:.
│  .gitignore
│  CMakeLists.txt
│  config.h
│  main.c
│  shell.c
│  sqlite3.c
│  sqlite3.h
│  sqlite3382_port.pro
│  sqlite3ext.h
│  sqlite3_demo.c
│
└─port
        os_quec_rtos.c
        os_win.c
```



config.h 是编译配置

sqlite3.c 和 os_quec_rtos.c 是参与编译的数据库源码

sqlite3_demo.c 是例程



main.c、shell.c 和 os_win.c 是 windows 环境下的适配代码，去掉了原来的系统调用，编译环境使用QT，工程文件 sqlite3382_port.pro，用来调试验证。