#ifndef ENLACESSIMULADOR_H
#define ENLACESSIMULADOR_H

#include <QHostAddress>
#include <QList>
#include <QObject>

class QSerialPort;
class QTcpServer;
class QTcpSocket;
class QUdpSocket;

/*!
 * \brief Las vias por las que el simulador habla con la estacion.
 *
 * Se pueden abrir varias a la vez; lo que manda el simulador sale por todas y
 * lo que llega por cualquiera se le entrega.
 *
 * - TCP: SERVIDOR en un puerto (la estacion es cliente). Admite varias
 *   estaciones a la vez.
 * - UDP: escucha en un puerto y manda a host:puerto (por defecto, a la
 *   estacion en 127.0.0.1:14551).
 * - Serie: un puerto a 115200 8N1. Para probar sin cable hace falta un par de
 *   puertos virtuales (com0com en Windows, socat en Linux).
 */
class EnlacesSimulador : public QObject
{
    Q_OBJECT
public:
    explicit EnlacesSimulador(QObject *parent = nullptr);

    bool abrirTcp(quint16 puerto, QString *error);
    bool abrirUdp(quint16 puertoLocal, const QString &hostRemoto, quint16 puertoRemoto,
                  QString *error);
    bool abrirSerie(const QString &puerto, QString *error);

    //! Estaciones TCP conectadas ahora mismo.
    int clientesTcp() const { return m_clientes.size(); }
    //! Puerto TCP de verdad (util si se pidio el 0 = cualquiera libre).
    quint16 puertoTcp() const;
    quint16 puertoUdp() const;

public slots:
    //! Manda \a bytes por todas las vias abiertas.
    void enviar(const QByteArray &bytes);

signals:
    //! Bytes llegados de una estacion, por cualquier via.
    void recibido(const QByteArray &bytes);
    void aviso(const QString &texto);

private:
    QTcpServer *m_servidor = nullptr;
    QList<QTcpSocket *> m_clientes;
    QUdpSocket *m_udp = nullptr;
    QHostAddress m_udpRemoto;
    quint16 m_udpPuertoRemoto = 0;
    QSerialPort *m_serie = nullptr;
};

#endif // ENLACESSIMULADOR_H
