# Estado del enlace (MonitorEnlace) y panel de vuelo (PanelVuelo).
QT += testlib widgets positioning
CONFIG += c++17 testcase
CONFIG -= app_bundle
TARGET = test_estado

INCLUDEPATH += $$PWD/../../Principal
SOURCES += \
    test_estado.cpp \
    $$PWD/../../Principal/Widgets/PanelVuelo.cpp
HEADERS += \
    $$PWD/../../Principal/Widgets/PanelVuelo.h \
    $$PWD/../../Principal/Protocolo/MonitorEnlace.h
