# simulador_dron: dron simulado que habla el protocolo de la estacion por TCP,
# UDP y puerto serie. Comparte con la estacion las tramas (Protocolo/Tramas) y
# la proyeccion (Protocolo/ProyeccionMercator).
QT += core network serialport positioning
QT -= gui
CONFIG += c++17 console
CONFIG -= app_bundle

TEMPLATE = app
TARGET = simulador_dron

CONFIG(debug, debug|release) {
    DESTDIR = $$PWD/../build/debug
} else {
    DESTDIR = $$PWD/../build/release
}
OBJECTS_DIR = $$DESTDIR/.obj/$$TARGET
MOC_DIR = $$DESTDIR/.moc/$$TARGET

INCLUDEPATH += $$PWD/../Principal

SOURCES += \
    main.cpp \
    SimuladorDron.cpp \
    EnlacesSimulador.cpp \
    $$PWD/../Principal/Protocolo/Tramas.cpp \
    $$PWD/../Principal/Protocolo/ProyeccionMercator.cpp

HEADERS += \
    SimuladorDron.h \
    EnlacesSimulador.h \
    $$PWD/../Principal/Protocolo/Tramas.h \
    $$PWD/../Principal/Protocolo/ProyeccionMercator.h
