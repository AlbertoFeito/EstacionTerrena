#include "cserialport.h"

#include <QThread>


cSerialPort::cSerialPort(QObject *parent, quint64 timeQuery, quint64 timeSleep):
    QObject(parent),
    m_tquery (timeQuery),
    m_tsleepBuff (timeSleep)

{
    m_serial = new QSerialPort();
buffer.clear ();
    //    if(m_tsleepBuff !=0)
    //    {
    //        m_ClkSleepBuffer = new QTimer();
    //        connect(m_ClkSleepBuffer, &QTimer::timeout, this, &cSerialPort::readyReadBuffer);
    //        //        if(m_tsleepBuff != 0)
    //        m_ClkSleepBuffer->start(m_tsleepBuff);
    //    }

    if (m_tquery != 0)
    {
        m_ClkQuery = new QTimer();
        connect(m_ClkQuery, &QTimer::timeout, this, &cSerialPort::readyRead);
        m_ClkQuery->start(m_tquery);
    }
    else
        connect(m_serial, &QSerialPort::readyRead, this, &cSerialPort::readyRead);

    connect(this, &cSerialPort::newVariableReceived,
            this, &cSerialPort::handleVariable);

    connect(this, &cSerialPort::newVariableEstaticaReceived,
            this, &cSerialPort::handleVariableEstatica);

    //    connect(this, &cSerialPort::newPlanificacionReceived,
    //            this, &cSerialPort::handlePlanificacion);

    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, this, &cSerialPort::procesarDatosABC);
    timer->start(1000); // Procesar "abc" cada 1 segundo
}

cSerialPort::cSerialPort(QString portName, quint64 timeQuery, quint64 timeSleep, QObject *parent, quint32 baudRate,
                         quint8 stopBits, quint8 flowControl, quint8 dataBits, quint8 parity):
    QObject(parent),
    m_PortName(portName),
    m_BaudRate(static_cast<QSerialPort::BaudRate>(baudRate)),
    m_FlowControl(static_cast<QSerialPort::FlowControl>(flowControl)),
    m_DataBits (static_cast<QSerialPort::DataBits>(dataBits)),
    m_Parity (static_cast<QSerialPort::Parity>(parity)),
    m_StopBits(static_cast<QSerialPort::StopBits>(stopBits)),
    m_tquery (timeQuery),
    m_tsleepBuff (timeSleep)
{

    cSerialPort(this,timeQuery, timeSleep);

    m_serial->setPortName(m_PortName);
    m_serial->setFlowControl(m_FlowControl);
    m_serial->setDataBits(m_DataBits);
    m_serial->setParity(m_Parity);
    m_serial->setBaudRate(m_BaudRate);
    m_serial->setStopBits(m_StopBits);
}

bool cSerialPort::openPort()
{
    closePort();
    return m_serial->open(QIODevice::ReadWrite);
}

void cSerialPort::closePort()
{
    if(m_serial->isOpen()) m_serial->close();
}

bool cSerialPort::isLittleEndian() const
{
    return QSysInfo::ByteOrder == QSysInfo::LittleEndian;
}

float cSerialPort::swapFloatEndianness(float value) const
{
    union {
        float f;
        quint8 b[4];
    } u, res;

    u.f = value;
    res.b[0] = u.b[3];
    res.b[1] = u.b[2];
    res.b[2] = u.b[1];
    res.b[3] = u.b[0];

    return res.f;
}

void cSerialPort::processVariable(const QByteArray &data)
{
    //    const Variable *v = reinterpret_cast<const Variable*>(data.constData());
    //    emit newTelemetry(*v);
}

void cSerialPort::processVariableEstatica(const QByteArray &data)
{
    //    const VariableEstatica *ve = reinterpret_cast<const VariableEstatica*>(data.constData());
    //    emit newStaticParams(*ve);
}

void cSerialPort::processPlanificacion(const QByteArray &data)
{
    //    const Planificacion *p = reinterpret_cast<const Planificacion*>(data.constData());
    //    emit newWaypoints(*p);
}

TRAMA2 cSerialPort::getS34() const
{
    return s34;
}

TRAMA1 cSerialPort::getS28() const
{
    return s28;
}

void cSerialPort::setPlanificacionOK(bool value)
{
    PlanificacionOK = value;
}

bool cSerialPort::getPlanificacionOK() const
{
    return PlanificacionOK;
}


e_RxMando cSerialPort::getMandosRX() const
{
    return mandosRX;
}

void cSerialPort::setMandosRX(const e_RxMando &value)
{
    mandosRX = value;
}

void cSerialPort::coordenadas(QString str)
{
    str = "2300.00000,N,08300.00000,W";
    auto latStr = str.split (',').at (0);
    auto latChar = str.split (',').at (1);
    auto lonStr = str.split (',').at (2);
    auto lonChar = str.split (',').at (3);

    QString auxLatGrad=latStr.left(2);
    latStr.remove(0,2);
    QString auxLatMin=latStr.left(2);
    latStr.remove(0,3);

    QString auxLatSeg;

    if (latStr.size() < 2)
        latStr = latStr.append("000");
    if (latStr.size() == 2)
        latStr = latStr.append("00");
    if (latStr.size() == 3)
        latStr = latStr.append('0');

    auxLatSeg = QString::number((latStr.toDouble()/10000) * 6);

    double LatDecim=auxLatGrad.toDouble ()+(double)(auxLatMin.toDouble ()/60+auxLatSeg.toDouble ()/3600);

    QString auxLonGrad=lonStr.left(3);
    lonStr.remove(0,3);
    QString auxLonMin=lonStr.left(2);
    lonStr.remove(0,3);

    QString auxLonSeg;

    if (lonStr.size() < 2)
        lonStr = lonStr.append("000");
    if (lonStr.size() == 2)
        lonStr = lonStr.append("00");
    if (lonStr.size() == 3)
        lonStr = lonStr.append('0');

    auxLonSeg = QString::number((lonStr.toDouble()/10000) * 6);

    double LonDecim=auxLonGrad.toDouble ()+(double)(auxLonMin.toDouble ()/60+auxLonSeg.toDouble ()/3600);

    LonDecim = (lonChar == 'W' ? LonDecim * -1 : LonDecim);
    LatDecim = (latChar == 'S' ? LatDecim * -1 : LatDecim);
    ////           geoPos.setLatitude(LatDecim);
    ////           geoPos.setLongitude(LonDecim);
    mandosRX.latitud   = LatDecim;
    mandosRX.longitud  = LonDecim;
}

QSerialPort *cSerialPort::getSerial() const
{
    return m_serial;
}

void cSerialPort::procesarDatosABC() {
    if (!buffer.isEmpty()) {
        if (buffer.contains("abc")) {
            int pos = buffer.indexOf("abc");
            buffer.remove(0, pos + 3); // Eliminar encabezado "abc"
            currentState = WaitingForStruct28;
            readyRead (); // Procesar los datos asociados a "abc"
        }
    }
}

void cSerialPort::handleVariable(const Variable &data)
{
    qDebug()<< data.Realimentacion_Latitud;
    qDebug()<< data.Realimentacion_Longitud;
}

void cSerialPort::handleVariableEstatica(const VariableEstatica &data)
{

}

void cSerialPort::handlePlanificacion(const Planificacion &data)
{

}

void cSerialPort::readyRead()
{
    buffer.append(m_serial->readAll ());

    switch (currentState) {

    case WaitingForHeader: {
        if (buffer.contains("uvw"))
        {
            PlanificacionOK = true;
            int headerPos = buffer.indexOf("uvw");
            if (headerPos == -1) {
                // Descarta datos basura, pero conserva los últimos 2 bytes (por si son "ab")
                buffer = buffer.size() >= 2 ? buffer.right(2) : QByteArray();
                return;
            }

            // Extrae "abc" y elimina los datos anteriores al encabezado
            char u = buffer[headerPos];
            char v = buffer[headerPos + 1];
            char w = buffer[headerPos + 2];
            buffer.remove(0, headerPos + 3);  // Elimina hasta después de "abc"
            int c = buffer[0];
            if(c == 48)
            {
                emit siVolverEnviar ();
                return;
            }
            else if(c == 49)
            {
               //enviar mensaje de planificacion exitosa al usuario
                return;
            }
//            currentState = WaitingForPlanificacion;
        }
//        else if(buffer.contains ("abc"))
//        {
//            int headerPos = buffer.indexOf("abc");
//            if (headerPos == -1) {
//                // Descarta datos basura, pero conserva los últimos 2 bytes (por si son "ab")
//                buffer = buffer.size() >= 2 ? buffer.right(2) : QByteArray();
//                return;
//            }

//            // Extrae "abc" y elimina los datos anteriores al encabezado
//            a = buffer[headerPos];
//            b = buffer[headerPos + 1];
//            c = buffer[headerPos + 2];
//            buffer.remove(0, headerPos + 3);  // Elimina hasta después de "abc"
//            currentState = WaitingForStruct28;
//        }
        break;
    }
        // --- Lectura de la estructura de 28 bytes ---
    case WaitingForStruct28: {
        if (buffer.size() < sizeof(TRAMA1)) return;  // Espera más datos

        memcpy(&s28, buffer.constData(), sizeof(TRAMA1));
        buffer.remove(0, sizeof(TRAMA1));
        currentState = WaitingForStruct32;
        break;
    }
        // --- Lectura de la estructura de 32 bytes ---
    case WaitingForStruct32: {
        if (buffer.size() < sizeof(TRAMA2)) return;  // Espera más datos
        memcpy(&s34, buffer.constData(), sizeof(TRAMA2));
        buffer.remove(0, sizeof(TRAMA2));
        currentState = WaitingForHeader;

        // Emite los datos procesados
        emit newTramaConstanteReceived (a, b, c, s28, s34);
        //        buffer.clear ();
        break;
    }
    case WaitingForPlanificacion: {
        auto s = sizeof(10 *sizeof (PUNTOXYZ));
        qDebug()<<buffer.size() << s;
        if (buffer.size() < s ) return;  // Espera más datos

        PUNTOXYZ puntosRecividos[10];
//        puntosRecividos[0].modo = buffer.at (0);
//        QString x = QString((char)buffer.at (1) + (char)buffer.at (2) + (char)buffer.at (3) + (char)buffer.at (4));
//        QString y = QString((char)buffer.at (5) + (char)buffer.at (6) + (char)buffer.at (7) + (char)buffer.at (8));
//        QString z = QString((char)buffer.at (9) + (char)buffer.at (10) + (char)buffer.at (11) + (char)buffer.at (12));
//        puntosRecividos[0].x = x.toFloat ();
//        puntosRecividos[0].y = y.toFloat ();
//        puntosRecividos[0].z = z.toFloat ();

        memcpy(puntosRecividos, buffer.constData(), sizeof(10 *sizeof (PUNTOXYZ)));
        buffer.remove(0, sizeof(10 *sizeof (PUNTOXYZ)));

        emit newPlanificacionReceived(puntosRecividos,10);
                buffer.clear ();
        currentState = WaitingForHeader;
        break;
    }
    }

}

void cSerialPort::readyReadBuffer()
{
    //    QList<QByteArray> tList = data.split ('%');
    //    //    qDebug() <<"tList: "<< tList<<"---------";
    //    int c = 0;
    //    foreach(auto tempByteArray, tList)
    //    {
    //        auto first = tempByteArray.startsWith ("1/M/");
    //        auto first2 = tempByteArray.startsWith ("1//");
    //        if(first || first2)
    //        {
    //            auto index = tempByteArray.lastIndexOf ('*');
    //            if(index != -1){
    //                tempByteArray.remove (index+1,2);
    //                auto last = tempByteArray.back ();
    //                if(last == '*')
    //                {
    //                    auto aux = tempByteArray.split('/');
    //                    mandosRX.cabeceo   = aux.at(2).toFloat() ;
    //                    mandosRX.banqueo   = aux.at(3).toFloat() ;
    //                    mandosRX.rumbo     = aux.at(4).toFloat() ;
    //                    mandosRX.velocidad = aux.at(8).toFloat() ;
    //                    mandosRX.altura    = aux.at(26).toFloat();
    //                    mandosRX.latitud   = aux.at(24).toFloat();
    //                    mandosRX.longitud  = aux.at(25).toFloat()*-1;
    //                    mandosRX.KPC       = aux.at(12).toFloat();
    //                    mandosRX.KIC       = aux.at(13).toFloat();
    //                    mandosRX.KDC       = aux.at(14).toFloat();
    //                    mandosRX.KPB       = aux.at(15).toFloat();
    //                    mandosRX.KIB       = aux.at(16).toFloat();
    //                    mandosRX.KDB       = aux.at(17).toFloat();
    //                    mandosRX.ConvBan   = aux.at(19).toFloat();
    //                    mandosRX.ConvCab   = aux.at(18).toFloat();

    //                }
    //            }
    //        }
    //    }

    //    //    emit serieRead(data);
    //    data.clear();
}

template<typename T>
void cSerialPort::convertStructEndianness(T &data)
{
    if(isLittleEndian()) return;

    quint8 *bytes = reinterpret_cast<quint8*>(&data);
    constexpr size_t floatSize = sizeof(float);

    for(size_t i = 0; i < sizeof(T); i += floatSize) {
        if(i + floatSize > sizeof(T)) break;
        std::reverse(bytes + i, bytes + i + floatSize);
    }
}
