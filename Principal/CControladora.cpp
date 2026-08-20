#include "CControladora.h"
#include <QAction>
#include <QMenuBar>
#include <QDir>
#include <QScrollBar>
#include <logger/LogHandler.h>

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
    tabOpciones->setTabPosition (QTabWidget::North);
    tabOpciones->setTabShape (QTabWidget::Rounded);


RecursosDir.setPath (QDir::currentPath ());
    RecursosDir.cdUp ();
    //    QVBoxLayout *layMapaDatos = new QVBoxLayout();
    libMapa = new LibMapaStatic(RecursosDir.path (),mPrincipal);
    libMapa->mostrarCapaImgSatSeleccionada (true);
    libMapa->getTb_zMas ()->setVisible (false);
    libMapa->getTb_zMenos ()->setVisible (false);
    libMapa->getTb_zoomArea ()->setVisible (false);
    libMapa->getTb_trazaRuta ()->setVisible (false);
    libMapa->getTb_nuevoPunto ()->setVisible (false);
    libMapa->getTb_nuevoPoligono ()->setVisible (false);
    libMapa->getTb_medirDistancia ()->setVisible (false);
    initTabOpciones();
    preparaDataBase();
    m_wConexion->cBDrones ()->setCurrentIndex (0);

    listaParametros.append ({"Latitud","Longitud","Banqueo","Cabeceo",
                             "ConvBanqueo","ConvCabeceo","ContEstab","ContAlero","ContMotor","ContRumbo",
                             "DistCheck","CursoDeseado","CursoIMU",
                             "KPCab","KICab","KPBan","KIBan","KPVel","KIVel",
                             "KPAlt","KIAlt","KCurso","KPRumbo","KIPRumbo"});

    m_wGraficas = new Widget_Graficas(libMapa->getOMapa ());
    m_wGraficas->raise();
    m_wGraficas->setVisible (false);

    m_wParametros = new Widget_Parametros(libMapa->getOMapa ());
    m_wParametros->raise();
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
    gridLayout->addWidget (libMapa,0,2,3,1);

    gridLayout->setHorizontalSpacing (3);
    gridLayout->setVerticalSpacing (3);
    gridLayout->setContentsMargins(0, 0, 0, 0);

    mPrincipal->centralWidget ()->setLayout (gridLayout);




    // Crear widget de puntos con el worker de alturas
    m_wPuntos = new Widget_Puntos(alturaWorkerPTR.get(), mPrincipal);
    m_wPuntos->setVisible(false);

    actualizarMenuRutas();
    conecciones ();

    mPrincipal->setWindowTitle ("ECT v.1.2 - Estación de Control Terrestre (CID3)");
    mPrincipal->showMaximized ();
}

bool CControladora::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == mPrincipal) {
        if (event->type() == QEvent::Move || event->type() == QEvent::Resize) {
            if(m_wPuntos && m_wPuntos->isVisible()) {
                QTimer::singleShot(50, this, [this]() {
                    updateRutaPosition();
                });
            }
        }
    }
    return QObject::eventFilter(obj, event);
}

void CControladora::updateParameters(const QVector<double> &values) {
    m_wParametros->updateValues (values);
    auto cantSatelites = m_wConexion->serialManager ()->getS34 ().Cantidad_Satelites;
    m_wParametros->updateSatelites (int(cantSatelites));
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
    m_wMandos = new Widget_Mandos(libMapa->getOMapa ()->getProjection (),&mandos);
    m_wMandosCuadroptero = new Widget_Mandos_Cuadroptero();
    LayOutMandos->addWidget (m_wMandos);
    LayOutMandos->addWidget (m_wMandosCuadroptero);
    w3->setLayout (LayOutMandos);

QWidget *w4 = new QWidget();
    QGridLayout *LayOutGanancias = new QGridLayout;
    m_wGanancias = new Widget_Ganancias_Control(libMapa->getOMapa ()->getProjection (),&mandos);
    LayOutGanancias->addWidget (m_wGanancias);
    w4->setLayout (LayOutGanancias);

    QWidget *w5 = new QWidget();
    QGridLayout *LayOutConexion = new QGridLayout;
    m_wConexion = new Widget_Conexion();
    LayOutConexion->addWidget (m_wConexion);
    w5->setLayout (LayOutConexion);

    tabOpciones->addTab (w5,"Configuración");
    tabOpciones->addTab (w1,"Datos_Drones");
    tabOpciones->addTab (w3,"Mandos");
    tabOpciones->addTab (w4,"Ganacias_Control");
    tabOpciones->addTab (w2,"Control_Objetivo");

    mPrincipal->installEventFilter(this);
}

void CControladora::preparaDataBase()
{
    _aQuery = new Database::AsyncQuery(this);
    _aQuery->setMode(Database::AsyncQuery::Mode_Parallel);
    _aQuery->prepare ("PRAGMA foreign_keys = ON");
    _aQuery->startExec ();

    query1 = "CREATE TABLE IF NOT EXISTS DatosVuelos("
             "Id_Vuelos INTEGER NOT NULL, "
             "Dron_ID INTEGER NOT NULL REFERENCES DronTable(id_Dron) ON DELETE CASCADE ON UPDATE CASCADE, "
             "Lat REAL, "
             "Lon REAL, "
             "Realimentacion_Cabeceo REAL, "
             "Realimentacion_Banqueo REAL, "
             "Realimentacion_Velocidad REAL, "
             "Realimentacion_Altura REAL, "
             "Realimentacion_Curso REAL, "
             "Distancia_Chequeo REAL, "
             "Curso_Deseado REAL, "
             "Realimentacion_Regimen INTEGER, "
             "Realimentacion_ComandoMision INTEGER, "
             "Realimentacion_CursoIMU REAL, "
             "CantidadSatelites INTEGER, "

             "KPCabeceo REAL, "
             "KICabeceo REAL, "
             "KPBanqueo REAL, "
             "KIBanqueo REAL, "
             "KPVelocidad REAL, "
             "KIVelocidad REAL, "
             "KPAltura REAL, "
             "KIAltura REAL, "
             "KPCurso REAL, "
             "KPRumbo REAL, "
             "KIRumbo REAL, "

             "Control_Estabilizadores REAL, "
             "Control_Alerones REAL, "
             "Control_Motor REAL, "
             "Control_Rumbo REAL, "

             "Convergencia_Banqueo REAL, "
             "Convergencia_Cabeceo REAL, "

             "Fecha DATETIME)";

    _aQuery->prepare (query1);
    _aQuery->startExec ();
    Database::AsyncQuery::startExecOnce(
                "SELECT Nombre FROM DronTable",
                [this](const Database::AsyncQueryResult& res) {
        for (int i = 0; i < res.count(); i++) {
            m_wConexion->cBDrones ()->addItem(res.value(i, "Nombre").toString());
        }
        //        if (res.count() > 0) {
        m_wConexion->cBDrones ()->setCurrentIndex(0);
        DronID = m_wConexion->cBDrones ()->currentText ();
        //        }
    });
}



void CControladora::Conectado(bool conectado)
{
    primeraCoordenada = conectado;
    m_wConexion->chB_Guardar ()->setChecked (false);
    m_Conectado = conectado;
    if(m_wConexion->cBDrones ()->currentIndex () != -1)
    {
        m_wConexion->cBDrones ()->setEnabled (!conectado);
        m_wConexion->chB_Guardar ()->setEnabled (conectado);
        indexAnterior = 0;
        if(conectado)
        {

            //            m_wEstado->setEnabled (true);
            start_time_ = QTime::currentTime ();
            m_wGraficas->getLEdron ()->setVisible (false);
            m_wGraficas->getLEvuelo ()->setVisible (false);
            m_wGraficas->getMpBCerrar ()->setVisible (false);
            QString queryVuelos = "SELECT Id_Dron, Tipo, Vuelos FROM DronTable WHERE Nombre = \'" +DronID+ "\'";

            Database::AsyncQuery::startExecOnce(
                        queryVuelos,
                        [=](const Database::AsyncQueryResult& res) {
                dronID = res.value(0, "Id_Dron").toInt () ;
                auto tipodrone  = res.value(0, "Tipo").toString ();
                tipoDrone = tipodrone == "Ala fija" ? 0 : 1;
                vueloID = res.value(0, "Vuelos").toInt () ;
                m_wConexion->serialManager ()->setTipo_dron (tipoDrone);
                m_wParametros->setTipo_dron (tipoDrone);
            });
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

            //            libMapa->getOMapa ()->eliminarPunto ("PuntoNuevo");
            libMapa->getOMapa ()->borrarElementosPorNombre ("PuntoNuevo");

            //            if(libMapa->getOMapa ()->getPuntoItem ().text == NULL)
            //                return;
            if(libMapa->getOMapa ()->getPuntoItem ().text != NULL)
            {
                libMapa->getOMapa ()->getPuntoItem ().text->setVisible (false);
                libMapa->getOMapa ()->getPuntoItem ().ellipse->setVisible (false);
                libMapa->getOMapa ()->getPuntoItem ().ellipse2->setVisible (false);
            }
            libMapa->getOMapa ()->getLineaRumboDestino ()->setVisible (false);
            libMapa->getOMapa ()->getLineaRumboVehiculo ()->setVisible (false);
            libMapa->getOMapa ()->getLineaRumboCalculado ()->setVisible (false);
            libMapa->getOMapa ()->layer (sMapaZOOM)->replot ();



        }
    }

}

void CControladora::createMenus()
{
    mMenu = mPrincipal->menuBar ()->addMenu (tr("&Menu"));
    mMenu->addAction (aCerrar);

    mCapas = mPrincipal->menuBar ()->addMenu (tr("&Capas"));
    mCapas->addAction (aSatelital);
    mCapas->addAction (aOSM);

    mTema = mPrincipal->menuBar ()->addMenu (tr("&Tema"));
    mTema->addAction (aClaro);
    mTema->addAction (aOscuro);

    mEstadoDrone = mPrincipal->menuBar ()->addMenu (tr("&Estado_Drone"));
    mEstadoDrone->addAction (aEstadoDrone);
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


    aSatelital = new QAction(tr("&Satelital"),mPrincipal);
    aSatelital->setShortcut (QKeySequence(tr("Ctrl+S")));
    aSatelital->setStatusTip(tr("Seleciona capa satelital"));
    aSatelital->setCheckable (true);
    aSatelital->setChecked (true);
    connect (aSatelital,&QAction::triggered,[this]()
    {
        libMapa->mostrarCapaImgSatSeleccionada(true);
    });

    aOSM = new QAction(tr("&OSM"),mPrincipal);
    aOSM->setShortcut (QKeySequence(tr("Ctrl+O")));
    aOSM->setStatusTip(tr("Seleciona capa Open Street Map (OSM)"));
    aOSM->setCheckable (true);
    aOSM->setChecked (false);
    connect (aOSM,&QAction::triggered,[this]()
    {
        libMapa->mostrarCapaImgOSMSeleccionada (true);
    });

    aG_Capas= new QActionGroup(mPrincipal);
    aG_Capas->addAction (aSatelital);
    aG_Capas->addAction (aOSM);

    aClaro = new QAction(tr("&Claro"),mPrincipal);
    aClaro->setShortcut (QKeySequence(tr("Ctrl+T+C")));
    aClaro->setStatusTip(tr("Seleciona tema Claro"));
    aClaro->setCheckable (true);
    aClaro->setChecked (false);
    connect (aClaro,&QAction::triggered,[this]()
    {
        mPrincipal->m_CurrentStyleS (true);
    });
    aOscuro = new QAction(tr("&Oscuro"),mPrincipal);
    aOscuro->setShortcut (QKeySequence(tr("Ctrl+T+O")));
    aOscuro->setStatusTip(tr("Seleciona tema Oscuro"));
    aOscuro->setCheckable (true);
    aOscuro->setChecked (true);
    connect (aOscuro,&QAction::triggered,[this]()
    {
        mPrincipal->m_CurrentStyleS (false);
    });

    aG_Tema= new QActionGroup(mPrincipal);
    aG_Tema->addAction (aClaro);
    aG_Tema->addAction (aOscuro);

    aEstadoDrone = new QAction(tr("&Datos"),mPrincipal);
    aEstadoDrone->setShortcut (QKeySequence(tr("Ctrl+E+D")));
    aEstadoDrone->setStatusTip(tr("Ver estado del drone activo"));
    aEstadoDrone->setCheckable (true);
    aEstadoDrone->setChecked (true);

    aVerGraficaDrone = new QAction(tr("&Gráficas"),mPrincipal);
    aVerGraficaDrone->setShortcut (QKeySequence(tr("Ctrl+E+G")));
    aVerGraficaDrone->setStatusTip(tr("Ver gráficas del drone activo"));
    aVerGraficaDrone->setCheckable (true);
    aVerGraficaDrone->setChecked (false);


    // En createActions(), define aRuta:
    aRuta = new QAction(tr("&Configuración de Rutas"), mPrincipal);
    aRuta->setShortcut(QKeySequence(tr("Ctrl+R")));
    aRuta->setStatusTip(tr("Abre la ventana de configuración de rutas"));
    aRuta->setCheckable(true);  // Para que se mantenga marcado cuando está visible

    connect(aRuta, &QAction::triggered, [this](bool checked) {
        if (checked) {
            m_wPuntos->show();
            m_wPuntos->raise();
            updateRutaPosition();   // posicionar en esquina inferior derecha
        } else {
            m_wPuntos->hide();
        }
    });   

    aGestionarDrones = new QAction(tr("&Gestionar Drones..."), this);
    aGestionarDrones->setStatusTip(tr("Abrir el gestor de drones"));
    connect(aGestionarDrones, &QAction::triggered, [this]() {
        DroneDialog dialog(DroneDialog::MANAGE_DRONES, mPrincipal);
        dialog.exec();
        // Actualizar combos si es necesario (se hace en Widget_Puntos al abrirse)
    });

    connect (aEstadoDrone,&QAction::triggered,[this]()
    {
        m_wParametros->setVisible (aEstadoDrone->isChecked ());
        m_wGraficas->setVisible (false);
        aVerGraficaDrone->setChecked (false);

    });

    connect (aVerGraficaDrone,&QAction::triggered,[this]()
    {
        m_wGraficas->setVisible (aVerGraficaDrone->isChecked ());
        m_wParametros->setVisible (false);
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

    aDistancia =  toolBar->addAction (QIcon(":/Iconos/Iconos/regla.png"),"Distancia");
    aDistancia->setCheckable (true);
    aDistancia->setChecked (false);
    toolBar->addSeparator ();

    aLupa =  toolBar->addAction (QIcon(":/Iconos/Iconos/Start Menu Search.png"),"ZoomArea");
    aLupa->setCheckable (true);
    aLupa->setChecked (false);
    aLupaMas =  toolBar->addAction (QIcon(":/Iconos/Iconos/Start Menu Search+.png"),"ZoomMas");
    aLupaMen =  toolBar->addAction (QIcon(":/Iconos/Iconos/Start Menu Search1-.png"),"ZoomMen");
    toolBar->addSeparator ();

    aPoligono =  toolBar->addAction (QIcon(":/Iconos/Iconos/poligono.png"),"Polígono");

}

void CControladora::conecciones()
{
    connect (tabOpciones,&QTabWidget::currentChanged, [this](int index)
    {
        if(tabOpciones->tabText (index) == "Configuración")
        {
            Database::AsyncQuery::startExecOnce(
                        "SELECT Nombre FROM DronTable",
                        [=](const Database::AsyncQueryResult& res) {
                for (int i = 0; i < res.count(); i++) {
                    auto str = res.value(i, "Nombre").toString();
                    if(m_wConexion->cBDrones ()->findText (str) < 0)
                        m_wConexion->cBDrones ()->addItem(str);
                }
                if (res.count() > 0) {

                    if(DronID.isEmpty ())
                        m_wConexion->cBDrones ()->setCurrentIndex(0);
                    else
                    {
                        int index = m_wConexion->cBDrones ()->findText (DronID);
                        m_wConexion->cBDrones ()->setCurrentIndex(index);
                    }
                    m_wConexion->setEnabled (true);
                }
                else
                {
                    m_wConexion->setEnabled (false);
                }
            });
        }

        if(tabOpciones->tabText (index) == "Control_Objetivo")
        {
            m_wControlObjetivo->setEnabled (!m_Conectado);

            if(!m_ControlObjetivo)
                Database::AsyncQuery::startExecOnce(
                            "SELECT Nombre FROM DronTable",
                            [=](const Database::AsyncQueryResult& res) {
                    QStringList drones;
                    for (int i = 0; i < res.count(); i++) {
                        drones.append(res.value(i, "Nombre").toString());
                    }
                    m_wControlObjetivo->setDrones (drones);
                });


        }

        if(tabOpciones->tabText (index) == "Datos_Drones")
        {
            m_wDrones->setEnabled (!m_Conectado);
            m_wDrones->actualizaTabla ();
        }

        if(tabOpciones->tabText (index) == "Mandos")
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

        if(tabOpciones->tabText (index) == "Ganacias_Control")
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

    connect(m_wConexion->chB_Guardar (), &QCheckBox::toggled,[=](bool checked)
    {
        if(checked)
        {
            QString queryVuelos = "SELECT Id_Dron, Vuelos FROM DronTable WHERE Nombre = \'" +DronID+ "\'";

            Database::AsyncQuery::startExecOnce(
                        queryVuelos,
                        [=](const Database::AsyncQueryResult& res) {
                dronID = res.value(0, "Id_Dron").toInt () ;
                vueloID = res.value(0, "Vuelos").toInt () + 1;

                QString queryUpdate = QString("UPDATE DronTable SET Vuelos = %1 WHERE Id_Dron = %2")
                        .arg (vueloID)
                        .arg (dronID);
                Database::AsyncQuery::startExecOnce(
                            queryUpdate,
                            [=](const Database::AsyncQueryResult& res) {

                });
            });
        }
    });

    connect (m_wControlObjetivo,&Widget_ControlObjetivo::datoContolObjetivo,this,&CControladora::recibirDatos);

    connect (m_wControlObjetivo,&Widget_ControlObjetivo::limpiarDatoContolObjetivo,this,&CControladora::on_EliminarPunto);

    connect (m_wControlObjetivo,&Widget_ControlObjetivo::detenerDatoContolObjetivo ,this,&CControladora::on_DetenerCO );

    connect (m_wControlObjetivo,&Widget_ControlObjetivo::iniciarDatoContolObjetivo ,this,&CControladora::on_IniciarCO);

    connect (m_wConexion,&Widget_Conexion::conectado, this,&CControladora::Conectado );

    connect (m_wMandos,&Widget_Mandos::enviaMandosDron, [this](){

        auto radioAcercamiento = mandos.Radio_Acercamiento;
        QByteArray sendMando;
        sendMando.append ("xyz");

        sendMando.append (reinterpret_cast<const char*>(&mandos),sizeof (sMANDOS));
        quint16 checksum = 0;
        for (char byte : sendMando) {
            checksum += static_cast<quint8>(byte);
        }
        sendMando.append (static_cast<char>(checksum & 0xFF));
        sendMando.append (static_cast<char>(checksum >>8));
        m_wConexion->sL_EnviaMandosDron (sendMando);

        libMapa->getOMapa()->setRadioAcercamiento (radioAcercamiento);
    });

    connect (m_wGanancias,&Widget_Ganancias_Control::enviaMandosDron, [this](){
        auto radioAcercamiento = mandos.Radio_Acercamiento;
        QByteArray sendMando;
        sendMando.append ("xyz");

        sendMando.append (reinterpret_cast<const char*>(&mandos),sizeof (sMANDOS));
        quint16 checksum = 0;
        for (char byte : sendMando) {
            checksum += static_cast<quint8>(byte);
        }
        sendMando.append (static_cast<char>(checksum & 0xFF));
        sendMando.append (static_cast<char>(checksum >>8));
        m_wConexion->sL_EnviaMandosDron (sendMando);
        libMapa->getOMapa()->setRadioAcercamiento (radioAcercamiento);

    });

    // Clic derecho en el mapa
    connect(libMapa->getOMapa(), &CMapaPlot::sig_devuelveWidgetClickDerechoParaDisenoExterno,
            [this](QGeoCoordinate puntoG) {
        if (m_esperandoPuntoInicial) {
            if (m_wPuntos) {
                m_wPuntos->agregarPuntoInicialYGenerarResto(puntoG);
            }
        }
    });

    connect(m_wConexion->serialManager (),&SerialManager::siVolverEnviar,[this](){
        onEnviaPlan ();
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

    connect(libMapa->getOMapa (), &CMapaPlot::sig_enviaPosActual,[this](QGeoCoordinate coord)
    {
        auto alt = alturaWorkerPTR->obtenerAltura (coord.latitude (),coord.longitude ());
        LogHandler& logHandler = LogHandler::getInstance();
        logHandler.appendLogMessage("Altura: "+ QString::number (alt));

    });

    connect(alturaWorkerPTR.get (), &AlturaWorker::alturaActualizada, [](qint16 altura) {
    });

    connect(libMapa->getOMapa(), &CMapaPlot::pointMoved,
            m_wPuntos, &Widget_Puntos::onPointMoved);

    connect(mPrincipal,&Ci_Principal::resize,[this](){
        updateOverlayPosition();
        // Solo actualizar posición de ruta si está visible
        if(m_wPuntos && m_wPuntos->isVisible()) {
            QTimer::singleShot(50, this, [this]() {
                updateRutaPosition();
            });
        }
    });

    connect(mPrincipal,&Ci_Principal::move, [this]() {
        if(m_wPuntos && m_wPuntos->isVisible()) {
            QTimer::singleShot(50, this, [this]() {
                updateRutaPosition();
            });
        }
    });

    ////acciones mapa en barra de herramientas
    connect (aDistancia,&QAction::triggered,[this]()
    {
        libMapa->accionarMedirDistancia ();
    });

    connect (aLupa,&QAction::triggered,[this]()
    {
        libMapa->getOMapa ()->AccionarZoomArea ();
        aDistancia->setChecked (false);
    });

    connect (libMapa->getOMapa (),&CMapaPlot::sig_finZoomArea ,[this]()
    {
        aLupa->setChecked (false);
    });

    connect (aLupaMas,&QAction::triggered,[this]()
    {
        libMapa->getOMapa ()->AccionarZoomMas ();
        aDistancia->setChecked (false);
    });

    connect (aLupaMen,&QAction::triggered,[this]()
    {
        libMapa->getOMapa ()->AccionarZoomMenos ();
        aDistancia->setChecked (false);
    });

    connect (aPoligono,&QAction::triggered,[this]()
    {
        libMapa->getOMapa ()->AccionarCrearPoligono ();
        aDistancia->setChecked (false);
    });

    // Acuse de recibo del plan de vuelo
    connect(m_wConexion->serialManager(), &SerialManager::si_AcuseReciboUVW,
            m_wPuntos, &Widget_Puntos::on_AcuseRecibo);

    // Pintar ruta en el mapa
    connect(m_wPuntos, &Widget_Puntos::sI_pintaRuta, [this](bool visible) {
        QList<EPuntoRuta> listPunto;
        QStringList nombrePuntos;
        for (const auto &punto : m_wPuntos->getModelo()->obtenerLista()) {
            EPuntoRuta puntoRuta;
            puntoRuta.pos = punto.pos;
            puntoRuta.radio = punto.radio;
            puntoRuta.descripcion = punto.nombre;
            listPunto.append(puntoRuta);
            nombrePuntos.append(punto.nombre);
        }
        libMapa->getOMapa()->pinta_PuntoRuta(listPunto, nombrePuntos, visible);
    });

    // Envío de la ruta (coordenadas simples)
    connect(m_wPuntos, &Widget_Puntos::sI_Ruta, [this](QList<QPair<double, double>> ruta) {
        if (m_Conectado) {
        }
    });

    // Envío de la ruta completa (objeto E_Punto)
    connect(m_wPuntos, &Widget_Puntos::sI_Ruta2, [this](QList<E_Punto> listaPuntos) {
        // Ya no usamos m_ListPuntos, se pasa directamente
        if (m_Conectado) {
            onEnviaPlan();   // onEnviaPlan() tomará los puntos del modelo
        }
    });

    // Cerrar ventana → desmarcar acción de menú
    connect(m_wPuntos, &Widget_Puntos::closed, [this]() {
        if (aRuta) aRuta->setChecked(false);
    });

    // También, si se destruye (poco probable), desmarcar
    connect(m_wPuntos, &Widget_Puntos::destroyed, [this]() {
        if (aRuta) aRuta->setChecked(false);
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

void CControladora::recibirDatos(QByteArray byteArray)
{
    QList<QByteArray> aux = byteArray.split(',');
    auto lat = aux.at(2).toFloat();
    auto lon = aux.at(3).toFloat();
    m_ParametrosValues[0] = aux.at(2).toFloat();
    m_ParametrosValues[1] = aux.at(3).toFloat();
    m_ParametrosValues[2] = aux.at(9).toFloat();
    auto cursoDeseado = qRadiansToDegrees(aux.at(10).toFloat());
    cursoDeseado = fmodf((360.0 + cursoDeseado), 360.0);
    m_ParametrosValues[3] = cursoDeseado;
    m_ParametrosValues[4] = aux.at(11).toFloat();
    m_ParametrosValues[5] = aux.at(12).toFloat();
    m_ParametrosValues[6] = aux.at(13).toFloat();
    m_ParametrosValues[7] = aux.at(14).toFloat();

    m_ParametrosValues[8] = aux.at(15).toFloat();
    m_ParametrosValues[9] = aux.at(16).toFloat();
    m_ParametrosValues[10] = aux.at(17).toFloat();
    m_ParametrosValues[11] = aux.at(18).toFloat();
    m_ParametrosValues[12] = aux.at(19).toFloat();
    m_ParametrosValues[13] = aux.at(20).toFloat();
    m_ParametrosValues[14] = aux.at(21).toFloat();
    m_ParametrosValues[15] = aux.at(22).toFloat();
    m_ParametrosValues[16] = aux.at(23).toFloat();
    m_ParametrosValues[17] = aux.at(24).toFloat();
    m_ParametrosValues[18] = aux.at(25).toFloat();

    m_ParametrosValues[19] = aux.at(26).toFloat();
    m_ParametrosValues[20] = aux.at(27).toFloat();
    m_ParametrosValues[21] = aux.at(28).toFloat();
    m_ParametrosValues[22] = aux.at(29).toFloat();

    m_ParametrosValues[23] = aux.at(30).toFloat();
    m_ParametrosValues[24] = aux.at(31).toFloat();
    updateParameters(m_ParametrosValues);

    indicador->setRoll ( aux.at(5).toFloat());
    indicador->setPitch (aux.at(4).toFloat());
    indicador->setHeading (aux.at(8).toFloat());
    indicador->setAirspeed (aux.at(6).toFloat());
    indicador->setAltitude (aux.at(7).toFloat());
    indicador->redraw ();

    QColor colorTraza = Qt::darkRed;
    QPixmap simbolo = QPixmap(RecursosDir.path ()+ "/Recursos/Iconos/avionComercialOSM4.png");
    if (aSatelital->isChecked())
    {
        colorTraza = Qt::yellow;
        simbolo = QPixmap(RecursosDir.path ()+ "/Recursos/Iconos/avionComercialSat4.png");
    }

    if (lat != 0)
    {
        QString descripcion = QGeoCoordinate(lat,lon).toString();
        libMapa->getOMapa ()->nuevoPunto (TipoPunto::VehiculoTerrestre,
                                          QGeoCoordinate(lat,lon),Actualizado,
                                          "PuntoNuevo",
                                          colorTraza,
                                          simbolo,
                                          descripcion,
                                          true,
                                          aux.at(7).toFloat(),
                                          aux.at(6).toFloat(),
                                          aux.at(9).toFloat());

        libMapa->getOMapa ()->getListVehiculos ().last ()->setCantPuntosTrayecAVisualizar (TODAS);
    }
}

QList<QPointF> CControladora::sL_cambiaProyeccion()
{
    QList<QPointF> listPunto;
    for (auto punto: m_wPuntos->getModelo ()->obtenerLista ())
    {
        QPointF pointf = libMapa->getOMapa()->getProjection()->forward(punto.pos);
        listPunto.append(QPoint (pointf.y (),pointf.x ()));
    }
    return listPunto;
}

QPointF CControladora::sL_cambiaProyeccionPunto(QGeoCoordinate pos)
{
    QPointF pointf = libMapa->getOMapa()->getProjection()->forward(pos);
    return pointf;
}

void CControladora::onNewTramaConstanteReceived(const char a, const char b, const char c, const sTRAMA1 &s_28, const sTRAMA2 &s_34)
{
    LogHandler& logHandler = LogHandler::getInstance();
    logHandler.appendLogMessage("TramaConstante(a,b,c)");
    if (a == 'a' && b == 'b' && c == 'c' )
    {
        logHandler.appendLogMessage("TramaConstante(a,b,c):Encabezado correcto");
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
                libMapa->getOMapa ()->pintaElipseHome(QGeoCoordinate(lat,lon));
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
            logHandler.appendLogMessage("TramaConstante(a,b,c):"
                                        "Parámetros actualizados");

            indicador->setPitch(m_wConexion->serialManager ()->getS28 ().Realimentacion_Cabeceo);
            indicador->setRoll (m_wConexion->serialManager ()->getS28 ().Realimentacion_Banqueo);
            indicador->setHeading (m_wConexion->serialManager ()->getS28 ().Realimentacion_Curso);
            indicador->setAirspeed (m_wConexion->serialManager ()->getS28 ().Realimentacion_Velocidad * 3.6);
            indicador->setAltitude (m_wConexion->serialManager ()->getS28 ().Realimentacion_Altura);
            indicador->redraw ();
            logHandler.appendLogMessage("TramaConstante(a,b,c):"
                                        "Indicador actualizado Cab:"+QString::number(m_wConexion->serialManager ()->getS28 ().Realimentacion_Cabeceo)+
                                        "Ban: "+QString::number(m_wConexion->serialManager ()->getS28 ().Realimentacion_Banqueo));

            //color de avion y traza en dependencia del modo de trabajo manu8al automatico semiautomatico
            QColor colorTraza = Qt::red;
            QPixmap simbolo = QPixmap(":/Iconos/AvionRojo.png");
            QString descripcion = "V: "+ QString::number (m_wConexion->serialManager ()->getS28 ().Realimentacion_Velocidad * 3.6,'f',2) +
                    " km/h\nH: " + QString::number(m_wConexion->serialManager ()->getS28 ().Realimentacion_Altura,'f',2) + " m";
            if(m_wConexion->serialManager ()->getS34().Realimentacion_Regimen == 0)
            {
                colorTraza = Qt::red;
                simbolo = QPixmap(":/Iconos/AvionRojo.png");
            }
            if(m_wConexion->serialManager ()->getS34().Realimentacion_Regimen == 1)
            {
                colorTraza = Qt::yellow;
                simbolo = QPixmap(":/Iconos/AvionAmarillo.png");
            }
            if(m_wConexion->serialManager ()->getS34().Realimentacion_Regimen == 2)
            {
                colorTraza = Qt::green;
                simbolo = QPixmap(":/Iconos/AvionVerde.png");
            }

            if (geoPos.latitude() != 0)
            {

                libMapa->getOMapa ()->nuevoPunto (TipoPunto::VehiculoTerrestre,
                                                  geoPos,Actualizado,
                                                  "PuntoNuevo",
                                                  colorTraza,
                                                  simbolo.scaled (40,40),
                                                  descripcion,
                                                  true,
                                                  m_wConexion->serialManager ()->getS28 ().Realimentacion_Velocidad *3.6,
                                                  m_wConexion->serialManager ()->getS28 ().Realimentacion_Curso,
                                                  m_wConexion->serialManager ()->getS28 ().Realimentacion_Altura);

                libMapa->getOMapa ()->getListVehiculos ().last ()->setCantPuntosTrayecAVisualizar (TODAS);

                auto x = m_wConexion->serialManager ()->getS34 ().Cumplir_Punto[0];
                auto y = m_wConexion->serialManager ()->getS34 ().Cumplir_Punto[1];
                auto z = m_wConexion->serialManager ()->getS34 ().Cumplir_Punto[2];

                auto puntoProxGeo = libMapa->getOMapa ()->getProjection ()->inverse (QPointF(y,x));
                rumboProxPunto = geoPos.azimuthTo (puntoProxGeo);

                auto cursoDeseado = m_wConexion->serialManager ()->getS34 ().Curso_Deseado;
                cursoDeseado = fmodf((360.0 + cursoDeseado), 360.0);

                bool automatico = false;
                automatico = m_wConexion->serialManager ()->getS34().Realimentacion_Regimen == 2 ? true : false;

                libMapa->getOMapa ()->pintaRumbosVehiculo (geoPos,
                                                           m_wConexion->serialManager ()->getS28 ().Realimentacion_Curso,
                                                           cursoDeseado,
                                                           puntoProxGeo,
                                                           QColor(0,0,255),QColor(120,0,255),QColor(0,255,0),
                                                           automatico);
                if(automatico)
                {
                    //actualizar color del proximo punto
                    int  index = listPuntoF.lastIndexOf(QPointF(x,y));
                    if(index > 0 )
                    {
                        if( index != indexAnterior)
                        {
                            if(!listPuntoF.isEmpty())
                                for(int i = 0;i < index; i++)
                                {
                                    libMapa->getOMapa ()->rellenaProximoPunto (i);
                                    indexAnterior = index;
                                }
                        }
                    }
                }

                logHandler.appendLogMessage("TramaConstante(a,b,c):"
                                            "Objetivo con rumbo representados");
            }
            //graficar
            double const t = start_time_.msecsTo(QTime::currentTime()) / 1000.0;
            m_wGraficas->datosTiempoReal (m_wConexion->serialManager ()->getS28 ().Realimentacion_Banqueo,
                                          m_wConexion->serialManager ()->getS28 ().Realimentacion_Cabeceo,
                                          /*m_wConexion->getSerie ()->getS28 ().rpm*/0,
                                          m_wConexion->serialManager ()->getS28 ().Realimentacion_Altura,
                                          m_wConexion->serialManager ()->getS28 ().Realimentacion_Velocidad *3.6,
                                          t);

            //guardar
            if(m_wConexion->chB_Guardar ()->isChecked ())
                guardaDatosDron ();
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
    if (puntos.size() != 10) {
        return;
    }

    listPuntoF = sL_cambiaProyeccion();   // convierte usando el modelo actualizado
    QByteArray data;
    data.append("uvw");

    for (int i = 0; i < puntos.size(); ++i) {
        sPUNTOXYZ punto;
        punto.modo = puntos.at(i).modo;
        punto.x = listPuntoF.at(i).x();
        punto.y = listPuntoF.at(i).y();
        punto.z = puntos.at(i).altura;          // antes era m_ListPuntos, ahora bien
        punto.velocidad = puntos.at(i).velocidad / 3.6;
        puntosEnviar[i] = punto;
    }

    data.append(reinterpret_cast<char*>(puntosEnviar), 10 * sizeof(sPUNTOXYZ));
    quint16 checkSum = m_wMandos->calculateChecksum(data);
    data.append(static_cast<char>(checkSum & 0xFF));
    data.append(static_cast<char>(checkSum >> 8));
    m_wConexion->serialManager()->serieWrite(data);
}

void CControladora::onNewCuadropteros(const sCUADROPTEROS &s_cuadropteros)
{
    LogHandler& logHandler = LogHandler::getInstance();
    logHandler.appendLogMessage("onNewCuadropteros");

    indicador->setRoll (s_cuadropteros.Realimentacion_Banqueo);
    indicador->setPitch (s_cuadropteros.Realimentacion_Cabeceo);
    indicador->setHeading (s_cuadropteros.Realimentacion_Curso);
    indicador->setAirspeed (s_cuadropteros.Realimentacion_Velocidad * 3.6);
    indicador->setAltitude (s_cuadropteros.Realimentacion_Altura);
    indicador->redraw ();

    auto latlon = libMapa->getOMapa()->getProjection()->inverse (s_cuadropteros.Realimentacion_Latitud,s_cuadropteros.Realimentacion_Longitud);
    logHandler.appendLogMessage("onNewCuadropteros: "
                                "Lat: " + QString::number (latlon.latitude()) +
                                "  Lon: " + QString::number (latlon.longitude()));

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
            libMapa->getOMapa ()->pintaElipseHome(geoPos);
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

        //color de avion y traza en dependencia del modo de trabajo manu8al automatico semiautomatico
        QColor colorTraza = Qt::red;
        QPixmap simbolo = QPixmap(":/Iconos/Iconos/rojo1.png");
        QString descripcion = "V: "+ QString::number (s_cuadropteros.Realimentacion_Velocidad * 3.6,'f',2) +
                " km/h\nH: " + QString::number(s_cuadropteros.Realimentacion_Altura,'f',2) + " m";
        if(s_cuadropteros.Realimentacion_Regimen == 0)
        {
            colorTraza = Qt::red;
            simbolo = QPixmap(":/Iconos/Iconos/rojo1.png");
        }
        if(s_cuadropteros.Realimentacion_Regimen == 1)
        {
            colorTraza = Qt::yellow;
            simbolo = QPixmap(":/Iconos/Iconos/amarillo1.png");
        }
        if(s_cuadropteros.Realimentacion_Regimen == 2)
        {
            colorTraza = Qt::green;
            simbolo = QPixmap(":/Iconos/Iconos/verde1.png");
        }

        //
        if (geoPos.latitude() != 0)
        {
            libMapa->getOMapa ()->nuevoPunto (TipoPunto::VehiculoTerrestre,
                                              geoPos,Actualizado,
                                              "PuntoNuevo",
                                              colorTraza,
                                              simbolo.scaled (40,40),
                                              descripcion,
                                              true,
                                              s_cuadropteros.Realimentacion_Velocidad *3.6,
                                              s_cuadropteros.Realimentacion_Curso,
                                              s_cuadropteros.Realimentacion_Altura);

            libMapa->getOMapa ()->getListVehiculos ().last ()->setCantPuntosTrayecAVisualizar (TODAS);

            auto x = s_cuadropteros.Cumplir_Punto[0];
            auto y = s_cuadropteros.Cumplir_Punto[1];
            auto z = s_cuadropteros.Cumplir_Punto[2];

            auto puntoProxGeo = libMapa->getOMapa ()->getProjection ()->inverse (QPointF(y,x));
            rumboProxPunto = geoPos.azimuthTo (puntoProxGeo);

            auto cursoDeseado = /*qRadiansToDegrees*/(s_cuadropteros.Curso_Deseado);
            cursoDeseado = fmodf((360.0 + cursoDeseado), 360.0);

            bool automatico = false;
            automatico = s_cuadropteros.Realimentacion_Regimen == 2 ? true : false;
            libMapa->getOMapa ()->pintaRumbosVehiculo (geoPos,
                                                       cursoDeseado,
                                                       s_cuadropteros.Realimentacion_Curso,

                                                       puntoProxGeo,
                                                       QColor(0,0,255),QColor(120,0,255),QColor(0,255,0),
                                                       automatico);
            if(automatico)
            {
                //actualizar color del proximo punto
                int  index = listPuntoF.lastIndexOf(QPointF(x,y));
                if(index > 0 )
                {
                    if( index != indexAnterior)
                    {
                        if(!listPuntoF.isEmpty())
                            for(int i = 0;i < index; i++)
                            {
                                libMapa->getOMapa ()->rellenaProximoPunto (i);
                                indexAnterior = index;
                            }
                    }
                }
            }

            logHandler.appendLogMessage("Objetivo con rumbo representados");

        }
        //graficar
        double const t = start_time_.msecsTo(QTime::currentTime()) / 1000.0;
        m_wGraficas->datosTiempoReal (m_wConexion->serialManager ()->getS28 ().Realimentacion_Banqueo,
                                      m_wConexion->serialManager ()->getS28 ().Realimentacion_Cabeceo,
                                      /*m_wConexion->getSerie ()->getS28 ().rpm*/0,
                                      m_wConexion->serialManager ()->getS28 ().Realimentacion_Altura,
                                      m_wConexion->serialManager ()->getS28 ().Realimentacion_Velocidad *3.6,
                                      t);

        //guardar
        if(m_wConexion->chB_Guardar ()->isChecked ())
            guardaDatosDron ();
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

    libMapa->getOMapa ()->borrarElementosPorNombre ("PuntoNuevo");

    if(libMapa->getOMapa ()->getPuntoItem ().text == NULL)
        return;
    libMapa->getOMapa ()->getPuntoItem ().text->setVisible (false);
    libMapa->getOMapa ()->getPuntoItem ().ellipse->setVisible (false);
    libMapa->getOMapa ()->getPuntoItem ().ellipse2->setVisible (false);
    libMapa->getOMapa ()->getLineaRumboDestino ()->setVisible (false);
    libMapa->getOMapa ()->getLineaRumboVehiculo ()->setVisible (false);
    libMapa->getOMapa ()->getLineaRumboCalculado ()->setVisible (false);
    libMapa->getOMapa ()->layer (sMapaZOOM)->replot ();
}

void CControladora::on_DetenerCO()
{
    m_ControlObjetivo = false;
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

void CControladora::guardaDatosDron()
{
    QString fecha =  QTime::currentTime ().toString ("hh:mm:ss");
    fecha.replace (":","-");
    query1 = "INSERT INTO DatosVuelos ("
             "Id_Vuelos, "
             "Dron_ID, "
             "Lat, "
             "Lon, "
             "Realimentacion_Cabeceo, "
             "Realimentacion_Banqueo, "
             "Realimentacion_Velocidad, "
             "Realimentacion_Altura, "
             "Realimentacion_Curso, "
             "Distancia_Chequeo, "
             "Curso_Deseado, "
             "Realimentacion_Regimen, "
             "Realimentacion_ComandoMision, "
             "Realimentacion_CursoIMU, "
             "CantidadSatelites, "

             "KPCabeceo, "
             "KICabeceo, "
             "KPBanqueo, "
             "KIBanqueo, "
             "KPVelocidad, "
             "KIVelocidad, "
             "KPAltura, "
             "KIAltura, "
             "KPCurso, "
             "KPRumbo, "
             "KIRumbo, "

             "Control_Estabilizadores, "
             "Control_Alerones, "
             "Control_Motor, "
             "Control_Rumbo, "

             "Convergencia_Banqueo, "
             "Convergencia_Cabeceo, "
             "Fecha) "

             "VALUES ("
             ":Id_Vuelos, "
             ":Dron_ID, "
             ":Lat, "
             ":Lon, "
             ":Realimentacion_Cabeceo, "
             ":Realimentacion_Banqueo, "
             ":Realimentacion_Velocidad, "
             ":Realimentacion_Altura, "
             ":Realimentacion_Curso, "
             ":Distancia_Chequeo, "
             ":Curso_Deseado, "
             ":Realimentacion_Regimen, "
             ":Realimentacion_ComandoMision, "
             ":Realimentacion_CursoIMU, "
             ":CantidadSatelites, "

             ":KPCabeceo, "
             ":KICabeceo, "
             ":KPBanqueo, "
             ":KIBanqueo, "
             ":KPVelocidad, "
             ":KIVelocidad, "
             ":KPAltura, "
             ":KIAltura, "
             ":KPCurso, "
             ":KPRumbo, "
             ":KIRumbo, "

             ":Control_Estabilizadores, "
             ":Control_Alerones, "
             ":Control_Motor, "
             ":Control_Rumbo, "

             ":Convergencia_Banqueo, "
             ":Convergencia_Cabeceo, "
             ":Fecha"
             ")";
    _aQuery->prepare (query1);

    _aQuery->bindValue(":Id_Vuelos", vueloID);
    _aQuery->bindValue(":Dron_ID", dronID);
    _aQuery->bindValue(":Lat", QString::number(geoPos.latitude (),'f',4));
    _aQuery->bindValue(":Lon", QString::number(geoPos.longitude (),'f',4));
    _aQuery->bindValue(":Realimentacion_Cabeceo", QString::number(m_wConexion->serialManager ()->getS28 ().Realimentacion_Banqueo,'f',2));
    _aQuery->bindValue(":Realimentacion_Banqueo", QString::number(m_wConexion->serialManager ()->getS28 ().Realimentacion_Cabeceo,'f',2));
    _aQuery->bindValue(":Realimentacion_Velocidad", QString::number(m_wConexion->serialManager ()->getS28 ().Realimentacion_Velocidad *3.6,'f',2));
    _aQuery->bindValue(":Realimentacion_Altura", QString::number(m_wConexion->serialManager ()->getS28 ().Realimentacion_Altura,'f',2));
    _aQuery->bindValue(":Realimentacion_Curso", QString::number(m_wConexion->serialManager ()->getS28 ().Realimentacion_Curso,'f',2));
    _aQuery->bindValue(":Distancia_Chequeo", QString::number(m_wConexion->serialManager ()->getS34 ().Distancia_Chequeo,'f',2));
    _aQuery->bindValue(":Curso_Deseado", QString::number(m_wConexion->serialManager ()->getS34 ().Curso_Deseado,'f',2));
    _aQuery->bindValue(":Realimentacion_Regimen", QString::number(m_wConexion->serialManager ()->getS34 ().Realimentacion_Regimen));
    _aQuery->bindValue(":Realimentacion_ComandoMision", QString::number(m_wConexion->serialManager ()->getS34 ().Realimentacion_ComendoMision));
    _aQuery->bindValue(":Realimentacion_CursoIMU", QString::number(m_wConexion->serialManager ()->getS28 ().Realimentacion_CursoIMO,'f',2));
    _aQuery->bindValue(":CantidadSatelites", QString::number(m_wConexion->serialManager ()->getS34 ().Cantidad_Satelites));

    _aQuery->bindValue(":KPCabeceo", QString::number(m_wConexion->serialManager ()->getS34 ().Ganancia_Cabeceo[0],'f',2));
    _aQuery->bindValue(":KICabeceo", QString::number(m_wConexion->serialManager ()->getS34 ().Ganancia_Cabeceo[1],'f',2));
    _aQuery->bindValue(":KPBanqueo", QString::number(m_wConexion->serialManager ()->getS34 ().Ganancia_Banqueo[0],'f',2));
    _aQuery->bindValue(":KIBanqueo", QString::number(m_wConexion->serialManager ()->getS34 ().Ganancia_Banqueo[1],'f',2));
    _aQuery->bindValue(":KPVelocidad", QString::number(m_wConexion->serialManager ()->getS34 ().Ganancia_Velocidad[0],'f',2));
    _aQuery->bindValue(":KIVelocidad", QString::number(m_wConexion->serialManager ()->getS34 ().Ganancia_Velocidad[1],'f',2));
    _aQuery->bindValue(":KPAltura", QString::number(m_wConexion->serialManager ()->getS34 ().Ganancia_Altura[0],'f',2));
    _aQuery->bindValue(":KIAltura", QString::number(m_wConexion->serialManager ()->getS34 ().Ganancia_Altura[1],'f',2));
    _aQuery->bindValue(":KPCurso", QString::number(m_wConexion->serialManager ()->getS34 ().Ganancia_Curso,'f',2));
    _aQuery->bindValue(":KPRumbo", QString::number(m_wConexion->serialManager ()->getS34 ().Ganancia_Rumbo[0],'f',2));
    _aQuery->bindValue(":KIRumbo", QString::number(m_wConexion->serialManager ()->getS34 ().Ganancia_Rumbo[1],'f',2));

    _aQuery->bindValue(":Control_Estabilizadores", QString::number(m_wConexion->serialManager ()->getS34 ().Control_Estabilizadores,'f',2));
    _aQuery->bindValue(":Control_Alerones", QString::number(m_wConexion->serialManager ()->getS34 ().Control_Alerones,'f',2));
    _aQuery->bindValue(":Control_Motor", QString::number(m_wConexion->serialManager ()->getS34 ().Control_Motor,'f',2));
    _aQuery->bindValue(":Control_Rumbo", QString::number(m_wConexion->serialManager ()->getS34 ().Control_Rumbo,'f',2));

    _aQuery->bindValue(":Convergencia_Banqueo", QString::number(m_wConexion->serialManager ()->getS34 ().Convergencia_Banqueo,'f',2));
    _aQuery->bindValue(":Convergencia_Cabeceo", QString::number(m_wConexion->serialManager ()->getS34 ().Convergencia_Cabeceo,'f',2));

    _aQuery->bindValue(":Fecha", QDateTime::currentDateTime ());
    _aQuery->startExec ();
}

void CControladora::updateOverlayPosition()
{   
    if(m_wParametros && libMapa->getOMapa ()) {
        // Calcular posición relativa al área visible
        QRect visibleRect = libMapa->getOMapa ()->geometry();
        int xPos = (visibleRect.right() - m_wParametros->width())/2;
        int yPos = visibleRect.top();

        // Ajustar altura si sobrepasa el área
        if(yPos + m_wParametros->height() > visibleRect.bottom()) {
            yPos = visibleRect.bottom() - m_wParametros->height();
        }

        m_wParametros->move(xPos, yPos);
    }
    if(m_wGraficas && libMapa->getOMapa ()) {
        m_wGraficas->setFixedWidth (libMapa->getOMapa ()->width () * 0.95);
        // Calcular posición relativa al área visible


        // Ajustar altura si sobrepasa el área
        QPoint newPos;
        newPos.setX ((libMapa->getOMapa ()->width ()- m_wGraficas->width ())/2);
        newPos.setY (10);

        m_wGraficas->move(newPos);
    }
}

void CControladora::updateRutaPosition()
{
    if(!m_wPuntos || !libMapa->getOMapa() || !m_wPuntos->isVisible()) {
        return;
    }

    //Usar el widget del mapa como referencia de pantalla
    QWidget *mapWidget = libMapa->getOMapa();

    // 1. Obtener esquina inferior derecha del mapa en coordenadas GLOBALES
    QPoint mapBottomRightGlobal = mapWidget->mapToGlobal(
                QPoint(mapWidget->width(), mapWidget->height()));

    // 2. Calcular posición global para m_wPuntos
    int globalX = mapBottomRightGlobal.x() - m_wPuntos->width() - 20;
    int globalY = mapBottomRightGlobal.y() - m_wPuntos->height() - 40;

    // 3. Asegurar que esté en la misma pantalla que el mapa
    QScreen *mapScreen = QGuiApplication::screenAt(mapWidget->mapToGlobal(QPoint(0, 0)));
    if (mapScreen) {
        QRect screenGeometry = mapScreen->availableGeometry();

        // Ajustar si se sale de la pantalla
        if (globalX + m_wPuntos->width() > screenGeometry.right()) {
            globalX = screenGeometry.right() - m_wPuntos->width() - 20;
        }
        if (globalX < screenGeometry.left()) {
            globalX = screenGeometry.left() + 20;
        }
        if (globalY + m_wPuntos->height() > screenGeometry.bottom()) {
            globalY = screenGeometry.bottom() - m_wPuntos->height() - 40;
        }
        if (globalY < screenGeometry.top()) {
            globalY = screenGeometry.top() + 40;
        }
    }

    // 4. Mover a posición global
    m_wPuntos->move(globalX, globalY);
}

void CControladora::loadInitialData()
{
    m_ParametrosValues.clear ();

    for(int i = 0; i < 25;i++)//24parametros
        m_ParametrosValues.append (0.0);
    updateParameters(m_ParametrosValues);
}

void CControladora::timerEvent(QTimerEvent *event)
{
    // Timer functionality removed - keeping override in header
}

void CControladora::updateStatus(double lat, double lon, bool isValid) {
    QString status = QString("Coordenada actual:"
                             "Lat: %1°"
                             "Lon: %2°"
                             "Estado: %3")
            .arg(lat, 0, 'f', 4)
            .arg(lon, 0, 'f', 4)
            .arg(isValid ? "VALIDA" : "INVALIDA");

    if(!isValid) {
        status += "¡ALERTA! Posición fuera del área de trabajo";
    }

    // Actualizar historial
    QString logEntry = QString("[%1] Lat: %2° Lon: %3° - %4")
            .arg(QTime::currentTime().toString("hh:mm:ss"))
            .arg(lat, 0, 'f', 4)
            .arg(lon, 0, 'f', 4)
            .arg(isValid ? "Valida" : "Invalida");
    LogHandler& logHandler = LogHandler::getInstance();
    logHandler.appendLogMessage (status);
}
