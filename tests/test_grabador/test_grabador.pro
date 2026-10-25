# Grabacion de vuelos (GrabadorVuelo) contra una base de datos SQLite temporal.
QT += testlib sql
QT -= gui
CONFIG += c++17 testcase
CONFIG -= app_bundle
TARGET = test_grabador

P = $$PWD/../../Principal
INCLUDEPATH += $$P
SOURCES += \
    test_grabador.cpp \
    $$P/Vuelos/GrabadorVuelo.cpp
HEADERS += \
    $$P/Vuelos/GrabadorVuelo.h
