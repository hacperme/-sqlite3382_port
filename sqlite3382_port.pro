TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += \
        main.c \
        shell.c \
        sqlite3.c \
        windows/os_win.c

HEADERS += \
    config.h \
    sqlite3.h \
    sqlite3ext.h
