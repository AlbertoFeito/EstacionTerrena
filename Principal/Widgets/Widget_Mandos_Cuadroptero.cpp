#include "Widget_Mandos_Cuadroptero.h"
#include "ui_Widget_Mandos_Cuadroptero.h"

Widget_Mandos_Cuadroptero::Widget_Mandos_Cuadroptero(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget_Mandos_Cuadroptero)
{
    ui->setupUi(this);
}

Widget_Mandos_Cuadroptero::~Widget_Mandos_Cuadroptero()
{
    delete ui;
}
