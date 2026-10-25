#ifndef SERIALMANAGER_H
#define SERIALMANAGER_H

#include <QHostAddress>
#include <QObject>
#include <QTimer>
#include "Estructuras/Estructuras.h"
#include "Protocolo/Tramas.h"

class QSerialPort;
class QTcpSocket;
class QUdpSocket;

/**
 * @brief Enlace con el dron: puerto serie, TCP o UDP, con el mismo protocolo.
 *
 * Las tramas son las de siempre (ver Protocolo/Tramas.h): "abc" con la
 * telemetria, "xyz"/"uvw" con los acuses; hacia el dron, mandos y plan. Da
 * igual por donde lleguen: la decodificacion es la misma y las senales
 * tambien, asi que el resto de la estacion no sabe si habla con un puerto
 * serie, con un puente TCP o con el simulador.
 *
 * - Serie: 115200 8N1, como siempre.
 * - TCP: la estacion es CLIENTE (se conecta a host:puerto; el simulador o el
 *   puente de radio hacen de servidor).
 * - UDP: escucha en un puerto local y manda al host:puerto remoto; sin
 *   remoto, contesta a quien le haya hablado el ultimo.
 */
class SerialManager : public QObject
{
    Q_OBJECT
public:
    enum class Enlace { Serie, Tcp, Udp };

    explicit SerialManager(QObject *parent = nullptr);
    ~SerialManager();

    bool isConnected() const;
    //! Por donde se esta hablando: "COM3", "tcp 127.0.0.1:5760", "udp 14550"...
    QString currentPort() const;
    Enlace enlace() const { return m_enlace; }

    sTRAMA1 getS28() const;
    sTRAMA2 getS34() const;

    sCUADROPTEROS getCuadropteros() const;

    int getTipo_dron() const;
    void setTipo_dron(int value);

public slots:
    //! Abre el puerto serie (como siempre).
    bool conectar(const QString &portName);
    //! Se conecta como cliente TCP. Devuelve true si el intento empezo; la
    //! conexion se confirma con connectionChanged(true) (o errorOccurred).
    bool conectarTcp(const QString &host, quint16 puerto);
    //! Escucha UDP en \a puertoLocal y manda a \a hostRemoto:\a puertoRemoto
    //! (si \a hostRemoto esta vacio, a quien haya mandado el ultimo datagrama).
    bool conectarUdp(quint16 puertoLocal, const QString &hostRemoto, quint16 puertoRemoto);
    void desconectar();
    //! Manda bytes por el enlace abierto.
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

private:
    void leerFlujo();
    void leerDatagramas();
    void entregar(const QByteArray &bytes);
    void cerrarTodo();
    void manejarError(const QString &mensaje);

    Enlace m_enlace = Enlace::Serie;
    QSerialPort *m_serial;
    QTcpSocket *m_tcp;
    QUdpSocket *m_udp;
    QTimer m_esperaTcp;            // tope para conectar por TCP
    QString m_currentPort;
    bool m_conectado = false;
    bool m_cerrando = false;       // cerrando a proposito: los errores no cuentan

    // UDP: a donde se manda.
    QHostAddress m_udpRemoto;
    quint16 m_udpPuertoRemoto = 0;
    bool m_udpRemotoFijo = false;

    Tramas::DecodificadorDron m_decodificador;

    // Ultimos datos recibidos (los leen CControladora y los widgets).
    sTRAMA1 m_trama28;
    sTRAMA2 m_trama34;
    sCUADROPTEROS m_cuadropteros;
};

#endif // SERIALMANAGER_H
