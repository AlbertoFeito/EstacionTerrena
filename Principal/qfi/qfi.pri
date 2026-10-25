# QFlightInstruments: solo el EADI (horizonte artificial con velocidad,
# altura y rumbo), que es el instrumento de la estacion. Los demas de la
# libreria (EHSI y los seis basicos) se quitaron: no se usaban.
HEADERS += \
    $$PWD/qfi_doxygen.h \
    $$PWD/qfi_Colors.h \
    $$PWD/qfi_Fonts.h \
    $$PWD/qfi_EADI.h

SOURCES += \
    $$PWD/qfi_Colors.cpp \
    $$PWD/qfi_Fonts.cpp \
    $$PWD/qfi_EADI.cpp

RESOURCES += \
    $$PWD/qfi.qrc
