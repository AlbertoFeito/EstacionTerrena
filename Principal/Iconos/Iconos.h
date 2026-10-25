#ifndef ICONOS_H
#define ICONOS_H

#include <QColor>
#include <QIcon>
#include <QPixmap>

/*!
 * \brief Iconos de la estacion, dibujados en codigo.
 *
 * Antes eran PNG e ICO de una carpeta de recursos (build/Recursos) que no
 * estaba completa en el repositorio. Dibujados aqui no dependen de ningun
 * fichero ni de plugins de imagen, y se ven igual en el tema claro y el oscuro.
 */
namespace Iconos {

//! Avion visto desde arriba con el morro hacia arriba (norte), del color del
//! regimen; con borde oscuro para que se vea sobre el satelite y sobre OSM.
QPixmap avion(const QColor &color, int lado = 40);
//! Cuadroptero visto desde arriba (brazos en X y cuatro rotores), el frente arriba.
QPixmap cuadroptero(const QColor &color, int lado = 40);

// Barra de herramientas del mapa.
QIcon regla();        //!< medir distancia y rumbo
QIcon lupaArea();     //!< ampliar un recuadro
QIcon lupaMas();      //!< acercar
QIcon lupaMenos();    //!< alejar
QIcon poligono();     //!< dibujar un poligono

// Reproduccion (control de objetivo).
QIcon reproducir();   //!< "play"; con el boton pulsado, "pausa"
QIcon parar();        //!< "stop"

} // namespace Iconos

#endif // ICONOS_H
