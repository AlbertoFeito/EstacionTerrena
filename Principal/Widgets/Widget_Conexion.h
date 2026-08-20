#ifndef WIDGET_CONEXION_H
#define WIDGET_CONEXION_H

#include <QtWidgets>
#include <SerialPortComboBox/SerialManager.h>
#include <SerialPortComboBox/SerialPortComboBox.h>

namespace Ui {
class Widget_Conexion;
}

class Widget_Conexion : public QWidget
{
    Q_OBJECT

public:
    explicit Widget_Conexion(QWidget *parent = nullptr);
    ~Widget_Conexion();

    QComboBox *cBDrones() const;
    void setCBDrones(QComboBox *cBDrones);

    QCheckBox *chB_Guardar() const;
    void setChB_Guardar(QCheckBox *chB_Guardar);

    SerialManager *serialManager() const;

    quint16 calculateChecksum(const QByteArray &data);
signals:
    void datosRecibidos(QByteArray);
    void conectado(bool);
private slots:
    void on_pB_Conectar_clicked();

    void openSerialPort();
    void closeSerialPort();
    void about();
    void writeData(const QByteArray &data);
    void readData(QByteArray data);

public slots:
    void sL_EnviaMandosDron(QByteArray arr);

    void handleConnectionChange(bool connected);
    void handlePortDisconnected(const QString &port);
private:
    Ui::Widget_Conexion *ui;

    int TimerID;
    QByteArray datosRecividos;

    QComboBox *m_cBDrones;
    QCheckBox *m_chB_Guardar;
    SerialPortComboBox *comboPuertos;
    DeviceHotplug *m_hotplug;
    SerialManager *m_serialManager;

};

#endif // WIDGET_CONEXION_H
