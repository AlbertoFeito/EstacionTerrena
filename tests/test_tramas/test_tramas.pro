QT += testlib
QT -= gui
CONFIG += c++17 testcase console
CONFIG -= app_bundle
TARGET = test_tramas

INCLUDEPATH += $$PWD/../../Principal
SOURCES += \
    test_tramas.cpp \
    $$PWD/../../Principal/Protocolo/Tramas.cpp
HEADERS += \
    $$PWD/../../Principal/Protocolo/Tramas.h
