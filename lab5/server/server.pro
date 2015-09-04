TEMPLATE = app
CONFIG += console
CONFIG -= app_bundle
CONFIG -= qt
QMAKE_CFLAGS += "-std=gnu99"
LIBS += -lutils
DEFINES += IPv4IPv6

SOURCES += main.c
