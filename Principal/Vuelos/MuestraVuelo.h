#ifndef MUESTRAVUELO_H
#define MUESTRAVUELO_H

// Lo que se sabe del dron en un instante (MuestraVuelo) y un punto del plan
// confirmado (PuntoPlan). Aparte de GrabadorVuelo para que lo use quien no
// graba: la telemetria en vivo (Protocolo/Telemetria) y su vista.

#include <QDateTime>
#include <QMetaType>
#include <QString>

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
    //! Punto al que va el dron (Cumplir_Punto: norte, este en metros Mercator,
    //! como se le mando en el plan; 0, 0 si no va a ninguno) y su altura.
    double puntoNorte = 0, puntoEste = 0, puntoAltura = 0;
    QDateTime fecha;             //!< si no se da, la hora de anotar()
};

/*!
 * \brief Un punto del plan confirmado por el dron: donde esta (para pintarlo)
 *        y en metros Mercator tal como se le mando (para saber cuales cumple).
 */
struct PuntoPlan
{
    QString nombre;
    double lat = 0, lon = 0, altura = 0, velocidad = 0;
    double norte = 0, este = 0;
};

Q_DECLARE_METATYPE(MuestraVuelo)
Q_DECLARE_METATYPE(PuntoPlan)

#endif // MUESTRAVUELO_H
