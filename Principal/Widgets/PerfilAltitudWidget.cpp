#include "perfilaltitudwidget.h"
#include <QVBoxLayout>
#include <QDebug>
#include <algorithm>

PerfilAltitudWidget::PerfilAltitudWidget(AlturaWorker *alturaWorker, QWidget *parent)
    : QWidget(parent)
    , m_alturaWorker(alturaWorker)
{
    m_plot = new QCustomPlot(this);
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_plot);
    setLayout(layout);

    configurarGrafico();
    connect(m_plot, &QCustomPlot::mousePress, this, &PerfilAltitudWidget::onGraphClicked);
}

PerfilAltitudWidget::~PerfilAltitudWidget()
{
}

void PerfilAltitudWidget::configurarGrafico()
{
    m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables);
    m_plot->xAxis->setLabel("Distancia (m)");
    m_plot->yAxis->setLabel("Altitud (m)");
    m_plot->legend->setVisible(true);
    m_plot->legend->setFont(QFont("Helvetica", 9));
    m_plot->axisRect()->setupFullAxesBox();
}

void PerfilAltitudWidget::setRuta(const QList<E_Punto> &puntos)
{
    m_puntos = puntos;
    generarPerfil();
}

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

void PerfilAltitudWidget::muestrearTerreno()
{
    m_distanciasMuestreo.clear();
    m_alturasTerreno.clear();

    const double paso = 20.0; // metros entre muestras, ajustable
    for (int i = 0; i < m_puntos.size() - 1; ++i) {
        double distSeg = m_puntos[i].distanciaA(m_puntos[i+1]);
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

void PerfilAltitudWidget::dibujarPerfil()
{
    m_plot->clearGraphs();
    m_plot->clearItems();

    // Trayectoria de vuelo
    QCPGraph *vueloGraph = m_plot->addGraph();
    vueloGraph->setData(m_distanciasAcumuladas, m_alturasVuelo);
    vueloGraph->setName("Trayectoria de vuelo");
    vueloGraph->setPen(QPen(Qt::blue, 2));
    vueloGraph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCircle, 5));

    // Terreno muestreado
    QCPGraph *terrenoGraph = m_plot->addGraph();
    terrenoGraph->setData(m_distanciasMuestreo, m_alturasTerreno);
    terrenoGraph->setName("Terreno");
    terrenoGraph->setPen(QPen(Qt::darkGreen, 1, Qt::DashLine));
    terrenoGraph->setScatterStyle(QCPScatterStyle::ssNone);

    // Detectar puntos donde el terreno supera la altitud de vuelo (con margen)
    const double margenSeguridad = 10.0; // metros por encima del terreno
    QVector<double> distPeligro, altPeligro;
    for (int i = 0; i < m_distanciasMuestreo.size(); ++i) {
        double d = m_distanciasMuestreo[i];
        // Encontrar segmento de vuelo que contiene esta distancia
        int idx = std::upper_bound(m_distanciasAcumuladas.begin(), m_distanciasAcumuladas.end(), d) - m_distanciasAcumuladas.begin() - 1;
        if (idx < 0 || idx >= m_distanciasAcumuladas.size()-1) continue;
        double frac = (d - m_distanciasAcumuladas[idx]) / (m_distanciasAcumuladas[idx+1] - m_distanciasAcumuladas[idx]);
        double altVueloInterp = m_alturasVuelo[idx] + frac * (m_alturasVuelo[idx+1] - m_alturasVuelo[idx]);

        if (m_alturasTerreno[i] + margenSeguridad > altVueloInterp) {
            distPeligro.append(d);
            altPeligro.append(m_alturasTerreno[i]);
        }
    }

    if (!distPeligro.isEmpty()) {
        QCPGraph *peligroGraph = m_plot->addGraph();
        peligroGraph->setData(distPeligro, altPeligro);
        peligroGraph->setName("Posible obstáculo");
        peligroGraph->setPen(QPen(Qt::red, 2));
        peligroGraph->setScatterStyle(QCPScatterStyle(QCPScatterStyle::ssCross, 8));
    }

    // Ajustar ejes
    m_plot->xAxis->setRange(0, m_distanciaTotal * 1.05);
    double minAlt = qMin(*std::min_element(m_alturasTerreno.begin(), m_alturasTerreno.end()),
                         *std::min_element(m_alturasVuelo.begin(), m_alturasVuelo.end())) - 50;
    double maxAlt = qMax(*std::max_element(m_alturasTerreno.begin(), m_alturasTerreno.end()),
                         *std::max_element(m_alturasVuelo.begin(), m_alturasVuelo.end())) + 50;
    m_plot->yAxis->setRange(minAlt, maxAlt);

    m_plot->replot();
}

void PerfilAltitudWidget::limpiar()
{
    m_plot->clearGraphs();
    m_plot->clearItems();
    m_plot->replot();
}

void PerfilAltitudWidget::onGraphClicked(QMouseEvent *event)
{
    double coordX = m_plot->xAxis->pixelToCoord(event->pos().x());
    int idx = qBound(0, int(coordX / (m_distanciaTotal / m_distanciasAcumuladas.size())), m_distanciasAcumuladas.size() - 1);
    emit puntoSeleccionado(idx);
}
