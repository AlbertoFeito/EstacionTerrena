#include "Widget_Drones.h"
#include "ui_Widget_Drones.h"
#include <QDir>
#include <QMessageBox>
#include "MyQSettings/myqsetting.h"
#include "Rutas.h"
#include "Vuelos/BaseDatos.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QSqlQueryModel>
Widget_Drones::Widget_Drones(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget_Drones)
{
    ui->setupUi(this);
    QString fileName = Rutas::confIni();
    MyQSetting mysettings;

    QVariant datosTiposDrones = mysettings.cargarSetting (
                fileName,
                "listaTiposDrones",
                "TIPOSDRONES",
                QStringList({"Ala fija","Cuadróptero"})
                );
    if(datosTiposDrones.canConvert<QStringList> ())
    {
        QStringList lTiposDrones = datosTiposDrones.toStringList ();
        ui->cB_Tipo->clear ();
        ui->cB_Tipo->addItems (lTiposDrones);
    }
    else
    {
    }


    BaseDatos::asegurarTablaDrones ();

    _tableModel = new QSqlQueryModel(this);
    ui->tV_Drones->setModel(_tableModel);
    ui->tV_Drones->verticalHeader ()->setVisible (false);
    actualizaTabla ();
    connect (this,&Widget_Drones::nuevoDron,this,&Widget_Drones::actualizaTabla);
    ui->tV_Drones->setSizeAdjustPolicy (QTableView::AdjustToContents);
}

Widget_Drones::~Widget_Drones()
{
    delete ui;
}

void Widget_Drones::on_pB_Agregar_clicked()
{
    if(ui->lE_DronID->text ().isEmpty () || ui->lE_NombreDron->text ().isEmpty ())
    {
        QMessageBox::critical(this,"Error","Entre todos los datos del dron");
        ui->lE_DronID->clear ();
        ui->lE_NombreDron->clear ();
        return;
    }
    int id_dron = ui->lE_DronID->text ().toInt ();
    QString nombre_Dron = ui->lE_NombreDron->text ();
    QString tipo = ui->cB_Tipo->currentText ();

    QSqlQuery q(BaseDatos::drones ());
    q.prepare ("INSERT INTO DronTable (Id_Dron, Nombre, Tipo, Vuelos) VALUES (?, ?, ?, 0)");
    q.addBindValue (id_dron);
    q.addBindValue (nombre_Dron);
    q.addBindValue (tipo);
    if(!q.exec ())
    {
        QMessageBox::critical(this,"Error Agregar",q.lastError ().text ());
        ui->lE_DronID->clear ();
        ui->lE_NombreDron->clear ();
        return;
    }

    //    _tableModel->startExec("SELECT * FROM DronTable");

    ui->lE_DronID->clear ();
    ui->lE_NombreDron->clear ();
    //    qDebug()<<"AGREGAR"<<query1;
    emit nuevoDron ();
}

void Widget_Drones::on_pB_Eliminar_clicked()
{
    int row = ui->tV_Drones->currentIndex ().row ();
    //    int col = ui->tV_Drones->currentIndex ().column ();
    int idDron = _tableModel->index (row,0).data ().toInt ();
    QSqlQuery q(BaseDatos::drones ());
    q.prepare ("DELETE FROM DronTable WHERE Id_Dron = ?");
    q.addBindValue (idDron);
    if(!q.exec ())
    {
        QMessageBox::critical(this,"Error",q.lastError ().text ());
        return;
    }
    //    qDebug()<<"DELETE"<<queryDelete;
    emit nuevoDron ();
}

// Vuelve a leer la tabla de drones.
void Widget_Drones::actualizaTabla()
{
    _tableModel->setQuery (QSqlQuery("SELECT * FROM DronTable", BaseDatos::drones ()));
}

// Crea la tabla de drones si no existe (true si hubo error, como antes).
bool Widget_Drones::createTable()
{
    return !BaseDatos::asegurarTablaDrones ();
}

// Modelo de la tabla de drones.
QSqlQueryModel *Widget_Drones::tableModel() const
{
    return _tableModel;
}
