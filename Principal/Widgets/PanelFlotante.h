#ifndef PANELFLOTANTE_H
#define PANELFLOTANTE_H

#include <QFrame>

class QVBoxLayout;

/*!
 * \brief Panel que flota ENCIMA del mapa (como los de QGroundControl): titulo,
 *        boton de cerrar y un contenido.
 *
 * El mapa ocupa siempre todo su sitio y el panel se dibuja encima: abrirlo o
 * cerrarlo no cambia el tamano del mapa, asi que no hay que rehacer la vista
 * ni pedir teselas y lo que se ve no se mueve. Antes los paneles eran
 * QDockWidget y cada vez el mapa encogia o crecia (salto, fotograma estirado).
 *
 * El fondo es el color de ventana del tema, casi opaco, con las esquinas
 * redondeadas. Quien lo usa lo hace hijo del mapa y lo coloca.
 */
class PanelFlotante : public QFrame
{
    Q_OBJECT
public:
    PanelFlotante(const QString &titulo, QWidget *padre = nullptr);

    //! Pone el contenido (el panel pasa a ser su padre).
    void setContenido(QWidget *contenido);

signals:
    //! Se ha pulsado su boton de cerrar (el panel ya esta oculto).
    void cerrado();

protected:
    void paintEvent(QPaintEvent *evento) override;
    // El raton y la rueda sobre el panel son del panel: si no, pasaban al mapa
    // de debajo (pinchar en un hueco arrastraba el mapa, la rueda lo acercaba).
    void mousePressEvent(QMouseEvent *evento) override;
    void mouseMoveEvent(QMouseEvent *evento) override;
    void mouseReleaseEvent(QMouseEvent *evento) override;
    void mouseDoubleClickEvent(QMouseEvent *evento) override;
    void wheelEvent(QWheelEvent *evento) override;

private:
    QVBoxLayout *m_caja = nullptr;
    QWidget *m_contenido = nullptr;
};

#endif // PANELFLOTANTE_H
