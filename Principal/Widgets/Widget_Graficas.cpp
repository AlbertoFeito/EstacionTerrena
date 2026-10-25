#include "Widget_Graficas.h"
#include "ui_Widget_Graficas.h"


Widget_Graficas::Widget_Graficas(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget_Graficas)
{
    ui->setupUi(this);

    lEdron = ui->dronLineEdit;
    lEvuelo = ui->vueloLineEdit;
    mpBCerrar = ui->pB_Cerrar;

    lEdron->setVisible (false);
    lEvuelo->setVisible (false);
    mpBCerrar->setVisible (false);
    ui->dronLabel->setVisible (false);
    ui->vueloLabel->setVisible (false);
    customPlot = new CustomPlot;
    QGridLayout *gridLayout = new QGridLayout;
    gridLayout->addWidget (customPlot);
    ui->w_Graficas->setLayout (gridLayout);

    customPlot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectAxes |
                                QCP::iSelectLegend | QCP::iSelectPlottables | QCP::iSelectItems | QCP::iMultiSelect);

    connect(customPlot, SIGNAL(mousePress(QMouseEvent *)), this,SLOT(slotMousePress(QMouseEvent *)));
    connect(customPlot, SIGNAL(mouseMove(QMouseEvent *)), this, SLOT(slotMouseMove(QMouseEvent *)));


    preparaGraficas();
    colocarLeyenda();
}

// La leyenda va debajo de la grafica, en una fila, con letra blanca: dentro
// tapaba la franja de arriba (la altura) y en negro no se leia sobre el fondo
// oscuro.
void Widget_Graficas::colocarLeyenda()
{
    QCPLegend *leyenda = customPlot->legend;
    QCPLayoutInset *dentro = customPlot->axisRect ()->insetLayout ();
    if (dentro->elements (false).contains (leyenda)) {
        dentro->take (leyenda);
        dentro->simplify ();
        customPlot->plotLayout ()->addElement (1, 0, leyenda);
        customPlot->plotLayout ()->setRowStretchFactor (1, 0.001);
        customPlot->plotLayout ()->setRowSpacing (2);
    }
    leyenda->setWrap (5);
    leyenda->setBrush (QColor (35, 35, 35));
    leyenda->setBorderPen (Qt::NoPen);
    leyenda->setTextColor (Qt::white);
    leyenda->setFont (QFont (QStringLiteral ("sans"), 9, QFont::Bold));
}

Widget_Graficas::~Widget_Graficas()
{
    delete ui;
}

QVector<double> Widget_Graficas::getBanqueo() const
{
    return Banqueo;
}

void Widget_Graficas::setBanqueo(const QVector<double> &value)
{
    Banqueo = value;
}

QVector<double> Widget_Graficas::getCabeceo() const
{
    return Cabeceo;
}

void Widget_Graficas::setCabeceo(const QVector<double> &value)
{
    Cabeceo = value;
}

QVector<double> Widget_Graficas::getRPM() const
{
    return RPM;
}

void Widget_Graficas::setRPM(const QVector<double> &value)
{
    RPM = value;
}

QVector<double> Widget_Graficas::getAltura() const
{
    return Altura;
}

void Widget_Graficas::setAltura(const QVector<double> &value)
{
    Altura = value;
}

QVector<double> Widget_Graficas::getVelocidad() const
{
    return Velocidad;
}

void Widget_Graficas::setVelocidad(const QVector<double> &value)
{
    Velocidad = value;
}

// Pinta lo que se le haya dado con setBanqueo... setTiempo (valores reales).
void Widget_Graficas::pintar()
{
    redibujar();
}

// Las cinco series, cada una con su color y su nombre. Deja las series vacias
// (al empezar un vuelo en vivo o una reproduccion).
void Widget_Graficas::preparaGraficas()
{
    customPlot->clearPlottables();
    customPlot->legend->setVisible(true);
    for(int i = 0; i < 5; i++)
    {
customPlot->addGraph ();
    }

    customPlot->graph (0)->setPen (QColor(Qt::red));
    customPlot->graph (1)->setPen (QColor(Qt::green));
    customPlot->graph (2)->setPen (QColor(Qt::cyan));
    customPlot->graph (3)->setPen (QColor(Qt::yellow));
    customPlot->graph (4)->setPen (QColor(80, 160, 255));
    // El eje vertical (0..1000) no tiene unidades: cada serie va en su franja.
    customPlot->yAxis->setTickLabels (false);

    Banqueo.clear ();
    Cabeceo.clear ();
    RPM.clear ();
    Altura.clear ();
    Velocidad.clear ();
    tiempo.clear ();
    redibujar ();
}

// Cada serie en su franja del eje 0..1000 (de arriba abajo: altura,
// velocidad, motor, cabeceo y banqueo), estirada entre el minimo y el maximo
// de lo que haya, y con el rango real en la leyenda ("Altura 98–152 m").
// Antes todas compartian una escala fija (la altura de 0 a 5000 m): con 100 m
// de diferencia la linea salia plana.
void Widget_Graficas::redibujar()
{
    struct Serie { int grafica; const QVector<double> *v; const char *nombre; const char *unidad;
                   double spanMinimo; int decimales; double franja; };
    const Serie series[] = {
        {4, &Altura,    "Altura",    "m",    10.0, 0, 800},
        {3, &Velocidad, "Velocidad", "km/h",  5.0, 0, 600},
        {2, &RPM,       "Motor",     "",      0.1, 2, 400},
        {1, &Cabeceo,   "Cabeceo",   "°",     5.0, 1, 200},
        {0, &Banqueo,   "Banqueo",   "°",     5.0, 1,   0},
    };
    for (const Serie &s : series) {
        QCPGraph *g = customPlot->graph (s.grafica);
        if (!g)
            continue;
        const QVector<double> &v = *s.v;
        const int n = qMin (v.size (), tiempo.size ());
        if (n == 0) {
            g->data ()->clear ();
            g->setName (QString::fromUtf8 (s.nombre));
            continue;
        }
        double lo = v.first (), hi = v.first ();
        for (int i = 0; i < n; ++i) {
            lo = qMin (lo, v.at (i));
            hi = qMax (hi, v.at (i));
        }
        double a = lo, b = hi;
        if (b - a < s.spanMinimo) {   // casi constante: un poco de margen alrededor
            const double c = (a + b) / 2.0;
            a = c - s.spanMinimo / 2.0;
            b = c + s.spanMinimo / 2.0;
        }
        QVector<double> y (n), t (n);
        for (int i = 0; i < n; ++i) {
            t[i] = tiempo.at (i);
            y[i] = s.franja + 20.0 + (v.at (i) - a) / (b - a) * 160.0;
        }
        g->setData (t, y, true);
        g->setName (QStringLiteral ("%1 %2–%3 %4").arg (QString::fromUtf8 (s.nombre))
                        .arg (lo, 0, 'f', s.decimales).arg (hi, 0, 'f', s.decimales)
                        .arg (QString::fromUtf8 (s.unidad)).trimmed ());
    }
    customPlot->yAxis->setRange (0, 1000);
    customPlot->xAxis->setRange (0, tiempo.isEmpty () ? 1.0 : tiempo.last () + 2);
    customPlot->replot ();
}

void Widget_Graficas::slotMousePress(QMouseEvent *event)
{

}

void Widget_Graficas::slotMouseMove(QMouseEvent *event)
{

}

// Al mostrarse se pinta lo que haya llegado mientras estaba oculta.
void Widget_Graficas::showEvent(QShowEvent *event)
{
    QWidget::showEvent (event);
    redibujar ();
}

// Un dato en vivo (valores reales: grados, motor, m, km/h). Se guarda y, si
// la ventana esta a la vista, se repinta.
void Widget_Graficas::datosTiempoReal(double Banq, double Cab, double rpm, double Alt, double Vel, double tiem)
{
    Banqueo.append (Banq);
    Cabeceo.append (Cab);
    RPM.append (rpm);
    Altura.append (Alt);
    Velocidad.append (Vel);
    tiempo.append (tiem);
    if (isVisible ())
        redibujar ();
}

QVector<double> Widget_Graficas::getTiempo() const
{
    return tiempo;
}

void Widget_Graficas::setTiempo(const QVector<double> &value)
{
    tiempo = value;
}

void Widget_Graficas::on_pB_Cerrar_clicked()
{
    this->close ();
}

QPushButton *Widget_Graficas::getMpBCerrar() const
{
    return mpBCerrar;
}

QLineEdit *Widget_Graficas::getLEvuelo() const
{
    return lEvuelo;
}

QLineEdit *Widget_Graficas::getLEdron() const
{
    return lEdron;
}

QString Widget_Graficas::getDronActivo() const
{
    return dronActivo;
}

void Widget_Graficas::setDronActivo(const QString &value)
{
    dronActivo = value;
    ui->dronLineEdit->setText (dronActivo);
}

QString Widget_Graficas::getVuelo() const
{
    return Vuelo;
}

void Widget_Graficas::setVuelo(const QString &value)
{
    Vuelo = value;
    ui->vueloLineEdit->setText (Vuelo);
}
