#ifndef GRABADORVUELO_H
#define GRABADORVUELO_H

#include <QDateTime>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QThread>
#include <QTimer>
#include <QVector>

#include "MuestraVuelo.h"


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
 *   transaccion con una consulta preparada, con los numeros tal cual (REAL),
 *   en un HILO APARTE: cerrar la transaccion espera al disco (en Windows, y
 *   mas en debug o con el antivirus, decenas o cientos de ms) y en el hilo de
 *   la interfaz eso paraba el mapa cada segundo y luego lo hacia correr; esa
 *   conexion usa WAL con synchronous=NORMAL (una espera al disco por escritura
 *   en lugar de varias, y sigue a salvo si se cae la estacion);
 * - cada muestra lleva el punto al que va el dron, y cada plan que confirma
 *   se guarda en PlanesVuelo: la reproduccion pinta la ruta, los cumplidos y
 *   la linea al proximo punto como en vivo.
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
    //! Guarda el plan que el dron acaba de confirmar (o el que ya tenia al
    //! empezar): la reproduccion lo pinta desde ese momento.
    bool anotarPlan(const QVector<PuntoPlan> &plan, const QDateTime &fecha = QDateTime());
    //! Escribe lo pendiente, espera a que este en disco y cierra el vuelo.
    void terminar();
    //! Manda ya a escribir lo pendiente (lo hace solo cada segundo); no espera.
    bool volcar();
    //! Espera a que el hilo de escritura haya escrito todo lo mandado.
    void esperar();

    bool grabando() const { return m_grabando; }
    int vuelo() const { return m_vuelo; }
    int dron() const { return m_dron; }
    int pendientes() const { return m_pendientes.size(); }
    QString error() const { return m_error; }

private:
    bool abrir();
    void anotarError(const QString &error);

    QString m_fichero;
    QString m_conexion;
    QThread m_hilo;                 // donde se escribe
    QObject *m_escritor = nullptr;  // vive en m_hilo: lo que se le encarga corre alli
    QVector<MuestraVuelo> m_pendientes;
    QTimer m_reloj;
    bool m_grabando = false;
    int m_vuelo = -1;
    int m_dron = -1;
    QString m_error;
};

#endif // GRABADORVUELO_H
