#include "ProyeccionMercator.h"

#include <QtMath>
#include <cmath>

namespace {
// Radio WGS84 y semiperimetro, con los mismos literales que CGoogleMercator
// para conservar el redondeo bit a bit.
const double kRadio = 6378137.0F;
const double kOrigen = 2.0F * M_PI * kRadio / 2.0F;
}

// Directa: x = lon·πR/180, y = R·ln(tan(45° + lat/2)) (escrito como en el
// codigo original, en grados, para no cambiar ni el ultimo decimal).
QPointF ProyeccionMercator::aMetros(const QGeoCoordinate &coord)
{
    const double lat = coord.latitude();
    const double lon = coord.longitude();

    const double x = lon * kOrigen / 180.0F;
    double y = std::log(std::tan((90.0 + lat) * M_PI / 360.0)) / (M_PI / 180.0);
    y *= kOrigen / 180.0F;

    return QPointF(x, y);
}

// Inversa: lon = x/R, lat = 2·atan(e^(y/R)) − π/2, en radianes y pasadas a grados.
QGeoCoordinate ProyeccionMercator::aGeo(const QPointF &xy)
{
    const double lon = xy.x() / kRadio;
    const double yTmp = xy.y() / kRadio;
    const double lat = 2 * std::atan(std::exp(yTmp)) - M_PI_2;

    return QGeoCoordinate(lat * 180 / M_PI, lon * 180 / M_PI);
}
