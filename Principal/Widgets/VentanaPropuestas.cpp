#include "VentanaPropuestas.h"

#include "PerfilAltitudWidget.h"
#include "Planificacion/InformeRuta.h"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>

namespace {
// Duracion como 4:05.
QString duracion(double segundos)
{
    const int s = int(std::lround(segundos));
    return QStringLiteral("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0'));
}
}

// La ventana: el pasillo y «Calcular» arriba, la tabla de propuestas, el
// detalle y el perfil de la elegida, y «Usar esta ruta» / «Cerrar».
VentanaPropuestas::VentanaPropuestas(AlturaWorker *alturaWorker, QWidget *padre)
    : QDialog(padre)
{
    setWindowTitle(tr("Rutas propuestas"));
    setModal(false);
    resize(820, 620);

    m_pasillo = new QSpinBox(this);
    m_pasillo->setRange(100, 5000);
    m_pasillo->setSingleStep(100);
    m_pasillo->setValue(1000);
    m_pasillo->setSuffix(tr(" m"));
    m_pasillo->setToolTip(tr("Hasta dónde puede apartarse la ruta de la recta P1-P10, a cada lado"));
    auto *recalcular = new QPushButton(tr("Calcular"), this);
    connect(recalcular, &QPushButton::clicked, this, &VentanaPropuestas::calcular);
    auto *arriba = new QHBoxLayout;
    arriba->addWidget(new QLabel(tr("De P1 a P10, buscando a cada lado de la recta hasta"), this));
    arriba->addWidget(m_pasillo);
    arriba->addWidget(recalcular);
    arriba->addStretch();

    m_alcance = new QLabel(this);
    m_alcance->setWordWrap(true);

    m_tabla = new QTableWidget(this);
    m_tabla->setColumnCount(7);
    m_tabla->setHorizontalHeaderLabels({tr("Ruta"), tr("Distancia"), tr("Tiempo"), tr("Altura máx."),
                                        tr("Sube en total"), tr("Margen mín."), tr("Estado")});
    m_tabla->horizontalHeaderItem(5)->setToolTip(tr("Lo menos que pasa sobre el terreno, a la altura a la "
                                                    "que iría de verdad"));
    m_tabla->verticalHeader()->setVisible(false);
    m_tabla->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tabla->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tabla->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tabla->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_tabla->horizontalHeader()->setStretchLastSection(true);
    m_tabla->setFixedHeight(150);
    connect(m_tabla, &QTableWidget::currentCellChanged, this, [this](int fila) { elegir(fila); });

    m_detalle = new QLabel(this);
    m_detalle->setWordWrap(true);
    m_detalle->setTextFormat(Qt::RichText);
    m_perfil = new PerfilAltitudWidget(alturaWorker, this);

    auto *botones = new QDialogButtonBox(this);
    m_usar = botones->addButton(tr("Usar esta ruta"), QDialogButtonBox::AcceptRole);
    m_usar->setToolTip(tr("Pone esta ruta en la tabla de puntos (se pierde la actual)"));
    connect(m_usar, &QPushButton::clicked, this, [this]() {
        const int fila = m_tabla->currentRow();
        if (fila >= 0 && fila < m_propuestas.size() && m_propuestas[fila].ok)
            emit usar(m_propuestas[fila].puntos);
    });
    QPushButton *cerrar = botones->addButton(tr("Cerrar"), QDialogButtonBox::RejectRole);
    connect(cerrar, &QPushButton::clicked, this, &QDialog::close);

    auto *capa = new QVBoxLayout(this);
    capa->addLayout(arriba);
    capa->addWidget(m_alcance);
    capa->addWidget(m_tabla);
    capa->addWidget(m_detalle);
    capa->addWidget(m_perfil, 1);
    capa->addWidget(botones);
}

// Ancho del pasillo elegido.
double VentanaPropuestas::anchoPasillo() const
{
    return m_pasillo->value();
}

// Una fila por propuesta, con lo que hace falta para compararlas, y cuanto
// podia apartarse de la recta (el pasillo se dibuja en el mapa).
void VentanaPropuestas::mostrar(const QVector<PropuestaRuta> &propuestas, const PasilloRuta &pasillo)
{
    m_propuestas = propuestas;
    if (pasillo.contorno.isEmpty()) {
        m_alcance->clear();
    } else if (pasillo.desvioPosible < 1.0) {
        m_alcance->setText(tr("Con P1 y P10 a esta distancia, cada tramo en recta ya mide %1 m (el máximo entre "
                              "puntos del dron): la ruta no puede apartarse de la recta. Acerca P10 para dejar "
                              "sitio.").arg(pasillo.largoTramo, 0, 'f', 0));
    } else {
        QString texto = tr("Puede apartarse hasta <b>%1 m</b> de la recta, en el centro (zona sombreada en el mapa). "
                           "Cada tramo en recta mide %2 m y el máximo entre puntos del dron recorta lo que cabe "
                           "a los lados.")
                            .arg(pasillo.desvioPosible, 0, 'f', 0).arg(pasillo.largoTramo, 0, 'f', 0);
        if (pasillo.desvioPosible < anchoPasillo() - 1.0)
            texto += tr(" Pedir más pasillo no cambia nada: para tener más sitio, acerca P10.");
        m_alcance->setText(texto);
    }
    emit pasilloPrevio(pasillo.contorno);
    m_tabla->blockSignals(true);
    m_tabla->setRowCount(propuestas.size());
    int elegida = -1;
    for (int i = 0; i < propuestas.size(); ++i) {
        const PropuestaRuta &p = propuestas[i];
        const RevisionRuta &r = p.revision;
        QStringList celdas{p.nombre};
        if (p.ok) {
            celdas << QStringLiteral("%1 m").arg(r.distancia, 0, 'f', 0) << duracion(r.tiempo)
                   << QStringLiteral("%1 m").arg(r.alturaMax, 0, 'f', 0)
                   << QStringLiteral("%1 m").arg(r.subida, 0, 'f', 0)
                   << (std::isfinite(r.holguraTerreno) ? QStringLiteral("%1 m").arg(r.holguraTerreno, 0, 'f', 0)
                                                       : QStringLiteral("—"))
                   << InformeRuta::estado(r);
        } else {
            celdas << QString() << QString() << QString() << QString() << QString() << tr("No se pudo");
        }
        for (int c = 0; c < celdas.size(); ++c) {
            auto *item = new QTableWidgetItem(celdas[c]);
            if (c >= 1 && c <= 5)
                item->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            if (c == 6 && p.ok) {
                const Hallazgo::Gravedad g = !r.valida() ? Hallazgo::Gravedad::Error : Hallazgo::Gravedad::Aviso;
                if (!r.valida() || r.cuantos(Hallazgo::Gravedad::Aviso) > 0)
                    item->setForeground(QColor(InformeRuta::colorGravedad(g)));
            }
            m_tabla->setItem(i, c, item);
        }
        if (p.ok && (elegida < 0 || p.prioridad == RoutePlanner::Prioridad::Equilibrada))
            elegida = i;
    }
    m_tabla->blockSignals(false);
    if (elegida < 0)
        elegida = 0;
    m_tabla->setCurrentCell(elegida, 0);
    elegir(elegida);
}

// La propuesta de la fila: su detalle, su perfil (con la trayectoria probable)
// y la vista previa en el mapa.
void VentanaPropuestas::elegir(int fila)
{
    if (fila < 0 || fila >= m_propuestas.size()) {
        m_detalle->clear();
        m_perfil->limpiar();
        m_usar->setEnabled(false);
        emit vistaPrevia({});
        return;
    }
    const PropuestaRuta &p = m_propuestas[fila];
    m_usar->setEnabled(p.ok);
    QString texto = QStringLiteral("<b>%1</b>").arg(p.nombre.toHtmlEscaped());
    switch (p.prioridad) {
    case RoutePlanner::Prioridad::Segura:
        texto += tr(": prima el margen sobre el terreno y las subidas suaves, aunque sea más larga.");
        break;
    case RoutePlanner::Prioridad::Rapida:
        texto += tr(": prima el tiempo de vuelo; sube lo que haga falta si el dron puede.");
        break;
    case RoutePlanner::Prioridad::Equilibrada:
        texto += tr(": una mezcla de las dos.");
        break;
    default:
        texto += tr(": la línea de P1 a P10, para comparar.");
    }
    if (!p.motivo.isEmpty())
        texto += QStringLiteral("<br><span style='color:%1'>%2</span>")
                     .arg(InformeRuta::colorGravedad(Hallazgo::Gravedad::Aviso), p.motivo.toHtmlEscaped());
    if (p.ok) {
        for (int i = 0; i < p.revision.hallazgos.size() && i < 3; ++i) {
            const Hallazgo &h = p.revision.hallazgos[i];
            texto += QStringLiteral("<br><span style='color:%1'>%2</span> · %3: %4")
                         .arg(InformeRuta::colorGravedad(h.gravedad), InformeRuta::nombreGravedad(h.gravedad),
                              h.lugar.toHtmlEscaped(), h.titulo.toHtmlEscaped());
        }
        if (p.revision.hallazgos.size() > 3)
            texto += tr("<br>… y %1 más (se ven todos en «Análisis» si la usas)").arg(p.revision.hallazgos.size() - 3);
        m_perfil->setRuta(p.puntos, p.revision.perfilReal);
        QVector<QGeoCoordinate> camino;
        for (const E_Punto &pt : p.puntos)
            camino.append(pt.pos);
        emit vistaPrevia(camino);
    } else {
        m_perfil->limpiar();
        emit vistaPrevia({});
    }
    m_detalle->setText(texto);
}

// Al cerrar, fuera la vista previa y el pasillo del mapa.
void VentanaPropuestas::hideEvent(QHideEvent *event)
{
    emit vistaPrevia({});
    emit pasilloPrevio({});
    QDialog::hideEvent(event);
}
