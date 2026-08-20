#include "Widget_Estado_Cuadroptero.h"
#include "ui_Widget_Estado_Cuadroptero.h"

Widget_Estado_Cuadroptero::Widget_Estado_Cuadroptero(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget_Estado_Cuadroptero)
{
    ui->setupUi(this);
}

Widget_Estado_Cuadroptero::~Widget_Estado_Cuadroptero()
{
    delete ui;
}
