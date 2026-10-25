#include "PerfilAltitudWidget.h"
#include <QVBoxLayout>
#include <QDebug>
#include <algorithm>

// Grafico sin margenes, para ir dentro del panel de rutas (o en una ventana
// aparte con «Ampliar perfil»).
PerfilAltitudWidget::PerfilAltitudWidget(AlturaWorker *alturaWorker, QWidget *parent)
    : QWidget(parent)
    , m_alturaWorker(alturaWorker)
    , m_distanciaTotal(0.0)
{
    m_plot = new QCustomPlot(this);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_plot);
    setLayout(layout);

    configurarGrafico();
    connect(m_plot, &QCustomPlot::mousePress, this, &PerfilAltitudWidget::onGraphClicked);
}

// Nada que liberar a mano: el grafico es hijo del widget.
PerfilAltitudWidget::~PerfilAltitudWidget()
{
}

// Ejes y leyenda con los colores del tema (claro u oscuro): fondo de la
// ventana, texto y rejilla suaves. Sin arrastre ni zoom: el perfil entero
// siempre a la vista, y un clic elige el punto mas cercano.
void PerfilAltitudWidget::configurarGrafico()
{
    const QPalette pal = palette();
    const QColor texto = pal.color(QPalette::WindowText);
    QColor rejilla = texto;
    rejilla.setAlpha(40);

    m_plot->setBackground(pal.brush(QPalette::Window));
    m_plot->axisRect()->setBackground(pal.brush(QPalette::Base));
    m_plot->setInteractions(QCP::Interactions());
    for (QCPAxis *eje : {m_plot->xAxis, m_plot->yAxis}) {
        eje->setBasePen(QPen(texto));
        eje->setTickPen(QPen(texto));
        eje->setSubTickPen(QPen(texto));
        eje->setTickLabelColor(texto);
        eje->setLabelColor(texto);
        eje->grid()->setPen(QPen(rejilla, 1, Qt::DotLine));
        QFont f = font();
        f.setPointSize(8);
        eje->setTickLabelFont(f);
        eje->setLabelFont(f);
    }
    m_plot->xAxis->setLabel(QStringLiteral("Distancia (m)"));
    m_plot->yAxis->setLabel(QStringLiteral("Altitud (m)"));
    m_plot->legend->setVisible(true);
    QFont fl = font();
    fl.setPointSize(8);
    m_plot->legend->setFont(fl);
    m_plot->legend->setTextColor(texto);
    m_plot->legend->setBrush(Qt::NoBrush);
    m_plot->legend->setBorderPen(Qt::NoPen);
    // La leyenda en una fila propia encima del grafico y en horizontal: dentro
    // tapaba la ruta y sus rotulos. Solo se mueve la primera vez.
    if (m_plot->plotLayout()->rowCount() == 1) {
        m_plot->axisRect()->insetLayout()->take(m_plot->legend);   // antes de moverla
        m_plot->plotLayout()->insertRow(0);
        m_plot->plotLayout()->addElement(0, 0, m_plot->legend);
        m_plot->legend->setFillOrder(QCPLegend::foColumnsFirst);
        m_plot->plotLayout()->setRowStretchFactor(0, 0.001);
        m_plot->legend->setMargins(QMargins(0, 0, 0, 0));
    }
}

// Con el cambio de tema se repinta con los colores nuevos.
void PerfilAltitudWidget::changeEvent(QEvent *event)
{
    QWidget::changeEvent(event);
    if (event->type() == QEvent::PaletteChange || event->type() == QEvent::StyleChange) {
        configurarGrafico();
        if (m_puntos.size() >= 2)
            dibujarPerfil();
        else
            m_plot->replot();
    }
}

// Pone la ruta y rehace el perfil.
void PerfilAltitudWidget::setRuta(const QList<E_Punto> &puntos)
{
    m_puntos = puntos;
    generarPerfil();
}

// Distancias acumuladas y alturas de vuelo de cada punto, terreno debajo y dibujo.
void PerfilAltitudWidget::generarPerfil()
{
    if (m_puntos.size() < 2 || !m_alturaWorker) return;

    // Calcular distancias acumuladas y altitudes de vuelo
    m_distanciasAcumuladas.clear();
    m_alturasVuelo.clear();
    m_distanciasAcumuladas.append(0.0);
    m_alturasVuelo.append(m_puntos.first().altura);

    for (int i = 1; i < m_puntos.size(); ++i) {
        double dist = m_puntos[i-1].distanciaA(m_puntos[i]);
        m_distanciasAcumuladas.append(m_distanciasAcumuladas.last() + dist);
        m_alturasVuelo.append(m_puntos[i].altura);
    }
    m_distanciaTotal = m_distanciasAcumuladas.last();

    // Muestrear terreno
    muestrearTerreno();

    // Dibujar
    dibujarPerfil();
}


// Cota del terreno cada 20 m a lo largo de la ruta (0 donde no hay dato).
void PerfilAltitudWidget::muestrearTerreno()
{
    m_distanciasMuestreo.clear();
    m_alturasTerreno.clear();

    const double paso = 20.0; // metros entre muestras, ajustable
    for (int i = 0; i < m_puntos.size() - 1; ++i) {
        double distSeg = m_puntos[i].distanciaA(m_puntos[i+1]);
        if (distSeg <= 0.0)
            continue;   // dos puntos en el mismo sitio: nada que muestrear
        int numMuestras = qMax(1, int(distSeg / paso));
        for (int j = 0; j <= numMuestras; ++j) {
            double frac = double(j * paso) / distSeg;
            if (frac > 1.0) frac = 1.0;

            // Interpolación lineal de coordenadas
            double lat = m_puntos[i].pos.latitude() + frac * (m_puntos[i+1].pos.latitude() - m_puntos[i].pos.latitude());
            double lon = m_puntos[i].pos.longitude() + frac * (m_puntos[i+1].pos.longitude() - m_puntos[i].pos.longitude());

            double altTerreno = m_alturaWorker->obtenerAltura(lat, lon);
            if (altTerreno < 0) altTerreno = 0; // valor por defecto si no hay datos

            double distAcum = m_distanciasAcumuladas[i] + frac * distSeg;
            m_distanciasMuestreo.append(distAcum);
            m_alturasTerreno.append(altTerreno);
        }
    }
}

// Terreno relleno abajo, la ruta en el naranja del mapa con sus puntos
// rotulados (P1...P10) y, en rojo, donde el terreno queda a menos de 10 m de
// la altura de vuelo.
void PerfilAltitudWidget::dibujarPerfil()
{
    m_plot->clearGraphs();
    m_plot->clearItems();
    if (m_alturasTerreno.isEmpty() || m_alturasVuelo.isEmpty())
        return;

    const double minAlt = qMin(*std::min_element(m_alturasTerreno.begin(), m_alturasTerreno.end()),
                               *std::min_element(m_alturasVuelo.begin(), m_alturasVuelo.end()));
    const double maxAlt = qMax(*std::max_element(m_alturasTerreno.begin(), m_alturasTerreno.end()),
                               *std::max_element(m_alturasVuelo.begin(), m_alturasVuelo.end()));
    const double margen = qMax(20.0, (maxAlt - minAlt) * 0.15);
    const double suelo = qMax(0.0, minAlt - margen);

    // Terreno: area rellena hasta el borde de abajo.
    QCPGraph *terrenoGraph = m_plot->addGraph();
    terrenoGraph->setData(m_distanciasMuestreo, m_alturasTerreno);
    terrenoGraph->setName(QStringLiteral("Terreno"));
    terrenoGraph->setPen(QPen(QColor(141, 110, 99), 1));
    terrenoGraph->setBrush(QColor(141, 110, 99, 90));
    QCPGraph *base = m_plot->addGraph();
    base->setData(QVector<double>{0.0, m_distanciaTotal}, QVector<double>{suelo, suelo});
    base->setPen(Qt::NoPen);
    base->removeFromLegend();
    terrenoGraph->setChannelFillGraph(base);

    // Trayectoria: el mismo naranja que la ruta en el mapa.
    const QColor naranja(255, 150, 10);
    QCPGraph *vueloGraph = m_plot->addGraph();
    vueloGraph->setData(m_distanciasAcumuladas, m_alturasVuelo);
    vueloGraph->setName(QStringLiteral("Ruta"));
    vueloGraph->setPen(QPen(naranja, 2));
    vueloGraph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssDisc, naranja, 6));
    const QColor texto = palette().color(QPalette::WindowText);
    for (int i = 0; i < m_puntos.size() && i < m_distanciasAcumuladas.size(); ++i) {
        auto *rotulo = new QCPItemText(m_plot);
        rotulo->position->setCoords(m_distanciasAcumuladas.at(i), m_alturasVuelo.at(i));
        rotulo->setPositionAlignment(Qt::AlignBottom | Qt::AlignHCenter);
        rotulo->setPadding(QMargins(0, 0, 0, 4));
        rotulo->setText(m_puntos.at(i).nombre);
        rotulo->setColor(texto);
        QFont f = font();
        f.setPointSize(7);
        rotulo->setFont(f);
    }

    // Donde el terreno se acerca a menos de 10 m de la altura de vuelo.
    const double margenSeguridad = 10.0;
    QVector<double> distPeligro, altPeligro;
    for (int i = 0; i < m_distanciasMuestreo.size(); ++i) {
        const double d = m_distanciasMuestreo[i];
        const int idx = int(std::upper_bound(m_distanciasAcumuladas.begin(), m_distanciasAcumuladas.end(), d)
                            - m_distanciasAcumuladas.begin()) - 1;
        if (idx < 0 || idx >= m_distanciasAcumuladas.size() - 1)
            continue;
        const double tramo = m_distanciasAcumuladas[idx + 1] - m_distanciasAcumuladas[idx];
        const double frac = tramo > 0.0 ? (d - m_distanciasAcumuladas[idx]) / tramo : 0.0;
        const double altVuelo = m_alturasVuelo[idx] + frac * (m_alturasVuelo[idx + 1] - m_alturasVuelo[idx]);
        if (m_alturasTerreno[i] + margenSeguridad > altVuelo) {
            distPeligro.append(d);
            altPeligro.append(m_alturasTerreno[i]);
        }
    }
    if (!distPeligro.isEmpty()) {
        QCPGraph *peligroGraph = m_plot->addGraph();
        peligroGraph->setData(distPeligro, altPeligro);
        peligroGraph->setName(QStringLiteral("Terreno a menos de 10 m"));
        peligroGraph->setLineStyle(QCPGraph::lsNone);
        peligroGraph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCross, QColor(229, 57, 53), 7));
    }

    // Un poco de aire a los lados para que no se corten los rotulos de P1 y del ultimo.
    m_plot->xAxis->setRange(-m_distanciaTotal * 0.04, m_distanciaTotal * 1.04);
    m_plot->yAxis->setRange(suelo, maxAlt + margen);
    m_plot->replot();
}

// Deja el grafico vacio (ruta de menos de dos puntos).
void PerfilAltitudWidget::limpiar()
{
    m_puntos.clear();
    m_plot->clearGraphs();
    m_plot->clearItems();
    m_plot->replot();
}

// Clic en el grafico: el punto de la ruta mas cercano en distancia.
void PerfilAltitudWidget::onGraphClicked(QMouseEvent *event)
{
    if (m_distanciasAcumuladas.isEmpty())
        return;
    const double x = m_plot->xAxis->pixelToCoord(event->pos().x());
    int mejor = 0;
    for (int i = 1; i < m_distanciasAcumuladas.size(); ++i)
        if (qAbs(m_distanciasAcumuladas.at(i) - x) < qAbs(m_distanciasAcumuladas.at(mejor) - x))
            mejor = i;
    emit puntoSeleccionado(mejor);
}
