// ============================================================================
// Widget_Puntos.cpp
// ============================================================================

#include "Widget_Puntos.h"
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
    gestion->addWidget(pB_CancelarSeleccion, 2, 0, 1, 2);
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
    connect(modelo, &MiModelo::analisisActualizado, this, &Widget_Puntos::onAnalisisActualizado, Qt::UniqueConnection);
    connect(modelo, &QAbstractItemModel::dataChanged,
            [this](const QModelIndex &topLeft, const QModelIndex &topRight, const QVector<int> &roles) {
        Q_UNUSED(topLeft); Q_UNUSED(topRight);
        if (roles.contains(MiModelo::ErrorRole) || roles.contains(MiModelo::WarningRole))
            tw_puntosRuta->viewport()->update();
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

/**
 * @brief Calcula la altitud de vuelo recomendada para una coordenada, sumando un margen de seguridad sobre el terreno.
 * @param terreno Altura del terreno en metros.
 * @param drone Características del dron para acotar la altitud.
 * @return Altitud de vuelo en metros.
 */
double Widget_Puntos::calcularAltitudVuelo(double terreno, const DroneCharacteristics &drone) const
{
    const double SAFETY_OFFSET = 50.0;
    double altitud = terreno + SAFETY_OFFSET;
    altitud = qBound(drone.minAltitude, altitud, drone.maxAltitude);
    altitud = qMax(altitud, 50.0);
    return altitud;
}

/**
 * @brief Genera puntos por defecto para la ruta actual (10 puntos) usando el modelo.
 */
void Widget_Puntos::generarPuntosPorDefecto()
{
    if (!modelo->isDroneConfigured()) {
        qWarning() << "No se puede generar puntos: dron no configurado";
        return;
    }
    modelo->borraTodosLosElementos();
    QList<E_Punto> puntosDefault = modelo->generarPuntosPorDefecto(10);
    for (const E_Punto &punto : puntosDefault)
        modelo->agregarElemento(punto);
}

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

/**
 * @brief Actualiza las etiquetas de métricas y el estado de la ruta en la interfaz.
 */
void Widget_Puntos::mostrarMetricasRuta()
{
    refrescarPerfil();
    if (!modelo->isDroneConfigured() || modelo->rowCount() == 0) {
        label_Distancia->setText("0 m");
        label_AutonomiaRest->setText("0%");
        progressAutonomia->setValue(0);
        label_Tiempo->setText("--:--");
        label_Estado->setText(modelo->isDroneConfigured() ? "Ruta vacía" : "No configurado");
        label_Estado->setStyleSheet(modelo->isDroneConfigured() ?
                                        "color: blue; font-weight: bold;" :
                                        "color: gray; font-weight: bold;");
        return;
    }

    QVariantMap metricas = modelo->obtenerMetricasRuta();
    if (metricas.contains("error")) {
        label_Distancia->setText("0 m");
        label_AutonomiaRest->setText("0%");
        progressAutonomia->setValue(0);
        label_Tiempo->setText("--:--");
        label_Estado->setText("Error");
        label_Estado->setStyleSheet("color: red; font-weight: bold;");
        return;
    }

    double distancia = metricas["distancia_total"].toDouble();
    double porcentaje = metricas["autonomia_porcentaje"].toDouble();
    double tiempoVuelo = metricas["tiempo_vuelo"].toDouble();
    bool valida = metricas["valida"].toBool();

    label_Distancia->setText(QString("%1 m").arg(distancia, 0, 'f', 0));
    label_AutonomiaRest->setText(QString("%1%").arg(100.0 - porcentaje, 0, 'f', 0));
    if (tiempoVuelo > 0) {
        int minutos = static_cast<int>(tiempoVuelo / 60);
        int segundos = static_cast<int>(tiempoVuelo) % 60;
        label_Tiempo->setText(QString("%1:%2").arg(minutos).arg(segundos, 2, 10, QChar('0')));
    } else {
        label_Tiempo->setText("--:--");
    }

    progressAutonomia->setValue(porcentaje);
    progressAutonomia->setFormat("");

    QVariantMap analisis = modelo->analizarRutaDetallada();
    bool tieneAdvertencias = !analisis.value("puntos_criticos").toList().isEmpty() ||
            analisis.value("num_puntos_criticos").toInt() > 0 ||
            !modelo->obtenerRecomendaciones().isEmpty();

    // Que hay que mirar, a mano en el tooltip (antes solo decia «con
    // advertencias» y habia que abrir el analisis para saber de que).
    QStringList detalle = modelo->obtenerRecomendaciones();
    if (!valida)
        detalle.prepend(modelo->validarRuta().second);
    const QString ayuda = detalle.isEmpty() ? tr("Sin observaciones")
                                            : detalle.join(QStringLiteral("\n"));
    if (valida && tieneAdvertencias)
        ponerEstado(tr("Válida, con advertencias"), QColor(255, 179, 0), ayuda);
    else if (valida)
        ponerEstado(tr("Válida"), QColor(67, 160, 71), ayuda);
    else
        ponerEstado(tr("No válida"), QColor(229, 57, 53), ayuda);
}

// Estado de la ruta como una pastilla de color (verde, ambar o rojo) con lo
// que hay que mirar en el tooltip.
void Widget_Puntos::ponerEstado(const QString &texto, const QColor &color, const QString &detalle)
{
    label_Estado->setText(texto);
    label_Estado->setToolTip(detalle + tr("\n\n(«Análisis» para el informe completo)"));
    label_Estado->setStyleSheet(QStringLiteral(
        "QLabel { color: %1; background: rgba(%2, %3, %4, 40); border: 1px solid %1;"
        " border-radius: 4px; padding: 2px 6px; font-weight: bold; }")
        .arg(color.name()).arg(color.red()).arg(color.green()).arg(color.blue()));
}

/**
 * @brief Muestra un cuadro de diálogo con el análisis detallado de la ruta.
 */
void Widget_Puntos::mostrarAnalisisDetallado()
{
    if (!modelo->isDroneConfigured()) {
        QMessageBox::information(this, "Análisis", "Configure un dron primero.");
        return;
    }

    auto analisis = modelo->analizarRutaDetallada();
    auto recomendaciones = modelo->obtenerRecomendaciones();

    if (analisis.contains("error")) {
        QMessageBox::warning(this, "Error", analisis["error"].toString());
        return;
    }

    QString mensaje = "📊 ANÁLISIS DETALLADO DE RUTA\n\n";
    mensaje += "📋 INFORMACIÓN BÁSICA:\n";
    mensaje += QString("• Ruta: %1\n").arg(modelo->getCurrentRouteName());
    mensaje += QString("• Dron: %1\n").arg(modelo->getDroneCharacteristics().nombre);
    mensaje += QString("• Tipo: %1\n").arg(modelo->getDroneCharacteristics().tipoToString());
    mensaje += QString("• Waypoints: %1\n").arg(analisis["num_puntos"].toInt());
    mensaje += QString("• Estado: %1\n\n").arg(analisis["valida"].toBool() ? "✓ Válida" : "✗ Inválida");

    mensaje += "📏 DISTANCIA Y TIEMPO:\n";
    mensaje += QString("• Distancia total: %1 m\n").arg(analisis["distancia_total"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Distancia en línea recta: %1 m\n").arg(analisis["distancia_linea_recta"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Eficiencia de ruta: %1%\n").arg(analisis["eficiencia_ruta"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Tiempo estimado: %1 s\n").arg(analisis["tiempo_vuelo"].toDouble(), 0, 'f', 0);
    mensaje += QString("• Consumo estimado: %1 Wh\n\n").arg(analisis["consumo_wh"].toDouble(), 0, 'f', 1);

    mensaje += "⚡ VELOCIDAD:\n";
    mensaje += QString("• Mínima: %1 m/s\n").arg(analisis["velocidad_min"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Máxima: %1 m/s\n").arg(analisis["velocidad_max"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Promedio: %1 m/s\n\n").arg(analisis["velocidad_promedio"].toDouble(), 0, 'f', 1);

    mensaje += "📈 ALTITUD:\n";
    mensaje += QString("• Mínima: %1 m\n").arg(analisis["altitud_min"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Máxima: %1 m\n").arg(analisis["altitud_max"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Promedio: %1 m\n").arg(analisis["altitud_promedio"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Ascenso total: %1 m\n").arg(analisis["ascenso_total"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Descenso total: %1 m\n").arg(analisis["descenso_total"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Desnivel total: %1 m\n\n").arg(analisis["desnivel_total"].toDouble(), 0, 'f', 1);

    mensaje += "🛡️ SEGURIDAD:\n";
    mensaje += QString("• Margen de seguridad: %1%\n").arg(analisis["margen_seguridad"].toDouble(), 0, 'f', 0);
    mensaje += QString("• Nivel de seguridad: %1\n").arg(analisis["nivel_seguridad"].toString());
    mensaje += QString("• Factor de carga máximo: %1 g\n").arg(analisis["factor_carga_max"].toDouble(), 0, 'f', 1);
    mensaje += QString("• Estrés estimado: %1%\n").arg(analisis["estres_estimado"].toDouble(), 0, 'f', 0);
    mensaje += QString("• Puntos críticos: %1\n\n").arg(analisis["num_puntos_criticos"].toInt());

    QVariantList puntosCriticos = analisis["puntos_criticos"].toList();
    if (!puntosCriticos.isEmpty()) {
        mensaje += "🔴 PUNTOS CRÍTICOS DETECTADOS:\n";
        for (const auto &punto : puntosCriticos) {
            QVariantMap datos = punto.toMap();
            mensaje += QString("• %1 (%2): %3\n")
                    .arg(datos["punto"].toString())
                    .arg(datos["nivel"].toString())
                    .arg(datos["problemas"].toStringList().join(", "));
        }
        mensaje += "\n";
    }

    if (!recomendaciones.isEmpty()) {
        mensaje += "💡 RECOMENDACIONES:\n";
        for (const auto &rec : recomendaciones)
            mensaje += "• " + rec + "\n";
    }

    QMessageBox msgBox;
    msgBox.setWindowTitle("Análisis Detallado de Ruta");
    msgBox.setText(mensaje);
    msgBox.setIcon(QMessageBox::Information);
    QPushButton *btnPuntosCriticos = nullptr;
    QPushButton *btnExportar = nullptr;
    if (!puntosCriticos.isEmpty())
        btnPuntosCriticos = msgBox.addButton("Ver puntos críticos", QMessageBox::ActionRole);
    btnExportar = msgBox.addButton("Exportar análisis", QMessageBox::ActionRole);
    msgBox.addButton(QMessageBox::Ok);

    int result = msgBox.exec();
    if (msgBox.clickedButton() == btnPuntosCriticos)
        mostrarPuntosCriticosDetalles(puntosCriticos);
    else if (msgBox.clickedButton() == btnExportar)
        exportarAnalisis(analisis);
}

/**
 * @brief Exporta el análisis de la ruta a un archivo de texto.
 * @param analisis Mapa con los datos del análisis.
 */
void Widget_Puntos::exportarAnalisis(const QVariantMap &analisis)
{
    QString fileName = QFileDialog::getSaveFileName(this, "Guardar análisis",
                                                    QDir::currentPath(),
                                                    "Archivos de texto (*.txt);;Archivos CSV (*.csv)");
    if (fileName.isEmpty()) return;

    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QMessageBox::warning(this, "Error", "No se pudo crear el archivo");
        return;
    }

    QTextStream out(&file);
    out << "ANÁLISIS DE RUTA\n";
    out << "================\n\n";
    out << "Ruta: " << modelo->getCurrentRouteName() << "\n";
    out << "Dron: " << modelo->getDroneCharacteristics().nombre << "\n";
    out << "Tipo: " << modelo->getDroneCharacteristics().tipoToString() << "\n\n";

    out << "Distancia total: " << analisis["distancia_total"].toDouble() << " m\n";
    out << "Tiempo estimado: " << analisis["tiempo_vuelo"].toDouble() << " s\n";
    out << "Consumo estimado: " << analisis["consumo_wh"].toDouble() << " Wh\n";
    out << "Velocidad promedio: " << analisis["velocidad_promedio"].toDouble() << " m/s\n";
    out << "Eficiencia: " << analisis["eficiencia_ruta"].toDouble() << "%\n";
    out << "Margen de seguridad: " << analisis["margen_seguridad"].toDouble() << "%\n";
    out << "Puntos críticos: " << analisis["num_puntos_criticos"].toInt() << "\n";

    file.close();
    QMessageBox::information(this, "Exportado", "Análisis exportado correctamente.");
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

/**
 * @brief Slot llamado cuando el modelo emite analisisActualizado.
 * @param analisis Mapa con el análisis actualizado.
 */
void Widget_Puntos::onAnalisisActualizado(const QVariantMap &analisis)
{
    actualizarPanelAnalisis(analisis);
}

/**
 * @brief Maneja el movimiento de un punto en el mapa, actualizando coordenadas y altitud.
 * ...
 *
 * \startuml
 * participant "MapaMision (mapa)" as Map
 * participant "Widget_Puntos" as Widget
 * participant "MiModelo" as Model
 * participant "AlturaWorker" as AltWorker
 *
 * Map -> Widget: onPointMoved(index, nuevaPos)
 * activate Widget
 *
 * Widget -> Model: setData(idxLat, nuevaPos.latitude())
 * Widget -> Model: setData(idxLon, nuevaPos.longitude())
 *
 * Widget -> AltWorker: obtenerAltura(lat, lon)
 * AltWorker --> Widget: terreno
 *
 * Widget -> Model: getDroneCharacteristics()
 * Model --> Widget: drone
 *
 * Widget -> Widget: calcularAltitudVuelo(terreno, drone)
 * Widget --> Widget: nuevaAltitud
 *
 * Widget -> Model: setData(idxAlt, nuevaAltitud)
 *
 * deactivate Widget
 * \enduml
 */
void Widget_Puntos::onPointMoved(int index, const QGeoCoordinate &nuevaPos)
{
    if (!modelo || !modelo->isDroneConfigured() || modelo->rowCount() == 0)
        return;
    if (index < 0 || index >= modelo->rowCount() || !nuevaPos.isValid())
        return;

    // Altitud segun el terreno del sitio nuevo; todo va al modelo de una vez
    // (un solo repintado de la ruta, sin estados a medio mover).
    double nuevaAltitud = std::numeric_limits<double>::quiet_NaN();
    if (m_alturaWorker) {
        DroneCharacteristics drone = modelo->getDroneCharacteristics();
        qint16 terreno = m_alturaWorker->obtenerAltura(nuevaPos.latitude(),
                                                       nuevaPos.longitude());
        if (terreno < 0) terreno = 0;
        nuevaAltitud = calcularAltitudVuelo(terreno, drone);
    }
    modelo->moverPunto(index, nuevaPos, nuevaAltitud);
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

/**
 * @brief Agrega el punto inicial y genera los 9 restantes.
 * ...
 *
 * \startuml
 * start
 * :Verificar modo selección y configuración;
 * if (no válido) then (sí)
 *   :cancelarSeleccionPuntoInicial();
 *   stop
 * endif
 *
 * :Obtener drone del modelo;
 * :Obtener altura terreno para P1;
 * :Calcular altitud P1 con margen;
 * :Calcular velocidad por defecto;
 * :Calcular radio por defecto;
 * :Calcular distancia óptima;
 *
 * :Crear punto P1;
 * :Agregar P1 al modelo;
 *
 * :Inicializar puntoActual = coordenada;
 * :azimut = 90°;
 *
 * while (i desde 2 hasta 10)
 *   :puntoActual = puntoActual.atDistanceAndAzimuth(distanciaOptima, azimut);
 *   :Obtener altura terreno;
 *   :Calcular altitud con margen;
 *   :Crear punto Pi;
 *   :Agregar Pi al modelo;
 * endwhile
 *
 * :Finalizar modo selección;
 * :Actualizar interfaz;
 * :emitir señales;
 * stop
 * \enduml
 */
void Widget_Puntos::agregarPuntoInicialYGenerarResto(const QGeoCoordinate &coordenada)
{
    // Verificar que estamos en modo selección y que todo está configurado
    if (!m_modoSeleccionPuntoInicial) return;
    if (!modelo->isDroneConfigured() || !m_alturaWorker) {
        cancelarSeleccionPuntoInicial();
        return;
    }

    DroneCharacteristics drone = modelo->getDroneCharacteristics();

    // ------------------------------------------------------------
    // 1. Obtener altura del terreno para el punto P1
    // ------------------------------------------------------------
    qint16 terrenoP1 = m_alturaWorker->obtenerAltura(coordenada.latitude(),
                                                     coordenada.longitude());
    if (terrenoP1 < 0) terrenoP1 = 0;  // si no hay datos, asumimos nivel del mar
    double altitudP1 = calcularAltitudVuelo(terrenoP1, drone);  // aplica margen y límites

    // ------------------------------------------------------------
    // 2. Calcular valores por defecto para velocidad y radio
    // ------------------------------------------------------------
    double defaultVelocidad;
    if (drone.type == DroneType::FIXED_WING) {
        defaultVelocidad = drone.maxSpeed * 0.65;        // 65% de la máxima para ala fija
    } else {
        defaultVelocidad = drone.maxSpeed * 0.45;        // 45% para quadcóptero
    }
    defaultVelocidad = qMax(defaultVelocidad, drone.minSpeed);  // no bajar del mínimo

    double defaultRadio;
    if (drone.type == DroneType::FIXED_WING) {
        double radioTeorico = (defaultVelocidad * defaultVelocidad) / (9.81 * 0.577);
        defaultRadio = qMax(radioTeorico * 1.5, drone.minTurnRadius * 2.0);
    } else {
        defaultRadio = 10.0;  // valor fijo para quadcóptero
    }

    // ------------------------------------------------------------
    // 3. Calcular distancia óptima entre puntos (punto medio)
    // ------------------------------------------------------------
    double distanciaOptima = (drone.minWaypointDistance + drone.maxWaypointDistance) / 2.0;

    // ------------------------------------------------------------
    // 4. Crear el punto P1 y agregarlo al modelo
    // ------------------------------------------------------------
    E_Punto p1;
    p1.id = 0;
    p1.nombre = "P1";
    p1.modo = 0;
    p1.pos = coordenada;
    p1.altura = altitudP1;
    p1.velocidad = defaultVelocidad;
    p1.radio = defaultRadio;
    p1.distanciaAnterior = 0.0;
    p1.tasaAscenso = 0.0;
    p1.puntoCritico = false;

    modelo->agregarElemento(p1);  // esto ya valida y añade el punto

    // ------------------------------------------------------------
    // 5. Generar los puntos P2 a P10
    // ------------------------------------------------------------
    double azimut = 90.0;  // dirección este (se puede cambiar o hacer configurable)
    QGeoCoordinate puntoActual = coordenada;

    for (int i = 2; i <= 10; ++i) {
        // Calcular la siguiente coordenada a la distancia óptima en el azimut fijo
        puntoActual = puntoActual.atDistanceAndAzimuth(distanciaOptima, azimut);

        // Obtener altura del terreno para este nuevo punto
        qint16 terreno = m_alturaWorker->obtenerAltura(puntoActual.latitude(),
                                                       puntoActual.longitude());
        if (terreno < 0) terreno = 0;
        double altitudPunto = calcularAltitudVuelo(terreno, drone);

        // Crear el punto
        E_Punto p;
        p.id = i - 1;
        p.nombre = QString("P%1").arg(i);
        p.modo = 0;
        p.pos = puntoActual;
        p.altura = altitudPunto;
        p.velocidad = defaultVelocidad;
        p.radio = defaultRadio;
        p.distanciaAnterior = distanciaOptima;  // será la distancia entre puntos consecutivos
        p.tasaAscenso = 0.0;  // se recalculará automáticamente al validar
        p.puntoCritico = false;

        modelo->agregarElemento(p);
    }

    // ------------------------------------------------------------
    // 6. Finalizar modo selección y actualizar interfaz
    // ------------------------------------------------------------
    m_modoSeleccionPuntoInicial = false;
    pB_CancelarSeleccion->setVisible(false);
    label_Estado->setText("Ruta de 10 puntos generada con altitud sobre terreno");
    label_Estado->setStyleSheet("color: green; font-weight: bold;");
    emit sI_cancelarSeleccionPuntoInicial();

    // Refrescar vista y habilitar botones
    updateUIForDroneConfigured(true);
    tw_puntosRuta->viewport()->update();
    tw_puntosRuta->resizeColumnsToContents();
    pb_Mostrar->setChecked(true);
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
                auto analisis = modelo->analizarRutaDetallada();
                if (!analisis.contains("error")) {
                    stream << "# ESTADÍSTICAS:\n";
                    stream << "# Distancia total: " << analisis["distancia_total"].toDouble() << " m\n";
                    stream << "# Tiempo estimado: " << analisis["tiempo_vuelo"].toDouble() << " s\n";
                    stream << "# Waypoints: " << analisis["num_puntos"].toInt() << "\n";
                    stream << "# Velocidad promedio: " << analisis["velocidad_promedio"].toDouble() << " m/s\n";
                    stream << "# Eficiencia: " << analisis["eficiencia_ruta"].toDouble() << "%\n";
                    stream << "# Margen seguridad: " << analisis["margen_seguridad"].toDouble() << "%\n\n";
                }
                stream << "ID,Nombre,Modo,Latitud,Longitud,Distancia_Anterior(m),Altura,Velocidad,Radio,Tasa_Vertical,Advertencias\n";
                const QList<E_Punto> &puntos = modelo->obtenerLista();
                for (const E_Punto &punto : puntos) {
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
                           << "," << punto.advertencias.join("; ").replace(",", ";")
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

    QMessageBox::StandardButton reply;
    reply = QMessageBox::question(this, "Restablecer ruta",
                                  QString("¿Restablecer los 10 puntos por defecto en '%1'?\n"
                                          "Se perderán todas las modificaciones actuales.")
                                  .arg(obtenerNombreTablaLimpio(nombreRuta)),
                                  QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
    if (reply != QMessageBox::Yes) return;

    QString nombreRutaLimpio = obtenerNombreTablaLimpio(nombreRuta);
    modelo->limpiaRutaCompleta(nombreRutaLimpio);
    generarPuntosPorDefecto();
    modelo->revalidarRutaCompleta();

    emit sI_pintaRuta(pb_Mostrar->isChecked());
    QMessageBox::information(this, "Ruta restablecida",
                             "La ruta ha sido restablecida a los 10 puntos por defecto.");
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

    auto validacion = modelo->validarRuta();
    if (!validacion.first) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Ruta no válida",
                                      QString("La ruta tiene problemas:\n%1\n\n¿Desea enviarla de todas formas?")
                                      .arg(validacion.second),
                                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);
        if (reply == QMessageBox::Cancel) return;
        if (reply == QMessageBox::No) {
            on_pB_AnalisisDetallado_clicked();
            return;
        }
    }

    QList<QPair<double, double>> coordenadas;
    QList<E_Punto> puntos = modelo->obtenerLista();
    if (puntos.size() >= 2) {
        coordenadas = modelo->obtenerCoordenadasRuta();
        emit sI_Ruta(coordenadas);
        emit sI_Ruta2(puntos);
        emit sI_RutaValidada(validacion.first, validacion.second);
        // El botón se pone verde cuando el dron confirma (acuse "uvw"), no ahora.

        QString mensaje = QString("Ruta enviada al dron. El botón Enviar se pondrá verde "
                                  "cuando el dron la confirme.\n\n");
        mensaje += QString("Puntos: %1\n").arg(puntos.size());
        auto metricas = modelo->obtenerMetricasRuta();
        mensaje += QString("Distancia: %1 m\n").arg(metricas["distancia_total"].toDouble(), 0, 'f', 1);
        auto analisis = modelo->analizarRutaDetallada();
        if (analisis.contains("tiempo_vuelo")) {
            int minutos = static_cast<int>(analisis["tiempo_vuelo"].toDouble() / 60);
            int segundos = static_cast<int>(analisis["tiempo_vuelo"].toDouble()) % 60;
            mensaje += QString("Tiempo estimado: %1:%2\n").arg(minutos).arg(segundos, 2, 10, QChar('0'));
        }
        mensaje += QString("Estado: %1\n").arg(validacion.first ? "✓ Válida" : "⚠ Con observaciones");
        if (!validacion.first) mensaje += QString("\nObservaciones:\n%1").arg(validacion.second);
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
    guia += "<p>Una ruta de vuelo está compuesta por una serie de waypoints (puntos) que el dron debe seguir. "
            "Cada waypoint tiene parámetros como posición, altitud, velocidad y radio de giro.</p>";

    guia += "<h3>Parámetros del dron</h3>";
    guia += "<ul>";
    guia += "<li><b>Velocidad máxima/mínima:</b> La velocidad en cada punto debe estar entre la mínima y máxima del dron. "
            "Para ala fija, no puede ser inferior a la velocidad de pérdida.</li>";
    guia += "<li><b>Altitud máxima/mínima:</b> La altitud (sobre el nivel del mar) debe estar dentro del rango permitido.</li>";
    guia += "<li><b>Tasa de ascenso/descenso:</b> Los cambios de altitud entre puntos no deben exceder la capacidad del dron. "
            "Se calcula como diferencia de altitud / tiempo del segmento.</li>";
    guia += "<li><b>Radio de giro mínimo:</b> Para ala fija, el radio de giro debe ser suficiente para la velocidad. "
            "Un radio demasiado pequeño puede provocar pérdida de control.</li>";
    guia += "<li><b>Distancia entre waypoints:</b> Debe estar entre la mínima y máxima configuradas. "
            "Distancias muy cortas pueden ser ineficientes; muy largas pueden exceder la autonomía.</li>";
    guia += "<li><b>Autonomía (distancia y tiempo):</b> La suma de distancias no debe superar el alcance máximo, "
            "y el tiempo estimado no debe exceder la duración de la batería.</li>";
    guia += "</ul>";

    guia += "<h3>Terreno y obstáculos</h3>";
    guia += "<p>La altitud de los waypoints es sobre el nivel del mar. El terreno se tiene en cuenta al generar puntos "
            "(se suma un margen de seguridad de 50 m sobre el terreno) y al analizar el perfil. "
            "Utilice el botón <b>Perfil</b> para visualizar la trayectoria de vuelo frente al terreno. "
            "Las cruces rojas indican puntos donde el terreno podría estar peligrosamente cerca.</p>";

    guia += "<h3>Interpretación de la tabla</h3>";
    guia += "<ul>";
    guia += "<li><b>✓</b> Punto válido, sin problemas.</li>";
    guia += "<li><b>⚠</b> Advertencia: el punto tiene algún aspecto mejorable (ej. radio algo pequeño, pendiente moderada).</li>";
    guia += "<li><b>🔴</b> Punto crítico: combina varias advertencias o presenta un riesgo potencial.</li>";
    guia += "<li><b>❌</b> Error: el punto incumple un límite y la ruta no es válida.</li>";
    guia += "</ul>";
    guia += "<p>Pase el ratón sobre la celda de estado para ver los detalles.</p>";

    guia += "<h3>Consejos prácticos</h3>";
    guia += "<ul>";
    guia += "<li>Comience con una ruta generada automáticamente (clic derecho en el mapa) y luego ajuste los puntos.</li>";
    guia += "<li>Evite cambios bruscos de altitud: distribuya el desnivel en varios segmentos.</li>";
    guia += "<li>Para ala fija, mantenga la velocidad constante y use radios de giro amplios.</li>";
    guia += "<li>Revise siempre el análisis detallado (botón <b>Análisis</b>) para ver recomendaciones.</li>";
    guia += "<li>Verifique la autonomía: si el porcentaje de uso supera el 80%, considere reducir la distancia.</li>";
    guia += "<li>En zonas montañosas, aumente el margen de seguridad y revise el perfil cuidadosamente.</li>";
    guia += "</ul>";

    guia += "<h3>Métricas en pantalla</h3>";
    guia += "<p>En la parte inferior se muestran: distancia total, porcentaje de autonomía restante, tiempo estimado y estado de la ruta. "
            "El estado puede ser: <span style='color:green;'>Válida</span>, <span style='color:orange;'>Válida con advertencias</span>, "
            "<span style='color:red;'>Inválida</span> o <span style='color:blue;'>Ruta vacía</span>.</p>";

    guia += "<p><b>Recuerde:</b> Siempre valide la ruta antes de enviarla al dron. Una ruta bien planificada aumenta la seguridad y eficiencia del vuelo.</p>";

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
            auto validacion = modelo->validarRuta();
            QVariantMap analisis = modelo->analizarRutaDetallada();
            bool tieneAdvertencias = !analisis.value("puntos_criticos").toList().isEmpty() ||
                    analisis.value("num_puntos_criticos").toInt() > 0 ||
                    !modelo->obtenerRecomendaciones().isEmpty();

            if (validacion.first) {
                if (tieneAdvertencias) {
                    label_Estado->setText("Válida (con advertencias)");
                    label_Estado->setStyleSheet("color: orange; font-weight: bold;");
                } else {
                    label_Estado->setText("Válida");
                    label_Estado->setStyleSheet("color: green; font-weight: bold;");
                }
            } else {
                label_Estado->setText("Inválida");
                label_Estado->setStyleSheet("color: red; font-weight: bold;");
            }
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

/**
 * @brief Muestra un cuadro de diálogo con las recomendaciones.
 * @param recomendaciones Lista de cadenas con recomendaciones.
 */
void Widget_Puntos::mostrarRecomendaciones(const QStringList &recomendaciones)
{
    if (recomendaciones.isEmpty()) return;
    QString mensaje = "💡 RECOMENDACIONES PARA SU RUTA:\n\n";
    for (const auto &rec : recomendaciones)
        mensaje += "• " + rec + "\n";
    bool mostrar = false;
    for (const auto &rec : recomendaciones) {
        if (rec.contains("⚠") || rec.contains("✗")) {
            mostrar = true;
            break;
        }
    }
    if (mostrar)
        QMessageBox::information(this, "Recomendaciones", mensaje);
}

/**
 * @brief Muestra un cuadro con los detalles de los puntos críticos.
 * @param puntosCriticos Lista de mapas con información de puntos críticos.
 */
void Widget_Puntos::mostrarPuntosCriticosDetalles(const QVariantList &puntosCriticos)
{
    if (puntosCriticos.isEmpty()) {
        QMessageBox::information(this, "Puntos Críticos", "No se detectaron puntos críticos.");
        return;
    }

    QString mensaje = "🔴 PUNTOS CRÍTICOS DETECTADOS:\n\n";
    for (const auto &punto : puntosCriticos) {
        QVariantMap datos = punto.toMap();
        mensaje += QString("📍 %1 (ID: %2)\n").arg(datos["punto"].toString()).arg(datos["id"].toInt());
        mensaje += QString("   Nivel: %1\n").arg(datos["nivel"].toString());
        mensaje += QString("   Problemas:\n");
        for (const QString &problema : datos["problemas"].toStringList())
            mensaje += QString("   • %1\n").arg(problema);
        mensaje += "\n";
    }
    QMessageBox::information(this, "Puntos Críticos", mensaje);
}

/**
 * @brief Actualiza el panel de análisis (solo emite señal, sin cambiar estilo de label_Estado).
 * @param analisis Mapa con el análisis.
 */
void Widget_Puntos::actualizarPanelAnalisis(const QVariantMap &analisis)
{
    if (analisis.contains("error")) return;

    emit sI_AnalisisActualizado(analisis);
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
