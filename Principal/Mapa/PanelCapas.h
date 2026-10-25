#ifndef PANELCAPAS_H
#define PANELCAPAS_H

#include <QWidget>

class MapaMision;
class QTreeWidget;
class QTreeWidgetItem;

/*!
 * \brief Panel de capas del mapa, como el de la demo de libmapa, pero solo con
 *        lo que trae el mapa.json del paquete.
 *
 * Un arbol con casillas:
 * - «Mapa base»: las capas base del paquete; se marca una (como un radio). Si
 *   una base es vectorial, sus grupos (calles, edificios...) cuelgan de ella.
 * - «Superpuestas»: el mapa hibrido (calles y nombres encima de cualquier
 *   base) con sus grupos, y las capas fijas del paquete (aguas, corredores...).
 *
 * Las capas de la estacion (ruta, mision, dibujos) no salen: no son del
 * paquete. Todo pasa por MapaMision (la estacion no habla con libmapa fuera de
 * ella) y el arbol se rehace cuando MapaMision avisa de un cambio.
 */
class PanelCapas : public QWidget
{
    Q_OBJECT
public:
    explicit PanelCapas(MapaMision *mapa, QWidget *padre = nullptr);

    //! Rehace el arbol con el estado del mapa.
    void reconstruir();

    //! El arbol (para pruebas).
    QTreeWidget *arbol() const { return m_arbol; }

private:
    void casillaCambiada(QTreeWidgetItem *item, int columna);
    QTreeWidgetItem *nuevoGrupo(const QString &titulo, const QString &ayuda);

    MapaMision *m_mapa = nullptr;
    QTreeWidget *m_arbol = nullptr;
};

#endif // PANELCAPAS_H
