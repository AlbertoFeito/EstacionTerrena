#ifndef INFORMERUTA_H
#define INFORMERUTA_H

// El informe de la revision de una ruta, en HTML (ventana de analisis) y en
// texto (exportar). Solo Qt Core: el aspecto lo pone quien lo muestra.

#include "Planificacion/RoutePlanner.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace InformeRuta {

//! "Error", "Aviso" o "Nota".
QString nombreGravedad(Hallazgo::Gravedad g);
//! Color de cada gravedad en #rrggbb, el mismo en la tabla, el estado y el informe.
QString colorGravedad(Hallazgo::Gravedad g);
//! Lo que se comprueba y lo que se supone, una frase por cosa (informe y guia).
QStringList comoSeRevisa();
//! "No válida: 2 errores y 1 aviso", "Válida, con 3 avisos" o "Válida".
QString estado(const RevisionRuta &r);

/**
 * @brief El informe completo en HTML: estado, cifras, que revisar (cada
 *        problema con su porque y su arreglo), los tramos y como se revisa.
 *
 * Los lugares van como enlaces "punto:N" (N = fila del punto) para que la
 * ventana lleve a ese punto de la tabla.
 */
QString html(const RevisionRuta &r, const DroneCharacteristics &dron, const QString &ruta,
             const QList<E_Punto> &puntos);

//! Lo mismo en texto plano, para guardarlo en un fichero.
QString texto(const RevisionRuta &r, const DroneCharacteristics &dron, const QString &ruta,
              const QList<E_Punto> &puntos);

} // namespace InformeRuta

#endif // INFORMERUTA_H
