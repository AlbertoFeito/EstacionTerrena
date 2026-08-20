#include "Widget_Conexion.h"
#include "ui_Widget_Conexion.h"


#include <QMessageBox>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>
#include <QThread>


Widget_Conexion::Widget_Conexion(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget_Conexion)
{
    ui->setupUi(this);
    //    serie = new cSerialPort (nullptr,0,0);
    m_cBDrones = ui->cB_Drones;
    m_chB_Guardar = ui->chB_Guardar;
//    ui->groupBox_2->hide ();

    comboPuertos = new SerialPortComboBox(this);
    // Conectar señales
    // Configurar hotplug
    m_hotplug = new DeviceHotplug(this);
    QVector<QUuid> uuids;
    uuids << GUID_DEVINTERFACE_COMPORT;
    m_hotplug->init(uuids);
    m_serialManager = new SerialManager(this);
    connect(m_serialManager, &SerialManager::connectionChanged, this, &Widget_Conexion::handleConnectionChange);
    connect(m_serialManager, &SerialManager::errorOccurred, this, [this](const QString &msg){
        QMessageBox::critical(this, tr("Error"), msg);
    });

    connect(m_hotplug, &DeviceHotplug::deviceDetached,
            this, [this](){
        handlePortDisconnected (comboPuertos->currentText ());
    });


    QFormLayout *forLayOutSerie = new QFormLayout(ui->groupBox);
    forLayOutSerie->addRow (ui->label,comboPuertos);

    ui->groupBox->setLayout (forLayOutSerie);
}

Widget_Conexion::~Widget_Conexion()
{
    delete ui;
}

void Widget_Conexion::on_pB_Conectar_clicked()
{
    if(m_serialManager->isConnected()) {
        m_serialManager->desconectar ();
    } else {
        QString port = comboPuertos->currentText();
        if(!port.isEmpty()) {
            m_serialManager->conectar (port);
        }
    }
}

void Widget_Conexion::openSerialPort()
{
    // Legacy: serial port handling moved to SerialManager
}

void Widget_Conexion::closeSerialPort()
{
    // Legacy: serial port handling moved to SerialManager
}

void Widget_Conexion::about()
{

}

void Widget_Conexion::writeData(const QByteArray &data)
{

}

void Widget_Conexion::readData(QByteArray data)
{

}

void Widget_Conexion::sL_EnviaMandosDron(QByteArray arr)
{
    m_serialManager->serieWrite (arr);
}
quint16 Widget_Conexion::calculateChecksum(const QByteArray &data) {
    quint16 checksum = 0;
    for (char byte : data) {
        checksum += static_cast<quint8>(byte);
    }
    return checksum;
}

void Widget_Conexion::handleConnectionChange(bool connected)
{    
    ui->pB_Conectar->setText(connected ? tr("Desconectar") : tr("Conectar"));
    comboPuertos->setEnabled(!connected);
    emit conectado(connected);
}

void Widget_Conexion::handlePortDisconnected(const QString &port)
{
    if(m_serialManager->isConnected() && m_serialManager->currentPort() == port) {
        m_serialManager->desconectar ();
        QMessageBox::warning(this, tr("Desconectado"),
                             tr("El puerto %1 fue desconectado físicamente").arg(port));
        emit conectado(false);
        ui->pB_Conectar->setText("Conectar");
        comboPuertos->setEnabled(true);
    }
}

SerialManager *Widget_Conexion::serialManager() const
{
    return m_serialManager;
}

QCheckBox *Widget_Conexion::chB_Guardar() const
{
    return m_chB_Guardar;
}

void Widget_Conexion::setChB_Guardar(QCheckBox *chB_Guardar)
{
    m_chB_Guardar = chB_Guardar;
}

QComboBox *Widget_Conexion::cBDrones() const
{
    return m_cBDrones;
}

void Widget_Conexion::setCBDrones(QComboBox *cBDrones)
{
    m_cBDrones = cBDrones;
}


