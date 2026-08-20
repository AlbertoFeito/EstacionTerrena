#include "Widget_Drones.h"
#include "ui_Widget_Drones.h"
#include <QDir>
#include <QMessageBox>
#include "MyQSettings/myqsetting.h"
Widget_Drones::Widget_Drones(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget_Drones)
{
    ui->setupUi(this);
    QString fileName = QDir::currentPath() + "/conf.ini";
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


    _aQuery = new Database::AsyncQuery(this);

    createTable ();

    _tableModel = new Database::AsyncQueryModel(this);
    ui->tV_Drones->setModel(_tableModel);
    ui->tV_Drones->verticalHeader ()->setVisible (false);
    ui->tV_Drones->setSizeAdjustPolicy (QTableView::AdjustToContents);
    _tableModel->startExec("SELECT * FROM DronTable");

    connect (this,&Widget_Drones::nuevoDron,[=](){
        Database::AsyncQuery *aQuery = _tableModel->asyncQuery();

        aQuery->setMode(Database::AsyncQuery::Mode_Parallel);
        aQuery->setDelayMs(500);
        aQuery->prepare("SELECT * FROM DronTable");
        aQuery->startExec();
    });
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

    query1 = "INSERT INTO DronTable (Id_Dron, Nombre, Tipo, Vuelos)"
             " VALUES (:Id_Dron, :Nombre, :Tipo, :Vuelos)";
    _aQuery->prepare (query1);

    _aQuery->bindValue(":Id_Dron", id_dron);
    _aQuery->bindValue(":Nombre", nombre_Dron);
    _aQuery->bindValue(":Tipo", tipo);
    _aQuery->bindValue(":Vuelos", 0);
    _aQuery->startExec ();

    if(_aQuery->result ().error ().isValid ())
    {
        QMessageBox::critical(this,"Error Agregar",_aQuery->result ().error ().text ());
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
    QString queryDelete = "DELETE FROM DronTable WHERE Id_Dron = " + QString::number (idDron);
    _aQuery->prepare (queryDelete);
    _aQuery->startExec();
    if(_aQuery->result ().error ().isValid ())
    {
        QMessageBox::critical(this,"Error",_aQuery->result ().error ().text ());
        return;
    }
    //    qDebug()<<"DELETE"<<queryDelete;
    emit nuevoDron ();
}

Database::AsyncQuery *Widget_Drones::aQuery() const
{
    return _aQuery;
}

void Widget_Drones::setAQuery(Database::AsyncQuery *aQuery)
{
    _aQuery = aQuery;
}

void Widget_Drones::actualizaTabla()
{
    emit nuevoDron ();
}

bool Widget_Drones::createTable()
{
    /**************************************************/
    query1 = "CREATE TABLE IF NOT EXISTS DronTable("
             "Id_Dron INTEGER PRIMARY KEY NOT NULL UNIQUE, "
             "Nombre TEXT, "
             "Tipo TEXT, "
             "Vuelos INTEGER DEFAULT 0)";

    _aQuery->prepare (query1);
    _aQuery->startExec ();
    return _aQuery->result ().error ().isValid ();
}

Database::AsyncQueryModel *Widget_Drones::tableModel() const
{
    return _tableModel;
}

void Widget_Drones::setTableModel(Database::AsyncQueryModel *tableModel)
{
    _tableModel = tableModel;
}
