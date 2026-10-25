# Validacion y analisis de rutas (Planificacion/RoutePlanner), sin pantalla.
QT += testlib positioning
QT -= gui
CONFIG += c++17 testcase console
CONFIG -= app_bundle
TARGET = test_planificador

P = $$PWD/../../Principal
INCLUDEPATH += $$P
SOURCES += \
    test_planificador.cpp \
    $$P/Planificacion/RoutePlanner.cpp
HEADERS += \
    $$P/Planificacion/RoutePlanner.h \
    $$P/Planificacion/CaracteristicasDron.h \
    $$P/Estructuras/E_Punto.h
