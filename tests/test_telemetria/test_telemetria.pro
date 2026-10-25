# De las tramas del dron (ala fija y cuadroptero) a MuestraVuelo (Protocolo/Telemetria).
QT += testlib positioning
QT -= gui
CONFIG += c++17 testcase console
CONFIG -= app_bundle
TARGET = test_telemetria

P = $$PWD/../../Principal
INCLUDEPATH += $$P
SOURCES += \
    test_telemetria.cpp \
    $$P/Protocolo/Telemetria.cpp \
    $$P/Protocolo/ProyeccionMercator.cpp
HEADERS += \
    $$P/Protocolo/Telemetria.h \
    $$P/Protocolo/ProyeccionMercator.h \
    $$P/Vuelos/MuestraVuelo.h
