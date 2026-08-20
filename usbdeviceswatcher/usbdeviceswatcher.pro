QT -= gui

TEMPLATE = lib
TARGET = $$qtLibraryTarget(UsbDevicesWatcher)
DEFINES += USBDEVICESWATCHER_LIBRARY

CONFIG += c++11

CONFIG(debug, debug|release) {
    DESTDIR = $$PWD/../build/debug
} else {
    DESTDIR = $$PWD/../build/release
}

OBJECTS_DIR = $$DESTDIR/.obj/$$TARGET
MOC_DIR = $$DESTDIR/.moc/$$TARGET
RCC_DIR = $$DESTDIR/.qrc/$$TARGET
UI_DIR = $$DESTDIR/.ui/$$TARGET

SOURCES += \
    usbdeviceswatcher.cpp

HEADERS += \
    usbdeviceswatcher_global.h \
    usbdeviceswatcher.h \
    usbdeviceswatcher_p.h

# Default rules for deployment.
unix:target.path = /usr/lib
!isEmpty(target.path): INSTALLS += target
