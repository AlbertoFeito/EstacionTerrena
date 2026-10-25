# Pestaña de Mandos (Widget_Mandos): estado del dron, trama y conf.ini.
QT += testlib widgets positioning
CONFIG += c++17 testcase
CONFIG -= app_bundle
TARGET = test_mandos

P = $$PWD/../../Principal
INCLUDEPATH += $$P
SOURCES += \
    test_mandos.cpp \
    $$P/Widgets/widget_mandos.cpp \
    $$P/MyQSettings/myqsetting.cpp \
    $$P/Protocolo/Tramas.cpp \
    $$P/Protocolo/ProyeccionMercator.cpp
HEADERS += \
    $$P/Widgets/widget_mandos.h \
    $$P/MyQSettings/myqsetting.h
FORMS += $$P/Widgets/widget_mandos.ui
