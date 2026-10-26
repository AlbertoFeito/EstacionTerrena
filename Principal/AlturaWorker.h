#ifndef ALTURAWORKER_H
#define ALTURAWORKER_H

#include <QtCore/QObject>
#include <QGeoCoordinate>

class MapaMision;

/*!
 * \brief Cota del terreno para la ruta y el perfil de altitud.
 *
 * Antes leia a mano los .hgt de C:/CubaAlt; ahora pregunta al mapa (libmapa),
 * que usa la elevacion del paquete de datos (o esa misma carpeta si el paquete
 * no trae elevacion, ver MapaMision). Mantiene la interfaz de siempre para que
 * Widget_Puntos y PerfilAltitudWidget no cambien.
 */
class AlturaWorker : public QObject
{
    Q_OBJECT
public:
    AlturaWorker(QObject *parent = nullptr);
    ~AlturaWorker();

    //! Mapa del que sacar la elevacion. Sin mapa, la altura es 0.
    void setMapa(MapaMision *mapa);
    //! true si ya tiene mapa del que sacar la elevacion.
    bool hayMapa() const { return m_mapa != nullptr; }
    //! Cota en metros, sin redondear, o NaN si no hay dato (ni mapa). No emite
    //! nada: la revision de la ruta la pide cada pocos metros de cada tramo.
    double cota(const QGeoCoordinate &pos) const;

public slots:
    //! Cota en metros (redondeada), o 0 si no hay dato, como antes.
    qint16 obtenerAltura(double lat, double lon);

signals:
    void alturaActualizada(qint16 altura);

private:
    MapaMision *m_mapa = nullptr;
};

#endif // ALTURAWORKER_H
