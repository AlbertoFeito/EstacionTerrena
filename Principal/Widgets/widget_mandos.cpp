#include "widget_mandos.h"
#include "ui_widget_mandos.h"

#include <QDir>
#include <QFormLayout>
#include <QScrollArea>
#include <QTimer>
#include <QVBoxLayout>

#include "Protocolo/ProyeccionMercator.h"
#include "Protocolo/Tramas.h"
#include "Rutas.h"

Widget_Mandos::Widget_Mandos(sMANDOS *MANDOS, QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget_Mandos),
    mandos(MANDOS)

{
    ui->setupUi(this);
    //QSettings
    fileName = Rutas::confIni();
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
    mandos->Regimen = regimen;
    mandos->Comando_Mision = comando;

    mandos->Radio_Acercamiento = ui->dsB_Radio_Acercamiento->value ();
    mandos->Distancia_Encuentro = ui->dsB_Distancia_Encuentro->value ();

    mandos->Paracaidas = paracaidas;

    double lat = ui->dsB_Home1->value ();
    double lon = ui->dsB_Home2->value ();
    auto p = ProyeccionMercator::aMetros(QGeoCoordinate(lat,lon));
    mandos->Home[0] = p.y ();
    mandos->Home[1] = p.x ();
    mandos->Home[2] = ui->dsB_Home3->value ();

    mandos->Velocidad_RTH = ui->dsB_Velocidad_RTH->value () / 3.6;

    // La trama sale de la estructura (antes se copiaban los bytes del puntero).
    emit enviaMandosDron(Tramas::mandos (*mandos));
MyQSetting settings;
    settings.guardarSetting (fileName,"Radio_Acercamiento",ui->dsB_Radio_Acercamiento->value (),"MANDOS");
    settings.guardarSetting (fileName,"Distancia_Encuentro",ui->dsB_Distancia_Encuentro->value (),"MANDOS");
    // En m/s, que es como se lee al arrancar (antes se guardaba en km/h y al
    // leerlo se multiplicaba otra vez por 3,6: crecia en cada arranque).
    settings.guardarSetting (fileName,"Velocidad_RTH",ui->dsB_Velocidad_RTH->value () / 3.6,"MANDOS");
    settings.guardarSetting (fileName,"Altura_Casa",ui->dsB_Home3->value (),"MANDOS");
}

// Ultima posicion del dron, para el boton "Usar posicion actual del dron como
// casa" (que solo se activa cuando hay una).
void Widget_Mandos::setPosicionDron(const QGeoCoordinate &pos)
{
    m_posicionDron = pos;
    ui->pB_CasaDron->setEnabled (pos.isValid ());
}

// Lo que dice la telemetria: regimen y mision en los que esta el dron. Se ve
// siempre arriba ("Dron: Automático · Puntos") y, si no coincide con lo elegido
// en la pestaña, en ambar: el dron sigue en lo suyo hasta que se pulse Enviar.
// Con \a adoptar (la primera trama tras conectar) la pestaña pasa a mostrar lo
// del dron: si el simulador o el autopiloto se quedaron en automatico de antes,
// la pestaña no puede decir "Manual" mientras el dron sigue la ruta.
void Widget_Mandos::setEstadoDron(int regimen, int mision, bool adoptar)
{
    static const char *regimenes[] = {"Manual", "Estabilizado", "Automático"};
    static const char *misiones[] = {"Puntos", "Circular", "Repetir misión", "RTL"};
    if (regimen < 0 || regimen > 2 || mision < 0 || mision > 3)
        return;
    if (adoptar) {
        QAbstractButton *botonesRegimen[] = {ui->rB_M, ui->rB_SA, ui->rB_A};
        QAbstractButton *botonesMision[] = {ui->rB_Defoult, ui->rB_Circular,
                                            ui->rB_Repetir_Mision, ui->rB_RTL};
        botonesRegimen[regimen]->setChecked (true);
        botonesMision[mision]->setChecked (true);
    }
    const bool distinto = regimen != this->regimen || mision != comando;
    if (regimen == m_regimenDron && mision == m_misionDron
        && ui->lEstadoDron->property ("distinto").toBool () == distinto)
        return;   // nada que cambiar (llegan 10 tramas por segundo)
    m_regimenDron = regimen;
    m_misionDron = mision;
    ui->lEstadoDron->setText (tr("Dron: %1 · %2").arg (QString::fromUtf8 (regimenes[regimen]),
                                                      QString::fromUtf8 (misiones[mision])));
    ui->lEstadoDron->setProperty ("distinto", distinto);
    ui->lEstadoDron->setStyleSheet (distinto ? QStringLiteral ("color: #ffb300; font-weight: bold;")
                                             : QStringLiteral ("color: #9e9e9e;"));
    ui->lEstadoDron->setToolTip (distinto
        ? tr("El dron sigue en esto: lo elegido abajo no le llega hasta pulsar «Enviar mandos»")
        : tr("Régimen y misión en los que está el dron ahora (lo que dice su telemetría)"));
}

// Al desconectar: ya no se sabe en que esta el dron.
void Widget_Mandos::olvidarEstadoDron()
{
    m_regimenDron = m_misionDron = -1;
    ui->lEstadoDron->setText (tr("Dron: sin datos"));
    ui->lEstadoDron->setProperty ("distinto", false);
    ui->lEstadoDron->setStyleSheet (QStringLiteral ("color: #9e9e9e;"));
}

// Texto del estado del dron (para pruebas).
QString Widget_Mandos::textoEstadoDron() const
{
    return ui->lEstadoDron->text ();
}

// Pone la ultima posicion del dron en la casa; se le manda con Enviar.
void Widget_Mandos::on_pB_CasaDron_clicked()
{
    if (!m_posicionDron.isValid ())
        return;
    ui->dsB_Home1->setValue (m_posicionDron.latitude ());
    ui->dsB_Home2->setValue (m_posicionDron.longitude ());
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



// Aspecto de la pestaña: el regimen y la mision son botones grandes que se
// encienden con el color del avion (rojo, ambar, verde) o en azul; Enviar va
// abajo a todo lo ancho. Carga los valores guardados en conf.ini.
void Widget_Mandos::configuraUI()
{
    setStyleSheet (QStringLiteral (
        "QLabel[rol=\"seccion\"] { color: #9e9e9e; font-size: 10px; font-weight: bold; padding-top: 4px; }"
        "QPushButton[rol] { min-height: 30px; padding: 0 2px; font-weight: bold; }"
        "QPushButton[rol=\"manual\"]:checked { background-color: #c62828; color: white; }"
        "QPushButton[rol=\"estabilizado\"]:checked { background-color: #ffb300; color: black; }"
        "QPushButton[rol=\"automatico\"]:checked { background-color: #2e7d32; color: white; }"
        "QPushButton[rol=\"mision\"]:checked { background-color: #1565c0; color: white; }"
        "QCheckBox#chB_Paracaidas:checked { color: #ff5252; font-weight: bold; }"
        "QPushButton#pB_Enviar { min-height: 38px; font-size: 14px; font-weight: bold; }"));

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
