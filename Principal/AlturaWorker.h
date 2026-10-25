#ifndef ALTURAWORKER_H
#define ALTURAWORKER_H

#include <QtCore/QObject>

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

public slots:
    //! Cota en metros (redondeada), o 0 si no hay dato, como antes.
    qint16 obtenerAltura(double lat, double lon);

signals:
    void alturaActualizada(qint16 altura);

private:
    MapaMision *m_mapa = nullptr;
};

#endif // ALTURAWORKER_H
