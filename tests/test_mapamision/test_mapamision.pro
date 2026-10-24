QT += testlib widgets positioning sql
CONFIG += c++17 testcase
CONFIG -= app_bundle
TARGET = test_mapamision

# libmapa instalada, como en Principal.pro.
isEmpty(LIBMAPA_PREFIX): LIBMAPA_PREFIX = C:/libmapa/qt$${QT_MAJOR_VERSION}
!exists($$LIBMAPA_PREFIX/libmapa.pri) {
    error("No encuentro libmapa en $${LIBMAPA_PREFIX}. qmake LIBMAPA_PREFIX=<carpeta>")
}
include($$LIBMAPA_PREFIX/libmapa.pri)

INCLUDEPATH += $$PWD/../../Principal $$PWD/../../Principal/graficar
SOURCES += \
    test_mapamision.cpp \
    $$PWD/../../Principal/Mapa/MapaMision.cpp
HEADERS += \
    $$PWD/../../Principal/Mapa/MapaMision.h
