#include "SerialManager.h"

SerialManager::SerialManager(QObject *parent)
    : QObject(parent),
      m_serial(new QSerialPort(this)),
      m_timeoutTimer(new QTimer(this))
{
    // Configuración del puerto serie
    m_serial->setBaudRate(QSerialPort::Baud115200);
    m_serial->setDataBits(QSerialPort::Data8);
    m_serial->setParity(QSerialPort::NoParity);
    m_serial->setStopBits(QSerialPort::OneStop);
    m_serial->setFlowControl(QSerialPort::NoFlowControl);

    // Conexión de señales
    connect(m_serial, &QSerialPort::readyRead, this, &SerialManager::readyRead);
    connect(m_timeoutTimer, &QTimer::timeout, this, &SerialManager::procesarBuffer);

    // Configurar timer para timeout (500ms)
    m_timeoutTimer->setInterval(500);
}

SerialManager::~SerialManager()
{
    desconectar();
}

bool SerialManager::isConnected() const
{
    return m_serial->isOpen();
}

QString SerialManager::currentPort() const
{
    return m_currentPort;
}

bool SerialManager::conectar(const QString &portName)
{
    if(isConnected()) {
        emit errorOccurred(tr("Ya hay una conexión activa"));
        return false;
    }

    m_serial->setPortName(portName);
    if(m_serial->open(QIODevice::ReadWrite)) {
        m_currentPort = portName;
        resetProtocolo();
        m_timeoutTimer->start();
        emit connectionChanged(true);
        return true;
    } else {
        emit errorOccurred(tr("Error al abrir puerto: %1").arg(m_serial->errorString()));
        return false;
    }
}

void SerialManager::desconectar()
{
    m_timeoutTimer->stop();
    if(m_serial->isOpen()) {
        m_serial->close();
    }
    m_currentPort.clear();
    emit connectionChanged(false);
}

void SerialManager::serieWrite(const QByteArray &data)
{
    if(!isConnected()) {
        manejarError("Intento de escritura en puerto no conectado");
        return;
    }

    qint64 bytesWritten = m_serial->write(data);
    if(bytesWritten != data.size()) {
        manejarError("Error al escribir datos en puerto serie");
    }
}

void SerialManager::readyRead()
{
    // Leer todos los datos disponibles
    QByteArray newData = m_serial->readAll();

    // Protección contra desbordamiento del buffer
    if(m_buffer.size() + newData.size() > MAX_BUFFER_SIZE) {
        manejarError("Desbordamiento de buffer - reiniciando protocolo");
        resetProtocolo();
        return;
    }

    m_buffer.append(newData);
}

void SerialManager::procesarBuffer()
{
    //    while(!m_buffer.isEmpty()) {
    switch(m_currentState) {
    case WaitingForHeader:
        procesarEncabezado(m_buffer);
        break;

    case WaitingForStruct28:
        procesarTrama28();
        break;

    case WaitingForStruct34:
        procesarTrama34();
        break;

    case WaitingForPlanificacion:
        procesarPlanificacion();
        break;

    case WaitingForCuadropteros:
        procesarCuadropteros ();
        break;
    }
    if(!m_buffer.isEmpty ())
        QMetaObject::invokeMethod (this,"procesarBuffer",Qt::QueuedConnection);
    //    }
}

int SerialManager::getTipo_dron() const
{
    return tipo_dron;
}

void SerialManager::setTipo_dron(int value)
{
    tipo_dron = value;
}

sCUADROPTEROS SerialManager::getCuadropteros() const
{
    return m_cuadropteros;
}

bool SerialManager::procesarEncabezado(const QByteArray &data)
{
    // Buscar posibles encabezados
    int posABC = data.indexOf("abc");
    int posXYZ = data.indexOf("xyz");
    int posUVW = data.indexOf("uvw");

    // Determinar qué encabezado viene primero
    int pos = -1;
    QByteArray header;

    if(posABC >= 0 && (posABC < posXYZ || posXYZ == -1) && (posABC < posUVW || posUVW == -1)) {
        pos = posABC;
        header = "abc";
    } else if(posXYZ >= 0 && (posXYZ < posUVW || posUVW == -1)) {
        pos = posXYZ;
        header = "xyz";
    } else if(posUVW >= 0) {
        pos = posUVW;
        header = "uvw";
    }

    if(pos == -1) {
        // No hay encabezado completo - descartar datos basura pero mantener últimos bytes
        m_buffer = m_buffer.size() >= 2 ? m_buffer.right(2) : QByteArray();
        return false;
    }

    // Extraer encabezado y cambiar estado
    m_headerA = data[pos];
    m_headerB = data[pos+1];
    m_headerC = data[pos+2];

    if(tipo_dron == 0)//0 ala fija
    {
        m_buffer.remove(0, pos + 3); // Eliminar encabezado
        if(header == "abc") {
            m_currentState = WaitingForStruct28;
        } else if(header == "xyz" || header == "uvw") {
            procesarAcuseRecibo(header);
        }
    }
    else
    {
        if(header == "abc")
            m_currentState = WaitingForCuadropteros;
    }

    return true;
}

bool SerialManager::procesarTrama28()
{
    if(m_buffer.size() < sizeof(sTRAMA1)) {
        return false; // Esperar más datos
    }

    // Copiar datos a estructura con verificación
    //    if(!validarChecksum(m_buffer.left(sizeof(sTRAMA1)))) {
    //        manejarError("Checksum inválido en TRAMA28");
    //        resetProtocolo();
    //        return;
    //    }

    memcpy(&m_trama28, m_buffer.constData(), sizeof(sTRAMA1));
    m_buffer.remove(0, sizeof(sTRAMA1));
    m_currentState = WaitingForStruct34;
    return true;
}

bool SerialManager::procesarTrama34()
{
    if(m_buffer.size() < sizeof(sTRAMA2)) {
        return false; // Esperar más datos
    }
    // Copiar datos a estructura con verificación
    //    if(!validarChecksum(m_buffer.left(sizeof(sTRAMA2)))) {
    //        manejarError("Checksum inválido en TRAMA34");
    //        resetProtocolo();
    //        return;
    //    }
    memcpy(&m_trama34, m_buffer.constData(), sizeof(sTRAMA2));
    m_buffer.remove(0, sizeof(sTRAMA2));

    // Emitir datos completos
    emit newTramaConstanteReceived(m_headerA, m_headerB, m_headerC, m_trama28, m_trama34);
    m_currentState = WaitingForHeader;
    return true;
}

bool SerialManager::procesarPlanificacion()
{
    const int puntosEsperados = 10;
    const int tamanoEsperado = puntosEsperados * sizeof(sPUNTOXYZ);

    if(m_buffer.size() < tamanoEsperado) {
        return false; // Esperar más datos
    }

    // Verificar checksum
    //    if(!validarChecksum(m_buffer.left(tamanoEsperado))) {
    //        manejarError("Checksum inválido en datos de planificación");
    //        resetProtocolo();
    //        return;
    //    }

    sPUNTOXYZ puntos[puntosEsperados];
    memcpy(puntos, m_buffer.constData(), tamanoEsperado);
    m_buffer.remove(0, tamanoEsperado);

    emit newPlanificacionReceived(puntos, puntosEsperados);
    m_currentState = WaitingForHeader;
    return true;
}

bool SerialManager::procesarCuadropteros()
{
    if(m_buffer.size() < sizeof(sCUADROPTEROS)) {
        return false; // Esperar más datos
    }

    memcpy(&m_cuadropteros, m_buffer.constData(), sizeof(sCUADROPTEROS));
    m_buffer.remove(0, sizeof(sCUADROPTEROS));

    // Emitir datos completos
    emit newCuadropteros(m_cuadropteros);
    m_currentState = WaitingForHeader;
    return true;
}

bool SerialManager::procesarAcuseRecibo(const QByteArray &tipo)
{
    if(m_buffer.size() < 1) {
        return false; // Esperar más datos
    }

    char respuesta = m_buffer.at(0);
    m_buffer.remove(0, 1);

    if(tipo == "xyz") {
        if(respuesta == '1') {
            emit si_AcuseReciboXYZ();
        } else {
            manejarError("Fallo en acuse de recibo XYZ");
        }
    } else if(tipo == "uvw") {
        if(respuesta == '1') {
            m_planificacionOK = true;
            emit si_AcuseReciboUVW();
        } else {
            m_planificacionOK = false;
            emit siVolverEnviar();
        }
    }

    m_currentState = WaitingForHeader;
    return true;
}

bool SerialManager::validarChecksum(const QByteArray &data)
{
    // Implementar lógica real de checksum según protocolo
    // Esta es una implementación de ejemplo
    quint8 checksum = 0;
    for(char byte : data) {
        checksum ^= byte; // XOR simple como ejemplo
    }
    return checksum == 0; // Suponiendo que el checksum debe dar 0
}

void SerialManager::resetProtocolo()
{
    m_buffer.clear();
    m_currentState = WaitingForHeader;
    m_planificacionOK = false;
}

void SerialManager::manejarError(const QString &mensaje)
{
    qWarning() << "Error SerialManager:" << mensaje;
    emit errorOccurred(mensaje);
    resetProtocolo();
}

sTRAMA2 SerialManager::getS34() const
{
    return m_trama34;
}

sTRAMA1 SerialManager::getS28() const
{
    return m_trama28;
}
