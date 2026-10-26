// ============================================================================
// Widget_Puntos.cpp
// ============================================================================

#include "Widget_Puntos.h"
#include "VentanaInforme.h"
#include "Planificacion/InformeRuta.h"
#include "dronedialog.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QProgressBar>
#include <QLabel>
#include <QGroupBox>
#include <QGridLayout>
#include <QSqlQuery>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QStyle>
#include <QListWidget>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QClipboard>
#include <QApplication>
#include <QProgressDialog>
#include <limits>
#include "Rutas.h"
#include "MyQSettings/myqsetting.h"

// ============================================================================
// IMPLEMENTACIÓN DE Widget_Puntos (VERSIÓN COMPACTA, SIN .ui)
// ============================================================================

/**
 * @brief Constructor del widget principal de planificación de rutas.
 * @param alturaWorker Puntero al worker de alturas (para obtener elevación del terreno).
 * @param parent Widget padre.
 */
Widget_Puntos::Widget_Puntos(AlturaWorker *alturaWorker, QWidget *parent) :
    QWidget(parent),
    mdb(nullptr),
    m_modoSeleccionPuntoInicial(false),
    m_alturaWorker(alturaWorker)
{
    // Es un panel (va en la parte de abajo de la ventana principal, dentro de
    // un QDockWidget que pone CControladora): sin barra de titulo propia ni
    // arrastre, y con los colores del tema.

    // Crear el modelo y la referencia a la base de datos
    modelo = new MiModelo();
    mdb = new QSqlDatabase(QSqlDatabase::database());
    modelo->setAlturaWorker(m_alturaWorker);

    // Perfil de altitud dentro del panel: la ruta sobre el terreno siempre a la
    // vista (antes era una ventana aparte que habia que abrir).
    m_perfilWidget = new PerfilAltitudWidget(m_alturaWorker, this);
    m_perfilWidget->setMinimumWidth(0);   // si no cabe se oculta (resizeEvent)
    connect(m_perfilWidget, &PerfilAltitudWidget::puntoSeleccionado, this, [this](int i) {
        tw_puntosRuta->selectRow(i);
    });

    // ------------------------------------------------------------
    // CREAR TODOS LOS WIDGETS DE LA INTERFAZ (antes en .ui)
    // ------------------------------------------------------------
    // Combo de rutas
    cB_Rutas = new QComboBox(this);
    cB_Rutas->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);

    // Tabla de puntos
    tw_puntosRuta = new QTableView(this);
    tw_puntosRuta->setModel(modelo);
    tw_puntosRuta->hideColumn(0);               // ID
    // Cada columna a su medida y la tabla con el ancho justo (ajustarAnchoTabla):
    // estirada a toda la pantalla, los numeros quedaban a un palmo unos de otros.
    for (int i = 1; i < modelo->columnCount(); ++i)
        tw_puntosRuta->horizontalHeader()->setSectionResizeMode(i, QHeaderView::ResizeToContents);
    tw_puntosRuta->horizontalHeader()->setStretchLastSection(false);
    tw_puntosRuta->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    tw_puntosRuta->verticalHeader()->setDefaultSectionSize(20);
    tw_puntosRuta->verticalHeader()->setVisible(false);
    tw_puntosRuta->setAlternatingRowColors(true);
    tw_puntosRuta->setSelectionBehavior(QAbstractItemView::SelectRows);
    tw_puntosRuta->setSelectionMode(QAbstractItemView::SingleSelection);
    tw_puntosRuta->setToolTip(tr("Doble clic en un punto: centra el mapa en él. "
                                 "Las celdas se editan escribiendo encima."));
    // Doble clic en la fila (fuera de una celda editable): el mapa va al punto.
    connect(tw_puntosRuta, &QTableView::doubleClicked, this, [this](const QModelIndex &indice) {
        const QList<E_Punto> &puntos = modelo->obtenerLista();
        if (indice.isValid() && indice.row() < puntos.size())
            emit sI_centrarEnPunto(puntos.at(indice.row()).pos);
    });
    QFont tablaFont = tw_puntosRuta->font();
    tablaFont.setPointSize(9);
    tw_puntosRuta->setFont(tablaFont);

    // Etiquetas de métricas
    label_Distancia = new QLabel("0 m", this);
    label_AutonomiaRest = new QLabel("0%", this);
    label_Tiempo = new QLabel("--:--", this);
    progressAutonomia = new QProgressBar(this);
    progressAutonomia->setFixedWidth(60);
    progressAutonomia->setRange(0, 100);
    progressAutonomia->setTextVisible(true);
    progressAutonomia->setFormat("%p%");
    progressAutonomia->setStyleSheet("QProgressBar::chunk { background-color: #4CAF50; }");
    label_Estado = new QLabel("Configure una ruta primero", this);
    label_Estado->setStyleSheet("color: red; font-weight: bold;");
    labelInfoDron = new QLabel("No configurado", this);
    label_PuntosFijos = new QLabel("Ruta fija de 10 puntos (no se pueden añadir/eliminar)", this);
    label_PuntosFijos->setStyleSheet("color: gray; font-style: italic;");

    // Botones principales
    pB_CrearRuta = new QPushButton("Nueva", this);
    pB_EliminaRuta = new QPushButton("Eliminar", this);
    pB_LimpiaRuta = new QPushButton("Restablecer", this);
    pb_guardar = new QPushButton("Guardar", this);
    pB_EnviaRuta = new QPushButton("Enviar", this);
    pb_Mostrar = new QPushButton("Mostrar", this);
    pb_Mostrar->setCheckable(true);
    pB_AnalisisDetallado = new QPushButton("Análisis", this);
    pB_CancelarSeleccion = new QPushButton("Cancelar", this);
    // A cuanto por encima del terreno se ponen los puntos al generar la ruta;
    // se recuerda en conf.ini.
    m_spinSobreTerreno = new QSpinBox(this);
    m_spinSobreTerreno->setRange(5, 3000);
    m_spinSobreTerreno->setSingleStep(10);
    m_spinSobreTerreno->setSuffix(tr(" m"));
    m_spinSobreTerreno->setValue(MyQSetting().cargarSetting(Rutas::confIni(), QStringLiteral("sobre_terreno"),
                                                            QStringLiteral("RUTAS"), 50).toInt());
    m_spinSobreTerreno->setToolTip(tr("Altura sobre el terreno, como mínimo en toda la ruta, al crearla (clic "
                                      "derecho en el mapa) o al restablecerla: cada punto queda así por encima "
                                      "de lo más alto de sus tramos. Al arrastrar un punto conserva la suya."));
    connect(m_spinSobreTerreno, QOverload<int>::of(&QSpinBox::valueChanged), this, [](int v) {
        MyQSetting().guardarSetting(Rutas::confIni(), QStringLiteral("sobre_terreno"), v, QStringLiteral("RUTAS"));
    });
    pB_CancelarSeleccion->setVisible(false);

    // Botones ocultos (por compatibilidad con código existente)
    pB_EliminaPunto = new QPushButton("Eliminar punto", this);
    pB_EliminaPunto->setVisible(false);
    pB_Cargar = new QPushButton("Cargar", this);
    pB_Cargar->setVisible(false);
    pB_ExportarAnalisis = new QPushButton("Exportar análisis", this);
    pB_ExportarAnalisis->setVisible(false);

    // Botones adicionales
    m_btnPerfil = new QPushButton(tr("Ampliar perfil"), this);
    m_btnPerfil->setToolTip(tr("El perfil de altitud y terreno en una ventana grande"));
    connect(m_btnPerfil, &QPushButton::clicked, this, &Widget_Puntos::mostrarPerfil);

    QPushButton *btnGuia = new QPushButton("Guía", this);
    btnGuia->setToolTip("Mostrar guía para establecer una ruta correcta");
    connect(btnGuia, &QPushButton::clicked, this, &Widget_Puntos::mostrarGuia);

    // ------------------------------------------------------------
    // CONSTRUIR EL PANEL: ruta | tabla | resumen y acciones
    // ------------------------------------------------------------
    setStyleSheet(QStringLiteral(
        "QLabel[rol=\"seccion\"] { color: #9e9e9e; font-size: 10px; font-weight: bold; }"
        "QLabel[rol=\"dato\"] { font-size: 15px; font-weight: bold; }"
        "QLabel[rol=\"unidad\"] { color: #9e9e9e; font-size: 10px; }"
        "QPushButton { min-height: 26px; padding: 0 8px; }"
        "QPushButton#enviar { min-height: 40px; font-size: 14px; font-weight: bold; }"
        "QPushButton#enviar:enabled { background-color: #1565c0; color: white; border-radius: 4px; }"
        // Interruptor: encendido con borde y texto verdes, sin rellenar (relleno
        // competia con «Enviar al dron», que es la accion principal).
        "QPushButton#verMapa:checked { border: 1px solid #43a047; color: #66bb6a; font-weight: bold; }"));
    auto seccion = [](const QString &texto) {
        auto *l = new QLabel(texto.toUpper());
        l->setProperty("rol", "seccion");
        return l;
    };

    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(6, 4, 6, 4);
    mainLayout->setSpacing(10);

    // ---- IZQUIERDA: que ruta y su gestion ----
    QWidget *izquierda = new QWidget(this);
    izquierda->setFixedWidth(230);
    QVBoxLayout *izq = new QVBoxLayout(izquierda);
    izq->setContentsMargins(0, 0, 0, 0);
    izq->setSpacing(4);
    izq->addWidget(seccion(tr("Ruta")));
    izq->addWidget(cB_Rutas);
    labelInfoDron->setWordWrap(true);
    labelInfoDron->setStyleSheet(QStringLiteral("color: #9e9e9e; font-size: 11px;"));
    izq->addWidget(labelInfoDron);
    QGridLayout *gestion = new QGridLayout();
    gestion->setSpacing(4);
    gestion->addWidget(pB_CrearRuta, 0, 0);
    gestion->addWidget(pb_guardar, 0, 1);
    gestion->addWidget(pB_LimpiaRuta, 1, 0);
    gestion->addWidget(pB_EliminaRuta, 1, 1);
    // Altura sobre el terreno de las rutas nuevas (clic derecho y Restablecer).
    auto *etiquetaSobre = new QLabel(tr("Sobre el terreno"));
    etiquetaSobre->setToolTip(m_spinSobreTerreno->toolTip());
    gestion->addWidget(etiquetaSobre, 2, 0);
    gestion->addWidget(m_spinSobreTerreno, 2, 1);
    gestion->addWidget(pB_CancelarSeleccion, 3, 0, 1, 2);
    izq->addLayout(gestion);
    izq->addStretch();
    label_PuntosFijos->setText(tr("10 puntos fijos: se mueven arrastrándolos en el mapa "
                                  "o editando la tabla"));
    label_PuntosFijos->setWordWrap(true);
    label_PuntosFijos->setStyleSheet(QStringLiteral("color: #9e9e9e; font-size: 10px; font-style: italic;"));
    izq->addWidget(label_PuntosFijos);
    mainLayout->addWidget(izquierda);

    // ---- CENTRO: la tabla de puntos y, al lado, el perfil de altitud ----
    mainLayout->addWidget(tw_puntosRuta, 0);
    mainLayout->addWidget(m_perfilWidget, 1);

    // ---- DERECHA: resumen de la ruta y acciones ----
    QWidget *derecha = new QWidget(this);
    derecha->setFixedWidth(260);
    QVBoxLayout *der = new QVBoxLayout(derecha);
    der->setContentsMargins(0, 0, 0, 0);
    der->setSpacing(4);
    der->addWidget(seccion(tr("Resumen")));
    QGridLayout *resumen = new QGridLayout();
    resumen->setHorizontalSpacing(12);
    resumen->setVerticalSpacing(0);
    auto dato = [&](QLabel *valor, const QString &nombre, int columna) {
        valor->setProperty("rol", "dato");
        auto *n = new QLabel(nombre);
        n->setProperty("rol", "unidad");
        resumen->addWidget(valor, 0, columna);
        resumen->addWidget(n, 1, columna);
    };
    dato(label_Distancia, tr("distancia"), 0);
    dato(label_Tiempo, tr("tiempo (min:s)"), 1);
    dato(label_AutonomiaRest, tr("autonomía que queda"), 2);
    der->addLayout(resumen);
    progressAutonomia->setMinimumWidth(0);
    progressAutonomia->setMaximumWidth(QWIDGETSIZE_MAX);
    progressAutonomia->setFixedHeight(8);
    progressAutonomia->setTextVisible(false);
    progressAutonomia->setToolTip(tr("Parte de la autonomía del dron que gasta la ruta"));
    der->addWidget(progressAutonomia);
    label_Estado->setAlignment(Qt::AlignCenter);
    // Pulsar el estado abre el analisis (es donde se mira que le pasa).
    label_Estado->setCursor(Qt::PointingHandCursor);
    label_Estado->installEventFilter(this);
    der->addWidget(label_Estado);

    der->addWidget(seccion(tr("Acciones")));
    pb_Mostrar->setObjectName(QStringLiteral("verMapa"));
    pb_Mostrar->setText(tr("Ver en el mapa"));
    QGridLayout *acciones = new QGridLayout();
    acciones->setSpacing(4);
    acciones->addWidget(pb_Mostrar, 0, 0, 1, 2);
    acciones->addWidget(m_btnPerfil, 1, 0);
    acciones->addWidget(pB_AnalisisDetallado, 1, 1);
    acciones->addWidget(btnGuia, 2, 0, 1, 2);
    der->addLayout(acciones);
    der->addStretch();
    pB_EnviaRuta->setObjectName(QStringLiteral("enviar"));
    pB_EnviaRuta->setText(tr("Enviar al dron"));
    der->addWidget(pB_EnviaRuta);
    mainLayout->addWidget(derecha);

    // ------------------------------------------------------------
    // CONEXIONES Y CONFIGURACIÓN INICIAL
    // ------------------------------------------------------------
    // Conexiones de los botones
    connect(pB_CrearRuta, &QPushButton::clicked, this, &Widget_Puntos::on_pB_CrearRuta_clicked);
    connect(pB_EliminaRuta, &QPushButton::clicked, this, &Widget_Puntos::on_pB_EliminaRuta_clicked);
    connect(pB_LimpiaRuta, &QPushButton::clicked, this, &Widget_Puntos::on_pB_LimpiaRuta_clicked);
    connect(pb_guardar, &QPushButton::clicked, this, &Widget_Puntos::on_pb_guardar_clicked);
    connect(pB_EnviaRuta, &QPushButton::clicked, this, &Widget_Puntos::on_pB_EnviaRuta_clicked);
    connect(pb_Mostrar, &QPushButton::toggled, this, &Widget_Puntos::on_pb_Mostrar_toggled);
    connect(pB_AnalisisDetallado, &QPushButton::clicked, this, &Widget_Puntos::on_pB_AnalisisDetallado_clicked);
    connect(pB_CancelarSeleccion, &QPushButton::clicked, this, &Widget_Puntos::cancelarSeleccionPuntoInicial);

    // Conexiones del modelo
    connect(modelo, &MiModelo::rutaModificada, this, &Widget_Puntos::mostrarMetricasRuta, Qt::UniqueConnection);
    connect(modelo, &MiModelo::rutaConfigurada, this, &Widget_Puntos::onRutaConfigurada, Qt::UniqueConnection);
    connect(modelo, &QAbstractItemModel::dataChanged,
            [this](const QModelIndex &topLeft, const QModelIndex &topRight, const QVector<int> &roles) {
        Q_UNUSED(topLeft); Q_UNUSED(topRight); Q_UNUSED(roles);
        emit sI_pintaRuta(true);
        mostrarMetricasRuta();
    });

    // Conexión del combo de rutas
    connect(cB_Rutas, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int index) {
        if (index >= 0) {
            QString realName = cB_Rutas->itemData(index, Qt::UserRole).toString();
            if (!realName.isEmpty()) seleccionarRuta(realName);
        }
    });

    // Inicializar combo de rutas
    cB_Rutas->blockSignals(true);
    actualizarComboBoxRutas();
    if (cB_Rutas->count() > 0) {
        cB_Rutas->setCurrentIndex(0);
    }
    cB_Rutas->blockSignals(false);
    if (cB_Rutas->count() > 0) {
        seleccionarRuta(cB_Rutas->itemData(0, Qt::UserRole).toString());
    }

    // Timer para acuse de recibo
    m_originalStyle = pB_EnviaRuta->styleSheet();
    connect(&m_timer, &QTimer::timeout, this, &Widget_Puntos::on_restoreStyle);

    // Al arrancar la ruta no se dibuja, pero los botones quedan segun la ruta
    // cargada (antes «Mostrar» y «Análisis» quedaban desactivados aunque hubiera
    // una ruta valida, y no se podia ver en el mapa sin cambiar de ruta).
    pb_Mostrar->setChecked(false);
    updateUIForDroneConfigured(modelo->isDroneConfigured());
}

Widget_Puntos::~Widget_Puntos()
{
    delete m_perfilWidget;
    delete modelo;
    delete mdb;
}

// ------------------------------------------------------------
// ACCESO A DATOS
// ------------------------------------------------------------

/**
 * @brief Obtiene el puntero al modelo de datos.
 * @return MiModelo*.
 */
MiModelo *Widget_Puntos::getModelo() const
{
    return modelo;
}

/**
 * @brief Obtiene el combo box de selección de rutas.
 * @return QComboBox*.
 */
QComboBox *Widget_Puntos::getBoxNombreRuta() const
{
    return cB_Rutas;
}

/**
 * @brief Indica si la ruta está actualmente visible en el mapa.
 * @return true si visible.
 */
bool Widget_Puntos::isRouteVisible() const
{
    return pb_Mostrar->isChecked();
}

/**
 * @brief Establece la visibilidad de la ruta en el mapa.
 * @param visible true para mostrar, false para ocultar.
 */
void Widget_Puntos::setRouteVisible(bool visible)
{
    pb_Mostrar->setChecked(visible);
}

// ------------------------------------------------------------
// MÉTODOS PRIVADOS AUXILIARES
// ------------------------------------------------------------



// ------------------------------------------------------------
// ACTUALIZACIÓN DE INTERFAZ
// ------------------------------------------------------------

/**
 * @brief Actualiza el combo box de rutas consultando las tablas existentes en la base de datos.
 */
void Widget_Puntos::actualizarComboBoxRutas()
{
    QString currentRoute = modelo->getCurrentRouteName();
    cB_Rutas->blockSignals(true);
    cB_Rutas->clear();

    QSqlQuery query("SELECT name FROM sqlite_master WHERE type = 'table' AND name NOT LIKE 'sqlite_%' "
                    "AND name NOT LIKE 'drones' AND name NOT LIKE 'rutas_metadata'");
    while (query.next()) {
        QString ruta = query.value(0).toString();
        QSqlQuery metaQuery;
        metaQuery.prepare("SELECT rm.nombre_ruta, d.nombre, d.tipo "
                          "FROM rutas_metadata rm "
                          "LEFT JOIN drones d ON rm.drone_id = d.id "
                          "WHERE rm.nombre_ruta = ?");
        metaQuery.addBindValue(ruta);
        if (metaQuery.exec() && metaQuery.next()) {
            QString droneNombre = metaQuery.value(1).toString();
            QString droneTipo = metaQuery.value(2).toString();
            if (!droneNombre.isEmpty()) {
                QString displayText = QString("%1").arg(ruta);
                cB_Rutas->addItem(displayText);
                cB_Rutas->setItemData(cB_Rutas->count()-1, ruta, Qt::UserRole);
            } else {
                QString displayText = QString("%1").arg(ruta);
                cB_Rutas->addItem(displayText);
                cB_Rutas->setItemData(cB_Rutas->count()-1, ruta, Qt::UserRole);
            }
        } else {
            QString displayText = QString("%1").arg(ruta);
            cB_Rutas->addItem(displayText);
            cB_Rutas->setItemData(cB_Rutas->count()-1, ruta, Qt::UserRole);
        }
    }
    // Restaurar selección
    if (!currentRoute.isEmpty()) {
        setCurrentRouteInCombo(currentRoute, false);
    } else if (cB_Rutas->count() > 0) {
        cB_Rutas->setCurrentIndex(0);
    }

    cB_Rutas->blockSignals(false);
}

// Cifras de la ruta (distancia, autonomia, tiempo) y su estado con lo que la
// revision ha visto: en la pastilla cuantos errores y avisos, en el tooltip
// cuales. Si el analisis esta abierto, se rehace tambien.
void Widget_Puntos::mostrarMetricasRuta()
{
    refrescarPerfil();
    if (!modelo->isDroneConfigured() || modelo->rowCount() == 0) {
        label_Distancia->setText("0 m");
        label_AutonomiaRest->setText("0%");
        progressAutonomia->setValue(0);
        label_Tiempo->setText("--:--");
        label_Estado->setText(modelo->isDroneConfigured() ? "Ruta vacía" : "No configurado");
        label_Estado->setToolTip(QString());
        label_Estado->setStyleSheet(modelo->isDroneConfigured() ?
                                        "color: blue; font-weight: bold;" :
                                        "color: gray; font-weight: bold;");
        refrescarInforme();
        return;
    }

    const RevisionRuta &r = modelo->revision();
    label_Distancia->setText(QString("%1 m").arg(r.distancia, 0, 'f', 0));
    label_AutonomiaRest->setText(QString("%1%").arg(100.0 - r.usoDistancia * 100.0, 0, 'f', 0));
    if (r.tiempo > 0) {
        const int s = int(std::lround(r.tiempo));
        label_Tiempo->setText(QString("%1:%2").arg(s / 60).arg(s % 60, 2, 10, QChar('0')));
    } else {
        label_Tiempo->setText("--:--");
    }
    progressAutonomia->setValue(qBound(0, int(std::lround(r.usoDistancia * 100.0)), 100));
    progressAutonomia->setFormat("");

    // En el tooltip, lo que hay que mirar (los primeros) y donde.
    QStringList detalle;
    for (int i = 0; i < r.hallazgos.size() && i < 8; ++i) {
        const Hallazgo &h = r.hallazgos.at(i);
        detalle << QStringLiteral("<span style='color:%1'>%2</span> · %3: %4")
                       .arg(InformeRuta::colorGravedad(h.gravedad), InformeRuta::nombreGravedad(h.gravedad),
                            h.lugar.toHtmlEscaped(), h.titulo.toHtmlEscaped());
    }
    if (r.hallazgos.size() > 8)
        detalle << tr("… y %1 más").arg(r.hallazgos.size() - 8);
    const QString ayuda = detalle.isEmpty() ? tr("Nada que revisar.") : detalle.join(QStringLiteral("<br>"));

    QColor color(67, 160, 71);
    if (!r.valida())
        color = QColor(InformeRuta::colorGravedad(Hallazgo::Gravedad::Error));
    else if (r.cuantos(Hallazgo::Gravedad::Aviso) > 0)
        color = QColor(InformeRuta::colorGravedad(Hallazgo::Gravedad::Aviso));
    QString texto = InformeRuta::estado(r);
    if (modelo->rowCount() != 10)
        texto += tr(" · faltan puntos (%1 de %2)").arg(modelo->rowCount()).arg(10);
    ponerEstado(texto, color, ayuda);
    refrescarInforme();
}

// Estado de la ruta como una pastilla de color (verde, ambar o rojo) con lo
// que hay que mirar en el tooltip; pulsandola se abre el analisis.
void Widget_Puntos::ponerEstado(const QString &texto, const QColor &color, const QString &detalle)
{
    label_Estado->setText(texto);
    label_Estado->setToolTip(detalle + tr("<br><br><i>Pulsa aquí (o «Análisis») para ver el porqué y cómo "
                                          "arreglarlo.</i>"));
    label_Estado->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: rgba(%2, %3, %4, 40); border: 1px solid %1;"
        " border-radius: 4px; padding: 2px 6px; font-weight: bold; }")
        .arg(color.name()).arg(color.red()).arg(color.green()).arg(color.blue()));
}

// Abre (o trae delante) la ventana del analisis. No es modal: se puede dejar
// abierta mientras se corrige, y pinchar en un problema lleva a su punto.
void Widget_Puntos::mostrarAnalisisDetallado()
{
    if (!modelo->isDroneConfigured()) {
        QMessageBox::information(this, "Análisis", "Configure un dron primero.");
        return;
    }
    if (!m_informe) {
        m_informe = new VentanaInforme(this);
        connect(m_informe, &VentanaInforme::puntoElegido, this, [this](int fila) {
            if (fila >= 0 && fila < modelo->rowCount()) {
                tw_puntosRuta->selectRow(fila);
                tw_puntosRuta->scrollTo(modelo->index(fila, 0));
            }
        });
    }
    refrescarInforme(true);
    m_informe->show();
    m_informe->raise();
    m_informe->activateWindow();
}

// El informe al dia con la revision, si la ventana esta abierta (o \a aunqueOculta).
void Widget_Puntos::refrescarInforme(bool aunqueOculta)
{
    if (!m_informe || (!aunqueOculta && !m_informe->isVisible()))
        return;
    m_informe->mostrar(modelo->revision(), modelo->getDroneCharacteristics(), modelo->getCurrentRouteName(),
                       modelo->obtenerLista());
}


// ------------------------------------------------------------
// SLOTS PÚBLICOS
// ------------------------------------------------------------

/**
 * @brief Agrega un nuevo punto a la ruta (llamado desde el mapa o desde otros módulos).
 * @param punto Punto a agregar.
 */
void Widget_Puntos::sL_agregarNuevoPunto(const E_Punto &punto)
{
    modelo->validarYAgregarElemento(punto);
    mostrarMetricasRuta();
    updateUIForDroneConfigured(true);
    tw_puntosRuta->viewport()->update();
    tw_puntosRuta->resizeColumnsToContents();
}

/**
 * @brief Actualiza puntos existentes en la ruta (llamado desde el mapa).
 * @param puntos Lista de puntos modificados.
 */
void Widget_Puntos::sL_actualizaPunto(const QList<E_Punto> &puntos)
{
    if (puntos.isEmpty()) return;
    if (!modelo->isDroneConfigured()) return;

    QString nombreRuta = cB_Rutas->currentText();
    if (nombreRuta.isEmpty()) return;

    QString nombreRutaLimpio = obtenerNombreTablaLimpio(nombreRuta);
    modelo->actualizarElementosEnTabla(nombreRutaLimpio, puntos);
    mostrarMetricasRuta();
    emit sI_pintaRuta(true);
    tw_puntosRuta->viewport()->update();
    tw_puntosRuta->resizeColumnsToContents();
    updateUIForDroneConfigured(true);
}

/**
 * @brief Selecciona la ruta cuyo nombre real está en el combo (usado internamente).
 * @param nombreRuta No usado.
 */
void Widget_Puntos::seleccionarRuta(const QString &/*nombreRuta*/)
{
    int index = cB_Rutas->currentIndex();
    if (index < 0) return;

    QString realRouteName = cB_Rutas->itemData(index, Qt::UserRole).toString();
    if (!realRouteName.isEmpty()) {
        cargarRuta(realRouteName);
    }
}

/**
 * @brief Muestra un acuse de recibo visual en el botón de enviar.
 */
/**
 * @brief Recuerda si hay enlace con el dron (lo avisa CControladora al conectar y desconectar).
 * @param disponible true mientras la estación está conectada.
 */
void Widget_Puntos::setEnlaceDisponible(bool disponible)
{
    m_enlaceDisponible = disponible;
    refrescarBotonEnviar();
}

void Widget_Puntos::on_AcuseRecibo()
{
    if (!m_timer.isActive())
        m_originalStyle = pB_EnviaRuta->styleSheet();
    pB_EnviaRuta->setStyleSheet("background-color: #00FF00; color: black; font-weight: bold; border-radius: 4px;");
    m_timer.start(1000);
}

/**
 * @brief Restaura el estilo original del botón de enviar.
 */
void Widget_Puntos::on_restoreStyle()
{
    pB_EnviaRuta->setStyleSheet(m_originalStyle);
    m_timer.stop();
}

/**
 * @brief Slot llamado cuando el modelo emite rutaConfigurada.
 * @param tipoDron Tipo de dron configurado.
 * @param nombreRuta Nombre de la ruta configurada.
 */
void Widget_Puntos::onRutaConfigurada(const QString &tipoDron, const QString &nombreRuta)
{
    QMessageBox::information(this, "Ruta Configurada",
                             QString("Ruta '%1' configurada exitosamente.\n"
                                     "Tipo de dron: %2")
                             .arg(nombreRuta).arg(tipoDron));

    updateUIForDroneConfigured(true);
    mostrarInfoDronActual();
    emit sI_DronConfigurado(tipoDron, nombreRuta);
    mostrarMetricasRuta();
    emit sI_rutasModificadas();
    emit sI_rutaSeleccionada(nombreRuta);
}


// El operador ha soltado el punto \a index en \a nuevaPos: el modelo lo mueve
// conservando su altura sobre el terreno (MiModelo::moverPunto).
void Widget_Puntos::onPointMoved(int index, const QGeoCoordinate &nuevaPos)
{
    if (!modelo || !modelo->isDroneConfigured() || modelo->rowCount() == 0)
        return;
    if (index < 0 || index >= modelo->rowCount() || !nuevaPos.isValid())
        return;

    // Conserva su altura sobre el terreno; todo va al modelo de una vez (un
    // solo repintado de la ruta, sin estados a medio mover).
    modelo->moverPunto(index, nuevaPos);
}

/**
 * @brief Inicia el modo de selección del punto inicial (P1) haciendo clic derecho en el mapa.
 */
void Widget_Puntos::iniciarSeleccionPuntoInicial()
{
    if (!modelo->isDroneConfigured()) {
        QMessageBox::warning(this, "Error", "No hay ruta configurada.");
        return;
    }
    if (modelo->rowCount() > 0) {
        QMessageBox::warning(this, "Error", "La ruta ya tiene puntos. No se puede iniciar selección.");
        return;
    }

    m_modoSeleccionPuntoInicial = true;
    label_Estado->setText("Seleccione punto inicial P1 en el mapa (clic derecho)");
    label_Estado->setStyleSheet("color: blue; font-weight: bold;");
    pB_CancelarSeleccion->setVisible(true);

    emit sI_iniciarSeleccionPuntoInicial();
}

/**
 * @brief Cancela el modo de selección de punto inicial.
 */
void Widget_Puntos::cancelarSeleccionPuntoInicial()
{
    if (!m_modoSeleccionPuntoInicial) return;

    m_modoSeleccionPuntoInicial = false;
    pB_CancelarSeleccion->setVisible(false);
    label_Estado->setText("Selección cancelada");
    label_Estado->setStyleSheet("color: orange; font-weight: bold;");

    emit sI_cancelarSeleccionPuntoInicial();
}

// Clic derecho en el mapa en modo "elegir P1": sale del modo y genera la ruta
// desde ahi (generarRutaDesde).
void Widget_Puntos::agregarPuntoInicialYGenerarResto(const QGeoCoordinate &coordenada)
{
    if (!m_modoSeleccionPuntoInicial) return;
    if (!modelo->isDroneConfigured()) {
        cancelarSeleccionPuntoInicial();
        return;
    }
    m_modoSeleccionPuntoInicial = false;
    pB_CancelarSeleccion->setVisible(false);
    emit sI_cancelarSeleccionPuntoInicial();
    generarRutaDesde(coordenada);
}

// Rehace los 10 puntos desde \a p1, cada uno a la altura sobre el terreno
// elegida en el panel. Lo usan el clic derecho y «Restablecer». Si falta la
// elevacion en algun punto, avisa: ahi la altura cuenta desde el nivel del mar.
void Widget_Puntos::generarRutaDesde(const QGeoCoordinate &p1)
{
    bool conTerreno = true;
    const QList<E_Punto> puntos = modelo->generarRuta(p1, m_spinSobreTerreno->value(), &conTerreno);
    modelo->borraTodosLosElementos();
    for (const E_Punto &punto : puntos)
        modelo->agregarElemento(punto);

    updateUIForDroneConfigured(true);
    modelo->revalidarRutaCompleta();
    emit sI_pintaRuta(pb_Mostrar->isChecked());
    if (!conTerreno)
        QMessageBox::warning(this, tr("Sin datos de elevación"),
                             tr("En parte de la ruta no hay datos de elevación: ahí la altura se ha puesto "
                                "a %1 m sobre el nivel del mar, sin saber cuánto hay de terreno debajo.\n\n"
                                "Revisa esas alturas a mano (el perfil y la columna «S/terr.» no lo pueden "
                                "comprobar).").arg(m_spinSobreTerreno->value()));
}

/**
 * @brief Carga una ruta desde la base de datos y actualiza la interfaz.
 * @param routeName Nombre de la ruta a cargar.
 */
void Widget_Puntos::cargarRuta(const QString &routeName)
{
    if (routeName.isEmpty()) return;

    modelo->CargarRutaDesdeBaseDatos(routeName);
    updateUIForDroneConfigured(modelo->isDroneConfigured());
    mostrarMetricasRuta();

    if (modelo->rowCount() > 0) {
        if (!pb_Mostrar->isChecked()) {
            pb_Mostrar->setChecked(true);
        } else {
            emit sI_pintaRuta(true);
        }
    } else {
        pb_Mostrar->setChecked(false);
        emit sI_pintaRuta(false);
    }

    // Sincronizar el combo con el nombre real
    setCurrentRouteInCombo(routeName, true);

    emit sI_rutaSeleccionada(routeName);
    emit sI_visibilidadCambiada(routeName, pb_Mostrar->isChecked());
}

/**
 * @brief Establece la ruta actual en el combo box (por nombre real).
 * @param routeName Nombre real de la ruta.
 * @param blockSignals Si true, bloquea las señales del combo durante la operación.
 */
void Widget_Puntos::setCurrentRouteInCombo(const QString &routeName, bool blockSignals)
{
    if (blockSignals) cB_Rutas->blockSignals(true);

    bool found = false;
    for (int i = 0; i < cB_Rutas->count(); ++i) {
        if (cB_Rutas->itemData(i, Qt::UserRole).toString() == routeName) {
            cB_Rutas->setCurrentIndex(i);
            found = true;
            break;
        }
    }

    if (!found) {
        qWarning() << "Ruta no encontrada en combo:" << routeName;
    }

    if (blockSignals) cB_Rutas->blockSignals(false);   // ← ¡siempre!
}

// ------------------------------------------------------------
// SLOTS PRIVADOS DE UI
// ------------------------------------------------------------

/**
 * @brief Guarda la ruta actual en un archivo CSV.
 */
void Widget_Puntos::on_pb_guardar_clicked()
{
    if (!modelo->isDroneConfigured()) {
        QMessageBox::warning(this, "Error", "Configure una ruta primero");
        return;
    }

    QString fileName = QFileDialog::getSaveFileName(this, tr("Guardar ruta"),
                                                    QDir::currentPath(), tr("Archivos CSV (*.csv)"));
    if (!fileName.isEmpty()) {
        if (!fileName.endsWith(".csv")) fileName.append(".csv");
        QMessageBox msgBox;
        msgBox.setWindowTitle("Opciones de guardado");
        msgBox.setText("¿Cómo desea guardar la ruta?");
        QPushButton *btnBasico = msgBox.addButton("Solo puntos", QMessageBox::ActionRole);
        QPushButton *btnCompleto = msgBox.addButton("Con análisis", QMessageBox::ActionRole);
        QPushButton *btnCancelar = msgBox.addButton("Cancelar", QMessageBox::RejectRole);
        msgBox.exec();
        if (msgBox.clickedButton() == btnCancelar) return;

        bool guardadoExitoso = false;
        if (msgBox.clickedButton() == btnBasico) {
            guardadoExitoso = modelo->guardarCSV(fileName);
        } else if (msgBox.clickedButton() == btnCompleto) {
            QFile file(fileName);
            if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
                QTextStream stream(&file);
                DroneCharacteristics drone = modelo->getDroneCharacteristics();
                stream << "# ANÁLISIS DE RUTA DE DRON\n";
                stream << "# Dron: " << drone.nombre << " (" << drone.tipoToString() << ")\n";
                stream << "# Ruta: " << modelo->getCurrentRouteName() << "\n";
                stream << "# Fecha: " << QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss") << "\n\n";
                const RevisionRuta &r = modelo->revision();
                stream << "# " << InformeRuta::estado(r) << "\n";
                stream << "# Distancia total: " << QString::number(r.distancia, 'f', 0) << " m\n";
                stream << "# Tiempo estimado: " << QString::number(r.tiempo, 'f', 0) << " s\n";
                stream << "# Waypoints: " << modelo->rowCount() << "\n\n";
                stream << "ID,Nombre,Modo,Latitud,Longitud,Distancia_Anterior(m),Altura,Velocidad,Radio,Tasa_Vertical,Observaciones\n";
                const QList<E_Punto> &puntos = modelo->obtenerLista();
                for (int fila = 0; fila < puntos.size(); ++fila) {
                    const E_Punto &punto = puntos.at(fila);
                    stream << punto.id
                           << "," << punto.nombre
                           << "," << punto.modo
                           << "," << QString::number(punto.pos.latitude(), 'f', 8)
                           << "," << QString::number(punto.pos.longitude(), 'f', 8)
                           << "," << QString::number(punto.distanciaAnterior, 'f', 2)
                           << "," << QString::number(punto.altura, 'f', 2)
                           << "," << QString::number(punto.velocidad, 'f', 2)
                           << "," << QString::number(punto.radio, 'f', 2)
                           << "," << QString::number(punto.tasaAscenso, 'f', 2)
                           << "," << modelo->observacionesPunto(fila)
                           << "\n";
                }
                file.close();
                guardadoExitoso = true;
            }
        }
        if (guardadoExitoso)
            QMessageBox::information(this, "Guardado", "Ruta guardada exitosamente en:\n" + fileName);
        else
            QMessageBox::warning(this, "Error", "No se pudo guardar la ruta");
    }
}

/**
 * @brief Crea una nueva ruta a través del diálogo de dron.
 */
void Widget_Puntos::on_pB_CrearRuta_clicked()
{
    DroneDialog dialog(DroneDialog::CREATE_ROUTE, this);
    if (dialog.exec() == QDialog::Accepted) {
        DroneCharacteristics drone = dialog.getSelectedDrone();
        QString routeName = dialog.getRouteName();
        if (routeName.isEmpty()) {
            QMessageBox::warning(this, "Error", "El nombre de la ruta no puede estar vacío.");
            return;
        }

        // Ocultar cualquier ruta visible
        emit sI_pintaRuta(false);
        pb_Mostrar->setChecked(false);
        pb_Mostrar->setEnabled(false);

        QSqlQuery checkQuery;
        checkQuery.prepare("SELECT COUNT(*) FROM sqlite_master WHERE type='table' AND name=?");
        checkQuery.addBindValue(routeName);
        if (checkQuery.exec() && checkQuery.next() && checkQuery.value(0).toInt() > 0) {
            QMessageBox::StandardButton respuesta = QMessageBox::question(
                        this, "Ruta existente",
                        QString("La ruta '%1' ya existe.\n¿Desea sobrescribirla?").arg(routeName),
                        QMessageBox::Yes | QMessageBox::No);
            if (respuesta != QMessageBox::Yes) return;
            modelo->limpiaRutaCompleta(routeName);
        }

        modelo->configurarRuta(drone, routeName);
        iniciarSeleccionPuntoInicial();

        mostrarMetricasRuta();
        mostrarInfoDronActual();
        updateUIForDroneConfigured(true);

        actualizarComboBoxRutas();
        cB_Rutas->blockSignals(true);
        for (int i = 0; i < cB_Rutas->count(); ++i) {
            if (cB_Rutas->itemData(i, Qt::UserRole).toString() == routeName) {
                cB_Rutas->setCurrentIndex(i);
                break;
            }
        }
        cB_Rutas->blockSignals(false);

        pb_Mostrar->setChecked(false);
        cargarRuta(routeName);
        emit sI_pintaRuta(false);
        emit sI_rutasModificadas();
        emit sI_rutaSeleccionada(routeName);
    }
}

/**
 * @brief Elimina la ruta seleccionada actualmente.
 */
void Widget_Puntos::on_pB_EliminaRuta_clicked()
{
    QString nombreRuta = cB_Rutas->currentText();
    if (nombreRuta.isEmpty()) {
        QMessageBox::warning(this, "Error", "No hay ruta seleccionada.");
        return;
    }

    QString nombreRutaLimpio = obtenerNombreTablaLimpio(nombreRuta);
    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Confirmar eliminación",
                                  QString("¿Está seguro de eliminar la ruta '%1' permanentemente?\n"
                                          "Esta acción eliminará todos los puntos y la configuración de la ruta.")
                                  .arg(nombreRutaLimpio),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (reply != QMessageBox::Yes) return;

    if (cB_Rutas->currentText() == nombreRuta) {
        emit sI_pintaRuta(false);
    }

    if (modelo->eliminarTablaRuta(nombreRutaLimpio)) {
        QString csvFile = QDir::currentPath() + "/" + nombreRutaLimpio + ".csv";
        if (QFile::exists(csvFile)) QFile::remove(csvFile);

        actualizarComboBoxRutas();
        mostrarMetricasRuta();

        if (cB_Rutas->count() == 0) {
            updateUIForDroneConfigured(false);
            emit sI_pintaRuta(false);
        } else {
            cB_Rutas->setCurrentIndex(0);
            seleccionarRuta(cB_Rutas->currentText());
        }

        QMessageBox::information(this, "Ruta eliminada",
                                 QString("La ruta '%1' ha sido eliminada correctamente.").arg(nombreRutaLimpio));

        emit sI_rutasModificadas();

    } else {
        QMessageBox::warning(this, "Error", "No se pudo eliminar la ruta.");
    }
}

/**
 * @brief Restablece la ruta actual a los 10 puntos por defecto.
 */
void Widget_Puntos::on_pB_LimpiaRuta_clicked()
{
    if (!modelo->isDroneConfigured()) {
        QMessageBox::warning(this, "Error", "No hay ruta configurada.");
        return;
    }

    QString nombreRuta = cB_Rutas->currentText();
    if (nombreRuta.isEmpty()) {
        QMessageBox::warning(this, "Error", "No hay ruta seleccionada.");
        return;
    }

    // Se rehace desde el P1 que hay (antes se iba a un punto fijo, 23° N 82° O).
    if (modelo->rowCount() == 0) {
        QMessageBox::information(this, tr("Restablecer ruta"),
                                 tr("La ruta no tiene puntos: haz clic derecho en el mapa donde quieras P1."));
        return;
    }
    const QGeoCoordinate p1 = modelo->obtenerLista().first().pos;

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Restablecer ruta",
                                  tr("¿Rehacer los 10 puntos de '%1' desde P1, a %2 m sobre el terreno?\n"
                                     "Se perderán las modificaciones actuales.")
                                  .arg(obtenerNombreTablaLimpio(nombreRuta)).arg(m_spinSobreTerreno->value()),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (reply != QMessageBox::Yes) return;

    QString nombreRutaLimpio = obtenerNombreTablaLimpio(nombreRuta);
    modelo->limpiaRutaCompleta(nombreRutaLimpio);
    generarRutaDesde(p1);
}

/**
 * @brief Envía la ruta al dron (a través de la controladora).
 */
void Widget_Puntos::on_pB_EnviaRuta_clicked()
{
    if (!modelo->isDroneConfigured()) {
        QMessageBox::warning(this, "Error", "Configure una ruta primero");
        return;
    }

    if (!m_enlaceDisponible) {
        QMessageBox::warning(this, "Sin enlace",
                             "No hay enlace con el dron: conéctate en la pestaña Configuración "
                             "y vuelve a enviar la ruta.");
        return;
    }

    if (modelo->rowCount() != 10) {
        QMessageBox::critical(this, "Error",
                              QString("La ruta debe tener EXACTAMENTE 10 puntos.\n"
                                      "Puntos actuales: %1").arg(modelo->rowCount()));
        return;
    }

    // Con errores se puede enviar igualmente (lo decide el operador), pero
    // viendo cuales son y con el analisis a un clic.
    const RevisionRuta &revision = modelo->revision();
    if (!revision.valida()) {
        QMessageBox caja(this);
        caja.setIcon(QMessageBox::Warning);
        caja.setWindowTitle(tr("Ruta con errores"));
        caja.setText(tr("<b>%1</b>").arg(InformeRuta::estado(revision)));
        caja.setInformativeText(revision.resumenErrores() + tr("\n\nEl dron no debería volar esta ruta así."));
        QPushButton *ver = caja.addButton(tr("Ver el análisis"), QMessageBox::AcceptRole);
        QPushButton *enviar = caja.addButton(tr("Enviar igualmente"), QMessageBox::DestructiveRole);
        caja.addButton(QMessageBox::Cancel);
        caja.setDefaultButton(ver);
        caja.exec();
        if (caja.clickedButton() == ver) {
            mostrarAnalisisDetallado();
            return;
        }
        if (caja.clickedButton() != enviar)
            return;
    }

    QList<QPair<double, double>> coordenadas;
    QList<E_Punto> puntos = modelo->obtenerLista();
    if (puntos.size() >= 2) {
        coordenadas = modelo->obtenerCoordenadasRuta();
        emit sI_Ruta(coordenadas);
        emit sI_Ruta2(puntos);
        emit sI_RutaValidada(revision.valida(), revision.resumenErrores());
        // El botón se pone verde cuando el dron confirma (acuse "uvw"), no ahora.

        QString mensaje = QString("Ruta enviada al dron. El botón Enviar se pondrá verde "
                                  "cuando el dron la confirme.\n\n");
        mensaje += QString("Puntos: %1\n").arg(puntos.size());
        mensaje += QString("Distancia: %1 m\n").arg(revision.distancia, 0, 'f', 0);
        const int seg = int(std::lround(revision.tiempo));
        mensaje += QString("Tiempo estimado: %1:%2\n").arg(seg / 60).arg(seg % 60, 2, 10, QChar('0'));
        mensaje += QString("Estado: %1\n").arg(InformeRuta::estado(revision));
        QMessageBox::information(this, "Ruta enviada", mensaje);
    } else {
        QMessageBox::critical(this, "Error", "La ruta debe tener al menos 2 puntos.");
    }
}

/**
 * @brief Alterna la visibilidad de la ruta en el mapa.
 * @param checked true para mostrar, false para ocultar.
 */
void Widget_Puntos::on_pb_Mostrar_toggled(bool checked)
{
    pb_Mostrar->setText(checked ? tr("Quitar del mapa") : tr("Ver en el mapa"));
    pb_Mostrar->setToolTip(checked ? tr("Quita la ruta del mapa") : tr("Dibuja la ruta en el mapa"));
    emit sI_pintaRuta(checked);
    emit sI_visibilidadCambiada(modelo->getCurrentRouteName(), checked);
    // Al pedir verla, el mapa va a la ruta (antes habia que buscarla).
    if (checked)
        emit sI_encuadrarRuta();
}

/**
 * @brief Muestra el análisis detallado de la ruta.
 */
void Widget_Puntos::on_pB_AnalisisDetallado_clicked()
{
    mostrarAnalisisDetallado();
}

/**
 * @brief Muestra el widget de perfil de altitud.
 */
void Widget_Puntos::mostrarPerfil()
{
    if (!modelo->isDroneConfigured() || modelo->rowCount() < 2) {
        QMessageBox::warning(this, "Perfil", "No hay suficientes puntos para mostrar el perfil.");
        return;
    }

    // El del panel sigue en su sitio: este es otro, grande y en su ventana.
    auto *ventana = new QDialog(this);
    ventana->setAttribute(Qt::WA_DeleteOnClose);
    ventana->setWindowTitle(tr("Perfil de altitud: %1").arg(modelo->getCurrentRouteName()));
    auto *caja = new QVBoxLayout(ventana);
    auto *grande = new PerfilAltitudWidget(m_alturaWorker, ventana);
    caja->addWidget(grande);
    grande->setRuta(modelo->obtenerLista());
    connect(grande, &PerfilAltitudWidget::puntoSeleccionado, this, [this](int i) {
        tw_puntosRuta->selectRow(i);
    });
    ventana->resize(900, 480);
    ventana->show();
}

// Perfil del panel al dia con la ruta, y la tabla a su ancho justo.
void Widget_Puntos::refrescarPerfil()
{
    const QList<E_Punto> &puntos = modelo->obtenerLista();
    if (modelo->isDroneConfigured() && puntos.size() >= 2)
        m_perfilWidget->setRuta(puntos);
    else
        m_perfilWidget->limpiar();
    ajustarAnchoTabla();
}

// La tabla mide lo que sus columnas (mas la barra vertical): el resto
// del ancho es para el perfil.
void Widget_Puntos::ajustarAnchoTabla()
{
    tw_puntosRuta->resizeColumnsToContents();
    // Siempre con el hueco de la barra: si el panel se encoge y aparece, no
    // tapa la ultima columna.
    const int ancho = tw_puntosRuta->horizontalHeader()->length() + 2 * tw_puntosRuta->frameWidth()
                      + tw_puntosRuta->verticalScrollBar()->sizeHint().width();
    tw_puntosRuta->setFixedWidth(ancho);
}

/**
 * @brief Muestra un cuadro de diálogo con una guía de planificación de rutas.
 */
void Widget_Puntos::mostrarGuia()
{
    QDialog *dialog = new QDialog(this);
    dialog->setWindowTitle("Guía de planificación de rutas");
    dialog->setMinimumSize(600, 500);
    dialog->setModal(true);

    QVBoxLayout *mainLayout = new QVBoxLayout(dialog);

    // Área de scroll con contenido de texto enriquecido
    QScrollArea *scrollArea = new QScrollArea(dialog);
    scrollArea->setWidgetResizable(true);
    scrollArea->setFrameShape(QFrame::NoFrame);

    QWidget *contentWidget = new QWidget();
    QVBoxLayout *contentLayout = new QVBoxLayout(contentWidget);

    QLabel *label = new QLabel(contentWidget);
    label->setTextFormat(Qt::RichText);
    label->setWordWrap(true);
    label->setOpenExternalLinks(false);
    label->setTextInteractionFlags(Qt::TextBrowserInteraction);

    // Construir el texto de la guía
    QString guia = "<h2>Guía para establecer una ruta correcta</h2>";
    guia += "<p>Una ruta son 10 puntos que el dron recorre en orden. Cada punto tiene posición, altura "
            "(sobre el nivel del mar) y velocidad (en m/s; en vuelo se ve en km/h).</p>";

    guia += "<h3>Cómo se crea</h3><ul>";
    guia += "<li>Con el dron elegido, clic derecho en el mapa: ese es P1 y los demás se generan hacia el este, "
            "a la altura <b>sobre el terreno</b> que digas en el panel (50 m si no la cambias) como mínimo en toda "
            "la ruta: cada punto queda así por encima de lo más alto de los tramos que lo tocan.</li>";
    guia += "<li>Después se arrastran los puntos en el mapa (cada uno conserva su altura sobre el terreno) "
            "o se cambian los valores en la tabla: «Alt.» es la altura sobre el mar, la que se manda al dron, "
            "y «S/terr.», la que queda sobre el terreno de debajo (editar una cambia la otra).</li>";
    guia += "<li>La altura mínima y la máxima del dron cuentan sobre el terreno.</li></ul>";

    guia += "<h3>Qué te dice la revisión</h3><ul>";
    guia += QStringLiteral("<li><span style='color:%1'><b>Error</b></span>: incumple un límite del dron o pasa por el "
                           "terreno. No se debería enviar.</li>").arg(InformeRuta::colorGravedad(Hallazgo::Gravedad::Error));
    guia += QStringLiteral("<li><span style='color:%1'><b>Aviso</b></span>: el dron la vuela, pero no como está "
                           "planeada (llega más bajo, da una vuelta, no alcanza la velocidad…).</li>")
                .arg(InformeRuta::colorGravedad(Hallazgo::Gravedad::Aviso));
    guia += QStringLiteral("<li><span style='color:%1'><b>Nota</b></span>: algo que no se ha podido comprobar (por "
                           "ejemplo, sin datos de terreno).</li></ul>")
                .arg(InformeRuta::colorGravedad(Hallazgo::Gravedad::Nota));
    guia += "<p>Cada problema dice dónde está, por qué es un problema (con los números y el límite del dron) y "
            "<b>cómo arreglarlo</b>, con el valor que hace falta. Lo ves en tres sitios:</p><ul>";
    guia += "<li>En la tabla: el icono y el color de la fila; pasando el ratón, el detalle.</li>";
    guia += "<li>En el estado de la ruta (abajo): cuántos errores y avisos; pulsándolo, el análisis.</li>";
    guia += "<li>En <b>Análisis</b>: todo junto, con las cifras de cada tramo. Se puede dejar abierto mientras "
            "se corrige, y pinchar en un problema lleva a su punto.</li></ul>";

    guia += "<h3>Qué se comprueba</h3><ul>";
    for (const QString &linea : InformeRuta::comoSeRevisa())
        guia += QStringLiteral("<li>%1</li>").arg(linea.toHtmlEscaped());
    guia += "</ul>";

    guia += "<h3>Consejos</h3><ul>";
    guia += "<li>Arregla primero los errores: a veces uno arrastra a los siguientes (si no le da para subir, "
            "llega más bajo y el tramo siguiente empieza ahí).</li>";
    guia += "<li>Mira el perfil (abajo a la derecha, o <b>Perfil</b> en grande): la línea es la ruta y el relleno, "
            "el terreno.</li>";
    guia += "<li>En ala fija, giros suaves y velocidad constante: cuanto más rápido, más radio de giro necesita.</li>";
    guia += "</ul>";

    label->setText(guia);
    contentLayout->addWidget(label);
    contentLayout->addStretch();
    contentWidget->setLayout(contentLayout);
    scrollArea->setWidget(contentWidget);

    mainLayout->addWidget(scrollArea);

    // Botón de cerrar
    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok);
    connect(buttonBox, &QDialogButtonBox::accepted, dialog, &QDialog::accept);
    mainLayout->addWidget(buttonBox);

    dialog->exec();
    delete dialog; // opcional, pero como tiene this como padre, se destruirá al cerrar
}

// ------------------------------------------------------------
// MÉTODOS PRIVADOS AUXILIARES (UI)
// ------------------------------------------------------------

/**
 * @brief Actualiza el estado de los botones de la interfaz según si hay dron configurado y puntos.
 * @param configured true si hay dron configurado.
 */
void Widget_Puntos::updateUIForDroneConfigured(bool configured)
{
    bool tienePuntos = (modelo->rowCount() > 0);
    QString nombreRuta = modelo->getCurrentRouteName();
    bool rutaCargada = !nombreRuta.isEmpty();
    int numPuntos = modelo->rowCount();
    bool esDiezPuntos = (numPuntos == 10);

    if (configured && rutaCargada) {
        pB_CrearRuta->setEnabled(true);
        m_rutaEnviable = tienePuntos && esDiezPuntos;
        pb_guardar->setEnabled(tienePuntos && esDiezPuntos);
        pB_EliminaRuta->setEnabled(rutaCargada);
        pB_LimpiaRuta->setEnabled(rutaCargada);
        pB_AnalisisDetallado->setEnabled(rutaCargada && tienePuntos);
        pb_Mostrar->setEnabled(true);

        if (!tienePuntos) {
            label_Estado->setText("Ruta vacía");
            label_Estado->setStyleSheet("color: blue; font-weight: bold;");
        } else if (esDiezPuntos) {
            mostrarMetricasRuta();
        } else {
            label_Estado->setText(QString("Inválida (%1/10 puntos)").arg(numPuntos));
            label_Estado->setStyleSheet("color: red; font-weight: bold;");
            m_rutaEnviable = false;
        }
    } else {
        pB_CrearRuta->setEnabled(true);
        m_rutaEnviable = false;
        pb_guardar->setEnabled(false);
        pB_EliminaRuta->setEnabled(false);
        pB_LimpiaRuta->setEnabled(false);
        pB_AnalisisDetallado->setEnabled(false);
        label_Estado->setText("No configurado");
        label_Estado->setStyleSheet("color: red; font-weight: bold;");
        pb_Mostrar->setChecked(false);
        pb_Mostrar->setEnabled(false);
    }

    refrescarBotonEnviar();
    mostrarInfoDronActual();
}

// «Enviar al dron» solo se puede pulsar con una ruta de 10 puntos lista y con
// enlace; si falta algo, el boton dice que (antes se pulsaba y salia un aviso).
void Widget_Puntos::refrescarBotonEnviar()
{
    pB_EnviaRuta->setEnabled(m_rutaEnviable && m_enlaceDisponible);
    if (!m_enlaceDisponible) {
        pB_EnviaRuta->setText(tr("Enviar al dron (sin enlace)"));
        pB_EnviaRuta->setToolTip(tr("Conéctate al dron en la pestaña Enlace"));
    } else {
        pB_EnviaRuta->setText(tr("Enviar al dron"));
        pB_EnviaRuta->setToolTip(m_rutaEnviable ? tr("Manda la ruta al autopiloto")
                                                : tr("La ruta tiene que tener 10 puntos"));
    }
}

/**
 * @brief Muestra la información del dron actual en la etiqueta correspondiente.
 */
void Widget_Puntos::mostrarInfoDronActual()
{
    if (modelo->isDroneConfigured()) {
        DroneCharacteristics drone = modelo->getDroneCharacteristics();
        labelInfoDron->setText(
                    QString("%1 (%2) | Vmax: %3 m/s | Autonomía: %4 m")
                    .arg(drone.nombre)
                    .arg(drone.tipoToString())
                    .arg(drone.maxSpeed, 0, 'f', 1)
                    .arg(drone.maxRange, 0, 'f', 0));
        labelInfoDron->setToolTip(
                    QString("%1 (%2)\n"
                            "Velocidad: %3-%4 m/s\n"
                            "Autonomía: %5 m\n"
                            "Endurance: %6 s\n"
                            "Altitud: %7-%8 m")
                    .arg(drone.nombre)
                    .arg(drone.tipoToString())
                    .arg(drone.minSpeed, 0, 'f', 1)
                    .arg(drone.maxSpeed, 0, 'f', 1)
                    .arg(drone.maxRange, 0, 'f', 0)
                    .arg(drone.endurance, 0, 'f', 0)
                    .arg(drone.minAltitude, 0, 'f', 0)
                    .arg(drone.maxAltitude, 0, 'f', 0));
    } else {
        labelInfoDron->setText("No configurado");
        labelInfoDron->setToolTip("");
    }
}

/**
 * @brief Limpia todo (reinicia el modelo y la interfaz).
 */
void Widget_Puntos::limpiarTodo()
{
    modelo->resetearModelo();
    label_Estado->setText("No configurado");
    label_Estado->setStyleSheet("color: gray; font-weight: bold;");
    labelInfoDron->setText("No configurado");
    label_Distancia->setText("0 m");
    label_AutonomiaRest->setText("0%");
    label_Tiempo->setText("--:--");
    progressAutonomia->setValue(0);
    updateUIForDroneConfigured(false);
}

/**
 * @brief Habilita o deshabilita los botones de gestión de ruta según el parámetro.
 * @param habilitar true para habilitar, false para deshabilitar.
 */
void Widget_Puntos::habilitarBotonesRuta(bool habilitar)
{
    bool tienePuntos = (modelo->rowCount() > 0);
    bool tieneRuta = !modelo->getCurrentRouteName().isEmpty();
    bool esDiezPuntos = (modelo->rowCount() == 10);

    m_rutaEnviable = habilitar && tienePuntos && tieneRuta && esDiezPuntos;
    refrescarBotonEnviar();
    pb_guardar->setEnabled(habilitar && tienePuntos && tieneRuta && esDiezPuntos);
    pB_EliminaRuta->setEnabled(habilitar && tieneRuta);
    pB_LimpiaRuta->setEnabled(habilitar && tieneRuta);
    pB_AnalisisDetallado->setEnabled(habilitar && tienePuntos && tieneRuta);
}

/**
 * @brief Obtiene el nombre real de la tabla de ruta a partir del texto mostrado en el combo.
 * @param nombreTablaConFormato Texto del combo.
 * @return Nombre real de la tabla.
 */
QString Widget_Puntos::obtenerNombreTablaLimpio(const QString &nombreTablaConFormato)
{
    int index = cB_Rutas->currentIndex();
    if (index >= 0) {
        QVariant nombreReal = cB_Rutas->itemData(index, Qt::UserRole);
        if (nombreReal.isValid() && !nombreReal.toString().isEmpty())
            return nombreReal.toString();
    }
    QString nombreLimpio = nombreTablaConFormato;
    nombreLimpio = nombreLimpio.replace("✓", "").trimmed()
            .replace("⚠", "").trimmed()
            .replace("✗", "").trimmed();
    if (nombreLimpio.contains('('))
        nombreLimpio = nombreLimpio.split('(').first().trimmed();
    nombreLimpio = nombreLimpio.replace("(", "").replace(")", "").trimmed();
    return nombreLimpio;
}




// Un clic en el estado de la ruta abre el analisis.
bool Widget_Puntos::eventFilter(QObject *obj, QEvent *event)
{
    if (obj == label_Estado && event->type() == QEvent::MouseButtonRelease && modelo->isDroneConfigured()
            && modelo->rowCount() > 0) {
        mostrarAnalisisDetallado();
        return true;
    }
    return QWidget::eventFilter(obj, event);
}

// El perfil se queda con lo que dejan las tres columnas; si son menos de 220 px
// (mapa estrecho) se oculta en vez de quedar aplastado en una raya.
void Widget_Puntos::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    const QMargins m = layout() ? layout()->contentsMargins() : QMargins();
    const int espacio = layout() ? layout()->spacing() : 0;
    const int libre = width() - m.left() - m.right() - 230 - tw_puntosRuta->width() - 260 - 3 * espacio;
    m_perfilWidget->setVisible(libre >= 220);
}
