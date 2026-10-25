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
 * Obedece los mandos como lo entiende la pantalla de Mandos de la estacion
 * (cinematica sencilla, no un autopiloto):
 * - Regimen Manual (0) o Estabilizado (1): sin mando de radio que lo guie, da
 *   vueltas alrededor de casa.
 * - Regimen Automatico (2), segun la Mision:
 *   - Puntos (0): recorre el plan una vez; al cumplir el ultimo punto vuelve
 *     a casa y da vueltas sobre ella (mision cumplida), como con RTL.
 *   - Circular (1): da vueltas alrededor del punto al que iba.
 *   - Repetir mision (2): recorre el plan una y otra vez.
 *   - RTL (3): vuelve a casa a la velocidad V.RTH y a la altura de casa, y da
 *     vueltas sobre ella.
 *   Un punto se da por cumplido dentro del radio de acercamiento (RPunto), en
 *   metros Mercator como el autopiloto. Cada punto trae su altura y velocidad.
 * - Paracaidas: se para y baja hasta el suelo.
 * - Ganancias: devuelve en la telemetria las que llegan marcadas con
 *   "Actualizar"; las demas siguen como estaban.
 * Gira con un ritmo maximo, sube y baja hacia la altura pedida y alabea y
 * cabecea en proporcion, para que el indicador se mueva.
 */
class SimuladorDron : public QObject
{
    Q_OBJECT
public:
    enum class Tipo { AlaFija, Cuadroptero };

    //! Que esta haciendo ahora.
    enum class Fase {
        VueltasCasa,      //!< Manual / Estabilizado, o automatico sin plan.
        Ruta,             //!< Siguiendo el plan.
        Circular,         //!< Vueltas alrededor del punto al que iba.
        MisionCumplida,   //!< Plan recorrido (mision Puntos): vuelve a casa y da vueltas.
        Regreso,          //!< RTL: volviendo a casa (y luego vueltas sobre ella).
        EnlacePerdido,    //!< Sin enlace con la estacion: a casa y vueltas sobre ella.
        Paracaidas        //!< Bajando con el paracaidas.
    };

    struct Opciones {
        Tipo tipo = Tipo::AlaFija;
        double hz = 10.0;                                   //!< Tramas por segundo.
        QGeoCoordinate casa = QGeoCoordinate(23.0400, -82.4300);
        double velocidad = 15.0;                            //!< m/s.
        double altura = 120.0;                              //!< m.
        double radioOrbita = 300.0;                         //!< m, sin plan.
        int regimen = 0;                                    //!< 0 manual, 1 estabilizado, 2 auto.
        int mision = 0;                                     //!< 0 puntos, 1 circular, 2 repetir, 3 RTL.
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
    Fase fase() const { return m_fase; }
    double altura() const { return m_altura; }
    double velocidad() const { return m_velocidad; }
    //! Se perdio el enlace y aun no han llegado mandos nuevos.
    bool enlacePerdido() const { return m_enlacePerdido; }
    //! Nombre de la fase para la consola.
    static QString nombreFase(Fase f);

public slots:
    //! Bytes de la estacion (mandos o plan), en trozos cualesquiera.
    void recibir(const QByteArray &bytes);
    //! Se ha cortado el enlace con la estacion: el dron vuelve a casa y da
    //! vueltas sobre ella (a V.RTH y a la altura de casa), en cualquier
    //! regimen, hasta que lleguen mandos otra vez.
    void perderEnlace();

signals:
    //! Bytes para la estacion: telemetria y acuses.
    void bytesSalida(const QByteArray &bytes);
    //! Algo que contar en la consola (plan recibido, mandos, punto cumplido...).
    void aviso(const QString &texto);

private:
    void alRecibirMandos(const sMANDOS &m, bool sumaCorrecta);
    void alRecibirPlan(const sPUNTOXYZ *puntos, bool sumaCorrecta);
    QByteArray trama() const;
    void cambiarFase(Fase f);
    double rumboOrbita(const QGeoCoordinate &centro) const;
    double distanciaMercator(const QGeoCoordinate &a, double norte, double este) const;
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
    bool m_paracaidas = false;
    bool m_enlacePerdido = false;
    double m_radio = 20.0;      // metros Mercator (como Radio_Acercamiento)
    double m_velocidadRtl = 0;  // m/s (V.RTH)
    double m_alturaCasa = 0;    // m (Home[2]); 0 = la de siempre
    QVector<Punto> m_plan;
    int m_indice = 0;
    bool m_cumplida = false;    // mision Puntos terminada
    Fase m_fase = Fase::VueltasCasa;
    Punto m_objetivo;           // a donde va (o alrededor de que da vueltas)
    sMANDOS m_ganancias;        // ganancias en uso (las que se devuelven)
};

#endif // SIMULADORDRON_H
