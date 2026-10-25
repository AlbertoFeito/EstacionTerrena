#include "EnlacesSimulador.h"

#include <QNetworkDatagram>
#include <QSerialPort>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QUdpSocket>

// Sin vias abiertas hasta que se pidan.
EnlacesSimulador::EnlacesSimulador(QObject *parent) : QObject(parent)
{
}

// Servidor TCP: cada estacion que se conecta recibe la telemetria y puede
// mandar mandos y planes.
bool EnlacesSimulador::abrirTcp(quint16 puerto, QString *error)
{
    m_servidor = new QTcpServer(this);
    if (!m_servidor->listen(QHostAddress::Any, puerto)) {
        if (error)
            *error = tr("TCP %1: %2").arg(puerto).arg(m_servidor->errorString());
        return false;
    }
    connect(m_servidor, &QTcpServer::newConnection, this, [this]() {
        while (QTcpSocket *s = m_servidor->nextPendingConnection()) {
            m_clientes.append(s);
            emit aviso(tr("Estación conectada por TCP desde %1:%2")
                           .arg(s->peerAddress().toString()).arg(s->peerPort()));
            connect(s, &QTcpSocket::readyRead, this, [this, s]() {
                const QByteArray bytes = s->readAll();
                if (!m_cortado)
                    emit recibido(bytes);
            });
            // Sin ninguna estacion TCP, el dron se ha quedado sin enlace.
            connect(s, &QTcpSocket::disconnected, this, [this, s]() {
                m_clientes.removeAll(s);
                s->deleteLater();
                emit aviso(tr("Estación TCP desconectada"));
                if (m_clientes.isEmpty())
                    emit enlacePerdido();
            });
        }
    });
    return true;
}

// UDP: escucha en el puerto local y manda al remoto.
bool EnlacesSimulador::abrirUdp(quint16 puertoLocal, const QString &hostRemoto,
                                quint16 puertoRemoto, QString *error)
{
    m_udp = new QUdpSocket(this);
    m_udpRemoto = QHostAddress(hostRemoto);
    m_udpPuertoRemoto = puertoRemoto;
    if (m_udpRemoto.isNull()) {
        if (error)
            *error = tr("UDP: dirección remota no válida: %1").arg(hostRemoto);
        return false;
    }
    if (!m_udp->bind(QHostAddress::AnyIPv4, puertoLocal)) {
        if (error)
            *error = tr("UDP %1: %2").arg(puertoLocal).arg(m_udp->errorString());
        return false;
    }
    connect(m_udp, &QUdpSocket::readyRead, this, [this]() {
        while (m_udp->hasPendingDatagrams()) {
            const QByteArray bytes = m_udp->receiveDatagram().data();
            if (!m_cortado)
                emit recibido(bytes);
        }
    });
    return true;
}

// Puerto serie a 115200 8N1, como el enlace de radio.
bool EnlacesSimulador::abrirSerie(const QString &puerto, QString *error)
{
    m_serie = new QSerialPort(puerto, this);
    m_serie->setBaudRate(QSerialPort::Baud115200);
    m_serie->setDataBits(QSerialPort::Data8);
    m_serie->setParity(QSerialPort::NoParity);
    m_serie->setStopBits(QSerialPort::OneStop);
    m_serie->setFlowControl(QSerialPort::NoFlowControl);
    if (!m_serie->open(QIODevice::ReadWrite)) {
        if (error)
            *error = tr("Serie %1: %2").arg(puerto, m_serie->errorString());
        return false;
    }
    connect(m_serie, &QSerialPort::readyRead, this, [this]() {
        const QByteArray bytes = m_serie->readAll();
        if (!m_cortado)
            emit recibido(bytes);
    });
    return true;
}

// Puerto en el que escucha el servidor TCP (0 si no hay).
quint16 EnlacesSimulador::puertoTcp() const
{
    return m_servidor ? m_servidor->serverPort() : 0;
}

// Puerto local del UDP (0 si no hay).
quint16 EnlacesSimulador::puertoUdp() const
{
    return m_udp ? m_udp->localPort() : 0;
}

// Lo mismo por todas las vias abiertas (nada durante un corte).
void EnlacesSimulador::enviar(const QByteArray &bytes)
{
    if (m_cortado)
        return;
    for (QTcpSocket *s : qAsConst(m_clientes))
        s->write(bytes);
    if (m_udp)
        m_udp->writeDatagram(bytes, m_udpRemoto, m_udpPuertoRemoto);
    if (m_serie && m_serie->isOpen())
        m_serie->write(bytes);
}

// Corte de radio de unos segundos: ni se manda ni se recibe nada; la estacion
// lo vera como "SIN DATOS" y el dron se queda sin enlace.
void EnlacesSimulador::cortar(int segundos)
{
    if (m_cortado || segundos <= 0)
        return;
    m_cortado = true;
    emit aviso(tr("CORTE DE RADIO simulado durante %1 s").arg(segundos));
    emit enlacePerdido();
    QTimer::singleShot(segundos * 1000, this, [this]() {
        m_cortado = false;
        emit aviso(tr("Fin del corte de radio: el dron sigue volviendo a casa hasta recibir mandos"));
    });
}
