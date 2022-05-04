TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.c \
        port/os_win.c \
        shell.c \
        sqlite3.c \
        sqlite3_demo.c

HEADERS += \
    config.h \
    sqlite3.h \
    sqlite3ext.h
