QT       += core gui sql network positioning printsupport serialport
# Módulo SVG según versión
greaterThan(QT_MAJOR_VERSION, 5) {
    # Para Qt 6
    QT += svg svgwidgets
} else {
    # Para Qt 5
    QT += svg
}


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets


CONFIG += c++17

CONFIG(debug, debug|release) {
    DESTDIR = $$PWD/../build/debug
} else {
    DESTDIR = $$PWD/../build/release
}
TEMPLATE = app
TARGET = EstacionTerrena
OBJECTS_DIR = $$DESTDIR/.obj/$$TARGET
MOC_DIR = $$DESTDIR/.moc/$$TARGET
RCC_DIR = $$DESTDIR/.qrc/$$TARGET
UI_DIR = $$DESTDIR/.ui/$$TARGET

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    AlturaWorker.cpp \
    CControladora.cpp \
    Database/AsyncQuery.cpp \
    Database/AsyncQueryResult.cpp \
    Database/AsynqQueryModel.cpp \
    Database/ConnectionManager.cpp \
    LocationValidator.cpp \
    Mapa/MapaMision.cpp \
    MyQSettings/myqsetting.cpp \
    Reporte/layoutcol.cpp \
    Reporte/qecsreport.cpp \
    Protocolo/ProyeccionMercator.cpp \
    Reporte/sectionclass.cpp \
    SerialPortComboBox/DeviceHotplug.cpp \
    SerialPortComboBox/SerialManager.cpp \
    SerialPortComboBox/SerialPortComboBox.cpp \
    Widgets/PerfilAltitudWidget.cpp \
    Widgets/Widget_Conexion.cpp \
    Widgets/Widget_ControlObjetivo.cpp \
    Widgets/Widget_Drones.cpp \
    Widgets/Widget_Estado_Cuadroptero.cpp \
    Widgets/Widget_Ganancias_Control.cpp \
    Widgets/Widget_Graficas.cpp \
    Widgets/Widget_Mandos_Cuadroptero.cpp \
    Widgets/Widget_Parametros.cpp \
    Widgets/Widget_Puntos.cpp \
    Widgets/dronedialog.cpp \
    Widgets/widget_mandos.cpp \
#    cSerialPort/cserialport.cpp \
    graficar/XxwTracer.cpp \
    graficar/customplot.cpp \
    header/ColumnHeaderLengthAdapter.cpp \
    header/flexibleheaderview.cpp \
    logger/LogHandler.cpp \
    main.cpp \
    Ci_Principal.cpp \
    autopiloto/CoordGPSaPosicion.cpp


HEADERS += \
    AlturaWorker.h \
    CControladora.h \
    Ci_Principal.h \
    Database/AsyncQuery.h \
    Database/AsyncQueryResult.h \
    Database/AsynqQueryModel.h \
    Database/ConnectionManager.h \
    Estructuras/E_Punto.h \
    Estructuras/Estructuras.h \
    Estructuras/Utils.h \
    LocationValidator.h \
    Mapa/MapaMision.h \
    MyQSettings/myqsetting.h \
    Reporte/layoutcol.h \
    Reporte/qecsreport.h \
    Protocolo/ProyeccionMercator.h \
    Reporte/sectionclass.h \
    SerialPortComboBox/DeviceHotplug.h \
    SerialPortComboBox/SerialManager.h \
    SerialPortComboBox/SerialPortComboBox.h \
    Widgets/PerfilAltitudWidget.h \
    Widgets/Widget_Conexion.h \
    Widgets/Widget_ControlObjetivo.h \
    Widgets/Widget_Drones.h \
    Widgets/Widget_Estado_Cuadroptero.h \
    Widgets/Widget_Ganancias_Control.h \
    Widgets/Widget_Graficas.h \
    Widgets/Widget_Mandos_Cuadroptero.h \
    Widgets/Widget_Parametros.h \
    Widgets/Widget_Puntos.h \
    Widgets/dronedialog.h \
    Widgets/widget_mandos.h \
#    cSerialPort/cserialport.h \
    graficar/XxwTracer.h \
    graficar/customplot.h \
    header/ColumnHeaderLengthAdapter.h \
    header/flexibleheaderview.h \
    autopiloto/CoordGPSaPosicion.h \
    autopiloto/CoordGPSaPosicion_private.h \
    autopiloto/CoordGPSaPosicion_types.h \
    autopiloto/rtwtypes.h \
    logger/LogHandler.h \
    logger/Singleton.h

FORMS += \
    Ci_Principal.ui \
    Widgets/Widget_Conexion.ui \
    Widgets/Widget_ControlObjetivo.ui \
    Widgets/Widget_Drones.ui \
    Widgets/Widget_Estado_Cuadroptero.ui \
    Widgets/Widget_Ganancias_Control.ui \
    Widgets/Widget_Graficas.ui \
    Widgets/Widget_Mandos_Cuadroptero.ui \
    Widgets/Widget_Parametros.ui \
    Widgets/widget_mandos.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

win32:LIBS += -L$$DESTDIR -lUsbDevicesWatcher
else:LIBS += -L$$DESTDIR -lUsbDevicesWatcher

INCLUDEPATH += $$PWD/../usbdeviceswatcher
DEPENDPATH += $$PWD/../usbdeviceswatcher

# libmapa INSTALADA (docs/DESPLIEGUE.md de LibMapaStatic). Por defecto la del Qt
# del kit: C:/libmapa/qt5 o C:/libmapa/qt6. Otra:  qmake LIBMAPA_PREFIX=D:/otra/carpeta
# Trae QCustomPlot 2.1.1 compilada dentro: aqui solo se usa su cabecera
# (graficar/qcustomplot.h, la misma version); compilarla otra vez duplicaria simbolos.
isEmpty(LIBMAPA_PREFIX): LIBMAPA_PREFIX = C:/libmapa/qt$${QT_MAJOR_VERSION}
!exists($$LIBMAPA_PREFIX/libmapa.pri) {
    error("No encuentro libmapa en $${LIBMAPA_PREFIX}. Instalala con este mismo Qt\
 o indica la carpeta:  qmake LIBMAPA_PREFIX=<carpeta>")
}
include($$LIBMAPA_PREFIX/libmapa.pri)


include (qfi/qfi.pri)

RESOURCES += \
    ../build/Recursos/estilos.qrc \
    ../build/Recursos/estilosnuevos/dark/darkstyle.qrc \
    ../build/Recursos/estilosnuevos/light/lightstyle.qrc \
    ../build/Recursos/res.qrc


DISTFILES +=
