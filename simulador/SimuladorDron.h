#ifndef SIMULADORDRON_H
#define SIMULADORDRON_H

#include "Estructuras/Estructuras.h"
#include "Protocolo/Tramas.h"

#include <QElapsedTimer>
#include <QGeoCoordinate>
#include <QObject>
#include <QTimer>
#include <QVector>

/*!
 * \brief Un dron de mentira que habla el protocolo de la estacion.
 *
 * Manda la telemetria de siempre (ala fija: "abc" + sTRAMA1 + sTRAMA2;
 * cuadroptero: sCUADROPTEROS) a la frecuencia pedida y contesta a lo que le
 * manda la estacion: a los mandos ("xyz") y al plan ("uvw") con su acuse.
 *
 * No sabe de puertos ni de red: saca bytes por bytesSalida() y recibe bytes
 * por recibir(). Las vias (TCP, UDP, serie) las pone el programa.
 *
 * Como vuela (cinematica sencilla, no un autopiloto):
 * - regimen 2 (automatico) con plan: va de punto en punto del plan; un punto se
 *   da por cumplido dentro del radio de acercamiento, en metros Mercator, como
 *   el autopiloto. Tras el ultimo vuelve al primero.
 * - regimen 0 / 1, o sin plan: da vueltas alrededor de casa.
 * Gira con un ritmo maximo, sube y baja hacia la altura pedida y alabea y
 * cabecea en proporcion, para que el indicador se mueva.
 */
class SimuladorDron : public QObject
{
    Q_OBJECT
public:
    enum class Tipo { AlaFija, Cuadroptero };

    struct Opciones {
        Tipo tipo = Tipo::AlaFija;
        double hz = 10.0;                                   //!< Tramas por segundo.
        QGeoCoordinate casa = QGeoCoordinate(23.0400, -82.4300);
        double velocidad = 15.0;                            //!< m/s.
        double altura = 120.0;                              //!< m.
        double radioOrbita = 300.0;                         //!< m, sin plan.
        int regimen = 0;                                    //!< 0 manual, 1 semi, 2 auto.
    };

    //! Un punto del plan tal como llega en "uvw" (metros Mercator, norte primero).
    struct Punto {
        QGeoCoordinate pos;
        double norte = 0.0;
        double este = 0.0;
        double altura = 0.0;
        double velocidad = 0.0;   //!< m/s; 0 = la de siempre.
    };

    explicit SimuladorDron(const Opciones &opciones, QObject *parent = nullptr);

    //! Empieza (o para) a mandar telemetria.
    void arrancar();
    void parar();

    //! Avanza la simulacion \a dt segundos y devuelve la trama de telemetria
    //! (sin emitirla). Lo usa el temporizador; las pruebas lo llaman a mano.
    QByteArray avanzar(double dt);

    // Estado, para mostrarlo y para las pruebas.
    QGeoCoordinate posicion() const { return m_pos; }
    double rumbo() const { return m_rumbo; }
    int regimen() const { return m_regimen; }
    QVector<Punto> plan() const { return m_plan; }
    int puntoActual() const { return m_indice; }
    QGeoCoordinate casa() const { return m_casa; }
    double radioAcercamiento() const { return m_radio; }

public slots:
    //! Bytes de la estacion (mandos o plan), en trozos cualesquiera.
    void recibir(const QByteArray &bytes);

signals:
    //! Bytes para la estacion: telemetria y acuses.
    void bytesSalida(const QByteArray &bytes);
    //! Algo que contar en la consola (plan recibido, mandos, punto cumplido...).
    void aviso(const QString &texto);

private:
    void alRecibirMandos(const sMANDOS &m, bool sumaCorrecta);
    void alRecibirPlan(const sPUNTOXYZ *puntos, bool sumaCorrecta);
    QByteArray trama() const;
    static double normalizar(double grados);

    Opciones m_op;
    QTimer m_reloj;
    QElapsedTimer m_desde;
    Tramas::DecodificadorEstacion m_decodificador;

    // Estado del vuelo.
    QGeoCoordinate m_pos;
    QGeoCoordinate m_casa;
    double m_rumbo = 0.0;
    double m_rumboDeseado = 0.0;
    double m_velocidad = 0.0;
    double m_altura = 0.0;
    double m_alabeo = 0.0;
    double m_cabeceo = 0.0;
    double m_distanciaPunto = 0.0;
    int m_regimen = 0;
    int m_comando = 0;
    double m_radio = 20.0;      // metros Mercator (como Radio_Acercamiento)
    QVector<Punto> m_plan;
    int m_indice = 0;
    sMANDOS m_mandos;           // ultimos mandos (las ganancias se devuelven)
};

#endif // SIMULADORDRON_H
