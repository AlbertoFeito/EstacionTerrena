#include "AlturaWorker.h"

#include "Mapa/MapaMision.h"

#include <QGeoCoordinate>
#include <algorithm>
#include <cmath>
#include <limits>

// Sin mapa hasta que CControladora lo crea (el worker nace antes).
AlturaWorker::AlturaWorker(QObject *parent) : QObject(parent)
{
}

// Nada que liberar: el mapa no es suyo.
AlturaWorker::~AlturaWorker()
{
}

// Recuerda el mapa del que leer la elevacion.
void AlturaWorker::setMapa(MapaMision *mapa)
{
    m_mapa = mapa;
}

// Cota tal cual del mapa (NaN donde no hay dato), para la revision de la ruta.
double AlturaWorker::cota(const QGeoCoordinate &pos) const
{
    return m_mapa ? m_mapa->alturaTerreno(pos) : std::numeric_limits<double>::quiet_NaN();
}

// Pide la cota al mapa. Un hueco o la falta de datos dan 0, que es lo que
// devolvia la lectura antigua de los .hgt cuando no encontraba el fichero.
qint16 AlturaWorker::obtenerAltura(double lat, double lon)
{
    qint16 altura = 0;
    if (m_mapa) {
        const double cota = m_mapa->alturaTerreno(QGeoCoordinate(lat, lon));
        if (std::isfinite(cota)) {
            const double limite = std::numeric_limits<qint16>::max();
            altura = static_cast<qint16>(std::lround(std::max(-limite, std::min(limite, cota))));
        }
    }
    emit alturaActualizada(altura);
    return altura;
}
