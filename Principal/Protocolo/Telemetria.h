#ifndef TELEMETRIA_H
#define TELEMETRIA_H

#include "Estructuras/Estructuras.h"
#include "Vuelos/MuestraVuelo.h"

#include <QGeoCoordinate>

/*!
 * \brief De las tramas del dron a una MuestraVuelo: lo que se sabe del dron en
 *        un instante, igual para el ala fija, el cuadroptero y un vuelo grabado.
 *
 * Antes CControladora leia campo a campo cada tipo de trama en tres sitios
 * (ala fija, cuadroptero y reproduccion) y lo repartia por el horizonte, el
 * panel, el mapa, las graficas y la grabacion. Ahora la trama se convierte una
 * vez aqui, y la vista y la grabacion usan la MuestraVuelo. Solo Qt Core y
 * Positioning: se prueba sin pantalla (test_telemetria).
 *
 * Unidades: grados, metros y velocidad en km/h (el dron la manda en m/s y se
 * muestra y graba en km/h, como hasta ahora).
 */
namespace Telemetria {

//! Ala fija: trama "abc" con sTRAMA1 (dinamica) y sTRAMA2 (estado y control).
MuestraVuelo desdeAlaFija(const sTRAMA1 &t1, const sTRAMA2 &t2);

//! Cuadroptero: la posicion llega en metros Mercator (norte en la latitud) y
//! se le suma el desplazamiento fijo que ya aplicaba la estacion. Sus
//! ganancias no se muestran (quedan a 0, como antes) y los actuadores son los
//! cuatro motores.
MuestraVuelo desdeCuadroptero(const sCUADROPTEROS &c);

//! Rumbo en 0..360 (el curso deseado puede llegar negativo).
double rumbo0a360(double grados);

//! Punto al que va el dron (Cumplir_Punto) en lat/lon; invalido si no va a
//! ninguno (0, 0).
QGeoCoordinate puntoQueVa(const MuestraVuelo &m);

//! Motor para las graficas: el del ala fija o la media de los cuatro del
//! cuadroptero (\a tipoDron 0 ala fija, 1 cuadroptero).
double motorParaGrafica(const MuestraVuelo &m, int tipoDron);

} // namespace Telemetria

#endif // TELEMETRIA_H
