#ifndef CSERIALPORT_H
#define CSERIALPORT_H
#include <minwindef.h>
#include <qglobal.h>

#include <QObject>
#include <QSerialPort>
#include <QSerialPortInfo>
#include <QTimer>
#include <QDebug>
//#include "const_dron.h"
#pragma once

#pragma pack(push, 1)  // Alineación de 1 byte

struct Variable {
    // Entradas y salidas dinámicas
    float Referencia_Manual_Cabeceo;
    float Referencia_Manual_Banqueo;
    float Referencia_Manual_Vreal;
    float Referencia_Cabeceo;
    float Referencia_Banqueo;
    float Control_Estabilizadores;
    float Control_Alerones;
    float Control_Motor;

    // Entradas dinámicas
    float Realimentacion_Cabeceo;
    float Realimentacion_Banqueo;
    float Realimentacion_Velocidad;
    float Realimentacion_Altura;
    float Realimentacion_Curso;
    float Realimentacion_Latitud;
    float Realimentacion_Longitud;

    // Salidas dinámicas
    float Convergencia_Cabeceo;
    float Ganancias_Cabeceo[2];
    float Convergencia_Banqueo;
    float Ganancias_Banqueo[2];
    float Ganancias_Velocidad[2];
    float Ganancias_Altura[2];
    float Ganancias_Curso;
    float Distancia_Chequeo;
    float Curso_Deseado;
};

struct VariableEstatica {
    unsigned char Sintonizacion_Cabeceo;
    float Ganancias_Cabeceo_Manual1;
    float Ganancias_Cabeceo_Manual2;
    unsigned char Actualizar_Ganancias_Cabeceo;
    unsigned char Sintonizacion_Banqueo;
    float Ganancias_Banqueo_Manual1;
    float Ganancias_Banqueo_Manual2;
    unsigned char Actualizar_Ganancias_Banqueo;
    float Ganancias_Velocidad_Manual1;
    float Ganancias_Velocidad_Manual2;
    unsigned char Actualizar_Ganancias_Velocidad;
    unsigned char Regimen;
    float Ganancias_Altura_Manual1;
    float Ganancias_Altura_Manual2;
    unsigned char Actualizar_Ganancias_Altura;
    float Ganancias_Curso_Manual;
    unsigned char Comando_Mision;
    float Latitud_Longitud1;
    float Latitud_Longitud2;
    unsigned char Actualizar_Ganancias_Curso;
};

struct PUNTOXYZ{
    char modo = 3; //0,1,2,3,4
    float x = 0;//lat
    float y = 0;//long
    float z = 0;//altura
    float parametro_00 = 0;
    float parametro_01 = 0;
    float parametro_02 = 0;
    float velocidad = 0;//velocidad
};
struct Planificacion {
    PUNTOXYZ p0[10];
};

struct TRAMA1
{
    // Entradas dinámicas
    float Realimentacion_Cabeceo = 0;
    float Realimentacion_Banqueo= 0;
    float Realimentacion_Velocidad= 0;
    float Realimentacion_Altura= 0;
    float Realimentacion_Curso= 0;
    float Realimentacion_Latitud= 0;
    float Realimentacion_Longitud= 0;
};

struct TRAMA2
{
    float Control_Estabilizadores = 0;
    float Control_Alerones= 0;
    float Control_Motor= 0;
    float Control_Rumbo= 0;
    float Convergencia_Cabeceo= 0;
    float Convergencia_Banqueo= 0;
    float Distancia_Chequeo = 0;
    float Curso_Deseado= 0;
};



struct e_RxMando
{
    quint8 Id = 0;
    float cabeceo =0;
    float banqueo =0;
    float rumbo =0;
    float velocidad =0;
    float altura =0;
    float latitud =0;
    float longitud =0;
    int rpm = 0;
    float KPC =0;
    float KIC =0;
    float KDC =0;
    float KPB =0;
    float KIB =0;
    float KDB =0;
    float ConvBan =0;
    float ConvCab =0;
};
#pragma pack(pop)
class cSerialPort : public QObject
{
    Q_OBJECT
public:
    cSerialPort(QObject *parent = nullptr, quint64 timeQuery = 0, quint64 timeSleep = 0);
    cSerialPort(QString portName, quint64 timeQuery = 0, quint64 timeSleep = 0, QObject *parent = nullptr, quint32 baudRate = 9600,
                quint8 stopBits = 1, quint8 flowControl = 0, quint8 dataBits = 8, quint8 parity = 0);

    void setPortName      (QString portName){m_PortName = portName; m_serial->setPortName(m_PortName);}
    void setBaudRate      (quint32 baudRate){m_BaudRate = baudRate; m_serial->setBaudRate(static_cast<QSerialPort::BaudRate>(m_BaudRate));}
    void setStopBits      (quint8 stopBits){m_StopBits = static_cast<QSerialPort::StopBits>(stopBits); m_serial->setStopBits(m_StopBits);}
    void setStopBits      (QSerialPort::StopBits stopBits){m_StopBits = stopBits; m_serial->setStopBits(m_StopBits);}
    void setFlowControl   (quint8 flowControl){m_FlowControl = static_cast<QSerialPort::FlowControl>(flowControl); m_serial->setFlowControl(m_FlowControl);}
    void setFlowControl   (QSerialPort::FlowControl flowControl){m_FlowControl = flowControl; m_serial->setFlowControl(m_FlowControl);}
    void setDataBits      (quint8 dataBits){m_DataBits = static_cast<QSerialPort::DataBits>(dataBits); m_serial->setDataBits(m_DataBits);}
    void setDataBits      (QSerialPort::DataBits dataBits){m_DataBits = dataBits; m_serial->setDataBits(m_DataBits);}
    void setParity        (quint8 parity){m_Parity = static_cast<QSerialPort::Parity>(parity); m_serial->setParity(m_Parity);}
    void setParity        (QSerialPort::Parity parity){m_Parity = parity; m_serial->setParity(m_Parity);}
    void setTimeQuery     (quint64 t){m_tquery = t; m_ClkQuery->start(m_tquery);}
    void setTimeSleepBuff (quint64 t){m_tsleepBuff = t;}

    QString getPortName()      {return m_serial->portName();}
    quint32 getBaudRate()      {return m_serial->baudRate();}
    quint8  getStopBits()      {return m_serial->stopBits();}
    quint8  getFlowControl()   {return m_serial->flowControl();}
    quint8  getDataBits()      {return m_serial->dataBits();}
    quint8  getParity()        {return m_serial->parity();}
    quint64 getTimeQuery()     {return m_tquery;}
    quint64 getTimeSleepBuff() {return m_tsleepBuff;}

    bool connectPort(){return openPort();}
    void disconnectPort( ){closePort();}


    e_RxMando getMandosRX() const;
    void setMandosRX(const e_RxMando &value);

    void coordenadas(QString str);

    QSerialPort *getSerial() const;


    bool getPlanificacionOK() const;

    void setPlanificacionOK(bool value);

    TRAMA1 getS28() const;

    TRAMA2 getS34() const;

public slots:
    void serieWrite(const QByteArray data)
    {
        qDebug()<<"data.size ()"<<m_serial->write(data);
    }

    void procesarDatosABC();

private slots:
    void handleVariable(const Variable &data);
    void handleVariableEstatica(const VariableEstatica &data);
    void handlePlanificacion(const Planificacion &data);


signals:
    void serieRead(QByteArray);

    void newVariableReceived(const Variable &data);
    void newVariableEstaticaReceived(const VariableEstatica &data);
    void newPlanificacionReceived(PUNTOXYZ *puntosRecividos,int NumParametros);
    void newTramaConstanteReceived(const char a, const char b, const char c, const TRAMA1 &s_28, const TRAMA2 &s_34);
    void errorOccurred(const QString &error);
    void  siVolverEnviar();

protected:
    QSerialPort *m_serial;
    QTimer *m_ClkQuery, *m_ClkSleepBuffer;

private:
    void readyRead();
    void readyReadBuffer();
    bool openPort();
    void closePort();

    QString m_PortName;
    quint32 m_BaudRate;
    QSerialPort::FlowControl m_FlowControl;
    QSerialPort::DataBits m_DataBits;
    QSerialPort::Parity m_Parity;
    QSerialPort::StopBits m_StopBits;
    quint64 m_tquery, m_tsleepBuff;
    QByteArray data;


    e_RxMando mandosRX;
    Variable variable;
    VariableEstatica vEstatica;
    Planificacion planificacion;

    QByteArray buffer;
    QByteArray bufferAcumulado;
    bool isLittleEndian() const;
    float swapFloatEndianness(float value) const;
    template<typename T> void convertStructEndianness(T &data);

    void processVariable(const QByteArray &data);
    void processVariableEstatica(const QByteArray &data);
    void processPlanificacion(const QByteArray &data);


    quint8 arr[65];
    quint8 chekSum;
    bool chekSumOK;


    //variante 2
    enum State { WaitingForHeader, WaitingForStruct28, WaitingForStruct32, WaitingForPlanificacion };
    State currentState = WaitingForHeader;
    QSerialPort serialPort;
    char a, b, c;
    TRAMA1 s28;
    TRAMA2 s34;
    Planificacion s290;
   bool PlanificacionOK = false;//respuesta de la planificacion

};

#endif // CSERIALPORT_H
