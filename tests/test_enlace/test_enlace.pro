# La estacion (SerialManager) y el simulador hablando de verdad por TCP, UDP y
# (si hay socat) por un par de puertos serie virtuales.
QT += testlib network serialport positioning
QT -= gui
CONFIG += c++17 testcase console
CONFIG -= app_bundle
TARGET = test_enlace

INCLUDEPATH += $$PWD/../../Principal $$PWD/../../simulador
SOURCES += \
    test_enlace.cpp \
    $$PWD/../../Principal/SerialPortComboBox/SerialManager.cpp \
    $$PWD/../../Principal/Protocolo/Tramas.cpp \
    $$PWD/../../Principal/Protocolo/ProyeccionMercator.cpp \
    $$PWD/../../simulador/SimuladorDron.cpp \
    $$PWD/../../simulador/EnlacesSimulador.cpp
HEADERS += \
    $$PWD/../../Principal/SerialPortComboBox/SerialManager.h \
    $$PWD/../../simulador/SimuladorDron.h \
    $$PWD/../../simulador/EnlacesSimulador.h
