#ifndef GRABADORVUELO_H
#define GRABADORVUELO_H

#include <QDateTime>
#include <QObject>
#include <QString>
#include <QTimer>
#include <QVector>

/*!
 * \brief Una muestra del vuelo, tal cual llega del dron (sin redondear).
 */
struct MuestraVuelo
{
    double lat = 0, lon = 0;
    double cabeceo = 0, banqueo = 0;
    double velocidadKmh = 0, altura = 0, curso = 0;
    double distanciaChequeo = 0, cursoDeseado = 0, cursoImu = 0;
    int regimen = 0, mision = 0, satelites = 0;
    //! P e I de cabeceo, banqueo, velocidad y altura; P de curso; P e I de rumbo.
    double ganancias[11] = {};
    double actuadores[4] = {};   //!< estabilizadores, alerones, motor, rumbo
    double convergenciaBanqueo = 0, convergenciaCabeceo = 0;
    QDateTime fecha;             //!< si no se da, la hora de anotar()
};

/*!
 * \brief Graba un vuelo en la tabla DatosVuelos de la base de datos de drones.
 *
 * Sustituye a la grabacion de antes, que en cada trama lanzaba un INSERT
 * asincrono con los valores pasados a texto con 2 o 4 decimales (la posicion,
 * unos 11 m: la traza reproducida salia en escalones) y pedia el numero de
 * vuelo tambien de forma asincrona, asi que las primeras tramas podian caer en
 * el vuelo anterior.
 *
 * Ahora:
 * - empezar() da de alta el vuelo en el momento (numero de vuelo del dron + 1,
 *   guardado en DronTable.Vuelos) antes de aceptar ninguna muestra;
 * - anotar() solo guarda la muestra en memoria (nada de disco a 10 Hz);
 * - cada segundo, y al terminar, las pendientes se escriben de una vez en una
 *   transaccion con una consulta preparada, con los numeros tal cual (REAL).
 *
 * Usa su propia conexion SQLite al mismo fichero (las conexiones de Qt no se
 * comparten entre objetos que pueden ir a otro ritmo) y solo Qt Core + Sql:
 * se prueba sin interfaz.
 */
class GrabadorVuelo : public QObject
{
    Q_OBJECT
public:
    explicit GrabadorVuelo(const QString &ficheroDb, QObject *parent = nullptr);
    ~GrabadorVuelo() override;

    //! Crea DatosVuelos (y su indice) si no existen. Lo usa tambien la estacion al arrancar.
    bool asegurarTabla();

    //! Empieza un vuelo nuevo del dron \a nombreDron. false (y error()) si no existe.
    bool empezar(const QString &nombreDron);
    //! Guarda una muestra del vuelo en curso (en memoria; se escribe cada segundo).
    void anotar(const MuestraVuelo &m);
    //! Escribe lo pendiente y cierra el vuelo.
    void terminar();
    //! Escribe ya lo pendiente (lo hace solo cada segundo).
    bool volcar();

    bool grabando() const { return m_grabando; }
    int vuelo() const { return m_vuelo; }
    int dron() const { return m_dron; }
    int pendientes() const { return m_pendientes.size(); }
    QString error() const { return m_error; }

private:
    bool abrir();

    QString m_fichero;
    QString m_conexion;
    QVector<MuestraVuelo> m_pendientes;
    QTimer m_reloj;
    bool m_grabando = false;
    int m_vuelo = -1;
    int m_dron = -1;
    QString m_error;
};

#endif // GRABADORVUELO_H
