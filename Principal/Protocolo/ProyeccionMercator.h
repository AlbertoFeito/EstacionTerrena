#ifndef PROYECCIONMERCATOR_H
#define PROYECCIONMERCATOR_H

#include <QGeoCoordinate>
#include <QPointF>

/*!
 * \brief Proyeccion Web Mercator esferica (EPSG:3857) que habla el autopiloto.
 *
 * El plan de vuelo (trama "uvw"), la casa de los mandos (trama "xyz") y el
 * punto a cumplir que devuelve el dron van en METROS de esta proyeccion. Es la
 * misma formula que traia CGoogleMercator en la libreria antigua y tiene que
 * seguir dando exactamente los mismos numeros: si cambia, el dron vuela a otro
 * sitio. Por eso vive aqui y no se toma de libmapa, que la tiene privada.
 *
 * En el QPointF, x es el ESTE (longitud) e y el NORTE (latitud). Ojo: las
 * tramas del dron los mandan al reves (primero el norte), y eso lo resuelve
 * quien arma o lee la trama.
 */
class ProyeccionMercator
{
public:
    //! De coordenada geografica a metros Mercator (x = este, y = norte).
    static QPointF aMetros(const QGeoCoordinate &coord);
    static QPointF aMetros(double lat, double lon) { return aMetros(QGeoCoordinate(lat, lon)); }

    //! De metros Mercator (x = este, y = norte) a coordenada geografica.
    static QGeoCoordinate aGeo(const QPointF &xy);
};

#endif // PROYECCIONMERCATOR_H
