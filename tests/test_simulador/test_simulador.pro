# El simulador obedeciendo los mandos (regimen, mision, RTL, paracaidas,
# ganancias), sin red: se le dan las tramas de la estacion y se avanza a mano.
QT += testlib positioning
QT -= gui
CONFIG += c++17 testcase console
CONFIG -= app_bundle
TARGET = test_simulador

INCLUDEPATH += $$PWD/../../Principal $$PWD/../../simulador
SOURCES += \
    test_simulador.cpp \
    $$PWD/../../Principal/Protocolo/Tramas.cpp \
    $$PWD/../../Principal/Protocolo/ProyeccionMercator.cpp \
    $$PWD/../../simulador/SimuladorDron.cpp
HEADERS += \
    $$PWD/../../simulador/SimuladorDron.h
