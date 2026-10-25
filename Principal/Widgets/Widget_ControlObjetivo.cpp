#include "Widget_ControlObjetivo.h"
#include "ui_Widget_ControlObjetivo.h"
#include "Iconos/Iconos.h"
#include "Vuelos/BaseDatos.h"
#include "header/ColumnHeaderLengthAdapter.h"

#include <QGridLayout>
#include <QHeaderView>
#include <QSqlQuery>
#include <QSqlQueryModel>
#include <QTableView>

namespace {
// Velocidades de reproduccion del deslizador (indice 1 = tiempo real).
const double kFactores[] = {0.5, 1, 2, 4, 8, 16, 32};

// "mm:ss" de unos milisegundos.
QString reloj(qint64 ms)
{
    const qint64 s = qMax<qint64>(0, ms / 1000);
    return QStringLiteral("%1:%2").arg(s / 60, 2, 10, QLatin1Char('0')).arg(s % 60, 2, 10, QLatin1Char('0'));
}
}

// Pestaña de Control de objetivo: elegir dron y vuelo grabado, reproducirlo
// (ReproductorVuelo), ver sus datos en tabla o en graficas.
Widget_ControlObjetivo::Widget_ControlObjetivo(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget_ControlObjetivo),
    m_reproductor(new ReproductorVuelo(this))
{
    ui->setupUi(this);
    ui->pB_Play->setIcon(Iconos::reproducir());
    ui->pB_Stop->setIcon(Iconos::parar());
    ui->pB_Stop->setEnabled(false);
    ui->hS_Velocidad->setRange(0, int(sizeof(kFactores) / sizeof(kFactores[0])) - 1);
    ui->hS_Velocidad->setValue(1);
    ui->hS_Velocidad->setToolTip(tr("Velocidad de reproducción (×1 = tiempo real)"));
    {
        const QSignalBlocker sinSenales(ui->hS_Avanzar);
        ui->hS_Avanzar->setRange(0, 0);
    }
    ui->hS_Avanzar->setToolTip(tr("Ir a un momento del vuelo"));
    on_hS_Velocidad_valueChanged(ui->hS_Velocidad->value());
    actualizarReloj(0);

    // Cada fotograma mueve el deslizador y el reloj (sin volver a saltar).
    connect(m_reproductor, &ReproductorVuelo::fotograma, this, [this](const MuestraVuelo &, qint64 ms) {
        const QSignalBlocker sinSenales(ui->hS_Avanzar);
        ui->hS_Avanzar->setValue(int(ms / 100));
        actualizarReloj(ms);
    });
    // Al acabar se queda en el ultimo momento, con la traza entera; Stop limpia.
    connect(m_reproductor, &ReproductorVuelo::terminado, this, [this]() {
        const QSignalBlocker sinSenales(ui->pB_Play);
        ui->pB_Play->setChecked(false);
    });

    wGraficas = new Widget_Graficas;
    wDatos = new QWidget;
    tV = new QTableView;
    m_modeloDatos = new QSqlQueryModel(this);
    tV->setModel(m_modeloDatos);
    tV->verticalHeader ()->hide ();
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
    auto layoutH = new QHBoxLayout;   // sin padre: entra en layoutV con addLayout
    layoutH->addStretch ();
    layoutH->addWidget (pB_Imprimir);
    layoutH->addWidget (pB_Cerrar);
    layoutV->addWidget (tV);
    layoutV->addLayout (layoutH);
    wDatos->setLayout (layoutV);
    wDatos->setFixedSize (1000,600);
    wDatos->setWindowTitle ("Reporte de vuelo");
    wDatos->setWindowFlags (Qt::FramelessWindowHint);
}

// Libera la interfaz y las ventanas sueltas.
Widget_ControlObjetivo::~Widget_ControlObjetivo()
{
    delete wGraficas;
    delete wDatos;
    delete ui;
}

// El reproductor (CControladora pinta lo que emite).
ReproductorVuelo *Widget_ControlObjetivo::reproductor() const
{
    return m_reproductor;
}

// Dron elegido.
QString Widget_ControlObjetivo::getDronActivo() const
{
    return dronActivo;
}

// Vuelo elegido.
QString Widget_ControlObjetivo::getVuelo() const
{
    return Vuelo;
}

// 0 ala fija, 1 cuadroptero (el del vuelo elegido).
int Widget_ControlObjetivo::tipoDron() const
{
    return m_tipoDron;
}

// Pone la lista de drones sin perder el elegido (si sigue en la lista) y
// vuelve a pedir sus vuelos una sola vez.
void Widget_ControlObjetivo::setDrones(const QStringList &value)
{
    const QString elegido = ui->cB_Drones->currentText ();
    {
        const QSignalBlocker sinSenales(ui->cB_Drones);
        ui->cB_Drones->clear ();
        ui->cB_Drones->addItems (value);
        const int i = ui->cB_Drones->findText (elegido);
        ui->cB_Drones->setCurrentIndex (i >= 0 ? i : 0);
    }
    if (m_enReproduccion && ui->cB_Drones->currentText () == elegido)
        refrescarVuelos ();   // reproduciendo: solo la lista, sin tocar lo que se ve
    else
        on_cB_Drones_currentTextChanged (ui->cB_Drones->currentText ());
}

// Otro dron: su tipo y sus vuelos.
void Widget_ControlObjetivo::on_cB_Drones_currentTextChanged(const QString &arg1)
{
    if (m_enReproduccion)
        on_pB_Stop_clicked ();
    dronActivo = arg1;
    const BaseDatos::Dron dron = BaseDatos::dron (dronActivo);
    m_idDron = dron.id;
    m_tipoDron = dron.tipo == QLatin1String("Ala fija") ? 0 : 1;
    refrescarVuelos ();
}

// Los vuelos grabados del dron. Se elige el mas reciente, salvo durante una
// reproduccion (de Play a Stop, tambien en pausa): entonces se queda el que
// se ve y la lista solo gana los nuevos.
void Widget_ControlObjetivo::refrescarVuelos()
{
    const QString anterior = ui->cB_Vuelos->currentText ();
    const QSignalBlocker sinSenales(ui->cB_Vuelos);
    ui->cB_Vuelos->clear ();
    QSqlQuery q(BaseDatos::drones ());
    q.prepare (QStringLiteral("SELECT DISTINCT Id_Vuelos FROM DatosVuelos WHERE Dron_ID = ? ORDER BY Id_Vuelos"));
    q.addBindValue (m_idDron);
    if (q.exec ())
        while (q.next ())
            ui->cB_Vuelos->addItem (q.value (0).toString ());
    const int previo = ui->cB_Vuelos->findText (anterior);
    if (m_enReproduccion && previo >= 0) {
        ui->cB_Vuelos->setCurrentIndex (previo);
        return;
    }
    ui->cB_Vuelos->setCurrentIndex (ui->cB_Vuelos->count () - 1);
    cargarVuelo (ui->cB_Vuelos->currentText ());
}

// El operador elige otro vuelo.
void Widget_ControlObjetivo::on_cB_Vuelos_currentTextChanged(const QString &arg1)
{
    if (m_enReproduccion)
        on_pB_Stop_clicked ();
    cargarVuelo (arg1);
}

// Lee el vuelo en el reproductor y prepara el deslizador y el reloj.
void Widget_ControlObjetivo::cargarVuelo(const QString &vuelo)
{
    Vuelo = vuelo;
    if (vuelo.isEmpty () || m_idDron < 0)
        m_reproductor->cargar (QVector<MuestraVuelo>(), QVector<PlanGrabado>());
    else
        m_reproductor->cargar (BaseDatos::drones (), m_idDron, vuelo.toInt ());
    {
        const QSignalBlocker sinSenales(ui->hS_Avanzar);   // si no, saltaria y empezaria a reproducir
        ui->hS_Avanzar->setRange (0, int(m_reproductor->duracionMs () / 100));
        ui->hS_Avanzar->setValue (0);
    }
    actualizarReloj (0);
}

// "Reproducir  01:23 / 05:40" en el titulo del grupo.
void Widget_ControlObjetivo::actualizarReloj(qint64 ms)
{
    ui->groupBox->setTitle (tr("Reproducir  %1 / %2").arg (reloj (ms), reloj (m_reproductor->duracionMs ())));
}

// Tabla con todos los datos del vuelo elegido.
void Widget_ControlObjetivo::on_pB_Ver_ControlObjetivo_clicked()
{
    if (Vuelo.isEmpty ())
        return;
    QSqlQuery q(BaseDatos::drones ());
    q.prepare (QStringLiteral("SELECT * FROM DatosVuelos WHERE Dron_ID = ? AND Id_Vuelos = ? ORDER BY Fecha"));
    q.addBindValue (m_idDron);
    q.addBindValue (Vuelo.toInt ());
    q.exec ();
    m_modeloDatos->setQuery (std::move (q));
    wDatos->setWindowTitle (QString("Datos del vuelo %1, del dron %2").arg (Vuelo).arg (dronActivo));
    if (m_modeloDatos->rowCount () > 0)
        wDatos->show ();
}

// Graficas del vuelo elegido, con los datos ya leidos.
void Widget_ControlObjetivo::on_pB_Graficar_clicked()
{
    const QVector<MuestraVuelo> &m = m_reproductor->muestras ();
    if (m.isEmpty ())
        return;
    QVector<double> banqueo, cabeceo, motor, altura, velocidad, tiempo;
    for (int i = 0; i < m.size (); ++i) {
        banqueo.append (m.at (i).banqueo);
        cabeceo.append (m.at (i).cabeceo);
        motor.append (m.at (i).actuadores[2]);
        altura.append (m.at (i).altura);
        velocidad.append (m.at (i).velocidadKmh);
        tiempo.append (m_reproductor->tiempos ().at (i) / 1000.0);
    }
    wGraficas->setBanqueo (banqueo);
    wGraficas->setCabeceo (cabeceo);
    wGraficas->setRPM (motor);
    wGraficas->setAltura (altura);
    wGraficas->setVelocidad (velocidad);
    wGraficas->setTiempo (tiempo);
    wGraficas->pintar ();
    wGraficas->setWindowTitle (QString("Gráficas del vuelo %1, del dron %2").arg (Vuelo).arg (dronActivo));
    wGraficas->setVuelo (Vuelo);
    wGraficas->setDronActivo (dronActivo);
    wGraficas->show ();
}

// Velocidad de reproduccion: x0,5 ... x32.
void Widget_ControlObjetivo::on_hS_Velocidad_valueChanged(int value)
{
    const int n = int(sizeof(kFactores) / sizeof(kFactores[0]));
    const double f = kFactores[qBound(0, value, n - 1)];
    m_reproductor->setVelocidad (f);
    ui->label_4->setText (f < 1.0 ? tr("Velocidad ×½") : tr("Velocidad ×%1").arg (f));
}

// Para, limpia el mapa y vuelve al principio.
void Widget_ControlObjetivo::on_pB_Stop_clicked()
{
    m_reproductor->parar ();
    {
        const QSignalBlocker a(ui->pB_Play), b(ui->hS_Avanzar);
        ui->pB_Play->setChecked (false);
        ui->hS_Avanzar->setValue (0);
    }
    ui->pB_Stop->setEnabled (false);
    actualizarReloj (0);
    m_enReproduccion = false;
    emit limpiarDatoContolObjetivo(dronActivo);
    emit detenerDatoContolObjetivo ();
}

// Play / pausa. La primera vez limpia el mapa y avisa de que empieza.
void Widget_ControlObjetivo::on_pB_Play_toggled(bool checked)
{
    if (!checked) {
        m_reproductor->pausar ();
        return;
    }
    if (m_reproductor->muestras ().isEmpty ()) {
        const QSignalBlocker sinSenal(ui->pB_Play);
        ui->pB_Play->setChecked (false);
        return;
    }
    if (!m_enReproduccion) {
        m_enReproduccion = true;
        ui->pB_Stop->setEnabled (true);
        emit limpiarDatoContolObjetivo(dronActivo);
        emit iniciarDatoContolObjetivo ();
    }
    m_reproductor->reproducir ();
}

// Ir a otro momento del vuelo (arrastrando o pinchando en el deslizador).
void Widget_ControlObjetivo::on_hS_Avanzar_valueChanged(int value)
{
    if (!m_enReproduccion) {
        m_enReproduccion = true;
        ui->pB_Stop->setEnabled (true);
        emit limpiarDatoContolObjetivo(dronActivo);
        emit iniciarDatoContolObjetivo ();
    }
    m_reproductor->irA (qint64(value) * 100);
}

// Ventana de graficas (la usa tambien la estacion en vivo).
Widget_Graficas *Widget_ControlObjetivo::getWGraficas() const
{
    return wGraficas;
}
