#include "SerialPortComboBox.h"
#include <windows.h>
// SerialPortComboBox.cpp
#include "SerialPortComboBox.h"
#include <QDebug>

// Fix para MinGW
#ifdef __MINGW32__
#ifndef _PHYSICAL_ADDRESS_
#define _PHYSICAL_ADDRESS_
typedef struct _PHYSICAL_ADDRESS {
    ULONGLONG QuadPart;
} PHYSICAL_ADDRESS, *PPHYSICAL_ADDRESS;
#endif
#endif

#include <QMessageBox>
#include <QSerialPortInfo>
#include <initguid.h>
#include <ntddser.h>  // GUID_DEVINTERFACE_COMPORT

SerialPortComboBox::SerialPortComboBox(QWidget *parent)
    : QComboBox(parent)
    , m_hotplug(new DeviceHotplug(this))
{
    // Configurar DeviceHotplug para monitorear puertos serie
    QVector<QUuid> uuids;
    uuids << GUID_DEVINTERFACE_COMPORT;
    m_hotplug->init(uuids);

    // Conectar señales de hotplug a un timer de refresco
    connect(m_hotplug, &DeviceHotplug::deviceAttached,
            this, [this](){ m_refreshTimer.start(500); });
    connect(m_hotplug, &DeviceHotplug::deviceDetached,
            this, [this](){ m_refreshTimer.start(500);

    });

    // Timer para refrescar con un pequeño retraso
    m_refreshTimer.setSingleShot(true);
    connect(&m_refreshTimer, &QTimer::timeout, this, &SerialPortComboBox::refreshPorts);

    // Señal de selección de puerto
    connect(this, QOverload<int>::of(&QComboBox::currentIndexChanged),
            [this](int index){
        if(index >= 0) emit portSelected(currentText());
    });
    refreshPorts(); // Carga inicial
}
void SerialPortComboBox::setConnectedPort(const QString &port)
{
    m_connectedPort = port;
}

void SerialPortComboBox::refreshPorts()
{
    // Limpiar caché de puertos de Qt
    QSerialPortInfo::availablePorts().clear(); // Forzar recarga

    // Obtener lista actual de puertos
    QStringList newPorts;
    foreach(const QSerialPortInfo &portInfo, QSerialPortInfo::availablePorts()) {
        newPorts << portInfo.portName();
    }
    newPorts.sort();

    if(newPorts != m_currentPorts) {
        // Detectar puertos añadidos
        QStringList added;
        foreach(const QString &port, newPorts) {
            if(!m_currentPorts.contains(port)) {
                added.append(port);
            }
        }

        // Detectar puertos removidos
        QStringList removed;
        foreach(const QString &port, m_currentPorts) {
            if(!newPorts.contains(port)) {
                removed.append(port);
            }
        }

        // Actualizar combo box
        blockSignals(true);
        clear();
        addItems(newPorts);
        blockSignals(false);

        // Manejar desconexiones
        foreach(const QString &port, removed) {
            if(port == m_connectedPort) {
                emit portDisconnected(port);
                m_connectedPort.clear();

                // Mostrar mensaje de desconexión
                QMessageBox::warning(parentWidget(),
                                     "Desconexión",
                                     QString("El puerto %1 fue desconectado").arg(port));
            }
        }

        // Seleccionar último puerto añadido
        if(!added.isEmpty()) {
            QString lastPort = added.last();
            int index = findText(lastPort);
            if(index != -1) {
                setCurrentIndex(index);
                emit portSelected(lastPort);
            }
        }

        m_currentPorts = newPorts;
    }
}
