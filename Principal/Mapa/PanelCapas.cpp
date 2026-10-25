#include "PanelCapas.h"

#include "MapaMision.h"

#include <QHeaderView>
#include <QLabel>
#include <QSignalBlocker>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {
// Que es cada elemento del arbol (se guarda en Qt::UserRole).
enum Tipo { Grupo = 0, Base, GrupoBase, Hibrido, GrupoHibrido, Fija };
const int RolTipo = Qt::UserRole;
const int RolId = Qt::UserRole + 1;
const int RolBaseDe = Qt::UserRole + 2;   // de que base cuelga un grupo
}

// Arma el panel: un titulo corto, el arbol y una nota de donde salen las capas.
PanelCapas::PanelCapas(MapaMision *mapa, QWidget *padre)
    : QWidget(padre), m_mapa(mapa)
{
    auto *caja = new QVBoxLayout(this);
    caja->setContentsMargins(6, 6, 6, 6);
    caja->setSpacing(4);
    caja->addWidget(new QLabel(tr("Marca para mostrar u ocultar")));

    m_arbol = new QTreeWidget(this);
    m_arbol->setHeaderHidden(true);
    m_arbol->setColumnCount(1);
    m_arbol->setRootIsDecorated(true);
    m_arbol->setUniformRowHeights(true);
    m_arbol->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    caja->addWidget(m_arbol, 1);

    auto *nota = new QLabel(tr("Solo las capas del paquete de mapas (mapa.json)."));
    nota->setStyleSheet(QStringLiteral("color: #9e9e9e; font-size: 10px;"));
    nota->setWordWrap(true);
    caja->addWidget(nota);

    connect(m_arbol, &QTreeWidget::itemChanged, this, &PanelCapas::casillaCambiada);
    // En cola: el aviso llega a menudo desde casillaCambiada, y rehacer el
    // arbol ahi mismo borraria el elemento que Qt esta tratando.
    connect(m_mapa, &MapaMision::capasCambiadas, this, &PanelCapas::reconstruir,
            Qt::QueuedConnection);
    reconstruir();
}

// Cabecera de grupo: en negrita, sin casilla y con una banda de fondo, como en
// la demo.
QTreeWidgetItem *PanelCapas::nuevoGrupo(const QString &titulo, const QString &ayuda)
{
    auto *g = new QTreeWidgetItem(m_arbol);
    g->setText(0, titulo);
    g->setToolTip(0, ayuda);
    g->setData(0, RolTipo, Grupo);
    g->setFlags(Qt::ItemIsEnabled);
    QFont f = g->font(0);
    f.setBold(true);
    g->setFont(0, f);
    // Gris translucido: se lee igual con el tema claro y con el oscuro (el
    // AlternateBase de la demo salia blanco sobre blanco en el oscuro).
    g->setBackground(0, QColor(128, 128, 128, 60));
    return g;
}

// Un elemento con casilla.
static QTreeWidgetItem *casilla(QTreeWidgetItem *padre, const QString &texto, int tipo,
                                const QString &id, bool marcada)
{
    auto *it = new QTreeWidgetItem(padre);
    it->setText(0, texto);
    it->setData(0, RolTipo, tipo);
    it->setData(0, RolId, id);
    it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
    it->setCheckState(0, marcada ? Qt::Checked : Qt::Unchecked);
    return it;
}

// Rehace el arbol. Los grupos que no tienen nada (un paquete sin hibrido ni
// capas fijas) no salen.
void PanelCapas::reconstruir()
{
    const QSignalBlocker sinSenales(m_arbol);
    m_arbol->clear();

    QTreeWidgetItem *gBase = nuevoGrupo(tr("Mapa base"), tr("Una a la vez"));
    for (const CapaMapa &b : m_mapa->capasBase()) {
        QTreeWidgetItem *it = casilla(gBase, b.nombre, Base, b.id, b.visible);
        it->setToolTip(0, b.id);
        for (const CapaMapa &g : m_mapa->gruposCapaBase(b.id)) {
            QTreeWidgetItem *hg = casilla(it, g.nombre, GrupoBase, g.id, g.visible);
            hg->setData(0, RolBaseDe, b.id);
        }
        it->setExpanded(b.visible);
    }
    gBase->setExpanded(true);

    const QVector<CapaMapa> fijas = m_mapa->capasFijas();
    if (m_mapa->hayHibrido() || !fijas.isEmpty()) {
        QTreeWidgetItem *gEncima = nuevoGrupo(tr("Superpuestas"),
                                              tr("Se dibujan encima del mapa base"));
        if (m_mapa->hayHibrido()) {
            QTreeWidgetItem *hib = casilla(gEncima, tr("Híbrido: %1").arg(m_mapa->nombreHibrido()),
                                           Hibrido, QString(), m_mapa->hibridoVisible());
            hib->setToolTip(0, tr("Calles y nombres encima de cualquier mapa base"));
            for (const CapaMapa &g : m_mapa->gruposHibrido())
                casilla(hib, g.nombre, GrupoHibrido, g.id, g.visible);
            hib->setExpanded(m_mapa->hibridoVisible());
        }
        for (const CapaMapa &c : fijas)
            casilla(gEncima, c.nombre, Fija, c.id, c.visible);
        gEncima->setExpanded(true);
    }
}

// Casilla cambiada: la capa base va como un radio (marcar una desmarca las
// demas; desmarcar la activa no la quita, siempre hay una base).
void PanelCapas::casillaCambiada(QTreeWidgetItem *item, int columna)
{
    if (columna != 0)
        return;
    const bool on = item->checkState(0) == Qt::Checked;
    const QString id = item->data(0, RolId).toString();
    switch (item->data(0, RolTipo).toInt()) {
    case Base:
        if (on)
            m_mapa->ponerCapaBaseId(id);
        // Siempre se rehace (en cola): si se desmarco la activa, vuelve a salir
        // marcada.
        QMetaObject::invokeMethod(this, &PanelCapas::reconstruir, Qt::QueuedConnection);
        break;
    case GrupoBase:
        m_mapa->ponerGrupoCapaBaseVisible(item->data(0, RolBaseDe).toString(), id, on);
        break;
    case Hibrido:
        m_mapa->ponerHibridoVisible(on);
        break;
    case GrupoHibrido:
        m_mapa->ponerGrupoHibridoVisible(id, on);
        break;
    case Fija:
        m_mapa->ponerCapaFijaVisible(id, on);
        break;
    default:
        break;
    }
}
