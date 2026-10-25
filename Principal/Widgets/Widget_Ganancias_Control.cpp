#include "Widget_Ganancias_Control.h"
#include "ui_Widget_Ganancias_Control.h"

#include <QDir>
#include "Protocolo/Tramas.h"
#include "Rutas.h"

Widget_Ganancias_Control::Widget_Ganancias_Control(sMANDOS *MANDOS, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget_Ganancias_Control),
    mandos(MANDOS)

{
    ui->setupUi(this);
    fileName = Rutas::confIni();
    configuraUI();

    m_originalStyle = ui->pB_Enviar->styleSheet(); // Guardar estilo inicial
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &Widget_Ganancias_Control::on_restoreStyle);
}

Widget_Ganancias_Control::~Widget_Ganancias_Control()
{
    delete ui;
}
void Widget_Ganancias_Control::configuraUI()
{

      ui->dsB_Ganancias_Cabeceo_Manual1->setValue (settings.cargarSetting(fileName,"KpC","MANDOS",0.0).value<double>());
      ui->dsB_Ganancias_Cabeceo_Manual1->setValue (settings.cargarSetting(fileName,"KiC","MANDOS",0.0).value<double>());
      ui->dsB_Ganancias_Banqueo_Manual1->setValue (settings.cargarSetting(fileName,"KpB","MANDOS",0.0).value<double>());
      ui->dsB_Ganancias_Banqueo_Manual1->setValue (settings.cargarSetting(fileName,"KiB","MANDOS",0.0).value<double>());
      ui->dsB_Ganancias_Altura_Manual1->setValue (settings.cargarSetting(fileName,"KpA","MANDOS",0.0).value<double>());
      ui->dsB_Ganancias_Altura_Manual1->setValue (settings.cargarSetting(fileName,"KiA","MANDOS",0.0).value<double>());
      ui->dsB_Ganancias_Velocidad_Manual1->setValue (settings.cargarSetting(fileName,"KpV","MANDOS",0.0).value<double>());
      ui->dsB_Ganancias_Velocidad_Manual1->setValue (settings.cargarSetting(fileName,"KiV","MANDOS",0.0).value<double>());
      ui->dsB_Ganancias_Rumbo_Manual1->setValue (settings.cargarSetting(fileName,"KpR","MANDOS",0.0).value<double>());
      ui->dsB_Ganancias_Rumbo_Manual1->setValue (settings.cargarSetting(fileName,"KiR","MANDOS",0.0).value<double>());
      ui->dsB_Ganancias_Curso_Manual->setValue (settings.cargarSetting(fileName,"KCu","MANDOS",0.0).value<double>());
      ui->dsB_ASc->setValue (settings.cargarSetting(fileName,"AsC","MANDOS",0.0).value<double>());
      ui->dsB_ASb->setValue (settings.cargarSetting(fileName,"AsB","MANDOS",0.0).value<double>());
}
void Widget_Ganancias_Control::on_pB_Enviar_clicked()
{
    mandos->Sintonizacion_Cabeceo = ui->cHB_Sintonizacion_Cabeceo->isChecked () ? 1 : 0 ;
    mandos->Sintonizacion_Banqueo = ui->cHB_Sintonizacion_banqueo->isChecked () ? 1 : 0 ;

    mandos->Ganancias_Cabeceo_Manual1 = ui->dsB_Ganancias_Cabeceo_Manual1->value ();
    mandos->Ganancias_Cabeceo_Manual2 = ui->dsB_Ganancias_Cabeceo_Manual2->value ();
    mandos->Actualizar_Ganancias_Cabeceo = ui->cHB_Actualizar_Ganancias_Cabeceo->isChecked () ? 1 : 0  ;

    mandos->Ganancias_Banqueo_Manual1 = ui->dsB_Ganancias_Banqueo_Manual1->value ();
    mandos->Ganancias_Banqueo_Manual2 = ui->dsB_Ganancias_Banqueo_Manual2->value ();
    mandos->Actualizar_Ganancias_Banqueo = ui->cHB_Actualizar_Ganancias_Banqueo->isChecked () ?  1 : 0  ;

    mandos->Ganancias_Altura_Manual1 = ui->dsB_Ganancias_Altura_Manual1->value ();
    mandos->Ganancias_Altura_Manual2 = ui->dsB_Ganancias_Altura_Manual2->value ();
    mandos->Actualizar_Ganancias_Altura = ui->cHB_Actualizar_Ganancias_Altura->isChecked () ?  1 : 0  ;

    mandos->Ganancias_Velocidad_Manual1 = ui->dsB_Ganancias_Velocidad_Manual1->value ();
    mandos->Ganancias_Velocidad_Manual2 = ui->dsB_Ganancias_Velocidad_Manual2->value ();
    mandos->Actualizar_Ganancias_Velocidad = ui->cHB_Actualizar_Ganancias_Velocidad->isChecked () ?  1 : 0  ;

    mandos->Ganancias_Curso_Manual = ui->dsB_Ganancias_Curso_Manual->value ();
    mandos->Actualizar_Ganancias_Curso = ui->cHB_Actualizar_Ganancias_Curso->isChecked () ?  1 : 0  ;

    mandos->Ganancias_Rumbo_Manual1 = ui->dsB_Ganancias_Rumbo_Manual1->value ();
    mandos->Ganancias_Rumbo_Manual2 = ui->dsB_Ganancias_Rumbo_Manual2->value ();
    mandos->Actualizar_Ganancias_Rumbo = ui->cHB_Actualizar_Ganancias_Rumbo->isChecked () ?  1 : 0  ;

    // La trama sale de la estructura, como en Widget_Mandos (antes se copiaban
    // los bytes del puntero, no los de la estructura).
    emit enviaMandosDron(Tramas::mandos (*mandos));

    // Todo de una vez y en otro hilo (valor a valor paraba la pantalla).
    settings.guardarSettings (fileName, "MANDOS", {
        {"KpC", ui->dsB_Ganancias_Cabeceo_Manual1->value ()},
        {"KiC", ui->dsB_Ganancias_Cabeceo_Manual2->value ()},
        {"KpB", ui->dsB_Ganancias_Banqueo_Manual1->value ()},
        {"KiB", ui->dsB_Ganancias_Banqueo_Manual2->value ()},
        {"KpA", ui->dsB_Ganancias_Altura_Manual1->value ()},
        {"KiA", ui->dsB_Ganancias_Altura_Manual2->value ()},
        {"KpV", ui->dsB_Ganancias_Velocidad_Manual1->value ()},
        {"KiV", ui->dsB_Ganancias_Velocidad_Manual2->value ()},
        {"KpR", ui->dsB_Ganancias_Rumbo_Manual1->value ()},
        {"KiR", ui->dsB_Ganancias_Rumbo_Manual2->value ()},
        {"KCu", ui->dsB_Ganancias_Curso_Manual->value ()},
        {"AsC", ui->dsB_ASc->value ()},
        {"AsB", ui->dsB_ASb->value ()}});

}

void Widget_Ganancias_Control::on_AcuseRecibo()
{
    if(!m_timer.isActive()) {  // Solo guardar estilo si no hay timer activo
        m_originalStyle = ui->pB_Enviar->styleSheet();
    }

    ui->pB_Enviar->setStyleSheet("background-color: #00FF00; color: black;");
    m_timer.start(1000);
}

void Widget_Ganancias_Control::on_restoreStyle()
{
    ui->pB_Enviar->setStyleSheet(m_originalStyle);
}

