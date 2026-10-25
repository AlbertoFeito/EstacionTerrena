#include "Widget_Conexion.h"
#include "ui_Widget_Conexion.h"


#include <QMessageBox>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QDebug>
#include <QThread>
#include "MyQSettings/myqsetting.h"
#include "Rutas.h"


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
#ifdef Q_OS_WIN
    uuids << GUID_DEVINTERFACE_COMPORT;
#endif
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


    crearSelectorEnlace();
}

// Monta en el recuadro de conexion el selector de enlace y, debajo, los campos
// de cada uno (en paginas) y el boton Conectar. Lo ultimo usado sale de
// conf.ini, seccion [ENLACE].
void Widget_Conexion::crearSelectorEnlace()
{
    MyQSetting settings;
    const QString fichero = Rutas::confIni();
    auto valor = [&](const QString &clave, const QVariant &defecto) {
        return settings.cargarSetting(fichero, clave, "ENLACE", defecto);
    };
    auto puerto = [](int valor) {
        auto *sb = new QSpinBox;
        sb->setRange(1, 65535);
        sb->setValue(valor);
        return sb;
    };

    m_cBEnlace = new QComboBox;
    m_cBEnlace->addItems({tr("Serie"), tr("TCP"), tr("UDP")});

    // Serie: el combo de puertos de siempre.
    auto *pSerie = new QWidget;
    auto *fSerie = new QFormLayout(pSerie);
    fSerie->setContentsMargins(0, 0, 0, 0);
    fSerie->addRow(ui->label, comboPuertos);

    // TCP: la estacion se conecta a host:puerto (el simulador o el puente).
    auto *pTcp = new QWidget;
    auto *fTcp = new QFormLayout(pTcp);
    fTcp->setContentsMargins(0, 0, 0, 0);
    m_leTcpHost = new QLineEdit(valor("tcp_host", "127.0.0.1").toString());
    m_sbTcpPuerto = puerto(valor("tcp_puerto", 5760).toInt());
    fTcp->addRow(tr("Host"), m_leTcpHost);
    fTcp->addRow(tr("Puerto"), m_sbTcpPuerto);

    // UDP: escucha en un puerto; el remoto es opcional (si no, contesta a
    // quien le hable).
    auto *pUdp = new QWidget;
    auto *fUdp = new QFormLayout(pUdp);
    fUdp->setContentsMargins(0, 0, 0, 0);
    m_sbUdpLocal = puerto(valor("udp_local", 14551).toInt());
    m_leUdpHost = new QLineEdit(valor("udp_host", "127.0.0.1").toString());
    m_leUdpHost->setPlaceholderText(tr("vacío = quien envíe"));
    m_sbUdpPuerto = puerto(valor("udp_puerto", 14550).toInt());
    fUdp->addRow(tr("Escuchar en"), m_sbUdpLocal);
    fUdp->addRow(tr("Enviar a"), m_leUdpHost);
    fUdp->addRow(tr("Puerto"), m_sbUdpPuerto);

    m_paginas = new QStackedWidget;
    m_paginas->addWidget(pSerie);
    m_paginas->addWidget(pTcp);
    m_paginas->addWidget(pUdp);
    connect(m_cBEnlace, QOverload<int>::of(&QComboBox::currentIndexChanged),
            m_paginas, &QStackedWidget::setCurrentIndex);
    m_cBEnlace->setCurrentIndex(qBound(0, valor("tipo", 0).toInt(), 2));
    m_paginas->setCurrentIndex(m_cBEnlace->currentIndex());

    auto *forLayOut = new QFormLayout(ui->groupBox);
    forLayOut->addRow(tr("Enlace"), m_cBEnlace);
    forLayOut->addRow(m_paginas);
    forLayOut->addRow(ui->pB_Conectar);
    ui->groupBox->setLayout(forLayOut);

    ordenarPestana();
}

// Orden de la pestaña como se usa: primero que dron, luego por donde y
// Conectar, y debajo si se graba el vuelo. Todo arriba, sin el hueco que
// quedaba entre el puerto y el boton (el recuadro se estiraba hasta abajo).
void Widget_Conexion::ordenarPestana()
{
    QGridLayout *g = ui->gridLayout_2;
    g->removeWidget(ui->groupBox);
    g->removeWidget(ui->chB_Guardar);
    g->removeItem(ui->horizontalLayout);
    ui->label_5->setText(tr("Dron"));
    g->addLayout(ui->horizontalLayout, 0, 0);
    g->addWidget(ui->groupBox, 1, 0);
    ui->chB_Guardar->setText(tr("Grabar el vuelo"));
    ui->chB_Guardar->setToolTip(tr("Guarda la telemetría para reproducir el vuelo después"));
    g->addWidget(ui->chB_Guardar, 2, 0);
    g->setRowStretch(0, 0);
    g->setRowStretch(1, 0);
    g->setRowStretch(3, 1);
    ui->groupBox->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
    ui->pB_Conectar->setMinimumHeight(34);
    ui->pB_Conectar->setStyleSheet(QStringLiteral(
        "QPushButton { font-weight: bold; }"
        "QPushButton:checked { background-color: #2e7d32; color: white; }"));
}

// Recuerda en conf.ini el enlace elegido y sus datos.
void Widget_Conexion::guardarEnlace()
{
    MyQSetting settings;
    settings.guardarSettings(Rutas::confIni(), "ENLACE", {
        {"tipo", m_cBEnlace->currentIndex()},
        {"tcp_host", m_leTcpHost->text()},
        {"tcp_puerto", m_sbTcpPuerto->value()},
        {"udp_local", m_sbUdpLocal->value()},
        {"udp_host", m_leUdpHost->text()},
        {"udp_puerto", m_sbUdpPuerto->value()}});
}

// Con enlace abierto no se cambia de enlace ni de datos.
void Widget_Conexion::habilitarCampos(bool habilitar)
{
    m_cBEnlace->setEnabled(habilitar);
    m_paginas->setEnabled(habilitar);
}

Widget_Conexion::~Widget_Conexion()
{
    delete ui;
}

void Widget_Conexion::on_pB_Conectar_clicked()
{
    if(m_serialManager->isConnected()) {
        m_serialManager->desconectar ();
        return;
    }
    guardarEnlace();
    bool empezo = false;
    switch (m_cBEnlace->currentIndex()) {
    case 0: {
        const QString port = comboPuertos->currentText();
        if(!port.isEmpty())
            empezo = m_serialManager->conectar (port);
        break;
    }
    case 1:
        empezo = m_serialManager->conectarTcp (m_leTcpHost->text().trimmed(),
                                               quint16(m_sbTcpPuerto->value()));
        if (empezo) {
            // Conectando: hasta que conteste no se tocan los campos.
            habilitarCampos(false);
            ui->pB_Conectar->setText(tr("Conectando..."));
        }
        break;
    case 2:
        empezo = m_serialManager->conectarUdp (quint16(m_sbUdpLocal->value()),
                                               m_leUdpHost->text(),
                                               quint16(m_sbUdpPuerto->value()));
        break;
    }
    if (!empezo)
        ui->pB_Conectar->setChecked(false);
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

void Widget_Conexion::handleConnectionChange(bool connected)
{    
    ui->pB_Conectar->setText(connected ? tr("Desconectar") : tr("Conectar"));
    ui->pB_Conectar->setChecked(connected);
    comboPuertos->setEnabled(!connected);
    habilitarCampos(!connected);
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
        ui->pB_Conectar->setChecked(false);
        comboPuertos->setEnabled(true);
        habilitarCampos(true);
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


