# Estacion, simulador y pruebas. La estacion enlaza con UsbDevicesWatcher, asi
# que se compila despues (en lugar del obsoleto CONFIG += ordered).
TEMPLATE = subdirs
SUBDIRS += \
    usbdeviceswatcher \
    Principal \
    simulador \
    tests
Principal.depends = usbdeviceswatcher
