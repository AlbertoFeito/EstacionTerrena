#ifndef SERIALPORTCOMBOBOX_H
#define SERIALPORTCOMBOBOX_H

#include <QComboBox>
#include <QTimer>
#include <QSerialPort>
#include <QSerialPortInfo>

#include "DeviceHotplug.h"

class SerialPortComboBox : public QComboBox
{
    Q_OBJECT
public:
    explicit SerialPortComboBox(QWidget *parent = nullptr);

    void setConnectedPort(const QString &port);
    QString connectedPort() const { return m_connectedPort; }

signals:
    void portSelected(const QString &port);
    void portDisconnected(const QString &port);

private slots:
    void refreshPorts();

private:
    DeviceHotplug *m_hotplug;
    QTimer m_refreshTimer;
    QStringList m_currentPorts;
    QString m_connectedPort;
};

#endif // SERIALPORTCOMBOBOX_H
