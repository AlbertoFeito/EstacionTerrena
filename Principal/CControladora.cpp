#include "CControladora.h"
#include "Iconos/Iconos.h"
#include "Vuelos/BaseDatos.h"
#include "Vuelos/GrabadorVuelo.h"
#include "Vuelos/ReproductorVuelo.h"

#include <cmath>
#include <QAction>
#include <QMenuBar>
#include <QCoreApplication>
#include <QDir>
#include <QScrollArea>
#include <QScrollBar>
#include <logger/LogHandler.h>
#include <libmapa/MapWidget.h>   // solo para usar MapaMision::mapa() como QWidget
#include <QMessageBox>
#include <QStatusBar>
#include "MyQSettings/myqsetting.h"
#include "Rutas.h"

CControladora::CControladora(QObject *parent) :
    QObject(parent),
    alturaWorkerPTR(std::make_unique<AlturaWorker>(parent)),
    m_esperandoPuntoInicial(false)

{
    initPrincipal();
}

void CControladora::initPrincipal()
{
    mPrincipal = new Ci_Principal();

    createActions ();
    createMenus ();
    createToolBar();

indicador = new qfi_EADI(mPrincipal);
    indicador->setFixedSize (300,300);
    indicador->setEnabled (false);

    indicador->setVerticalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
    indicador->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);

tabOpciones = new QTabWidget(mPrincipal);
    tabOpciones->setFixedWidth (300);
    // Pestanas a la izquierda, en vertical: con nombres cortos caben todas y
    // enteras (arriba no cabian en 300 px y salian cortadas con flechas).
    tabOpciones->setTabPosition (QTabWidget::West);
    tabOpciones->setTabShape (QTabWidget::Rounded);


    crearMapa();
    crearPanelCapas();
    // Los paneles de abajo y de la derecha cambian el tamano del mapa sin
    // cambiar el de la ventana: los overlays se recolocan con el mapa.
    m_mapa->mapa ()->installEventFilter (this);
    initTabOpciones();
    preparaDataBase();
    m_wConexion->cBDrones ()->setCurrentIndex (0);


    m_wGraficas = new Widget_Graficas(m_mapa->mapa ());
    m_wGraficas->raise();
    m_wGraficas->setVisible (false);

    // Panel de vuelo con el detalle tecnico (ganancias, actuadores) plegado
    // debajo: un solo panel en lugar de dos.
    m_panelVuelo = new PanelVuelo(m_mapa->mapa ());
    m_panelVuelo->raise();
    m_panelVuelo->adjustSize();
    connect (m_panelVuelo, &PanelVuelo::detalleConmutado, this, [this](bool abierto) {
        aDetalleTecnico->setChecked (abierto);
        updateOverlayPosition ();
    });
    loadInitialData ();

    auto vLine = new QFrame();
    vLine->setFrameShape (QFrame::VLine);

    auto hLine = new QFrame();
    hLine->setFrameShape (QFrame::HLine);

    QGridLayout *gridLayout = new QGridLayout;
    gridLayout->addWidget (indicador,0,0);
    gridLayout->addWidget (hLine,1,0);
    gridLayout->addWidget (tabOpciones,2,0);

    gridLayout->addWidget (vLine,0,1,3,1);
    gridLayout->addWidget (m_mapa->mapa (),0,2,3,1);

    gridLayout->setHorizontalSpacing (3);
    gridLayout->setVerticalSpacing (3);
    gridLayout->setContentsMargins(0, 0, 0, 0);

    mPrincipal->centralWidget ()->setLayout (gridLayout);




    // Crear widget de puntos con el worker de alturas
    m_wPuntos = new Widget_Puntos(alturaWorkerPTR.get(), mPrincipal);
    crearPanelRutas();

    actualizarMenuRutas();
    conecciones ();

    mPrincipal->setWindowTitle ("ECT v.1.2 - Estación de Control Terrestre (CID3)");
    mPrincipal->showMaximized ();
    QTimer::singleShot (0, this, [this]() { updateOverlayPosition(); });
}

bool CControladora::eventFilter(QObject *obj, QEvent *event)
{
    if (m_mapa && obj == m_mapa->mapa () && event->type () == QEvent::Resize)
        updateOverlayPosition ();
    return QObject::eventFilter(obj, event);
}

// Detalle tecnico del panel de vuelo a partir del vector de siempre: 6 rumbo
// de la IMU, 8..18 ganancias, 19..22 actuadores, 23..24 convergencias. El
// cuadroptero no manda ganancias: se quedan sin dato. Tambien da al boton
// "Usar posicion actual del dron como casa" la ultima posicion.
void CControladora::updateParameters(const QVector<double> &values) {
    const bool cuadro = m_detalle.tipoDron != 0;
    m_detalle.rumboImu = values[6];
    for (int i = 0; i < 11; ++i)
        m_detalle.ganancias[i] = cuadro ? std::numeric_limits<double>::quiet_NaN () : values[8 + i];
    for (int i = 0; i < 4; ++i)
        m_detalle.actuadores[i] = values[19 + i];
    m_detalle.convergenciaBanqueo = values[23];
    m_detalle.convergenciaCabeceo = values[24];
    m_panelVuelo->mostrarDetalle (m_detalle);
    m_wMandos->setPosicionDron (geoPos);
}

void CControladora::initTabOpciones()
{
QWidget *w1 = new QWidget();
    QGridLayout *LayOutDrones = new QGridLayout;
    m_wDrones = new Widget_Drones();
    LayOutDrones->addWidget (m_wDrones);
    LayOutDrones->setVerticalSpacing (0);
    LayOutDrones->setHorizontalSpacing (0);
    w1->setLayout (LayOutDrones);


QWidget *w2 = new QWidget();
    QGridLayout *LayOutCObjetivo = new QGridLayout;
    m_wControlObjetivo = new Widget_ControlObjetivo();
    LayOutCObjetivo->addWidget (m_wControlObjetivo);
    w2->setLayout (LayOutCObjetivo);


    QWidget *w3 = new QWidget();
    QGridLayout *LayOutMandos = new QGridLayout;
    m_wMandos = new Widget_Mandos(&mandos);
    m_wMandosCuadroptero = new Widget_Mandos_Cuadroptero();
    LayOutMandos->setContentsMargins (0, 0, 0, 0);
    LayOutMandos->addWidget (m_wMandos);
    LayOutMandos->addWidget (m_wMandosCuadroptero);
    w3->setLayout (LayOutMandos);

QWidget *w4 = new QWidget();
    QGridLayout *LayOutGanancias = new QGridLayout;
    m_wGanancias = new Widget_Ganancias_Control(&mandos);
    LayOutGanancias->addWidget (m_wGanancias);
    w4->setLayout (LayOutGanancias);

    QWidget *w5 = new QWidget();
    QGridLayout *LayOutConexion = new QGridLayout;
    m_wConexion = new Widget_Conexion();
    LayOutConexion->addWidget (m_wConexion);
    w5->setLayout (LayOutConexion);

    const struct { QWidget *pagina; const char *nombre; const char *ayuda; } pestanas[] = {
        {w5, "Enlace", "Configuración: dron, enlace y conexión"},
        {w1, "Drones", "Datos de los drones"},
        {w3, "Mandos", "Régimen, misión, navegación y casa"},
        {w4, "Ganancias", "Ganancias del control"},
        {w2, "Objetivo", "Control de objetivo"},
    };
    // Cada pagina, con barra de desplazamiento si no cabe: sin ella la pagina
    // mas alta obligaba a la ventana a medir 800 px de alto y, en una pantalla
    // de 768 (o con la escala de Windows al 125-150 %), no se podia maximizar.
    for (const auto &p : pestanas) {
        auto *desplazable = new QScrollArea;
        desplazable->setWidget (p.pagina);
        desplazable->setWidgetResizable (true);
        desplazable->setFrameShape (QFrame::NoFrame);
        desplazable->setHorizontalScrollBarPolicy (Qt::ScrollBarAlwaysOff);
        const int i = tabOpciones->addTab (desplazable, tr(p.nombre));
        tabOpciones->setTabToolTip (i, tr(p.ayuda));
    }

    mPrincipal->installEventFilter(this);
}

void CControladora::preparaDataBase()
{
    BaseDatos::asegurarTablaDrones ();

    // La tabla de vuelos (y su indice) la crea el grabador, que es quien escribe en ella.
    m_grabador = new GrabadorVuelo(BaseDatos::fichero (), this);
    if (!m_grabador->asegurarTabla ())
        LogHandler::getInstance ().appendLogMessage ("Vuelos: " + m_grabador->error ());
    m_wConexion->cBDrones ()->addItems (BaseDatos::nombresDrones ());
    m_wConexion->cBDrones ()->setCurrentIndex(0);
    DronID = m_wConexion->cBDrones ()->currentText ();
}



void CControladora::Conectado(bool conectado)
{
    primeraCoordenada = conectado;
    m_posicionValida = -1;   // en cada conexion se vuelve a decir si la posicion es valida
    m_wConexion->chB_Guardar ()->setChecked (false);
    m_Conectado = conectado;
    const QString descripcionEnlace = m_monitor.descripcion ();
    const bool estabaConectado = m_monitor.msConectado (0) != -1;
    m_monitor.setConectado (conectado, m_wConexion->serialManager ()->currentPort (),
                            m_reloj.isValid () ? m_reloj.elapsed () : 0);
    // Cuanto duro el enlace queda en la ventana del log, en log.txt y en la
    // barra de estado.
    if (!conectado && estabaConectado) {
        const QString nota = "Enlace cerrado (" + descripcionEnlace + ") tras "
            + MonitorEnlace::formatoDuracion (m_monitor.duracionUltima ()) + " conectado";
        LogHandler::getInstance ().appendLogMessage (nota);
        qInfo ().noquote () << nota;
    }
    if (!conectado && m_panelVuelo) {
        m_panelVuelo->limpiar ();
        m_puntoActual = -1;
    }
    // La pestaña de Mandos adopta lo que diga la primera trama de este enlace.
    m_estadoDronAdoptado = false;
    if (!conectado && m_wMandos)
        m_wMandos->olvidarEstadoDron ();
    if (!conectado)
        refrescarControlObjetivo ();
    if (m_lEnlace)
        refrescarIndicadorEnlace ();
    if (m_wPuntos)
        m_wPuntos->setEnlaceDisponible (conectado);
    if (!conectado && m_esperaAcusePlan) {
        m_esperaAcusePlan->stop ();
        m_esperaAcuseMandos->stop ();
    }
    if(m_wConexion->cBDrones ()->currentIndex () != -1)
    {
        m_wConexion->cBDrones ()->setEnabled (!conectado);
        m_wConexion->chB_Guardar ()->setEnabled (conectado);
        if(conectado)
        {

            //            m_wEstado->setEnabled (true);
            start_time_ = QTime::currentTime ();
            m_wGraficas->getLEdron ()->setVisible (false);
            m_wGraficas->getLEvuelo ()->setVisible (false);
            m_wGraficas->getMpBCerrar ()->setVisible (false);
            const BaseDatos::Dron dron = BaseDatos::dron (DronID);
            dronID = dron.id;
            tipoDrone = dron.tipo == "Ala fija" ? 0 : 1;
            vueloID = dron.vuelos;
            m_wConexion->serialManager ()->setTipo_dron (tipoDrone);
            m_detalle.tipoDron = tipoDrone;
        }
        else
        {
            loadInitialData();

            indicador->setRoll (0);
            indicador->setPitch (0);
            indicador->setHeading (0);
            indicador->setAirspeed (0);
            indicador->setAltitude (0);
            indicador->redraw ();

            limpiarVueloEnMapa();



        }
    }

}

void CControladora::createMenus()
{
    mMenu = mPrincipal->menuBar ()->addMenu (tr("&Menu"));
    mMenu->addAction (aCerrar);

    mCapas = mPrincipal->menuBar ()->addMenu (tr("&Capas"));   // lo llena actualizarMenuCapas

    mTema = mPrincipal->menuBar ()->addMenu (tr("&Tema"));
    mTema->addAction (aClaro);
    mTema->addAction (aOscuro);

    mEstadoDrone = mPrincipal->menuBar ()->addMenu (tr("&Estado_Drone"));
    mEstadoDrone->addAction (aEstadoDrone);
    mEstadoDrone->addAction (aDetalleTecnico);
    mEstadoDrone->addAction (aVerGraficaDrone);

    mRuta = mPrincipal->menuBar()->addMenu(tr("&Rutas"));
    mRuta->addAction(aRuta);
    mRuta->addAction(aGestionarDrones);
    mRuta->addSeparator();

    // --- NUEVO: Submenú con QActionGroup exclusivo ---
    mMenuMostrarRutas = mRuta->addMenu(tr("Mostrar ruta"));
    m_rutasActionGroup = new QActionGroup(this);
    m_rutasActionGroup->setExclusive(true);   
    mRuta->addSeparator();

    mLog = mPrincipal->menuBar ()->addMenu (tr("&Logger"));
    mLog->addAction (aLogger);
}

void CControladora::createActions()
{
    aCerrar = new QAction(tr("&Cerrar"), mPrincipal);
    aCerrar->setShortcuts(QKeySequence::Quit);
    aCerrar->setStatusTip(tr("Cierra la aplicacion"));
    connect(aCerrar, &QAction::triggered, [=]()
    {
        QApplication::closeAllWindows ();
    });


    // Las capas base salen del paquete: sus acciones las pone actualizarMenuCapas.
    aG_Capas= new QActionGroup(mPrincipal);

    aClaro = new QAction(tr("&Claro"),mPrincipal);
    aClaro->setShortcut (QKeySequence(tr("Ctrl+T+C")));
    aClaro->setStatusTip(tr("Seleciona tema Claro"));
    aClaro->setCheckable (true);
    aClaro->setChecked (false);
    connect (aClaro,&QAction::triggered,[this]()
    {
        mPrincipal->ponerTema (true);
    });
    aOscuro = new QAction(tr("&Oscuro"),mPrincipal);
    aOscuro->setShortcut (QKeySequence(tr("Ctrl+T+O")));
    aOscuro->setStatusTip(tr("Seleciona tema Oscuro"));
    aOscuro->setCheckable (true);
    aOscuro->setChecked (true);
    connect (aOscuro,&QAction::triggered,[this]()
    {
        mPrincipal->ponerTema (false);
    });

    aG_Tema= new QActionGroup(mPrincipal);
    aG_Tema->addAction (aClaro);
    aG_Tema->addAction (aOscuro);

    aEstadoDrone = new QAction(tr("&Datos de vuelo"),mPrincipal);
    aEstadoDrone->setShortcut (QKeySequence(tr("Ctrl+E+D")));
    aEstadoDrone->setStatusTip(tr("Ver estado del drone activo"));
    aEstadoDrone->setCheckable (true);
    aEstadoDrone->setChecked (true);

    aVerGraficaDrone = new QAction(tr("&Gráficas"),mPrincipal);
    aVerGraficaDrone->setShortcut (QKeySequence(tr("Ctrl+E+G")));
    aVerGraficaDrone->setStatusTip(tr("Ver gráficas del drone activo"));
    aVerGraficaDrone->setCheckable (true);
    aVerGraficaDrone->setChecked (false);


    // Muestra u oculta el panel de rutas de abajo (se engancha a su
    // QDockWidget en crearPanelRutas).
    aRuta = new QAction(tr("Panel de &rutas"), mPrincipal);
    aRuta->setShortcut(QKeySequence(tr("Ctrl+R")));
    aRuta->setStatusTip(tr("Muestra u oculta el panel de rutas, abajo"));
    aRuta->setCheckable(true);

    aGestionarDrones = new QAction(tr("&Gestionar Drones..."), this);
    aGestionarDrones->setStatusTip(tr("Abrir el gestor de drones"));
    connect(aGestionarDrones, &QAction::triggered, [this]() {
        DroneDialog dialog(DroneDialog::MANAGE_DRONES, mPrincipal);
        dialog.exec();
        // Actualizar combos si es necesario (se hace en Widget_Puntos al abrirse)
    });

    aDetalleTecnico = new QAction(tr("Detalle &técnico (ganancias y actuadores)"), mPrincipal);
    aDetalleTecnico->setStatusTip(tr("Ver todos los datos del autopiloto"));
    aDetalleTecnico->setCheckable (true);
    aDetalleTecnico->setChecked (false);

    // Datos de vuelo y detalle tecnico se pueden ver a la vez; las graficas
    // ocupan su sitio, asi que los esconden.
    connect (aEstadoDrone,&QAction::triggered,[this]()
    {
        m_panelVuelo->setVisible (aEstadoDrone->isChecked ());
        m_wGraficas->setVisible (false);
        aVerGraficaDrone->setChecked (false);
        updateOverlayPosition();
    });

    // El detalle va dentro del panel de vuelo: abrirlo lo muestra tambien.
    connect (aDetalleTecnico,&QAction::triggered,[this]()
    {
        m_panelVuelo->setDetalleVisible (aDetalleTecnico->isChecked ());
        if (aDetalleTecnico->isChecked ()) {
            m_panelVuelo->setVisible (true);
            aEstadoDrone->setChecked (true);
        }
        m_wGraficas->setVisible (false);
        aVerGraficaDrone->setChecked (false);
        updateOverlayPosition();
    });

    connect (aVerGraficaDrone,&QAction::triggered,[this]()
    {
        m_wGraficas->setVisible (aVerGraficaDrone->isChecked ());
        m_panelVuelo->setVisible (false);
        aEstadoDrone->setChecked (false);
    });

    aLogger = new QAction(tr("&Logger"), mPrincipal);
    aLogger->setStatusTip(tr("Logger de la app"));

    connect(aLogger, &QAction::triggered, [=](){
        LogHandler& logHandler = LogHandler::getInstance();
        logHandler.appendLogMessage("Visibilidad");
        logHandler.setLogHandlerVisible (logHandler.isHidden ());
    });
}

void CControladora::createToolBar()
{
    toolBar = mPrincipal->addToolBar ("Barra de Herramientas");
    toolBar->setMovable (false);

    aDistancia =  toolBar->addAction (Iconos::regla(),"Distancia");
    aDistancia->setCheckable (true);
    aDistancia->setChecked (false);
    toolBar->addSeparator ();

    aLupa =  toolBar->addAction (Iconos::lupaArea(),"ZoomArea");
    aLupa->setCheckable (true);
    aLupa->setChecked (false);
    aLupaMas =  toolBar->addAction (Iconos::lupaMas(),"ZoomMas");
    aLupaMen =  toolBar->addAction (Iconos::lupaMenos(),"ZoomMen");
    toolBar->addSeparator ();

    aPoligono =  toolBar->addAction (Iconos::poligono(),"Polígono");

}

void CControladora::conecciones()
{
    // Cada pestaña se reconoce por el widget que contiene, no por su texto: al
    // acortar los nombres de las pestañas, las comparaciones con el texto
    // dejaron de cumplirse (p. ej. Control de objetivo no cargaba los drones).
    connect (tabOpciones,&QTabWidget::currentChanged, [this](int index)
    {
        QWidget *pagina = tabOpciones->widget (index);
        auto es = [pagina](QWidget *w) { return pagina && w && pagina->isAncestorOf (w); };
        if(es (m_wConexion))
        {
            const QStringList nombres = BaseDatos::nombresDrones ();
            for (const QString &str : nombres)
                if(m_wConexion->cBDrones ()->findText (str) < 0)
                    m_wConexion->cBDrones ()->addItem(str);
            if (!nombres.isEmpty ()) {
                if(DronID.isEmpty ())
                    m_wConexion->cBDrones ()->setCurrentIndex(0);
                else
                    m_wConexion->cBDrones ()->setCurrentIndex(m_wConexion->cBDrones ()->findText (DronID));
                m_wConexion->setEnabled (true);
            }
            else
            {
                m_wConexion->setEnabled (false);
            }
        }

        if(es (m_wControlObjetivo))
        {
            m_wControlObjetivo->setEnabled (!m_Conectado);

            refrescarControlObjetivo ();


        }

        if(es (m_wDrones))
        {
            m_wDrones->setEnabled (!m_Conectado);
            m_wDrones->actualizaTabla ();
        }

        if(es (m_wMandos))
        {
            m_wMandos->setEnabled (m_Conectado);
            m_wMandosCuadroptero->hide ();
            if(m_Conectado)
                switch (tipoDrone) {
                case 0:
                    m_wMandos->show ();

                    m_wMandosCuadroptero->hide ();
                    break;
                case 1:
                    m_wMandosCuadroptero->show ();

                    m_wMandos->hide ();
                    break;
                default:
                    break;
                }
        }

        if(es (m_wGanancias))
        {
            m_wGanancias->setEnabled (m_Conectado);
            m_wMandosCuadroptero->hide ();
            if(m_Conectado)
                switch (tipoDrone) {
                case 0:
                    m_wGanancias->show ();

                    m_wMandosCuadroptero->hide ();
                    break;
                case 1:
                    m_wMandosCuadroptero->show ();

                    m_wGanancias->hide ();
                    break;
                default:
                    break;
                }
        }

    });

    connect(m_wConexion->cBDrones (), &QComboBox::currentTextChanged,[=](QString text)
    {
        DronID = text;
    });

    // Grabar el vuelo: el grabador da de alta el vuelo en el momento (antes se
    // pedia el numero de vuelo de forma asincrona y las primeras tramas podian
    // ir al vuelo anterior).
    connect(m_wConexion->chB_Guardar (), &QCheckBox::toggled,[=](bool checked)
    {
        if (!checked) {
            m_grabador->terminar ();
            refrescarControlObjetivo ();   // el vuelo recien grabado ya sale en la lista
            return;
        }
        if (!m_grabador->empezar (DronID)) {
            LogHandler::getInstance ().appendLogMessage ("Grabar vuelo: " + m_grabador->error ());
            mPrincipal->statusBar ()->showMessage (tr("⚠ No se puede grabar el vuelo: %1")
                                                       .arg (m_grabador->error ()), 8000);
            const QSignalBlocker sinSenal(m_wConexion->chB_Guardar ());
            m_wConexion->chB_Guardar ()->setChecked (false);
            return;
        }
        dronID = m_grabador->dron ();
        vueloID = m_grabador->vuelo ();
        // El plan que el dron ya tenia (confirmado en esta sesion) va con el vuelo.
        if (!m_planEnDron.isEmpty ())
            m_grabador->anotarPlan (planConfirmado ());
        mPrincipal->statusBar ()->showMessage (tr("Grabando el vuelo %1 de %2").arg (vueloID).arg (DronID), 5000);
    });

    // Reproduccion de un vuelo grabado: cada fotograma y el plan de cada momento.
    connect (m_wControlObjetivo->reproductor (), &ReproductorVuelo::fotograma,
             this, &CControladora::mostrarMuestraReproducida);
    connect (m_wControlObjetivo->reproductor (), &ReproductorVuelo::planCambiado,
             this, &CControladora::pintarPlanReproducido);

    connect (m_wControlObjetivo,&Widget_ControlObjetivo::limpiarDatoContolObjetivo,this,&CControladora::on_EliminarPunto);

    connect (m_wControlObjetivo,&Widget_ControlObjetivo::detenerDatoContolObjetivo ,this,&CControladora::on_DetenerCO );

    connect (m_wControlObjetivo,&Widget_ControlObjetivo::iniciarDatoContolObjetivo ,this,&CControladora::on_IniciarCO);

    connect (m_wConexion,&Widget_Conexion::conectado, this,&CControladora::Conectado );

    crearEsperasDeAcuse();
    crearIndicadorEnlace();
    connect (m_wMandos,&Widget_Mandos::enviaMandosDron, this, &CControladora::enviarMandos);
    connect (m_wGanancias,&Widget_Ganancias_Control::enviaMandosDron, this, &CControladora::enviarMandos);

    // Clic derecho en el mapa
    connect(m_mapa, &MapaMision::clicDerecho,
            [this](const QGeoCoordinate &puntoG) {
        if (m_esperandoPuntoInicial) {
            if (m_wPuntos) {
                m_wPuntos->agregarPuntoInicialYGenerarResto(puntoG);
            }
        }
    });

    // El dron rechazo el plan (acuse "uvw" negativo): se reenvia unas pocas veces.
    connect(m_wConexion->serialManager (),&SerialManager::siVolverEnviar,[this](){
        m_esperaAcusePlan->stop ();
        if (++m_reintentosPlan <= kMaxReintentosPlan) {
            mPrincipal->statusBar ()->showMessage (
                tr("El dron rechazó el plan: reenviando (%1 de %2)")
                    .arg (m_reintentosPlan).arg (kMaxReintentosPlan), 5000);
            onEnviaPlan ();
        } else {
            m_reintentosPlan = 0;
            QMessageBox::warning (mPrincipal, tr("Plan de vuelo"),
                                  tr("El dron ha rechazado el plan %1 veces. Revisa el enlace "
                                     "y vuelve a enviar la ruta.").arg (kMaxReintentosPlan));
        }
    });

    connect(m_wConexion->serialManager (),&SerialManager::newTramaConstanteReceived,this,&CControladora::onNewTramaConstanteReceived );

    connect(m_wConexion->serialManager (),&SerialManager::newCuadropteros, this,&CControladora::onNewCuadropteros);

    connect(m_wConexion->serialManager (),&SerialManager::newPlanificacionReceived,this,&CControladora::onNewPlanificacionReceived);

    connect(m_wConexion->serialManager (),&SerialManager::si_AcuseReciboXYZ,m_wMandos,&Widget_Mandos::on_AcuseRecibo);

    connect(m_wConexion->serialManager (),&SerialManager::si_AcuseReciboXYZ,m_wGanancias,&Widget_Ganancias_Control::on_AcuseRecibo);

    connect (mPrincipal,&Ci_Principal::visibleLog,[](bool visible){
        LogHandler& logHandler = LogHandler::getInstance();
        logHandler.appendLogMessage("Visibilidad");
        logHandler.setVisible (visible);
    });

    // Posicion del raton y cota del terreno en la barra de estado (antes iban
    // al log en cada movimiento del raton).
    // La cota se lee de disco: se pide solo cuando el raton se para (150 ms),
    // no en cada movimiento, para no frenar el arrastre del mapa.
    m_timerCota = new QTimer(this);
    m_timerCota->setSingleShot (true);
    m_timerCota->setInterval (150);
    connect(m_timerCota, &QTimer::timeout, [this]()
    {
        auto alt = alturaWorkerPTR->obtenerAltura (m_posRaton.latitude (),m_posRaton.longitude ());
        m_lPosicionRaton->setText (QString("Lat: %1  Lon: %2  Alt: %3 m")
                                   .arg (m_posRaton.latitude (),0,'f',6)
                                   .arg (m_posRaton.longitude (),0,'f',6)
                                   .arg (alt));
    });
    connect(m_mapa, &MapaMision::posicionRaton,[this](const QGeoCoordinate &coord)
    {
        m_posRaton = coord;
        m_lPosicionRaton->setText (QString("Lat: %1  Lon: %2")
                                   .arg (coord.latitude (),0,'f',6)
                                   .arg (coord.longitude (),0,'f',6));
        m_timerCota->start ();
    });

    connect(m_mapa, &MapaMision::medidaTerminada,[this](double rumbo, double distancia)
    {
        mPrincipal->statusBar ()->showMessage (QString("Azimut: %1°  Distancia: %2 m")
                                               .arg (rumbo,0,'f',1)
                                               .arg (distancia,0,'f',1));
    });

    connect(alturaWorkerPTR.get (), &AlturaWorker::alturaActualizada, [](qint16 altura) {
    });

    connect(m_mapa, &MapaMision::puntoRutaMovido,
            m_wPuntos, &Widget_Puntos::onPointMoved);

    connect(mPrincipal,&Ci_Principal::resize,[this](){
        updateOverlayPosition();
    });

    ////acciones mapa en barra de herramientas
    connect (aDistancia,&QAction::triggered,[this](bool checked)
    {
        m_mapa->medirDistancia (checked);
        aLupa->setChecked (false);
    });

    connect (aLupa,&QAction::triggered,[this](bool checked)
    {
        m_mapa->zoomArea (checked);
        aDistancia->setChecked (false);
    });

    connect (m_mapa,&MapaMision::zoomAreaTerminado ,[this]()
    {
        aLupa->setChecked (false);
    });

    connect (aLupaMas,&QAction::triggered,[this]()
    {
        m_mapa->zoomMas ();
    });

    connect (aLupaMen,&QAction::triggered,[this]()
    {
        m_mapa->zoomMenos ();
    });

    connect (aPoligono,&QAction::triggered,[this]()
    {
        m_mapa->dibujarPoligono ();
        aDistancia->setChecked (false);
        aLupa->setChecked (false);
    });

    // Acuse de recibo del plan de vuelo
    connect(m_wConexion->serialManager(), &SerialManager::si_AcuseReciboUVW,
            m_wPuntos, &Widget_Puntos::on_AcuseRecibo);

    // Pintar ruta en el mapa
    connect(m_wPuntos, &Widget_Puntos::sI_pintaRuta, [this](bool visible) {
        pintarRutaEnMapa(visible);
    });

    // Envío de la ruta (coordenadas simples)
    connect(m_wPuntos, &Widget_Puntos::sI_Ruta, [this](QList<QPair<double, double>> ruta) {
        if (m_Conectado) {
        }
    });

    // Envío de la ruta completa (objeto E_Punto): un plan nuevo, sin reintentos aún.
    connect(m_wPuntos, &Widget_Puntos::sI_Ruta2, [this](QList<E_Punto>) {
        if (m_Conectado) {
            m_reintentosPlan = 0;
            onEnviaPlan();   // onEnviaPlan() tomará los puntos del modelo
        }
    });

    // Cerrar ventana → desmarcar acción de menú
    // Doble clic en un punto de la tabla: el mapa se centra en el.
    connect(m_wPuntos, &Widget_Puntos::sI_centrarEnPunto, this, [this](const QGeoCoordinate &pos) {
        m_mapa->centrarEn (pos);
    });

    connect(m_wPuntos, &Widget_Puntos::sI_iniciarSeleccionPuntoInicial, [this]() {
        m_esperandoPuntoInicial = true;
        // Opcional: cambiar cursor del mapa
    });

    connect(m_wPuntos, &Widget_Puntos::sI_cancelarSeleccionPuntoInicial, [this]() {
        m_esperandoPuntoInicial = false;
    });

    connect(this, &CControladora::rutasActualizadas, this, &CControladora::actualizarMenuRutas);

    // Sincronizar menú con cambios de visibilidad desde el botón de la ventana de rutas
    connect(m_wPuntos, &Widget_Puntos::sI_visibilidadCambiada,
            this, [this](const QString &routeName, bool visible)
    {
        if (routeName == m_wPuntos->getModelo()->getCurrentRouteName()) {
            for (auto it = m_actionToRoute.begin(); it != m_actionToRoute.end(); ++it) {
                it.key()->setChecked(it.value() == routeName && visible);
            }
            // Actualizar "Ocultar ruta": marcada si la ruta está oculta
            if (a_ocultarRutaAction) {
                a_ocultarRutaAction->setChecked(!visible);
            }
        }
    });

    // Sincronizar menú con selección desde el combo de la ventana de rutas
    connect(m_wPuntos, &Widget_Puntos::sI_rutaSeleccionada,
            this, [this](const QString &routeName)
    {
        bool visible = m_wPuntos->isRouteVisible();
        for (auto it = m_actionToRoute.begin(); it != m_actionToRoute.end(); ++it) {
            it.key()->setChecked(it.value() == routeName && visible);
        }
    });

    connect(m_wPuntos, &Widget_Puntos::sI_rutasModificadas, this, &CControladora::actualizarMenuRutas);
}

// Vuelve a pedir los drones de Control de objetivo, y con ellos los vuelos
// grabados. Se llama al abrir la pestaña, al desconectar y al dejar de grabar.
// Antes no se hacia mientras habia una reproduccion empezada, y en pausa eso
// era para siempre: un vuelo recien grabado no salia hasta cerrar la estacion.
// Durante una reproduccion la pestaña conserva el vuelo que se esta viendo.
void CControladora::refrescarControlObjetivo()
{
    if (!m_wControlObjetivo)
        return;
    m_wControlObjetivo->setDrones (BaseDatos::nombresDrones ());
}

// Un instante de un vuelo grabado, mostrado como en vivo: instrumentos, panel
// y detalle, el dron del color del regimen con su traza, las lineas de rumbo
// (la morada al punto al que iba) y los puntos del plan que va cumpliendo.
void CControladora::mostrarMuestraReproducida(const MuestraVuelo &m)
{
    const double cursoDeseado = std::fmod (360.0 + m.cursoDeseado, 360.0);
    m_ParametrosValues[0] = m.lat;
    m_ParametrosValues[1] = m.lon;
    m_ParametrosValues[2] = m.distanciaChequeo;
    m_ParametrosValues[3] = cursoDeseado;
    m_ParametrosValues[4] = m.regimen;
    m_ParametrosValues[5] = m.mision;
    m_ParametrosValues[6] = m.cursoImu;
    m_ParametrosValues[7] = m.satelites;
    for (int i = 0; i < 11; ++i)
        m_ParametrosValues[8 + i] = m.ganancias[i];
    for (int i = 0; i < 4; ++i)
        m_ParametrosValues[19 + i] = m.actuadores[i];
    m_ParametrosValues[23] = m.convergenciaBanqueo;
    m_ParametrosValues[24] = m.convergenciaCabeceo;
    if (m.lat == 0)
        return;
    geoPos = QGeoCoordinate (m.lat, m.lon);

    // Punto al que iba (norte, este como en el plan) y cumplidos.
    const bool automatico = m.regimen == 2;
    const QPointF cumplir (m.puntoNorte, m.puntoEste);
    QGeoCoordinate proximo;
    if (m.puntoNorte != 0.0 || m.puntoEste != 0.0)
        proximo = ProyeccionMercator::aGeo (QPointF (m.puntoEste, m.puntoNorte));
    m_puntoReproducido = m_planReproducido.lastIndexOf (cumplir);
    if (automatico && !m_planReproducido.isEmpty ()) {
        const QPointF me = ProyeccionMercator::aMetros (geoPos);
        for (int i : m_cumplidosReproducidos.actualizar (cumplir, QPointF (me.y (), me.x ()),
                                                         mandos.Radio_Acercamiento))
            m_mapa->marcarPuntoCumplido (i);
    }

    m_detalle.tipoDron = m_wControlObjetivo->tipoDron ();
    updateParameters (m_ParametrosValues);
    actualizarPanel (m.regimen, m.mision, m.distanciaChequeo, m.curso, cursoDeseado, m.satelites,
                     false);

    indicador->setRoll (m.banqueo);
    indicador->setPitch (m.cabeceo);
    indicador->setHeading (m.curso);
    indicador->setAirspeed (m.velocidadKmh);
    indicador->setAltitude (m.altura);
    indicador->redraw ();

    static const QColor colores[] = {Qt::red, Qt::yellow, Qt::green};
    const QColor color = colores[qBound (0, m.regimen, 2)];
    const QString descripcion = "V: " + QString::number (m.velocidadKmh, 'f', 2) +
            " km/h\nH: " + QString::number (m.altura, 'f', 2) + " m";
    m_mapa->actualizarVehiculo (geoPos, m.curso, color,
                                m_detalle.tipoDron == 0 ? Iconos::avion (color) : Iconos::cuadroptero (color),
                                descripcion);
    m_mapa->ponerRumbos (m.curso, cursoDeseado, proximo, automatico);
}

// El plan que tenia el dron en ese momento del vuelo grabado: su ruta en el
// mapa y la cuenta de cumplidos desde cero (vacio: el vuelo no lo guardo).
void CControladora::pintarPlanReproducido(const QVector<PuntoPlan> &plan)
{
    QVector<PuntoRutaMapa> puntos;
    m_planReproducido.clear ();
    for (const PuntoPlan &p : plan) {
        puntos.append ({QGeoCoordinate (p.lat, p.lon, p.altura), p.nombre});
        m_planReproducido.append (QPointF (p.norte, p.este));
    }
    m_cumplidosReproducidos.setPlan (m_planReproducido);
    m_mapa->pintarRuta (puntos, !puntos.isEmpty ());
}

// Plan confirmado por el dron (el que tiene ahora), con los nombres, alturas
// y velocidades de la ruta y los metros Mercator tal como se le mandaron.
QVector<PuntoPlan> CControladora::planConfirmado() const
{
    QVector<PuntoPlan> plan;
    if (!m_wPuntos || !m_wPuntos->getModelo ())
        return plan;
    const QList<E_Punto> &ruta = m_wPuntos->getModelo ()->obtenerLista ();
    for (int i = 0; i < ruta.size () && i < m_planEnDron.size (); ++i) {
        PuntoPlan p;
        p.nombre = ruta.at (i).nombre;
        p.lat = ruta.at (i).pos.latitude ();
        p.lon = ruta.at (i).pos.longitude ();
        p.altura = ruta.at (i).altura;
        p.velocidad = ruta.at (i).velocidad;
        p.norte = m_planEnDron.at (i).x ();
        p.este = m_planEnDron.at (i).y ();
        plan.append (p);
    }
    return plan;
}

QList<QPointF> CControladora::sL_cambiaProyeccion()
{
    QList<QPointF> listPunto;
    for (auto punto: m_wPuntos->getModelo ()->obtenerLista ())
    {
        // Norte primero y truncado a metros enteros (QPoint), como espera el
        // autopiloto y como luego se compara con Cumplir_Punto.
        QPointF pointf = ProyeccionMercator::aMetros(punto.pos);
        listPunto.append(QPoint (pointf.y (),pointf.x ()));
    }
    return listPunto;
}

QPointF CControladora::sL_cambiaProyeccionPunto(QGeoCoordinate pos)
{
    QPointF pointf = ProyeccionMercator::aMetros(pos);
    return pointf;
}

void CControladora::onNewTramaConstanteReceived(const char a, const char b, const char c, const sTRAMA1 &s_28, const sTRAMA2 &s_34)
{
    LogHandler& logHandler = LogHandler::getInstance();
    if (a == 'a' && b == 'b' && c == 'c' )
    {
        double lat = m_wConexion->serialManager ()->getS28 ().Realimentacion_Latitud;
        double lon = m_wConexion->serialManager ()->getS28 ().Realimentacion_Longitud;

        // Validar ubicación
        bool isValid = validator.validateLocation(lat, lon);
        updateStatus (lat,lon,isValid);
        if(isValid)
        {
            if(primeraCoordenada)
            {
                m_wMandos->getDsB_Lat ()->setValue (lat);
                m_wMandos->getDsB_Lon ()->setValue (lon);
                m_mapa->ponerHome(QGeoCoordinate(lat,lon));
                primeraCoordenada = false;
                logHandler.appendLogMessage("TramaConstante(a,b,c):"
                                            "Home: --- Lat " + QString::number (lat) +
                                            "  Lon: " + QString::number (lon));

                return;
            }

            geoPos.setLatitude (lat);
            geoPos.setLongitude (lon);

            m_ParametrosValues[0] = lat;
            m_ParametrosValues[1] = lon;
            m_ParametrosValues[2] = m_wConexion->serialManager ()->getS34 ().Distancia_Chequeo;
            auto cursoDeseado = m_wConexion->serialManager ()->getS34 ().Curso_Deseado;
            cursoDeseado = fmodf((360.0 + cursoDeseado), 360.0);
            m_ParametrosValues[3] = cursoDeseado;
            m_ParametrosValues[4] = m_wConexion->serialManager ()->getS34 ().Realimentacion_Regimen;
            m_ParametrosValues[5] = m_wConexion->serialManager ()->getS34 ().Realimentacion_ComendoMision;
            m_ParametrosValues[6] = m_wConexion->serialManager ()->getS28 ().Realimentacion_CursoIMO;
            m_ParametrosValues[7] = m_wConexion->serialManager ()->getS34 ().Cantidad_Satelites;

            m_ParametrosValues[8] = m_wConexion->serialManager ()->getS34 ().Ganancia_Cabeceo[0];
            m_ParametrosValues[9] = m_wConexion->serialManager ()->getS34 ().Ganancia_Cabeceo[1];
            m_ParametrosValues[10] = m_wConexion->serialManager ()->getS34 ().Ganancia_Banqueo[0];
            m_ParametrosValues[11] = m_wConexion->serialManager ()->getS34 ().Ganancia_Banqueo[1];
            m_ParametrosValues[12] = m_wConexion->serialManager ()->getS34 ().Ganancia_Velocidad[0];
            m_ParametrosValues[13] = m_wConexion->serialManager ()->getS34 ().Ganancia_Velocidad[1];
            m_ParametrosValues[14] = m_wConexion->serialManager ()->getS34 ().Ganancia_Altura[0];
            m_ParametrosValues[15] = m_wConexion->serialManager ()->getS34 ().Ganancia_Altura[1];
            m_ParametrosValues[16] = m_wConexion->serialManager ()->getS34 ().Ganancia_Curso;
            m_ParametrosValues[17] = m_wConexion->serialManager ()->getS34 ().Ganancia_Rumbo[0];
            m_ParametrosValues[18] = m_wConexion->serialManager ()->getS34 ().Ganancia_Rumbo[1];

            m_ParametrosValues[19] = m_wConexion->serialManager ()->getS34 ().Control_Estabilizadores;
            m_ParametrosValues[20] = m_wConexion->serialManager ()->getS34 ().Control_Alerones;
            m_ParametrosValues[21] = m_wConexion->serialManager ()->getS34 ().Control_Motor;
            m_ParametrosValues[22] = m_wConexion->serialManager ()->getS34 ().Control_Rumbo;

            m_ParametrosValues[23] = m_wConexion->serialManager ()->getS34 ().Convergencia_Banqueo;
            m_ParametrosValues[24] = m_wConexion->serialManager ()->getS34 ().Convergencia_Cabeceo;

            updateParameters(m_ParametrosValues);
            actualizarPanel (m_wConexion->serialManager ()->getS34 ().Realimentacion_Regimen,
                             m_wConexion->serialManager ()->getS34 ().Realimentacion_ComendoMision,
                             m_wConexion->serialManager ()->getS34 ().Distancia_Chequeo,
                             m_wConexion->serialManager ()->getS28 ().Realimentacion_Curso,
                             cursoDeseado,
                             m_wConexion->serialManager ()->getS34 ().Cantidad_Satelites);

            indicador->setPitch(m_wConexion->serialManager ()->getS28 ().Realimentacion_Cabeceo);
            indicador->setRoll (m_wConexion->serialManager ()->getS28 ().Realimentacion_Banqueo);
            indicador->setHeading (m_wConexion->serialManager ()->getS28 ().Realimentacion_Curso);
            indicador->setAirspeed (m_wConexion->serialManager ()->getS28 ().Realimentacion_Velocidad * 3.6);
            indicador->setAltitude (m_wConexion->serialManager ()->getS28 ().Realimentacion_Altura);
            indicador->redraw ();

            //color de avion y traza en dependencia del modo de trabajo manu8al automatico semiautomatico
            QColor colorTraza = Qt::red;
            QPixmap simbolo = Iconos::avion(Qt::red);
            QString descripcion = "V: "+ QString::number (m_wConexion->serialManager ()->getS28 ().Realimentacion_Velocidad * 3.6,'f',2) +
                    " km/h\nH: " + QString::number(m_wConexion->serialManager ()->getS28 ().Realimentacion_Altura,'f',2) + " m";
            if(m_wConexion->serialManager ()->getS34().Realimentacion_Regimen == 0)
            {
                colorTraza = Qt::red;
                simbolo = Iconos::avion(Qt::red);
            }
            if(m_wConexion->serialManager ()->getS34().Realimentacion_Regimen == 1)
            {
                colorTraza = Qt::yellow;
                simbolo = Iconos::avion(Qt::yellow);
            }
            if(m_wConexion->serialManager ()->getS34().Realimentacion_Regimen == 2)
            {
                colorTraza = Qt::green;
                simbolo = Iconos::avion(Qt::green);
            }

            if (geoPos.latitude() != 0)
            {

                m_mapa->actualizarVehiculo (geoPos,
                                            m_wConexion->serialManager ()->getS28 ().Realimentacion_Curso,
                                            colorTraza,
                                            simbolo.scaled (40,40),
                                            descripcion);

                auto x = m_wConexion->serialManager ()->getS34 ().Cumplir_Punto[0];
                auto y = m_wConexion->serialManager ()->getS34 ().Cumplir_Punto[1];
                auto z = m_wConexion->serialManager ()->getS34 ().Cumplir_Punto[2];

                // Cumplir_Punto llega con el norte primero: x = norte, y = este.
                auto puntoProxGeo = ProyeccionMercator::aGeo (QPointF(y,x));
                rumboProxPunto = geoPos.azimuthTo (puntoProxGeo);

                auto cursoDeseado = m_wConexion->serialManager ()->getS34 ().Curso_Deseado;
                cursoDeseado = fmodf((360.0 + cursoDeseado), 360.0);

                bool automatico = false;
                automatico = m_wConexion->serialManager ()->getS34().Realimentacion_Regimen == 2 ? true : false;

                m_mapa->ponerRumbos (m_wConexion->serialManager ()->getS28 ().Realimentacion_Curso,
                                     cursoDeseado,
                                     puntoProxGeo,
                                     automatico);
                if(automatico)
                    marcarCumplidos (QPointF(x,y), geoPos);
            }
            //graficar
            double const t = start_time_.msecsTo(QTime::currentTime()) / 1000.0;
            m_wGraficas->datosTiempoReal (m_wConexion->serialManager ()->getS28 ().Realimentacion_Banqueo,
                                          m_wConexion->serialManager ()->getS28 ().Realimentacion_Cabeceo,
                                          m_wConexion->serialManager ()->getS34 ().Control_Motor,
                                          m_wConexion->serialManager ()->getS28 ().Realimentacion_Altura,
                                          m_wConexion->serialManager ()->getS28 ().Realimentacion_Velocidad *3.6,
                                          t);

            //guardar
            if(m_wConexion->chB_Guardar ()->isChecked ())
                guardaDatosDron (m_wConexion->serialManager ()->getS28 ().Realimentacion_Cabeceo,
                                 m_wConexion->serialManager ()->getS28 ().Realimentacion_Banqueo,
                                 m_wConexion->serialManager ()->getS28 ().Realimentacion_Velocidad * 3.6,
                                 m_wConexion->serialManager ()->getS28 ().Realimentacion_Altura,
                                 m_wConexion->serialManager ()->getS28 ().Realimentacion_Curso,
                                 m_wConexion->serialManager ()->getS34 ().Cumplir_Punto);
        }
    }
}

void CControladora::onNewPlanificacionReceived(sPUNTOXYZ *puntosRecividos, int NumParametros)
{
    for (int i = 0; i < NumParametros; ++i) {
    }
}

void CControladora::onEnviaPlan()
{
    if (!m_wPuntos || !m_wPuntos->getModelo())
        return;

    const QList<E_Punto> &puntos = m_wPuntos->getModelo()->obtenerLista();
    if (puntos.size() != Tramas::kPuntosPlan) {
        return;
    }

    listPuntoF = sL_cambiaProyeccion();   // convierte usando el modelo actualizado

    for (int i = 0; i < puntos.size(); ++i) {
        sPUNTOXYZ punto;
        punto.modo = puntos.at(i).modo;
        punto.x = listPuntoF.at(i).x();
        punto.y = listPuntoF.at(i).y();
        punto.z = puntos.at(i).altura;          // antes era m_ListPuntos, ahora bien
        // En m/s, como en el planificador: es lo que vuela el autopiloto (antes
        // se dividia por 3,6 y con 16,5 m/s en la ruta el dron iba a 4,6 m/s).
        // En km/h solo se muestra (la telemetria x 3,6).
        punto.velocidad = puntos.at(i).velocidad;
        puntosEnviar[i] = punto;
    }

    m_wConexion->serialManager()->serieWrite(Tramas::plan(puntosEnviar));
    m_esperaAcusePlan->start();
}

void CControladora::onNewCuadropteros(const sCUADROPTEROS &s_cuadropteros)
{
    LogHandler& logHandler = LogHandler::getInstance();

    indicador->setRoll (s_cuadropteros.Realimentacion_Banqueo);
    indicador->setPitch (s_cuadropteros.Realimentacion_Cabeceo);
    indicador->setHeading (s_cuadropteros.Realimentacion_Curso);
    indicador->setAirspeed (s_cuadropteros.Realimentacion_Velocidad * 3.6);
    indicador->setAltitude (s_cuadropteros.Realimentacion_Altura);
    indicador->redraw ();

    // inverse(lat, lon) de la libreria antigua: el norte va en la latitud y el
    // este en la longitud de la trama.
    auto latlon = ProyeccionMercator::aGeo (QPointF(s_cuadropteros.Realimentacion_Longitud,
                                                    s_cuadropteros.Realimentacion_Latitud));

    geoPos.setLatitude (latlon.latitude ()  +23.09759);
    geoPos.setLongitude (latlon.longitude ()-82.44113);

    // Validar ubicación
    bool isValid = validator.validateLocation(geoPos.latitude(), geoPos.longitude());
    updateStatus (geoPos.latitude(), geoPos.longitude(),isValid);
    if(isValid)
    {
        if(primeraCoordenada)
        {
            m_wMandos->getDsB_Lat ()->setValue (geoPos.latitude());
            m_wMandos->getDsB_Lon ()->setValue (geoPos.longitude());
            m_mapa->ponerHome(geoPos);
            primeraCoordenada = false;
            //            qDebug()<<"Busca primera coordenada: "<<primeraCoordenada;
            logHandler.appendLogMessage("onNewCuadropteros: "
                                        "Home: --- Lat " + QString::number (geoPos.latitude()) +
                                        "  Lon: " + QString::number (geoPos.longitude()));

            return;
        }

        m_ParametrosValues[0] = geoPos.latitude ();
        m_ParametrosValues[1] = geoPos.longitude ();

        m_ParametrosValues[2] = s_cuadropteros.Distancia_Chequeo;
        auto cursoDeseado = s_cuadropteros.Curso_Deseado;
        cursoDeseado = fmodf((360.0 + cursoDeseado), 360.0);
        m_ParametrosValues[3] = cursoDeseado;
        m_ParametrosValues[4] = s_cuadropteros.Realimentacion_Regimen;
        m_ParametrosValues[5] = s_cuadropteros.Realimentacion_ComandoMision;
        m_ParametrosValues[6] = s_cuadropteros.Realimentacion_CursoIMU;
        m_ParametrosValues[7] = (int)s_cuadropteros.Cantidad_Satelites;


        //Actuadores
        m_ParametrosValues[19] = s_cuadropteros.ControlM1;
        m_ParametrosValues[20] = s_cuadropteros.ControlM2;
        m_ParametrosValues[21] = s_cuadropteros.ControlM3;
        m_ParametrosValues[22] = s_cuadropteros.ControlM4;

        //sintonizacion
        m_ParametrosValues[23] = s_cuadropteros.Convergencia_Banqueo;
        m_ParametrosValues[24] = s_cuadropteros.Convergencia_Cabeceo;

        updateParameters(m_ParametrosValues);
        actualizarPanel (s_cuadropteros.Realimentacion_Regimen,
                         s_cuadropteros.Realimentacion_ComandoMision,
                         s_cuadropteros.Distancia_Chequeo,
                         s_cuadropteros.Realimentacion_Curso,
                         cursoDeseado,
                         s_cuadropteros.Cantidad_Satelites);

        //color de avion y traza en dependencia del modo de trabajo manu8al automatico semiautomatico
        QColor colorTraza = Qt::red;
        QPixmap simbolo = Iconos::cuadroptero(Qt::red);
        QString descripcion = "V: "+ QString::number (s_cuadropteros.Realimentacion_Velocidad * 3.6,'f',2) +
                " km/h\nH: " + QString::number(s_cuadropteros.Realimentacion_Altura,'f',2) + " m";
        if(s_cuadropteros.Realimentacion_Regimen == 0)
        {
            colorTraza = Qt::red;
            simbolo = Iconos::cuadroptero(Qt::red);
        }
        if(s_cuadropteros.Realimentacion_Regimen == 1)
        {
            colorTraza = Qt::yellow;
            simbolo = Iconos::cuadroptero(Qt::yellow);
        }
        if(s_cuadropteros.Realimentacion_Regimen == 2)
        {
            colorTraza = Qt::green;
            simbolo = Iconos::cuadroptero(Qt::green);
        }

        //
        if (geoPos.latitude() != 0)
        {
            m_mapa->actualizarVehiculo (geoPos,
                                        s_cuadropteros.Realimentacion_Curso,
                                        colorTraza,
                                        simbolo.scaled (40,40),
                                        descripcion);

            auto x = s_cuadropteros.Cumplir_Punto[0];
            auto y = s_cuadropteros.Cumplir_Punto[1];
            auto z = s_cuadropteros.Cumplir_Punto[2];

            auto puntoProxGeo = ProyeccionMercator::aGeo (QPointF(y,x));
            rumboProxPunto = geoPos.azimuthTo (puntoProxGeo);

            auto cursoDeseado = /*qRadiansToDegrees*/(s_cuadropteros.Curso_Deseado);
            cursoDeseado = fmodf((360.0 + cursoDeseado), 360.0);

            bool automatico = false;
            automatico = s_cuadropteros.Realimentacion_Regimen == 2 ? true : false;
            // Mismo orden que pasaba la libreria antigua al cuadroptero.
            m_mapa->ponerRumbos (cursoDeseado,
                                 s_cuadropteros.Realimentacion_Curso,
                                 puntoProxGeo,
                                 automatico);
            if(automatico)
                marcarCumplidos (QPointF(x,y), geoPos);

        }
        //graficar
        double const t = start_time_.msecsTo(QTime::currentTime()) / 1000.0;
        // Los datos del cuadroptero (antes se graficaba la ultima trama de ala
        // fija); de motor, la media de los cuatro.
        m_wGraficas->datosTiempoReal (s_cuadropteros.Realimentacion_Banqueo,
                                      s_cuadropteros.Realimentacion_Cabeceo,
                                      (s_cuadropteros.ControlM1 + s_cuadropteros.ControlM2 +
                                       s_cuadropteros.ControlM3 + s_cuadropteros.ControlM4) / 4.0,
                                      s_cuadropteros.Realimentacion_Altura,
                                      s_cuadropteros.Realimentacion_Velocidad *3.6,
                                      t);

        //guardar
        if(m_wConexion->chB_Guardar ()->isChecked ())
            guardaDatosDron (s_cuadropteros.Realimentacion_Cabeceo, s_cuadropteros.Realimentacion_Banqueo,
                             s_cuadropteros.Realimentacion_Velocidad * 3.6,
                             s_cuadropteros.Realimentacion_Altura, s_cuadropteros.Realimentacion_Curso,
                             s_cuadropteros.Cumplir_Punto);
    }
}

void CControladora::on_EliminarPunto(QString s)
{
    loadInitialData();

    indicador->setRoll (0);
    indicador->setPitch (0);
    indicador->setHeading (0);
    indicador->setAirspeed (0);
    indicador->setAltitude (0);
    indicador->redraw ();

    limpiarVueloEnMapa();
}

// Fin de la reproduccion: fuera la ruta del vuelo grabado y vuelve la de
// planificacion (si estaba a la vista) con sus cumplidos.
void CControladora::on_DetenerCO()
{
    m_ControlObjetivo = false;
    m_planReproducido.clear ();
    m_cumplidosReproducidos.setPlan (m_planReproducido);
    if (m_wPuntos && m_wPuntos->isRouteVisible ())
        pintarRutaEnMapa (true);
    else
        m_mapa->pintarRuta (QVector<PuntoRutaMapa>(), false);
    indicador->setRoll (0);
    indicador->setPitch (0);
    indicador->setHeading (0);
    indicador->setAirspeed (0);
    indicador->update ();
}

void CControladora::on_IniciarCO()
{
    m_ControlObjetivo = true;
}

void CControladora::actualizarMenuRutas()
{
    if (!mMenuMostrarRutas || !m_rutasActionGroup) return;

    // Desconectar grupo para evitar señales durante la limpieza
    disconnect(m_rutasActionGroup, &QActionGroup::triggered,
               this, &CControladora::onRouteActionTriggered);

    // Limpiar menú y grupo
    mMenuMostrarRutas->clear();
    QList<QAction*> oldActions = m_rutasActionGroup->actions();
    for (QAction *a : oldActions) {
        m_rutasActionGroup->removeAction(a);
        delete a;   // las acciones fueron creadas con new
    }
    m_actionToRoute.clear();

    // --- NUEVO: Crear acción "Ocultar ruta" ---
    a_ocultarRutaAction = new QAction(tr("Ocultar ruta"), this);
    a_ocultarRutaAction->setCheckable(true);
    a_ocultarRutaAction->setActionGroup(m_rutasActionGroup);
    mMenuMostrarRutas->addAction(a_ocultarRutaAction);
    mMenuMostrarRutas->addSeparator();

    // Consultar tablas de rutas (excluyendo tablas internas)
    QSqlQuery query("SELECT name FROM sqlite_master "
                    "WHERE type='table' AND name NOT LIKE 'sqlite_%' "
                    "AND name NOT LIKE 'drones' AND name NOT LIKE 'rutas_metadata'");
    while (query.next()) {
        QString routeName = query.value(0).toString();
        QAction *action = new QAction(routeName, this);
        action->setCheckable(true);
        action->setActionGroup(m_rutasActionGroup);
        mMenuMostrarRutas->addAction(action);
        m_actionToRoute[action] = routeName;
    }

    // Determinar el estado actual de la ruta cargada (si existe)
    QString currentRoute = m_wPuntos ? m_wPuntos->getModelo()->getCurrentRouteName() : QString();
    bool anyVisible = false;
    if (!currentRoute.isEmpty() && m_wPuntos) {
        anyVisible = m_wPuntos->isRouteVisible();
        // Marcar la acción correspondiente a la ruta actual según su visibilidad
        for (auto it = m_actionToRoute.begin(); it != m_actionToRoute.end(); ++it) {
            if (it.value() == currentRoute) {
                it.key()->setChecked(anyVisible);
            } else {
                it.key()->setChecked(false);
            }
        }
    }

    // Establecer el estado de "Ocultar ruta": marcado si hay ruta cargada y está oculta
    if (a_ocultarRutaAction) {
        a_ocultarRutaAction->setChecked(!anyVisible && !currentRoute.isEmpty());
    }

    // Reconectar el grupo (una sola conexión para todas las acciones)
    connect(m_rutasActionGroup, &QActionGroup::triggered,
            this, &CControladora::onRouteActionTriggered);
}

void CControladora::onRouteActionTriggered(QAction *action)
{
    if (!action || !m_wPuntos) return;

    // Si es la acción "Ocultar ruta", simplemente ocultar
    if (action == a_ocultarRutaAction) {
        m_wPuntos->setRouteVisible(false);
        return;
    }

    QString routeName = m_actionToRoute.value(action);
    if (routeName.isEmpty()) return;

    // Cargar la ruta si no es la actual
    if (m_wPuntos->getModelo()->getCurrentRouteName() != routeName) {
        m_wPuntos->cargarRuta(routeName);
    }

    // Sincronizar combo y mostrar
    m_wPuntos->setCurrentRouteInCombo(routeName, true);
    m_wPuntos->setRouteVisible(true);
}

// Anota una muestra del vuelo en curso (casilla "Guardar") en el grabador.
// Lo comun a los dos tipos de dron sale de m_ParametrosValues, que cada
// manejador de trama rellena con lo suyo; la actitud, de los argumentos.
// Antes leia siempre la trama de ala fija, cambiaba el cabeceo por el banqueo
// y guardaba la posicion como texto con 4 decimales (la traza en escalones).
void CControladora::guardaDatosDron(double cabeceo, double banqueo, double velocidadKmh,
                                    double altura, double curso, const float *cumplirPunto)
{
    const QVector<double> &v = m_ParametrosValues;
    MuestraVuelo m;
    m.lat = geoPos.latitude ();
    m.lon = geoPos.longitude ();
    m.cabeceo = cabeceo;
    m.banqueo = banqueo;
    m.velocidadKmh = velocidadKmh;
    m.altura = altura;
    m.curso = curso;
    m.distanciaChequeo = v[2];
    m.cursoDeseado = v[3];
    m.regimen = int (v[4]);
    m.mision = int (v[5]);
    m.cursoImu = v[6];
    m.satelites = int (v[7]);
    for (int k = 0; k < 11; ++k)
        m.ganancias[k] = v[8 + k];
    for (int k = 0; k < 4; ++k)
        m.actuadores[k] = v[19 + k];
    m.convergenciaBanqueo = v[23];
    m.convergenciaCabeceo = v[24];
    m.puntoNorte = cumplirPunto[0];
    m.puntoEste = cumplirPunto[1];
    m.puntoAltura = cumplirPunto[2];
    m_grabador->anotar (m);
}

void CControladora::updateOverlayPosition()
{   
    if(m_panelVuelo && m_mapa) {
        // Panel de vuelo (con su detalle) arriba en el centro (los overlays
        // son hijos del mapa: coordenadas del mapa).
        const QRect visibleRect = m_mapa->mapa ()->rect();
        int yPos = visibleRect.top() + 8;
        m_panelVuelo->adjustSize();
        // Centrado, pero sin salirse por la izquierda si el mapa es estrecho.
        m_panelVuelo->move(qMax(8, (visibleRect.width() - m_panelVuelo->width())/2), yPos);
    }
    if(m_wGraficas && m_mapa) {
        m_wGraficas->setFixedWidth (m_mapa->mapa ()->width () * 0.95);
        // Calcular posición relativa al área visible


        // Ajustar altura si sobrepasa el área
        QPoint newPos;
        newPos.setX ((m_mapa->mapa ()->width ()- m_wGraficas->width ())/2);
        newPos.setY (10);

        m_wGraficas->move(newPos);
    }
}

// Panel de rutas abajo, a todo lo ancho de la ventana (antes era una ventana
// flotante que tapaba el mapa). Se muestra u oculta con Rutas → Panel de rutas
// (Ctrl+R) o con su boton de cerrar, y conf.ini ([VENTANAS] rutas) recuerda
// como se dejo.
void CControladora::crearPanelRutas()
{
    m_dockRuta = new QDockWidget(tr("Rutas"), mPrincipal);
    m_dockRuta->setObjectName (QStringLiteral("panelRutas"));
    m_dockRuta->setAllowedAreas (Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_dockRuta->setFeatures (QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable);
    m_dockRuta->setWidget (m_wPuntos);
    mPrincipal->addDockWidget (Qt::BottomDockWidgetArea, m_dockRuta);

    const bool visible = MyQSetting().cargarSetting (Rutas::confIni (), "rutas", "VENTANAS", false).toBool ();
    m_dockRuta->setVisible (visible);
    aRuta->setChecked (visible);
    connect (aRuta, &QAction::toggled, m_dockRuta, &QDockWidget::setVisible);
    connect (m_dockRuta->toggleViewAction (), &QAction::toggled, aRuta, &QAction::setChecked);
    connect (aRuta, &QAction::toggled, this, [](bool v) {
        MyQSetting().guardarSettings (Rutas::confIni (), "VENTANAS", {{"rutas", v}});
    });
}

void CControladora::loadInitialData()
{
    m_ParametrosValues.clear ();

    for(int i = 0; i < 25;i++)//24parametros
        m_ParametrosValues.append (0.0);
    updateParameters(m_ParametrosValues);
    if (m_panelVuelo)
        m_panelVuelo->limpiar ();
}

void CControladora::timerEvent(QTimerEvent *event)
{
    // Timer functionality removed - keeping override in header
}

// Avisa en el log cuando la posicion del dron entra o sale del area de
// trabajo (y la primera vez). Antes escribia una linea en cada trama: a 10 Hz
// el log crecia sin fin y frenaba la estacion.
void CControladora::updateStatus(double lat, double lon, bool isValid) {
    if (m_posicionValida == int(isValid))
        return;
    m_posicionValida = int(isValid);
    QString status = QString("Posición del dron %1: Lat %2°, Lon %3°")
            .arg(isValid ? "VÁLIDA" : "INVÁLIDA")
            .arg(lat, 0, 'f', 4)
            .arg(lon, 0, 'f', 4);
    if(!isValid)
        status += " ¡ALERTA! Fuera del área de trabajo";
    LogHandler::getInstance().appendLogMessage (status);
}

// Paquete de datos del mapa: la ruta de su mapa.json (o la carpeta que lo
// contiene). Por orden: el primer argumento de la linea de ordenes
// (EstacionTerrena D:/QtPro/Recursos/mapa.json), conf.ini (seccion [MAPA],
// clave paquete) o la de siempre, <carpeta de trabajo>/../Recursos.
QString CControladora::dirPaqueteMapa() const
{
    const QStringList args = QCoreApplication::arguments();
    for (int i = 1; i < args.size(); ++i) {
        if (!args.at(i).startsWith('-'))
            return QDir::fromNativeSeparators(args.at(i));
    }

    MyQSetting settings;
    const QString fichero = Rutas::confIni();
    const QString defecto = Rutas::carpetaRecursos();
    return settings.cargarSetting(fichero, "paquete", "MAPA", defecto).toString();
}

// Crea el mapa (libmapa) sobre el paquete de datos, le da al worker de alturas
// de donde leer la cota y pone en la barra de estado la posicion del raton.
void CControladora::crearMapa()
{
    const QString paquete = dirPaqueteMapa();
    m_mapa = new MapaMision(paquete, mPrincipal);
    alturaWorkerPTR->setMapa(m_mapa);

    m_lPosicionRaton = new QLabel(mPrincipal);
    mPrincipal->statusBar()->addPermanentWidget(m_lPosicionRaton);

    LogHandler& logHandler = LogHandler::getInstance();
    if (!m_mapa->listo()) {
        logHandler.appendLogMessage("Mapa: no se pudo abrir " + paquete + ": " + m_mapa->error());
        QMessageBox::warning(mPrincipal, tr("Mapa"),
                             tr("No se pudo abrir el paquete de mapas en %1:\n%2\n\n"
                                "Pasa la ruta de su mapa.json como argumento "
                                "(EstacionTerrena D:/QtPro/Recursos/mapa.json) "
                                "o indícala en conf.ini, sección [MAPA], clave paquete.")
                             .arg(QDir::toNativeSeparators(paquete), m_mapa->error()));
        return;
    }
    for (const QString &aviso : m_mapa->avisos())
        logHandler.appendLogMessage("Mapa: " + aviso);
}

// Claves de [CAPAS] en conf.ini que son listas de ids: se guardan unidas con
// ';' para distinguir "ninguno" (cadena vacia) de "nunca guardado" (sin clave).
static bool esListaDeCapas(const QString &clave)
{
    return clave != QLatin1String("base") && clave != QLatin1String("hibrido");
}

// Panel de capas a la derecha, como en la demo de libmapa, y menu Capas con las
// capas base del paquete. Recupera de conf.ini ([CAPAS]) las capas que se
// dejaron puestas y ([VENTANAS] capas) si el panel estaba abierto.
void CControladora::crearPanelCapas()
{
    MyQSetting settings;
    const QString ini = Rutas::confIni();
    QStringList claves = {"base", "hibrido", "hibrido_ocultos", "fijas"};
    for (const CapaMapa &b : m_mapa->capasBase())
        claves.append("ocultos_" + b.id);
    QVariantMap estado;
    for (const QString &clave : claves) {
        const QVariant v = settings.cargarSetting (ini, clave, "CAPAS");
        if (!v.isValid ())
            continue;
        estado.insert (clave, esListaDeCapas (clave)
                                  ? QVariant(v.toString ().split (';', Qt::SkipEmptyParts))
                                  : v);
    }
    if (!estado.isEmpty ())
        m_mapa->restaurarCapas (estado);

    m_dockCapas = new QDockWidget(tr("Capas"), mPrincipal);
    m_dockCapas->setObjectName (QStringLiteral("panelCapas"));
    m_dockCapas->setAllowedAreas (Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_panelCapas = new PanelCapas(m_mapa, m_dockCapas);
    m_dockCapas->setWidget (m_panelCapas);
    m_dockCapas->setMinimumWidth (240);
    mPrincipal->addDockWidget (Qt::RightDockWidgetArea, m_dockCapas);
    m_dockCapas->setVisible (settings.cargarSetting (ini, "capas", "VENTANAS", false).toBool ());

    QAction *ver = m_dockCapas->toggleViewAction ();
    ver->setText (tr("Panel de &capas"));
    ver->setShortcut (QKeySequence(tr("Ctrl+L")));
    ver->setStatusTip (tr("Muestra u oculta el panel de capas del mapa"));
    connect (ver, &QAction::toggled, this, [](bool visible) {
        MyQSetting().guardarSettings (Rutas::confIni (), "VENTANAS", {{"capas", visible}});
    });

    // En cola: el aviso puede llegar desde una accion del propio menu, que
    // actualizarMenuCapas borra al rehacerlo.
    connect (m_mapa, &MapaMision::capasCambiadas, this, &CControladora::actualizarMenuCapas,
             Qt::QueuedConnection);
    connect (m_mapa, &MapaMision::capasCambiadas, this, &CControladora::guardarCapas);
    actualizarMenuCapas ();
}

// Menu Capas: una accion por capa base del paquete (la activa marcada) y, al
// final, el panel de capas. Se conservan los atajos de siempre para la
// satelital (Ctrl+S) y OSM (Ctrl+O); el resto, Ctrl+1, Ctrl+2...
void CControladora::actualizarMenuCapas()
{
    mCapas->clear ();
    int n = 0;
    for (const CapaMapa &b : m_mapa->capasBase()) {
        QAction *a = mCapas->addAction (b.nombre);
        a->setCheckable (true);
        a->setChecked (b.visible);
        a->setStatusTip (tr("Mapa base: %1").arg (b.nombre));
        aG_Capas->addAction (a);
        if (b.id == QLatin1String("satelital"))
            a->setShortcut (QKeySequence(tr("Ctrl+S")));
        else if (b.id == QLatin1String("osm"))
            a->setShortcut (QKeySequence(tr("Ctrl+O")));
        else if (++n <= 9)
            a->setShortcut (QKeySequence(QStringLiteral("Ctrl+%1").arg (n)));
        const QString id = b.id;
        connect (a, &QAction::triggered, this, [this, id]() { m_mapa->ponerCapaBaseId (id); });
    }
    mCapas->addSeparator ();
    if (m_dockCapas)
        mCapas->addAction (m_dockCapas->toggleViewAction ());
}

// Guarda en conf.ini ([CAPAS]) como estan las capas; en otro hilo y solo si
// algo cambio (MyQSetting).
void CControladora::guardarCapas()
{
    QVariantMap valores = m_mapa->estadoCapas ();
    for (auto it = valores.begin (); it != valores.end (); ++it)
        if (esListaDeCapas (it.key ()))
            it.value () = it.value ().toStringList ().join (';');
    MyQSetting().guardarSettings (Rutas::confIni (), "CAPAS", valores);
}

// Pinta (u oculta) en el mapa la ruta del modelo de Widget_Puntos.
void CControladora::pintarRutaEnMapa(bool visible)
{
    QVector<PuntoRutaMapa> puntos;
    for (const auto &punto : m_wPuntos->getModelo()->obtenerLista())
        puntos.append({punto.pos, punto.nombre});
    m_mapa->pintarRuta(puntos, visible);
    // Repintar no borra lo ya cumplido.
    for (int i = 0; i < puntos.size(); ++i)
        if (m_cumplidos.cumplido (i))
            m_mapa->marcarPuntoCumplido (i);
}

// Al desconectar o limpiar el control de objetivo: fuera el dron, sus rumbos y la casa.
void CControladora::limpiarVueloEnMapa()
{
    m_mapa->limpiarVuelo();
}

// Manda los mandos de la pantalla de Mandos o de Ganancias (la misma
// estructura) y espera su acuse. El radio de acercamiento se ve ya en el mapa.
void CControladora::enviarMandos()
{
    m_wConexion->sL_EnviaMandosDron (Tramas::mandos (mandos));
    m_mapa->ponerRadioAcercamiento (mandos.Radio_Acercamiento);
    if (m_Conectado)
        m_esperaAcuseMandos->start ();
}

// Esperas de los acuses del plan y de los mandos: el acuse para la espera y lo
// dice en la barra de estado; si no llega a tiempo, se avisa. Con el plan
// confirmado se repinta la ruta (sin puntos cumplidos) y se empieza de nuevo a
// contar los cumplidos.
void CControladora::crearEsperasDeAcuse()
{
    m_esperaAcusePlan = new QTimer(this);
    m_esperaAcusePlan->setSingleShot (true);
    m_esperaAcusePlan->setInterval (kEsperaAcuseMs);
    connect (m_esperaAcusePlan, &QTimer::timeout, this, [this]() {
        m_reintentosPlan = 0;
        LogHandler::getInstance ().appendLogMessage ("Plan: el dron no contesto");
        QMessageBox::warning (mPrincipal, tr("Plan de vuelo"),
                              tr("El dron no ha confirmado el plan en %1 s. Revisa el enlace "
                                 "y vuelve a enviar la ruta.").arg (kEsperaAcuseMs / 1000));
    });

    m_esperaAcuseMandos = new QTimer(this);
    m_esperaAcuseMandos->setSingleShot (true);
    m_esperaAcuseMandos->setInterval (kEsperaAcuseMs);
    connect (m_esperaAcuseMandos, &QTimer::timeout, this, [this]() {
        LogHandler::getInstance ().appendLogMessage ("Mandos: el dron no contesto");
        mPrincipal->statusBar ()->showMessage (tr("⚠ El dron no ha confirmado los mandos"), 8000);
    });

    SerialManager *enlace = m_wConexion->serialManager ();
    connect (enlace, &SerialManager::si_AcuseReciboUVW, this, [this]() {
        m_esperaAcusePlan->stop ();
        m_reintentosPlan = 0;
        m_cumplidos.setPlan (listPuntoF);
        m_planEnDron = listPuntoF;
        if (m_grabador->grabando ())
            m_grabador->anotarPlan (planConfirmado ());
        if (m_wPuntos && m_wPuntos->isRouteVisible ())
            pintarRutaEnMapa (true);
        mPrincipal->statusBar ()->showMessage (tr("✓ El dron ha confirmado el plan"), 5000);
    });
    connect (enlace, &SerialManager::si_AcuseReciboXYZ, this, [this]() {
        m_esperaAcuseMandos->stop ();
        // La "H" del mapa es la casa que el dron acaba de aceptar (norte, este).
        if (mandos.Home[0] != 0.0f || mandos.Home[1] != 0.0f)
            m_mapa->ponerHome (ProyeccionMercator::aGeo (QPointF(mandos.Home[1], mandos.Home[0])));
        mPrincipal->statusBar ()->showMessage (tr("✓ El dron ha confirmado los mandos"), 5000);
    });
}

// Pinta de verde los puntos del plan que el dron ya ha cumplido: los
// anteriores al que va (\a cumplir, Cumplir_Punto: norte, este) y el propio
// punto en cuanto el dron entra en su radio de acercamiento (asi se marca
// tambien el ultimo).
void CControladora::marcarCumplidos(const QPointF &cumplir, const QGeoCoordinate &posDron)
{
    m_puntoActual = listPuntoF.lastIndexOf (cumplir);
    const QPointF m = ProyeccionMercator::aMetros (posDron);
    const auto nuevos = m_cumplidos.actualizar (cumplir, QPointF(m.y (), m.x ()),
                                                mandos.Radio_Acercamiento);
    for (int i : nuevos)
        m_mapa->marcarPuntoCumplido (i);
}

// Pone en el panel de vuelo los datos de una trama (ala fija o cuadroptero).
// El punto al que va solo tiene sentido en automatico. La pestaña de Mandos
// muestra tambien en que regimen y mision esta el dron.
void CControladora::actualizarPanel(int regimen, int mision, double distancia, double rumbo,
                                    double rumboDeseado, int satelites, bool enVivo)
{
    PanelVuelo::Datos d;
    d.regimen = regimen;
    d.mision = mision;
    d.puntoActual = regimen != 2 ? -1 : enVivo ? m_puntoActual : m_puntoReproducido;
    d.totalPuntos = enVivo ? listPuntoF.size () : m_planReproducido.size ();
    d.puntosCumplidos = enVivo ? m_cumplidos.cuantos () : m_cumplidosReproducidos.cuantos ();
    d.distanciaPunto = distancia;
    d.rumbo = rumbo;
    d.rumboDeseado = rumboDeseado;
    d.satelites = satelites;
    d.posicion = geoPos;
    m_panelVuelo->mostrar (d);
    if (!enVivo)
        return;   // un vuelo grabado no dice en que esta el dron ahora
    m_wMandos->setEstadoDron (regimen, mision, !m_estadoDronAdoptado);
    m_estadoDronAdoptado = true;
}

// Indicador del enlace en la barra de estado: cuenta las tramas de telemetria
// que llegan y cada 250 ms dice como va (gris sin enlace, ambar esperando o
// lento, verde bien, rojo sin datos).
void CControladora::crearIndicadorEnlace()
{
    m_reloj.start ();
    m_lEnlace = new QLabel(mPrincipal);
    m_lEnlace->setMinimumWidth (330);
    mPrincipal->statusBar ()->insertPermanentWidget (0, m_lEnlace);
    // Al lado, cuanto lleva abierto el enlace (y, al cerrarlo, cuanto duro).
    m_lTiempoConexion = new QLabel(mPrincipal);
    m_lTiempoConexion->setToolTip (tr("Tiempo que lleva abierto el enlace con el dron"));
    m_lTiempoConexion->setStyleSheet (QStringLiteral("QLabel { padding: 1px 8px; font-weight: bold; }"));
    mPrincipal->statusBar ()->insertPermanentWidget (1, m_lTiempoConexion);

    SerialManager *enlace = m_wConexion->serialManager ();
    connect (enlace, &SerialManager::newTramaConstanteReceived, this,
             [this]() { m_monitor.registrarTrama (m_reloj.elapsed ()); });
    connect (enlace, &SerialManager::newCuadropteros, this,
             [this]() { m_monitor.registrarTrama (m_reloj.elapsed ()); });

    m_timerEnlace = new QTimer(this);
    connect (m_timerEnlace, &QTimer::timeout, this, &CControladora::refrescarIndicadorEnlace);
    m_timerEnlace->start (250);
    refrescarIndicadorEnlace ();
}

// Texto y color del indicador segun el estado del enlace. Al perder los datos
// (mas de 3 s sin tramas): rojo, aviso en el panel de vuelo, pitido (y otro
// cada 5 s mientras dure) y nota en el log; al recuperarlos, nota en el log.
void CControladora::refrescarIndicadorEnlace()
{
    const qint64 ahora = m_reloj.elapsed ();
    const MonitorEnlace::Estado e = m_monitor.estado (ahora);

    QString fondo = QStringLiteral("#616161"), letra = QStringLiteral("white");
    switch (e) {
    case MonitorEnlace::Estado::Desconectado: break;
    case MonitorEnlace::Estado::Esperando:
    case MonitorEnlace::Estado::Lento:   fondo = QStringLiteral("#ffb300"); letra = QStringLiteral("black"); break;
    case MonitorEnlace::Estado::Bien:    fondo = QStringLiteral("#2e7d32"); break;
    case MonitorEnlace::Estado::Perdido: fondo = QStringLiteral("#c62828"); break;
    }
    m_lEnlace->setText (m_monitor.texto (ahora));
    m_lTiempoConexion->setText (m_monitor.textoTiempo (ahora));
    m_lEnlace->setStyleSheet (QStringLiteral("QLabel { background: %1; color: %2; padding: 1px 8px;"
                                             " border-radius: 3px; font-weight: bold; }")
                                  .arg (fondo, letra));

    LogHandler& logHandler = LogHandler::getInstance ();
    if (e == MonitorEnlace::Estado::Perdido) {
        m_panelVuelo->setAviso (tr("SIN DATOS DEL DRON hace %1 s")
                                    .arg (m_monitor.msDesdeUltima (ahora) / 1000));
        if (m_estadoEnlace != MonitorEnlace::Estado::Perdido || ahora - m_ultimoPitido > 5000) {
            QApplication::beep ();
            m_ultimoPitido = ahora;
        }
        if (m_estadoEnlace != MonitorEnlace::Estado::Perdido)
            logHandler.appendLogMessage ("Enlace: sin datos del dron (" + m_monitor.descripcion () + ")");
    } else {
        m_panelVuelo->setAviso (QString());
        if (m_estadoEnlace == MonitorEnlace::Estado::Perdido && e == MonitorEnlace::Estado::Bien)
            logHandler.appendLogMessage ("Enlace: datos recuperados");
    }
    m_estadoEnlace = e;
}
