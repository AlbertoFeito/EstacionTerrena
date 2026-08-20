#include "Widget_Parametros.h"
#include "ui_Widget_Parametros.h"

Widget_Parametros::Widget_Parametros(QWidget *parent)
    : QWidget(parent)
    , ui(new Ui::Widget_Parametros)
{
    ui->setupUi(this);
}

Widget_Parametros::~Widget_Parametros()
{
    delete ui;
}

void Widget_Parametros::updateValues(const QVector<double> &values) {

    ui->lB_Lat->setText (QString::number (values[0],'f',4));
    ui->lB_Lon->setText (QString::number (values[1],'f',4));
    ui->lB_DistCheq->setText (QString::number (values[2],'f',2));
    ui->lB_CursoDeseado->setText (QString::number (values[3],'f',2));
    //    ui->lB_Regimen->setText (QString::number (values[4]));

    if(values[4] == 0)
        ui->lB_Regimen->setText ("M");
    else if(values[4] == 1)
        ui->lB_Regimen->setText ("S");
    else
        ui->lB_Regimen->setText ("A");

    ui->lB_CmdMision->setText (QString::number (values[5]));
    ui->lB_CursoIMU->setText (QString::number (values[6],'f',2));
    ui->lB_CantSat->setText (QString::number (values[7]));

    ui->lB_KPCab->setText (QString::number(values[8],'f',2));
    ui->lB_KICab->setText (QString::number(values[9],'f',2));
    ui->lB_KPBan->setText (QString::number(values[10],'f',2));
    ui->lB_KIBan->setText (QString::number(values[11],'f',2));
    ui->lB_KPVel->setText (QString::number(values[12],'f',2));
    ui->lB_KIVel->setText (QString::number(values[13],'f',2));
    ui->lB_KPAlt->setText (QString::number(values[14],'f',2));
    ui->lB_KIAlt->setText (QString::number(values[15],'f',2));
    ui->lB_KPCurso->setText (QString::number(values[16],'f',2));
    ui->lB_KPRumbo->setText (QString::number(values[17],'f',2));
    ui->lB_KIRumbo->setText (QString::number(values[18],'f',2));

    ui->lB_Estabilizador->setText (QString::number (values[19],'f',2));
    ui->lB_Alerones->setText (QString::number (values[20],'f',2));
    ui->lB_Motor->setText (QString::number (values[21],'f',2));
    ui->lB_Rumbo->setText (QString::number (values[22],'f',2));

    ui->lB_ConvBan->setText (QString::number (values[23],'f',2));
    ui->lB_ConvCab->setText (QString::number (values[24],'f',2));

}

void Widget_Parametros::updateSatelites(int cantSatelites)
{
    // qDebug()<<"cantSatelites"<<cantSatelites;
}

int Widget_Parametros::tipo_dron() const
{
    return m_tipo_dron;
}

void Widget_Parametros::setTipo_dron(int newTipo_dron)
{
    m_tipo_dron = newTipo_dron;
    if(m_tipo_dron != 0)
    {
        ui->lb_Actuador1->setText ("MotorIF:");
        ui->lb_Actuador2->setText ("MotorDF:");
        ui->lb_Actuador3->setText ("MotorIT:");
        ui->lb_Actuador4->setText ("MotorIT:");
    }
    else
    {
        ui->lb_Actuador1->setText ("Estab:");
        ui->lb_Actuador2->setText ("Alerones:");
        ui->lb_Actuador3->setText ("Motor:");
        ui->lb_Actuador4->setText ("Rumbo:");
    }

}
