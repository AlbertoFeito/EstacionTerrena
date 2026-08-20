#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include <QObject>
#include <QSerialPort>
#include <QMessageBox>
#include <QDebug>
#include <QTimer>
#include "Estructuras/Estructuras.h"

/**
 * @brief Clase para manejo de comunicación serial con protocolo estructurado
 *
 * Protocolo esperado:
 * - Tramas comienzan con encabezados "abc", "xyz" o "uvw"
 * - Tras "abc" vienen estructuras sTRAMA1 y sTRAMA2
 * - Tras "xyz" o "uvw" vienen acuses de recibo
 * - Se esperan tramas de planificación con múltiples sPUNTOXYZ
 */
class SerialManager : public QObject
{
    Q_OBJECT
public:
    explicit SerialManager(QObject *parent = nullptr);
    ~SerialManager();


    bool isConnected() const;
    QString currentPort() const;

    sTRAMA1 getS28() const;
    sTRAMA2 getS34() const;

    sCUADROPTEROS getCuadropteros() const;

    int getTipo_dron() const;
    void setTipo_dron(int value);

public slots:
    bool conectar(const QString &portName);
    void desconectar();
    void serieWrite(const QByteArray &data);

signals:
    void connectionChanged(bool connected);
    void errorOccurred(const QString &message);
    void newPlanificacionReceived(sPUNTOXYZ *puntosRecibidos, int numParametros);
    void newTramaConstanteReceived(const char a, const char b, const char c,
                                  const sTRAMA1 &s_28, const sTRAMA2 &s_34);
    void newCuadropteros(const sCUADROPTEROS &s_cuadropteros);
    void siVolverEnviar();
    void si_AcuseReciboXYZ();
    void si_AcuseReciboUVW();

private slots:
    void readyRead();
    void procesarBuffer();

private:
    QSerialPort *m_serial;
    QString m_currentPort;
    QTimer *m_timeoutTimer;

    int tipo_dron = 0;
    // Buffer y máquina de estados
    QByteArray m_buffer;
    static constexpr int MAX_BUFFER_SIZE = 4096; // 4KB máximo para prevenir desbordamiento

    enum State {
        WaitingForHeader,
        WaitingForStruct28,
        WaitingForStruct34,
        WaitingForPlanificacion,
        WaitingForCuadropteros
    };
    State m_currentState = WaitingForHeader;

    // Variables para almacenar datos recibidos
    char m_headerA, m_headerB, m_headerC;
    sTRAMA1 m_trama28;
    sTRAMA2 m_trama34;
    bool m_planificacionOK = false;

    sCUADROPTEROS m_cuadropteros;

    // Métodos auxiliares
    void resetProtocolo();
    bool validarChecksum(const QByteArray &data);
    bool procesarEncabezado(const QByteArray &header);
    bool procesarTrama28();
    bool procesarTrama34();
    bool procesarPlanificacion();
    bool procesarCuadropteros();
    void manejarError(const QString &mensaje);
    bool procesarAcuseRecibo(const QByteArray &tipo);
};

#endif // SERIALMANAGER_H
