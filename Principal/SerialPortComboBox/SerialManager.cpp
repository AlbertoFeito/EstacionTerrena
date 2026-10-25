#include "SerialManager.h"

#include <QDebug>
#include <QNetworkDatagram>
#include <QSerialPort>
#include <QTcpSocket>
#include <QUdpSocket>

// Prepara las tres vias (cerradas) y engancha su lectura al decodificador.
SerialManager::SerialManager(QObject *parent)
    : QObject(parent),
      m_serial(new QSerialPort(this)),
      m_tcp(new QTcpSocket(this)),
      m_udp(new QUdpSocket(this))
{
    // Configuración del puerto serie
    m_serial->setBaudRate(QSerialPort::Baud115200);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    connect(m_serial, &QSerialPort::readyRead, this, &SerialManager::leerFlujo);
    connect(m_tcp, &QTcpSocket::readyRead, this, &SerialManager::leerFlujo);
    connect(m_udp, &QUdpSocket::readyRead, this, &SerialManager::leerDatagramas);

    // TCP: la conexion se confirma o falla mas tarde.
    m_esperaTcp.setSingleShot(true);
    m_esperaTcp.setInterval(5000);
    // Primero se desconecta y luego se avisa: el aviso puede ser un dialogo
    // modal y, mientras esta abierto, la estacion ya tiene que verse desconectada.
    connect(&m_esperaTcp, &QTimer::timeout, this, [this]() {
        const QString donde = m_currentPort;
        desconectar();
        manejarError(tr("No se pudo conectar a %1: no responde").arg(donde));
    });
    connect(m_tcp, &QTcpSocket::connected, this, [this]() {
        m_esperaTcp.stop();
        m_conectado = true;
        emit connectionChanged(true);
    });
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
    const auto senalError = &QAbstractSocket::errorOccurred;
#else
    const auto senalError = QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::error);
#endif
    connect(m_tcp, senalError, this, [this](QAbstractSocket::SocketError) {
        // Al cerrar a proposito (o si el cierre provoca otro error) no se avisa.
        if (m_enlace != Enlace::Tcp || m_currentPort.isEmpty() || m_cerrando)
            return;
        const QString motivo = m_tcp->errorString();
        const QString donde = m_currentPort;
        m_esperaTcp.stop();
        desconectar();
        manejarError(tr("Enlace TCP %1: %2").arg(donde, motivo));
    });
}

// Cierra lo que este abierto.
SerialManager::~SerialManager()
{
    cerrarTodo();
}

// Hay enlace abierto (en TCP, ya conectado).
bool SerialManager::isConnected() const
{
    return m_conectado;
}

// Descripcion del enlace abierto, o vacia.
QString SerialManager::currentPort() const
{
    return m_currentPort;
}

// Abre el puerto serie a 115200 8N1.
bool SerialManager::conectar(const QString &portName)
{
    if(isConnected()) {
        emit errorOccurred(tr("Ya hay una conexión activa"));
        return false;
    }

    m_enlace = Enlace::Serie;
    m_serial->setPortName(portName);
    if(m_serial->open(QIODevice::ReadWrite)) {
        m_currentPort = portName;
        m_decodificador.reiniciar();
        m_conectado = true;
        emit connectionChanged(true);
        return true;
    } else {
        emit errorOccurred(tr("Error al abrir puerto: %1").arg(m_serial->errorString()));
        return false;
    }
}

// Empieza la conexion TCP; connected() o errorOccurred() dicen como acaba.
bool SerialManager::conectarTcp(const QString &host, quint16 puerto)
{
    if (isConnected() || m_tcp->state() != QAbstractSocket::UnconnectedState) {
        emit errorOccurred(tr("Ya hay una conexión activa"));
        return false;
    }
    m_enlace = Enlace::Tcp;
    m_currentPort = QStringLiteral("tcp %1:%2").arg(host).arg(puerto);
    m_decodificador.reiniciar();
    m_esperaTcp.start();
    m_tcp->connectToHost(host, puerto);
    return true;
}

// Escucha UDP en el puerto local; el remoto puede ser fijo o el ultimo que hable.
bool SerialManager::conectarUdp(quint16 puertoLocal, const QString &hostRemoto, quint16 puertoRemoto)
{
    if (isConnected()) {
        emit errorOccurred(tr("Ya hay una conexión activa"));
        return false;
    }
    m_enlace = Enlace::Udp;
    m_udpRemotoFijo = !hostRemoto.trimmed().isEmpty() && puertoRemoto != 0;
    m_udpRemoto = m_udpRemotoFijo ? QHostAddress(hostRemoto.trimmed()) : QHostAddress();
    m_udpPuertoRemoto = m_udpRemotoFijo ? puertoRemoto : 0;
    if (m_udpRemotoFijo && m_udpRemoto.isNull()) {
        emit errorOccurred(tr("Dirección UDP remota no válida: %1").arg(hostRemoto));
        return false;
    }
    if (!m_udp->bind(QHostAddress::AnyIPv4, puertoLocal)) {
        emit errorOccurred(tr("No se pudo escuchar UDP en el puerto %1: %2")
                           .arg(puertoLocal).arg(m_udp->errorString()));
        return false;
    }
    m_currentPort = m_udpRemotoFijo
        ? QStringLiteral("udp %1 → %2:%3").arg(puertoLocal).arg(hostRemoto.trimmed()).arg(puertoRemoto)
        : QStringLiteral("udp %1").arg(puertoLocal);
    m_decodificador.reiniciar();
    m_conectado = true;
    emit connectionChanged(true);
    return true;
}

// Cierra el enlace y avisa (siempre, como antes).
void SerialManager::desconectar()
{
    cerrarTodo();
    m_currentPort.clear();
    emit connectionChanged(false);
}

// Cierra las tres vias sin avisar.
void SerialManager::cerrarTodo()
{
    m_esperaTcp.stop();
    m_conectado = false;
    m_cerrando = true;
    if (m_serial->isOpen())
        m_serial->close();
    if (m_tcp->state() != QAbstractSocket::UnconnectedState)
        m_tcp->abort();
    if (m_udp->state() != QAbstractSocket::UnconnectedState)
        m_udp->close();
    m_cerrando = false;
}

// Escribe por el enlace abierto. En UDP hace falta saber a quien: el remoto
// fijo o el ultimo que mando algo.
void SerialManager::serieWrite(const QByteArray &data)
{
    if(!isConnected()) {
        manejarError("Intento de escritura sin enlace con el dron");
        return;
    }

    qint64 bytesWritten = -1;
    switch (m_enlace) {
    case Enlace::Serie:
        bytesWritten = m_serial->write(data);
        break;
    case Enlace::Tcp:
        bytesWritten = m_tcp->write(data);
        break;
    case Enlace::Udp:
        if (m_udpRemoto.isNull() || m_udpPuertoRemoto == 0) {
            manejarError("UDP: aún no se sabe a quién enviar (no ha llegado nada del dron "
                         "y no hay dirección remota)");
            return;
        }
        bytesWritten = m_udp->writeDatagram(data, m_udpRemoto, m_udpPuertoRemoto);
        break;
    }
    if(bytesWritten != data.size()) {
        manejarError("Error al escribir datos en el enlace con el dron");
    }
}

// Serie y TCP: un flujo de bytes, que se va dando al decodificador.
void SerialManager::leerFlujo()
{
    QIODevice *origen = m_enlace == Enlace::Tcp ? static_cast<QIODevice *>(m_tcp)
                                                : static_cast<QIODevice *>(m_serial);
    entregar(origen->readAll());
}

// UDP: cada datagrama al decodificador; sin remoto fijo, se contesta al
// ultimo que hablo.
void SerialManager::leerDatagramas()
{
    while (m_udp->hasPendingDatagrams()) {
        const QNetworkDatagram d = m_udp->receiveDatagram();
        if (!m_udpRemotoFijo) {
            m_udpRemoto = d.senderAddress();
            m_udpPuertoRemoto = quint16(d.senderPort());
        }
        entregar(d.data());
    }
}

// Decodifica y reparte cada trama como siempre: guarda los ultimos datos y
// emite las senales de antes.
void SerialManager::entregar(const QByteArray &bytes)
{
    m_decodificador.agregar(bytes);
    for (const auto &e : m_decodificador.eventos()) {
        switch (e.tipo) {
        case Tramas::DecodificadorDron::Tipo::Constante:
            m_trama28 = e.t1;
            m_trama34 = e.t2;
            emit newTramaConstanteReceived('a', 'b', 'c', m_trama28, m_trama34);
            break;
        case Tramas::DecodificadorDron::Tipo::Cuadroptero:
            m_cuadropteros = e.cuadro;
            emit newCuadropteros(m_cuadropteros);
            break;
        case Tramas::DecodificadorDron::Tipo::AcuseMandos:
            if (e.recibido)
                emit si_AcuseReciboXYZ();
            else
                manejarError("Fallo en acuse de recibo XYZ");
            break;
        case Tramas::DecodificadorDron::Tipo::AcusePlan:
            if (e.recibido)
                emit si_AcuseReciboUVW();
            else
                emit siVolverEnviar();
            break;
        }
    }
}

// Tipo de dron activo (0 = ala fija, 1 = cuadroptero).
int SerialManager::getTipo_dron() const
{
    return m_decodificador.tipoDron();
}

// Cambia el tipo de dron: cambia como se leen las tramas "abc".
void SerialManager::setTipo_dron(int value)
{
    m_decodificador.setTipoDron(value);
}

// Ultima trama de cuadroptero recibida.
sCUADROPTEROS SerialManager::getCuadropteros() const
{
    return m_cuadropteros;
}

// Avisa del error y descarta lo que hubiera a medias.
void SerialManager::manejarError(const QString &mensaje)
{
    qWarning() << "Error SerialManager:" << mensaje;
    emit errorOccurred(mensaje);
    m_decodificador.reiniciar();
}

// Ultima sTRAMA2 recibida.
sTRAMA2 SerialManager::getS34() const
{
    return m_trama34;
}

// Ultima sTRAMA1 recibida.
sTRAMA1 SerialManager::getS28() const
{
    return m_trama28;
}
