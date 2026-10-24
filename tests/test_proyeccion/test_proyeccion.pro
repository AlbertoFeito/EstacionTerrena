QT += testlib positioning
QT -= gui
CONFIG += c++17 testcase console
CONFIG -= app_bundle
TARGET = test_proyeccion

INCLUDEPATH += $$PWD/../../Principal
SOURCES += \
    test_proyeccion.cpp \
    $$PWD/../../Principal/Protocolo/ProyeccionMercator.cpp
HEADERS += \
    $$PWD/../../Principal/Protocolo/ProyeccionMercator.h
