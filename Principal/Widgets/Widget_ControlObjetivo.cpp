#include "Widget_ControlObjetivo.h"
#include "ui_Widget_ControlObjetivo.h"
#include <QDebug>
#include <QTableView>
#include <QHeaderView>
#include <QScrollBar>
#include <QGridLayout>
#include "header/ColumnHeaderLengthAdapter.h"

Widget_ControlObjetivo::Widget_ControlObjetivo(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget_ControlObjetivo)
{
    ui->setupUi(this);


    wGraficas = new Widget_Graficas;

    wDatos = new QWidget;

    tV = new QTableView;

    _queryModel = new Database::AsyncQueryModel(this);
    tV->setModel(_queryModel);
    tV->verticalHeader ()->hide ();
    //    tV->setVerticalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
    tV->setHorizontalScrollBarPolicy (Qt::ScrollBarAsNeeded);

    FlexibleHeaderView *headerView = new FlexibleHeaderView(Qt::Horizontal);
    tV->setHorizontalHeader (headerView);

    headerView->setSectionProportion(32, 0.4);

    tV->setSizeAdjustPolicy (QAbstractScrollArea::AdjustToContents);
    tV->setSizePolicy (QSizePolicy::Expanding,QSizePolicy::Expanding);

    pB_Imprimir = new QPushButton("Imprimir",wDatos);
    auto pB_Cerrar = new QPushButton("Cerrar",wDatos);
    connect (pB_Cerrar,&QPushButton::clicked ,this,[=](){
        wDatos->close ();
    });

    connect (pB_Imprimir,&QPushButton::clicked ,this,[=](){
        tV->setSizeAdjustPolicy (QAbstractScrollArea::AdjustToContents);
        QEcsReport ecsRep(tV); // QEcsReport ecsRep(ui->tableView);
        ecsRep.Reset();
        ecsRep.setLogo(":/logos/Logos/LOGOS/03.png", Qt::AlignRight, 60, 130); // // optional  >>  Default = Null
        ecsRep.setHeaderCBackColor(0xD4FFDC); // optional  >>  Default = 0xEDEDED >> Grey
        ecsRep.setPageSize(QPageSize::A4); // optional  >>  Default = A4
        ecsRep.setOrientation(QPageLayout::Landscape); // optional  >>  Default = Portrait
        ecsRep.setPageNumberFormat("Hoja %p de %P ", Qt::AlignRight); // optional  >>  Default = Null
        ecsRep.setPageNumberFont(QFont("Arial", 9)); // optional  >>  Default "Arial",8
        ecsRep.setGrudFont(QFont("tahoma", 10)); // optional  >>  Default "Arial",10
        ecsRep.addText(pageHeader, "Reporte General", QFont("Tahoma", 18), Qt::AlignCenter);
        ecsRep.addText(pageHeader, "Fecha: " + QDate::currentDate ().toString (), QFont("Arial", 14), Qt::AlignCenter);
        //        ecsRep.addText(pageHeader, "Products Test", QFont("Arial", 10), Qt::AlignLeft);
        //        ecsRep.addText(reportFooter, "Total 500.00", QFont("Tahoma", 14), Qt::AlignCenter);
        ecsRep.addText(pageFooter, QString("Dron: %1, Vuelo: %2 ").arg (dronActivo).arg (Vuelo), QFont("Arial", 14), Qt::AlignLeft);
        ecsRep.addText(reportHeader, "CID-MECATRONICS", QFont("Arial", 18), Qt::AlignCenter);
        //        ecsRep.addText(reportFooter, "Sign", QFont("Arial", 12), Qt::AlignRight);
        ecsRep.setLayoutCol(0, Qt::AlignCenter);
        ecsRep.setLayoutCol(1, Qt::AlignCenter);
        ecsRep.setLayoutCol(2, Qt::AlignCenter);
        ecsRep.setLayoutCol(3, Qt::AlignCenter);
        ecsRep.setLayoutCol(4, Qt::AlignCenter);
        ecsRep.setLayoutCol(5, Qt::AlignCenter);
        ecsRep.setLayoutCol(6, Qt::AlignCenter);
        ecsRep.setLayoutCol(8, Qt::AlignCenter);
        ecsRep.setLayoutCol(9, Qt::AlignCenter);
        ecsRep.setLayoutCol(10, Qt::AlignCenter);
        ecsRep.setLayoutCol(11, Qt::AlignCenter);
        ecsRep.setLayoutCol(12, Qt::AlignCenter);
        ecsRep.setLayoutCol(13, Qt::AlignCenter);
        ecsRep.setLayoutCol(14, Qt::AlignCenter);
        ecsRep.setLayoutCol(15, Qt::AlignCenter);
        ecsRep.setLayoutCol(16, Qt::AlignCenter);
        ecsRep.setLayoutCol(17, Qt::AlignCenter);
        ecsRep.setLayoutCol(18, Qt::AlignCenter);
        ecsRep.setLayoutCol(19, Qt::AlignCenter);
        ecsRep.setLayoutCol(20, Qt::AlignCenter);
        ecsRep.setLayoutCol(21, Qt::AlignCenter);
        ecsRep.setLayoutCol(22, Qt::AlignCenter);
        ecsRep.setLayoutCol(23, Qt::AlignCenter);
        ecsRep.setLayoutCol(24, Qt::AlignCenter);
        ecsRep.setLayoutCol(25, Qt::AlignCenter);
        ecsRep.setLayoutCol(26, Qt::AlignCenter);
        ecsRep.setLayoutCol(27, Qt::AlignCenter);
        ecsRep.setLayoutCol(28, Qt::AlignCenter);
        ecsRep.setLayoutCol(29, Qt::AlignCenter);
        ecsRep.setLayoutCol(30, Qt::AlignCenter);
        ecsRep.setLayoutCol(31, Qt::AlignCenter);
        ecsRep.setLayoutCol(32, Qt::AlignCenter);
        //        ecsRep.ExportPDF ("Reporte.pdf");

        ecsRep.setWindowModality (Qt::WindowModal);
        ecsRep.setWindowFlags (Qt::WindowStaysOnTopHint | Qt::FramelessWindowHint);
        ecsRep.Preview ();

    });
    auto layoutV = new QVBoxLayout(wDatos);
    auto layoutH = new QHBoxLayout(wDatos);

    layoutH->addStretch ();
    layoutH->addWidget (pB_Imprimir);
    layoutH->addWidget (pB_Cerrar);

    layoutV->addWidget (tV);
    layoutV->addLayout (layoutH);
    wDatos->setLayout (layoutV);
    wDatos->setFixedSize (1000,600);
    wDatos->setWindowTitle ("Reporte de vuelo");
    wDatos->setWindowFlags (Qt::FramelessWindowHint);


    estadoSimulacion = false;
    posDatoActual = 0;
    clkVelocidad = new QTimer(this);
    connect(clkVelocidad,&QTimer::timeout,this,[&](){
        if(ui->hS_Avanzar->value() < ui->hS_Avanzar->maximum())
        {

            posDatoActual = ui->hS_Avanzar->value();
            prepara_envia_datos(posDatoActual++);
            ui->hS_Avanzar->setValue(posDatoActual);
        }
        else
        {
            ui->pB_Play->clicked(false);
            ui->pB_Play->setChecked (false);
            ui->hS_Avanzar->setValue(0);
            emit limpiarDatoContolObjetivo(dronActivo);
            emit detenerDatoContolObjetivo ();
        }

    });
}

void Widget_ControlObjetivo::prepara_envia_datos(int pos)
{
    if(pos < listDatosVuelos.size ())
    {
        QByteArray aux;
        aux.append("=");
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Id_Vuelos));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Lat,'f',4));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Lon,'f',4));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Realimentacion_Cabeceo,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Realimentacion_Banqueo,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Realimentacion_Velocidad,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Realimentacion_Altura,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Realimentacion_Curso,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Distancia_Chequeo,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Curso_Deseado,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Realimentacion_Regimen));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Realimentacion_ComandoMision));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Realimentacion_CursoIMO,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).CantidadSatelites));
        aux.append(",");

        aux.append(QByteArray::number(listDatosVuelos.at(pos).KPCabeceo,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).KICabeceo,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).KPBanqueo,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).KIBanqueo,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).KPVelocidad,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).KIVelocidad,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).KPAltura,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).KIAltura,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).KPCurso,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).KPRumbo,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).KIRumbo,'f',2));
        aux.append(",");

        aux.append(QByteArray::number(listDatosVuelos.at(pos).Control_Estabilizadores,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Control_Alerones,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Control_Motor,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Control_Rumbo,'f',2));
        aux.append(",");

        aux.append(QByteArray::number(listDatosVuelos.at(pos).Convergencia_Banqueo,'f',2));
        aux.append(",");
        aux.append(QByteArray::number(listDatosVuelos.at(pos).Convergencia_Cabeceo,'f',2));
        aux.append(",");
        auto timeFin = listDatosVuelos.at(pos).Fecha;
        auto timeIni = listDatosVuelos.at(0).Fecha;
        auto t = timeIni.time ().msecsTo (timeFin.time ())/1000.0;

        aux.append(QByteArray::number (t));//32
        emit datoContolObjetivo(aux);
    }
}

Widget_ControlObjetivo::~Widget_ControlObjetivo()
{
    delete ui;
}


QString Widget_ControlObjetivo::getDronActivo() const
{
    return dronActivo;
}

void Widget_ControlObjetivo::setDronActivo(const QString &value)
{
    dronActivo = value;
}

QStringList Widget_ControlObjetivo::getDrones() const
{
    return drones;
}

void Widget_ControlObjetivo::setDrones(const QStringList &value)
{
    ui->cB_Drones->clear ();
    drones = value;
    ui->cB_Drones->addItems (drones);
}

void Widget_ControlObjetivo::on_pB_Ver_ControlObjetivo_clicked()
{
    int col = _queryModel->asyncQuery ()->result ().headRecord().count ();
    int rows = _queryModel->asyncQuery ()->result ().count ();

    wDatos->setWindowTitle (QString("Datos del vuelo %1, del dron %2").arg (Vuelo).arg (dronActivo));
    if(col > 0 && rows > 0)
        wDatos->show ();
}

QString Widget_ControlObjetivo::getVuelo() const
{
    return Vuelo;
}

void Widget_ControlObjetivo::setVuelo(const QString &value)
{
    Vuelo = value;
}


void Widget_ControlObjetivo::on_pB_Graficar_clicked()
{
    if(ui->pB_Play->isChecked ())
    {
         wGraficas->show ();
    }
    else
    {

        QVector<double> Banqueo;
        QVector<double> Cabeceo;
        QVector<double> RPM;
        QVector<double> Altura;
        QVector<double> Velocidad;
        QVector<double> tiempo;
        double t = 0;
        bool accion = false;


        Database::AsyncQueryResult res = _queryModel->asyncQuery ()->result ();
        for (int i = 0; i < res.count(); i++) {
            accion = true;

            auto aux = res.value(i, "Cabeceo").toDouble ();
            Cabeceo.append (Map (aux,-90,90,0,1000));

            aux = res.value(i, "Banqueo").toDouble ();
            Banqueo.append (Map (aux,-20,20,0,1000));

            aux = res.value(i, "RPM").toDouble ();
            RPM.append (Map (aux,0,1000,0,1000));

            aux = res.value(i, "Velocidad").toDouble ();
            Velocidad.append (Map (aux,0,320,0,1000));

            aux = res.value(i, "Altura").toDouble ();
            Altura.append (Map (aux,0,5000,0,1000));

            auto timeFin = res.value (i,"Fecha").toDateTime ();
            auto timeIni = res.value (0,"Fecha").toDateTime ();
            t = timeIni.time ().msecsTo (timeFin.time ())/1000.0;
            tiempo.append (t);
//            t+=0.2;
        }

        if(accion)
        {
            wGraficas->setCabeceo (Cabeceo);
            wGraficas->setBanqueo (Banqueo);
            wGraficas->setRPM (RPM);
            wGraficas->setVelocidad (Velocidad);
            wGraficas->setAltura (Altura);
            wGraficas->setTiempo (tiempo);
            wGraficas->pintar();
            wGraficas->setWindowTitle (QString("Gráficas del vuelo %1, del dron %2").arg (Vuelo).arg (dronActivo));
            wGraficas->setVuelo (Vuelo);
            wGraficas->setDronActivo (dronActivo);
            wGraficas->show ();
        }
    }
}



float Widget_ControlObjetivo::Map(float x, float in_min, float in_max, float out_min, float out_max)
{
    return (x-in_min)*(out_max - out_min)/(in_max-in_min)+out_min;
}

void Widget_ControlObjetivo::on_cB_Drones_currentIndexChanged(const QString &arg1)
{
    ui->cB_Vuelos->clear ();
    listDatosVuelos.clear();

    dronActivo = arg1;

    QString queryVuelos = "SELECT DISTINCT Id_Vuelos "
                          "FROM DatosVuelos "
                          "INNER JOIN DronTable ON DronTable.Id_Dron = DatosVuelos.Dron_ID "
                          "WHERE DronTable.Nombre = \'" + dronActivo +"\'";

    Database::AsyncQuery::startExecOnce(queryVuelos,
                                        [=](const Database::AsyncQueryResult& res)
    {
        for (int i = 0; i < res.count(); i++) {
            ui->cB_Vuelos->addItem(res.value(i, "Id_Vuelos").toString());
        }
        ui->cB_Vuelos->setCurrentIndex(0);
        Vuelo = ui->cB_Vuelos->currentText();
    });
}

void Widget_ControlObjetivo::on_cB_Vuelos_currentIndexChanged(const QString &arg1)
{
    Vuelo = arg1;
    dronActivo = ui->cB_Drones->currentText();

    QString queryDatos = "SELECT DatosVuelos.Lat, "
                         "DatosVuelos.Lon, "
                         "DatosVuelos.Realimentacion_Cabeceo, "
                         "DatosVuelos.Realimentacion_Banqueo, "
                         "DatosVuelos.Realimentacion_Velocidad, "
                         "DatosVuelos.Realimentacion_Altura, "
                         "DatosVuelos.Realimentacion_Curso, "
                         "DatosVuelos.Distancia_Chequeo, "
                         "DatosVuelos.Curso_Deseado, "
                         "DatosVuelos.Realimentacion_Regimen, "
                         "DatosVuelos.Realimentacion_ComandoMision, "
                         "DatosVuelos.Realimentacion_CursoIMU, "
                         "DatosVuelos.CantidadSatelites, "

                         "DatosVuelos.KPCabeceo, "
                         "DatosVuelos.KICabeceo, "
                         "DatosVuelos.KPBanqueo, "
                         "DatosVuelos.KIBanqueo, "
                         "DatosVuelos.KPVelocidad, "
                         "DatosVuelos.KIVelocidad, "
                         "DatosVuelos.KPAltura, "
                         "DatosVuelos.KIAltura, "
                         "DatosVuelos.KPCurso, "
                         "DatosVuelos.KPRumbo, "
                         "DatosVuelos.KIRumbo, "

                         "DatosVuelos.Control_Estabilizadores, "
                         "DatosVuelos.Control_Alerones, "
                         "DatosVuelos.Control_Motor, "
                         "DatosVuelos.Control_Rumbo, "

                         "DatosVuelos.Convergencia_Banqueo, "
                         "DatosVuelos.Convergencia_Cabeceo, "
                         "DatosVuelos.Fecha "
                         "FROM DatosVuelos "
                         "INNER JOIN DronTable ON DronTable.Id_Dron = DatosVuelos.Dron_ID "
                         "WHERE DronTable.Nombre = \'" + dronActivo +"\'"
                                                                     "AND DatosVuelos.Id_Vuelos = " + Vuelo;

    listDatosVuelos.clear();

    Database::AsyncQuery::startExecOnce(queryDatos,
                                        [=](const Database::AsyncQueryResult& res)
    {
        for (int i = 0; i < res.count(); i++) {
            listDatosVuelos.append( s_datosVuelos(
                                        res.value(i,"Id_Vuelos").toInt (),
                                        res.value(i,"Dron_ID").toInt (),
                                        res.value(i,"Lat").toDouble(),
                                        res.value(i,"Lon").toDouble (),
                                        res.value(i,"Realimentacion_Cabeceo").toDouble (),
                                        res.value(i,"Realimentacion_Banqueo").toDouble (),
                                        res.value(i,"Realimentacion_Velocidad").toDouble (),
                                        res.value(i,"Realimentacion_Altura").toDouble (),
                                        res.value(i,"Realimentacion_Curso").toDouble (),
                                        res.value(i,"Distancia_Chequeo").toDouble (),
                                        res.value(i,"Curso_Deseado").toDouble (),
                                        res.value(i,"Realimentacion_Regimen").toInt (),
                                        res.value(i,"Realimentacion_ComandoMision").toInt (),
                                        res.value(i,"Realimentacion_CursoIMU").toDouble (),
                                        res.value(i,"CantidadSatelites").toDouble (),

                                        res.value(i,"KPCabeceo").toDouble (),
                                        res.value(i,"KICabeceo").toDouble (),
                                        res.value(i,"KPBanqueo").toDouble (),
                                        res.value(i,"KIBanqueo").toDouble (),
                                        res.value(i,"KPVelocidad").toDouble (),
                                        res.value(i,"KIVelocidad").toDouble (),
                                        res.value(i,"KPAltura").toDouble (),
                                        res.value(i,"KIAltura").toDouble (),
                                        res.value(i,"KPCurso").toDouble (),
                                        res.value(i,"KPRumbo").toDouble (),
                                        res.value(i,"KIRumbo").toDouble (),

                                        res.value(i,"Control_Estabilizadores").toDouble (),
                                        res.value(i,"Control_Alerones").toDouble (),
                                        res.value(i,"Control_Motor").toDouble (),
                                        res.value(i,"Control_Rumbo").toDouble (),

                                        res.value(i,"Convergencia_Banqueo").toDouble (),
                                        res.value(i,"Convergencia_Cabeceo").toDouble (),

                                        res.value(i,"Fecha").toDateTime()));
        }
        _queryModel->startExec (queryDatos);
        configuraComponentesPlay();
    });
}

void Widget_ControlObjetivo::configuraComponentesPlay()
{
    ui->hS_Avanzar->setMaximum(listDatosVuelos.size());
}

void Widget_ControlObjetivo::on_hS_Velocidad_valueChanged(int value)
{
    if(estadoSimulacion)
        clkVelocidad->start(TIME_BASE/value);
}

void Widget_ControlObjetivo::on_pB_Stop_clicked()
{
    ui->hS_Avanzar->setValue (0);
    ui->hS_Velocidad->setValue (0);
    ui->pB_Stop->setEnabled (false);
    ui->pB_Play->setChecked (false);
    emit limpiarDatoContolObjetivo(dronActivo);
    emit detenerDatoContolObjetivo ();
}

void Widget_ControlObjetivo::on_pB_Play_toggled(bool checked)
{
    if(checked)
    {
        wGraficas->preparaGraficas ();
        estadoSimulacion = true;
        ui->pB_Stop->setEnabled (true);
        emit limpiarDatoContolObjetivo(dronActivo);
        emit iniciarDatoContolObjetivo ();
        clkVelocidad->start(TIME_BASE/ui->hS_Velocidad->value());
//        ui->pB_Play->setStyleSheet ("image: url(:/iconos/Recursos/Iconos/pause.ico);");
        setControlObjetivoTime (QTime::currentTime ());
    }
    else
    {
        clkVelocidad->stop();
        estadoSimulacion = false;
//        ui->pB_Play->setStyleSheet ("image: url(:/iconos/Recursos/Iconos/play.ico);");
    }
}

void Widget_ControlObjetivo::on_hS_Avanzar_valueChanged(int value)
{
    //    emit limpiarDatoContolObjetivo(dronActivo);
    if(ui->pB_Play->isChecked ())
        prepara_envia_datos(value);
}

void Widget_ControlObjetivo::on_hS_Avanzar_sliderPressed()
{
    emit limpiarDatoContolObjetivo(dronActivo);
}

void Widget_ControlObjetivo::on_hS_Avanzar_sliderReleased()
{
    // Slider released - position is handled by valueChanged
}

QTime Widget_ControlObjetivo::getControlObjetivoTime() const
{
    return controlObjetivoTime;
}

void Widget_ControlObjetivo::setControlObjetivoTime(const QTime &value)
{
    controlObjetivoTime = value;
}

Widget_Graficas *Widget_ControlObjetivo::getWGraficas() const
{
    return wGraficas;
}

void Widget_ControlObjetivo::setWGraficas(Widget_Graficas *value)
{
    wGraficas = value;
}
