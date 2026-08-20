#include "widget_mandos.h"
#include "ui_widget_mandos.h"

#include <QDir>
#include <QFormLayout>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

Widget_Mandos::Widget_Mandos(CProjection *projection, sMANDOS *MANDOS, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget_Mandos),
    mandos(MANDOS),
    m_projection(projection)

{
    ui->setupUi(this);
    //QSettings
    fileName = QDir::currentPath() + "/conf.ini";
    configuraUI();
    //    qDebug()<<"LittleEndian" <<(QSysInfo::ByteOrder == QSysInfo::LittleEndian);

    m_originalStyle = ui->pB_Enviar->styleSheet(); // Guardar estilo inicial
    m_timer.setSingleShot(true);
    connect(&m_timer, &QTimer::timeout, this, &Widget_Mandos::on_restoreStyle);
}

Widget_Mandos::~Widget_Mandos()
{
    delete ui;
}

void Widget_Mandos::on_pB_Enviar_clicked()
{
    QByteArray sendMando;
    sendMando.append ("xyz");

    mandos->Regimen = regimen;
    mandos->Comando_Mision = comando;

    mandos->Radio_Acercamiento = ui->dsB_Radio_Acercamiento->value ();
    mandos->Distancia_Encuentro = ui->dsB_Distancia_Encuentro->value ();

    mandos->Paracaidas = paracaidas;

    double lat = ui->dsB_Home1->value ();
    double lon = ui->dsB_Home2->value ();
    auto p = m_projection->forward(QGeoCoordinate(lat,lon));
    mandos->Home[0] = p.y ();
    mandos->Home[1] = p.x ();
    mandos->Home[2] = ui->dsB_Home3->value ();

    mandos->Velocidad_RTH = ui->dsB_Velocidad_RTH->value () / 3.6;

    sendMando.append (reinterpret_cast<const char*>(&mandos),sizeof (sMANDOS));

    emit enviaMandosDron(sendMando);
MyQSetting settings;
    settings.guardarSetting (fileName,"Radio_Acercamiento",ui->dsB_Radio_Acercamiento->value (),"MANDOS");
    settings.guardarSetting (fileName,"Distancia_Encuentro",ui->dsB_Distancia_Encuentro->value (),"MANDOS");
    settings.guardarSetting (fileName,"Velocidad_RTH",ui->dsB_Velocidad_RTH->value (),"MANDOS");
    settings.guardarSetting (fileName,"Altura_Casa",ui->dsB_Home3->value (),"MANDOS");
}

void Widget_Mandos::on_rB_M_toggled(bool checked)
{
    if(checked)
        regimen = 0;
}

void Widget_Mandos::on_rB_SA_toggled(bool checked)
{
    if(checked)
        regimen = 1;
}

void Widget_Mandos::on_rB_A_toggled(bool checked)
{
    if(checked)
        regimen = 2;
}

void Widget_Mandos::on_rB_Defoult_toggled(bool checked)
{
    if(checked)
        comando = 0;
}

void Widget_Mandos::on_rB_Circular_toggled(bool checked)
{
    if(checked)
        comando = 1;
}

void Widget_Mandos::on_rB_Repetir_Mision_toggled(bool checked)
{
    if(checked)
        comando = 2;
}

void Widget_Mandos::on_rB_RTL_toggled(bool checked)
{
    if(checked)
        comando = 3;
}



void Widget_Mandos::configuraUI()
{
    // Botón enviar
    ui->pB_Enviar->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    // Políticas de tamaño para elementos del formulario
    QSizePolicy spinPolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    ui->dsB_Radio_Acercamiento->setSizePolicy(spinPolicy);
    ui->dsB_Distancia_Encuentro->setSizePolicy(spinPolicy);
    ui->dsB_Velocidad_RTH->setSizePolicy(spinPolicy);

    ui->dsB_Radio_Acercamiento->setDecimals(2);
    ui->dsB_Distancia_Encuentro->setDecimals(2);
    ui->dsB_Velocidad_RTH->setDecimals(2);

    m_dsB_Lat = ui->dsB_Home1;
    m_dsB_Lon = ui->dsB_Home2;

    ui->dsB_Radio_Acercamiento->setValue (settings.cargarSetting(fileName,"Radio_Acercamiento","MANDOS",20).value<double>());
    ui->dsB_Distancia_Encuentro->setValue (settings.cargarSetting(fileName,"Distancia_Encuentro","MANDOS",25).value<double>());
    ui->dsB_Velocidad_RTH->setValue (settings.cargarSetting(fileName,"Velocidad_RTH","MANDOS",20).value<double>() *3.6);
    ui->dsB_Home3->setValue (settings.cargarSetting(fileName,"Altura_Casa","MANDOS",100).value<double>());

}

QDoubleSpinBox *Widget_Mandos::getDsB_Lat() const
{
    return m_dsB_Lat;
}

void Widget_Mandos::on_AcuseRecibo()
{
    if(!m_timer.isActive()) {  // Solo guardar estilo si no hay timer activo
        m_originalStyle = ui->pB_Enviar->styleSheet();
    }

    ui->pB_Enviar->setStyleSheet("background-color: #00FF00; color: black;");
    m_timer.start(1000);
}
void Widget_Mandos::on_restoreStyle() {
    ui->pB_Enviar->setStyleSheet(m_originalStyle);
}
QDoubleSpinBox *Widget_Mandos::getDsB_Lon() const
{
    return m_dsB_Lon;
}


quint16 Widget_Mandos::calculateChecksum(const QByteArray &data) {
    quint16 checksum = 0;
    for (char byte : data) {
        checksum += static_cast<quint8>(byte);
    }
    return checksum;
}



void Widget_Mandos::on_chB_Paracaidas_toggled(bool checked)
{
    paracaidas = checked ? 1 : 0;

}
