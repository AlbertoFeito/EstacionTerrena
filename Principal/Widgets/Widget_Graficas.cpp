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

void Widget_Graficas::pintar()
{
    customPlot->graph (0)->setData (tiempo,Banqueo);

    customPlot->graph (1)->setData (tiempo,Cabeceo);

    customPlot->graph (2)->setData (tiempo,RPM);

    customPlot->graph (3)->setData (tiempo,Velocidad);

    customPlot->graph (4)->setData (tiempo,Altura);

    customPlot->xAxis->setRange (0,tiempo.last () + 2);
    customPlot->replot ();
}

void Widget_Graficas::preparaGraficas()
{
    customPlot->clearPlottables();
    customPlot->legend->setVisible(true);
    for(int i = 0; i < 5; i++)
    {
customPlot->addGraph ();
    }

    customPlot->graph (0)->setPen (QColor(Qt::red));
    customPlot->graph (0)->setName ("Banqueo");

    customPlot->graph (1)->setPen (QColor(Qt::green));
    customPlot->graph (1)->setName ("Cabeceo");

    customPlot->graph (2)->setPen (QColor(Qt::cyan));
    customPlot->graph (2)->setName ("RPM");

    customPlot->graph (3)->setPen (QColor(Qt::yellow));
    customPlot->graph (3)->setName ("Velocidad");

    customPlot->graph (4)->setPen (QColor(Qt::blue));
    customPlot->graph (4)->setName ("Altura");
}

void Widget_Graficas::slotMousePress(QMouseEvent *event)
{

}

void Widget_Graficas::slotMouseMove(QMouseEvent *event)
{

}

void Widget_Graficas::datosTiempoReal(double Banq, double Cab, double rpm, double Alt, double Vel, double tiem)
{
    customPlot->graph (0)->addData (tiem,Map (Banq,-20,20,0,1000));

    customPlot->graph (1)->addData (tiem,Map(Cab,-90,90,0,1000));

    customPlot->graph (2)->addData (tiem,Map (rpm,0,1000,0,1000));

    customPlot->graph (3)->addData (tiem,Map(Vel,0,320,0,1000));

    customPlot->graph (4)->addData (tiem,Map(Alt,0,5000,0,1000));

    customPlot->xAxis->setRange (0,tiem+1);
    customPlot->replot ();
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

float Widget_Graficas::Map(float x, float in_min, float in_max, float out_min, float out_max)
{
    return (x-in_min)*(out_max - out_min)/(in_max-in_min)+out_min;
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
