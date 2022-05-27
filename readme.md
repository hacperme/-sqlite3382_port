

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



![image-20220526165939412](readme.assets/image-20220526165939412.png)

```
FLAGS_WAL_enabled
```

![image-20220526181008496](readme.assets/image-20220526181008496.png)

```
FLAGS_WAL_disabled
```

![image-20220526181444903](readme.assets/image-20220526181444903.png)





windows

```
SQLite:     version 3.38.2
Keys:       16 bytes each
Values:     100 bytes each
Entries:    1000000
RawSize:    110.6 MB (estimated)
------------------------------------------------
fillseq      :      48.647 micros/op;    2.3 MB/s
fillseqsync  :      59.896 micros/op;    1.8 MB/s
fillseqbatch :       2.543 micros/op;   43.5 MB/s
fillrandom   :      69.527 micros/op;    1.6 MB/s
fillrandsync :      73.792 micros/op;    1.5 MB/s
fillrandbatch :      39.262 micros/op;    2.8 MB/s
overwrite    :      81.093 micros/op;    1.4 MB/s
overwritebatch :      67.556 micros/op;    1.6 MB/s
readrandom   :      17.090 micros/op;
readseq      :       9.226 micros/op;
fillrand100K :     684.756 micros/op;  139.3 MB/s
fillseq100K  :     357.854 micros/op;  266.5 MB/s
readseq      :       1.071 micros/op;
readrand100K :     104.038 micros/op;
```



