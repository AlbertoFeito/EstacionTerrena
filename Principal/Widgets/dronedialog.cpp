#include "dronedialog.h"
#include <QCloseEvent>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QPushButton>
#include <QLabel>
#include <QMessageBox>
#include <QApplication>
#include <QStyle>
#include <QListWidgetItem>
#include <QHeaderView>

DroneDialog::DroneDialog(Mode mode, QWidget *parent)
    : QDialog(parent), currentMode(mode), originalMode(mode),
      cancelled(true), isEditingExistingDrone(false)
{
    setupUI();
    setupValidators();

    // Cargar drones al inicio
    loadDronesFromDatabase();

    // Mostrar pantalla según el modo
    switch (mode) {
    case SELECT_DRONE:
        setWindowTitle("Seleccionar Dron");
        showDroneListScreen();
        break;
    case CREATE_ROUTE:
        setWindowTitle("Crear Nueva Ruta");
        showRouteFormScreen();
        break;
    case MANAGE_DRONES:
        setWindowTitle("Gestión de Drones");
        showDroneListScreen();
        break;
    }

    setMinimumSize(700, 600);  // Aumentado para nuevos campos
}

DroneDialog::~DroneDialog()
{
}

void DroneDialog::setupUI()
{
    QVBoxLayout *mainLayout = new QVBoxLayout(this);

    // Widget apilado para las diferentes pantallas
    stackedWidget = new QStackedWidget(this);
    mainLayout->addWidget(stackedWidget);

    // ============================================
    // PANTALLA 1: Lista de drones
    // ============================================
    QWidget *listScreen = new QWidget();
    QVBoxLayout *listLayout = new QVBoxLayout(listScreen);

    // Título
    QLabel *titleLabel = new QLabel("Drones Disponibles", this);
    titleLabel->setStyleSheet("font-size: 16px; font-weight: bold;");
    listLayout->addWidget(titleLabel);

    // Lista de drones
    listDrones = new QListWidget(this);
    listDrones->setSelectionMode(QAbstractItemView::SingleSelection);
    listLayout->addWidget(listDrones);

    // Botones de gestión
    QHBoxLayout *buttonLayout1 = new QHBoxLayout();
    btnNewDrone = new QPushButton("Nuevo Dron", this);
    btnEditDrone = new QPushButton("Editar Dron", this);
    btnDeleteDrone = new QPushButton("Eliminar Dron", this);
    btnSelectDrone = new QPushButton("Seleccionar", this);
    btnBackFromList = new QPushButton("Volver", this);

    btnEditDrone->setEnabled(false);
    btnDeleteDrone->setEnabled(false);
    btnSelectDrone->setEnabled(false);

    buttonLayout1->addWidget(btnNewDrone);
    buttonLayout1->addWidget(btnEditDrone);
    buttonLayout1->addWidget(btnDeleteDrone);
    buttonLayout1->addStretch();
    buttonLayout1->addWidget(btnSelectDrone);
    buttonLayout1->addWidget(btnBackFromList);

    listLayout->addLayout(buttonLayout1);
    stackedWidget->addWidget(listScreen);

    // ============================================
    // PANTALLA 2: Formulario de dron (ACTUALIZADO)
    // ============================================
    QWidget *formScreen = new QWidget();
    QVBoxLayout *formLayout = new QVBoxLayout(formScreen);

    groupDroneConfig = new QGroupBox("Configuración del Dron", this);
    QGridLayout *droneLayout = new QGridLayout(groupDroneConfig);

    int row = 0;

    // Fila 0: Nombre del dron
    droneLayout->addWidget(new QLabel("Nombre del dron:*", this), row, 0);
    editDroneName = new QLineEdit(this);
    editDroneName->setPlaceholderText("Ej: Quadcopter Estándar");
    droneLayout->addWidget(editDroneName, row++, 1);

    // Fila 1: Tipo de dron
    droneLayout->addWidget(new QLabel("Tipo de dron:*", this), row, 0);
    QWidget *typeWidget = new QWidget(this);
    QHBoxLayout *typeLayout = new QHBoxLayout(typeWidget);
    typeLayout->setContentsMargins(0, 0, 0, 0);
    radioQuadcopter = new QRadioButton("Quadcopter", this);
    radioFixedWing = new QRadioButton("Ala Fija", this);
    QPushButton *btnLoadDefaults = new QPushButton("Cargar valores por defecto", this);
    radioQuadcopter->setChecked(true);
    typeLayout->addWidget(radioQuadcopter);
    typeLayout->addWidget(radioFixedWing);
    typeLayout->addWidget(btnLoadDefaults);
    typeLayout->addStretch();
    droneLayout->addWidget(typeWidget, row++, 1);

    // Separador
    QFrame *separator1 = new QFrame();
    separator1->setFrameShape(QFrame::HLine);
    separator1->setFrameShadow(QFrame::Sunken);
    droneLayout->addWidget(separator1, row++, 0, 1, 2);

    // Fila 2: Velocidad máxima
    droneLayout->addWidget(new QLabel("Velocidad máxima (m/s):*", this), row, 0);
    editMaxSpeed = new QLineEdit(this);
    editMaxSpeed->setToolTip("Velocidad máxima que puede alcanzar el dron");
    droneLayout->addWidget(editMaxSpeed, row++, 1);

    // Fila 3: Velocidad mínima
    droneLayout->addWidget(new QLabel("Velocidad mínima (m/s):*", this), row, 0);
    editMinSpeed = new QLineEdit(this);
    editMinSpeed->setToolTip("Velocidad mínima para mantener vuelo (crítico para ala fija)");
    droneLayout->addWidget(editMinSpeed, row++, 1);

    // Fila 4: Aceleración máxima
    droneLayout->addWidget(new QLabel("Aceleración máxima (m/s²):*", this), row, 0);
    editMaxAcceleration = new QLineEdit(this);
    editMaxAcceleration->setToolTip("Máxima capacidad de aceleración");
    droneLayout->addWidget(editMaxAcceleration, row++, 1);

    // Fila 5: Desaceleración máxima
    droneLayout->addWidget(new QLabel("Desaceleración máxima (m/s²):*", this), row, 0);
    editMaxDeceleration = new QLineEdit(this);
    editMaxDeceleration->setToolTip("Máxima capacidad de frenado/desaceleración");
    droneLayout->addWidget(editMaxDeceleration, row++, 1);

    // Fila 6: Tasa de ascenso máxima
    droneLayout->addWidget(new QLabel("Tasa ascenso máxima (m/s):", this), row, 0);
    editMaxClimbRate = new QLineEdit(this);
    editMaxClimbRate->setToolTip("Velocidad vertical máxima de ascenso");
    droneLayout->addWidget(editMaxClimbRate, row++, 1);

    // Fila 7: Tasa de descenso máxima
    droneLayout->addWidget(new QLabel("Tasa descenso máxima (m/s):", this), row, 0);
    editMaxDescentRate = new QLineEdit(this);
    editMaxDescentRate->setToolTip("Velocidad vertical máxima de descenso");
    droneLayout->addWidget(editMaxDescentRate, row++, 1);

    // Separador
    QFrame *separator2 = new QFrame();
    separator2->setFrameShape(QFrame::HLine);
    separator2->setFrameShadow(QFrame::Sunken);
    droneLayout->addWidget(separator2, row++, 0, 1, 2);

    // Fila 8: Autonomía máxima
    droneLayout->addWidget(new QLabel("Autonomía máxima (m):*", this), row, 0);
    editMaxRange = new QLineEdit(this);
    droneLayout->addWidget(editMaxRange, row++, 1);

    // Fila 9: Radio mínimo de giro
    droneLayout->addWidget(new QLabel("Radio mínimo de giro (m):*", this), row, 0);
    editMinTurnRadius = new QLineEdit(this);
    droneLayout->addWidget(editMinTurnRadius, row++, 1);

    // Fila 10: Distancia máxima entre waypoints
    droneLayout->addWidget(new QLabel("Dist. máx entre waypoints (m):*", this), row, 0);
    editMaxWaypointDistance = new QLineEdit(this);
    droneLayout->addWidget(editMaxWaypointDistance, row++, 1);

    // Fila 11: Distancia mínima entre waypoints
    droneLayout->addWidget(new QLabel("Dist. mín entre waypoints (m):*", this), row, 0);
    editMinWaypointDistance = new QLineEdit(this);
    droneLayout->addWidget(editMinWaypointDistance, row++, 1);

    // Fila 12: Endurance
    droneLayout->addWidget(new QLabel("Endurance (segundos):*", this), row, 0);
    editEndurance = new QLineEdit(this);
    droneLayout->addWidget(editEndurance, row++, 1);

    // Separador
    QFrame *separator3 = new QFrame();
    separator3->setFrameShape(QFrame::HLine);
    separator3->setFrameShadow(QFrame::Sunken);
    droneLayout->addWidget(separator3, row++, 0, 1, 2);

    // Fila 13: Altura mínima
    droneLayout->addWidget(new QLabel("Altura mínima (m):*", this), row, 0);
    editMinAltitude = new QLineEdit(this);
    droneLayout->addWidget(editMinAltitude, row++, 1);

    // Fila 14: Altura máxima
    droneLayout->addWidget(new QLabel("Altura máxima (m):*", this), row, 0);
    editMaxAltitude = new QLineEdit(this);
    droneLayout->addWidget(editMaxAltitude, row++, 1);

    formLayout->addWidget(groupDroneConfig);

    // Botones del formulario
    QHBoxLayout *formButtonLayout = new QHBoxLayout();
    btnSaveDrone = new QPushButton("Guardar", this);
    btnCancelEdit = new QPushButton("Cancelar", this);

    formButtonLayout->addStretch();
    formButtonLayout->addWidget(btnSaveDrone);
    formButtonLayout->addWidget(btnCancelEdit);

    formLayout->addLayout(formButtonLayout);
    stackedWidget->addWidget(formScreen);

    // ============================================
    // PANTALLA 3: Formulario de ruta
    // ============================================
    QWidget *routeScreen = new QWidget();
    QVBoxLayout *routeLayout = new QVBoxLayout(routeScreen);

    groupRouteConfig = new QGroupBox("Configuración de Ruta", this);
    QVBoxLayout *routeGroupLayout = new QVBoxLayout(groupRouteConfig);

    QLabel *routeInfoLabel = new QLabel("Seleccione un dron y asigne un nombre a la ruta:", this);
    routeGroupLayout->addWidget(routeInfoLabel);

    // Lista de drones para selección
    QListWidget *routeDroneList = new QListWidget(this);
    routeDroneList->setSelectionMode(QAbstractItemView::SingleSelection);
    routeGroupLayout->addWidget(routeDroneList);

    // Nombre de la ruta
    QHBoxLayout *routeNameLayout = new QHBoxLayout();
    routeNameLayout->addWidget(new QLabel("Nombre de la ruta:*", this));
    editRouteName = new QLineEdit(this);
    editRouteName->setPlaceholderText("Ingrese nombre para la nueva ruta");
    routeNameLayout->addWidget(editRouteName);
    routeGroupLayout->addLayout(routeNameLayout);

    routeLayout->addWidget(groupRouteConfig);

    // Botones de ruta
    QHBoxLayout *routeButtonLayout = new QHBoxLayout();
    btnCreateRoute = new QPushButton("Crear Ruta", this);
    btnBackFromRoute = new QPushButton("Volver", this);

    routeButtonLayout->addStretch();
    routeButtonLayout->addWidget(btnCreateRoute);
    routeButtonLayout->addWidget(btnBackFromRoute);

    routeLayout->addLayout(routeButtonLayout);
    stackedWidget->addWidget(routeScreen);

    // ============================================
    // BOTONES GENERALES (aceptar/cancelar)
    // ============================================
    QWidget *buttonWidget = new QWidget(this);
    QHBoxLayout *generalButtonLayout = new QHBoxLayout(buttonWidget);
    generalButtonLayout->setContentsMargins(0, 0, 0, 0);

    btnAccept = new QPushButton("Aceptar", this);
    btnCancel = new QPushButton("Cancelar", this);

    btnAccept->setMinimumHeight(40);
    btnCancel->setMinimumHeight(40);

    btnAccept->setStyleSheet("QPushButton { background-color: #4CAF50; color: white; font-weight: bold; padding: 8px; }");
    btnCancel->setStyleSheet("QPushButton { background-color: #f44336; color: white; padding: 8px; }");

    generalButtonLayout->addStretch();
    generalButtonLayout->addWidget(btnAccept);
    generalButtonLayout->addWidget(btnCancel);

    mainLayout->addWidget(buttonWidget);

    // ============================================
    // CONEXIONES DE SEÑALES
    // ============================================
    // Lista de drones
    connect(listDrones, &QListWidget::itemSelectionChanged, this, &DroneDialog::onDroneListSelectionChanged);
    connect(listDrones, &QListWidget::itemDoubleClicked, this, &DroneDialog::onDroneDoubleClicked);

    // Botones de lista
    connect(btnNewDrone, &QPushButton::clicked, this, &DroneDialog::onNewDrone);
    connect(btnEditDrone, &QPushButton::clicked, this, &DroneDialog::onEditDrone);
    connect(btnDeleteDrone, &QPushButton::clicked, this, &DroneDialog::onDeleteDrone);
    connect(btnSelectDrone, &QPushButton::clicked, this, &DroneDialog::onAccept);
    connect(btnBackFromList, &QPushButton::clicked, this, &DroneDialog::onCancel);

    // Botones de formulario
    connect(btnSaveDrone, &QPushButton::clicked, this, &DroneDialog::onSaveDrone);
    connect(btnCancelEdit, &QPushButton::clicked, this, &DroneDialog::onCancelEdit);
    connect(btnLoadDefaults, &QPushButton::clicked, [this]() {
        if (radioQuadcopter->isChecked()) {
            loadDefaultValues(DroneType::QUADCOPTER);
        } else {
            loadDefaultValues(DroneType::FIXED_WING);
        }
    });

    // Botones de ruta
    connect(btnCreateRoute, &QPushButton::clicked, this, &DroneDialog::onAccept);
    connect(btnBackFromRoute, &QPushButton::clicked, this, &DroneDialog::onCancel);

    // Botones generales
    connect(btnAccept, &QPushButton::clicked, this, &DroneDialog::onAccept);
    connect(btnCancel, &QPushButton::clicked, this, &DroneDialog::onCancel);

    // Tipo de dron
    connect(radioQuadcopter, &QRadioButton::toggled, this, &DroneDialog::onDroneTypeChanged);
    connect(radioFixedWing, &QRadioButton::toggled, this, &DroneDialog::onDroneTypeChanged);

    // Cargar valores por defecto
    loadDefaultValues(DroneType::QUADCOPTER);
}

void DroneDialog::setupValidators()
{
    QDoubleValidator *positiveValidator = new QDoubleValidator(0.1, 999999.9, 2, this);
    QDoubleValidator *speedValidator = new QDoubleValidator(0.0, 100.0, 2, this);
    QDoubleValidator *accelerationValidator = new QDoubleValidator(0.1, 20.0, 2, this);
    QDoubleValidator *climbRateValidator = new QDoubleValidator(0.1, 20.0, 2, this);
    QDoubleValidator *altitudeValidator = new QDoubleValidator(0.1, 5000.0, 2, this);
    QDoubleValidator *radiusValidator = new QDoubleValidator(0.0, 1000.0, 2, this);
    QDoubleValidator *distanceValidator = new QDoubleValidator(0.1, 10000.0, 2, this);

    editMaxRange->setValidator(positiveValidator);
    editMaxSpeed->setValidator(speedValidator);
    editMinSpeed->setValidator(speedValidator);
    editMaxAcceleration->setValidator(accelerationValidator);
    editMaxDeceleration->setValidator(accelerationValidator);
    editMaxClimbRate->setValidator(climbRateValidator);
    editMaxDescentRate->setValidator(climbRateValidator);
    editMinTurnRadius->setValidator(radiusValidator);
    editMaxWaypointDistance->setValidator(distanceValidator);
    editMinWaypointDistance->setValidator(distanceValidator);
    editEndurance->setValidator(positiveValidator);
    editMinAltitude->setValidator(altitudeValidator);
    editMaxAltitude->setValidator(altitudeValidator);
}

void DroneDialog::loadDefaultValues(DroneType type, bool preserveName)
{
    QString currentName = editDroneName->text();

    if (type == DroneType::QUADCOPTER) {
        radioQuadcopter->setChecked(true);
        editMaxSpeed->setText("15");
        editMinSpeed->setText("0");       // Quadcopter puede hover
        editMaxAcceleration->setText("3");
        editMaxDeceleration->setText("4");
        editMaxClimbRate->setText("5");
        editMaxDescentRate->setText("5");
        editMaxRange->setText("5000");
        editMinTurnRadius->setText("2");
        editMaxWaypointDistance->setText("100");
        editMinWaypointDistance->setText("5");
        editEndurance->setText("1800");
        editMinAltitude->setText("10");
        editMaxAltitude->setText("120");
    } else {
        radioFixedWing->setChecked(true);
        editMaxSpeed->setText("25");
        editMinSpeed->setText("10");      // Ala fija necesita velocidad mínima
        editMaxAcceleration->setText("2");
        editMaxDeceleration->setText("3");
        editMaxClimbRate->setText("3");
        editMaxDescentRate->setText("3");
        editMaxRange->setText("20000");
        editMinTurnRadius->setText("50");
        editMaxWaypointDistance->setText("500");
        editMinWaypointDistance->setText("100");
        editEndurance->setText("3600");
        editMinAltitude->setText("50");
        editMaxAltitude->setText("500");
    }

    // Actualizar límites recomendados
    updateRecommendedLimits(type);

    // Preservar el nombre si se especifica
    if (preserveName && !currentName.isEmpty()) {
        editDroneName->setText(currentName);
    }
}

void DroneDialog::updateRecommendedLimits(DroneType type)
{
    DroneCharacteristics dummy;
    dummy.type = type;
    auto limits = dummy.getRecommendedLimits();

    QString tooltip = "Límites recomendados:\n";
    for (auto it = limits.begin(); it != limits.end(); ++it) {
        tooltip += QString("%1: %2 - %3\n")
            .arg(it.key())
            .arg(it.value().first)
            .arg(it.value().second);
    }

    if (type == DroneType::QUADCOPTER) {
        editMinSpeed->setToolTip(tooltip + "\nQuadcopter puede volar a velocidad 0 (hovering)");
        editMaxSpeed->setToolTip(tooltip + "\nQuadcopter típicamente 10-25 m/s");
        editMinTurnRadius->setToolTip(tooltip + "\nQuadcopter puede girar en radio muy pequeño");
    } else {
        editMinSpeed->setToolTip(tooltip + "\nAla fija necesita velocidad mínima para sustentación");
        editMaxSpeed->setToolTip(tooltip + "\nAla fija típicamente 15-40 m/s");
        editMinTurnRadius->setToolTip(tooltip + "\nAla fija necesita radio de giro mayor");
    }
}

void DroneDialog::loadDronesFromDatabase()
{
    dronesList.clear();
    listDrones->clear();

    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        QMessageBox::warning(this, "Error", "No hay conexión a la base de datos");
        return;
    }

    QSqlQuery query("SELECT * FROM drones ORDER BY nombre");
    while (query.next()) {
        DroneCharacteristics drone;
        drone.id = query.value("id").toInt();
        drone.nombre = query.value("nombre").toString();
        drone.type = DroneCharacteristics::stringToTipo(query.value("tipo").toString());
        drone.maxRange = query.value("max_range").toDouble();
        drone.maxSpeed = query.value("max_speed").toDouble();
        drone.minSpeed = query.value("min_speed").toDouble();
        drone.maxAcceleration = query.value("max_acceleration").toDouble();
        drone.maxDeceleration = query.value("max_deceleration").toDouble();
        drone.minTurnRadius = query.value("min_turn_radius").toDouble();
        drone.maxWaypointDistance = query.value("max_waypoint_distance").toDouble();
        drone.minWaypointDistance = query.value("min_waypoint_distance").toDouble();
        drone.endurance = query.value("endurance").toDouble();
        drone.minAltitude = query.value("min_altitude").toDouble();
        drone.maxAltitude = query.value("max_altitude").toDouble();
        drone.maxClimbRate = query.value("max_climb_rate").toDouble();
        drone.maxDescentRate = query.value("max_descent_rate").toDouble();

        dronesList.append(drone);

        // Agregar a la lista
        QListWidgetItem *item = new QListWidgetItem(
                    QString("%1 (%2) | Vmax: %3 m/s | Autonomía: %4 m")
                    .arg(drone.nombre)
                    .arg(drone.tipoToString())
                    .arg(drone.maxSpeed)
                    .arg(drone.maxRange)
                    );
        item->setData(Qt::UserRole, drone.id);
        listDrones->addItem(item);
    }
}

void DroneDialog::showDroneListScreen()
{
    stackedWidget->setCurrentIndex(0);
    btnAccept->setVisible(currentMode == SELECT_DRONE);
    btnCancel->setVisible(true);
    btnSelectDrone->setVisible(currentMode == SELECT_DRONE);
    btnBackFromList->setVisible(currentMode == MANAGE_DRONES);

    // Actualizar estado de botones según selección
    onDroneListSelectionChanged();
}

void DroneDialog::showDroneFormScreen(bool isNew)
{
    stackedWidget->setCurrentIndex(1);
    btnAccept->setVisible(false);
    btnCancel->setVisible(false);

    isEditingExistingDrone = !isNew;

    if (isNew) {
        groupDroneConfig->setTitle("Nuevo Dron");
        clearDroneForm();
        loadDefaultValues(DroneType::QUADCOPTER);
        editDroneName->setFocus();
    } else {
        groupDroneConfig->setTitle("Editar Dron");
        updateFormFromDrone(editingDrone);
        // Actualizar límites recomendados según tipo
        updateRecommendedLimits(editingDrone.type);
    }
}

void DroneDialog::showRouteFormScreen()
{
    stackedWidget->setCurrentIndex(2);
    btnAccept->setVisible(false);
    btnCancel->setVisible(false);

    // Actualizar lista de drones para la ruta
    QListWidget *routeDroneList = groupRouteConfig->findChild<QListWidget*>();
    if (routeDroneList) {
        routeDroneList->clear();
        for (const auto &drone : dronesList) {
            QListWidgetItem *item = new QListWidgetItem(
                        QString("%1 (%2) | Vmax: %3 m/s | Autonomía: %4 m")
                        .arg(drone.nombre)
                        .arg(drone.tipoToString())
                        .arg(drone.maxSpeed)
                        .arg(drone.maxRange)
                        );
            item->setData(Qt::UserRole, drone.id);
            routeDroneList->addItem(item);
        }
    }
}

void DroneDialog::onDroneListSelectionChanged()
{
    bool hasSelection = !listDrones->selectedItems().isEmpty();

    btnEditDrone->setEnabled(hasSelection);
    btnDeleteDrone->setEnabled(hasSelection);
    btnSelectDrone->setEnabled(hasSelection && currentMode == SELECT_DRONE);

    if (hasSelection) {
        QListWidgetItem *item = listDrones->currentItem();
        int droneId = item->data(Qt::UserRole).toInt();

        // Buscar el dron seleccionado
        for (const auto &drone : dronesList) {
            if (drone.id == droneId) {
                selectedDrone = drone;
                break;
            }
        }
    }
}

void DroneDialog::onDroneDoubleClicked(QListWidgetItem *item)
{
    if (currentMode == SELECT_DRONE) {
        onAccept();
    } else if (currentMode == MANAGE_DRONES) {
        onEditDrone();
    }
}

void DroneDialog::onNewDrone()
{
    editingDrone = DroneCharacteristics();
    showDroneFormScreen(true);
}

void DroneDialog::onEditDrone()
{
    if (listDrones->currentItem()) {
        int droneId = listDrones->currentItem()->data(Qt::UserRole).toInt();

        // Buscar el dron a editar
        for (const auto &drone : dronesList) {
            if (drone.id == droneId) {
                editingDrone = drone;
                showDroneFormScreen(false);
                break;
            }
        }
    }
}

void DroneDialog::onDeleteDrone()
{
    if (!listDrones->currentItem()) {
        QMessageBox::warning(this, "Error", "Seleccione un dron para eliminar.");
        return;
    }

    int droneId = listDrones->currentItem()->data(Qt::UserRole).toInt();
    QString droneName;

    // Buscar nombre del dron
    for (const auto &drone : dronesList) {
        if (drone.id == droneId) {
            droneName = drone.nombre;
            break;
        }
    }

    // Verificar si el dron tiene rutas asociadas
    int routeCount = countRoutesUsingDrone(droneId);

    if (routeCount > 0) {
        QMessageBox::StandardButton reply = QMessageBox::question(
                    this, "Confirmar eliminación",
                    QString("El dron '%1' tiene %2 ruta(s) asociada(s).\n"
                            "¿Desea eliminar el dron y todas sus rutas?")
                    .arg(droneName).arg(routeCount),
                    QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel
                    );

        if (reply == QMessageBox::Cancel) {
            return;
        } else if (reply == QMessageBox::Yes) {
            // Eliminar rutas asociadas primero
            if (!deleteRoutesUsingDrone(droneId)) {
                QMessageBox::warning(this, "Error",
                                     "No se pudieron eliminar las rutas asociadas.");
                return;
            }
        } else {
            QMessageBox::information(this, "Información",
                                     "No se puede eliminar el dron porque tiene rutas asociadas.\n"
                                     "Elimine primero las rutas o cambie el dron asociado.");
            return;
        }
    }

    // Confirmar eliminación
    QMessageBox::StandardButton confirm = QMessageBox::question(
                this, "Confirmar eliminación",
                QString("¿Está seguro de eliminar el dron '%1'?")
                .arg(droneName),
                QMessageBox::Yes | QMessageBox::No
                );

    if (confirm == QMessageBox::Yes) {
        if (deleteDroneFromDatabase(droneId)) {
            QMessageBox::information(this, "Éxito",
                                     QString("Dron '%1' eliminado correctamente.").arg(droneName));
            loadDronesFromDatabase();
        } else {
            QMessageBox::warning(this, "Error",
                                 "No se pudo eliminar el dron.");
        }
    }
}

void DroneDialog::onSaveDrone()
{
    if (!validateDroneInputs()) {
        return;
    }

    // Actualizar objeto drone con los valores del formulario
    editingDrone.nombre = editDroneName->text().trimmed();
    editingDrone.type = radioQuadcopter->isChecked() ? DroneType::QUADCOPTER : DroneType::FIXED_WING;
    editingDrone.maxRange = editMaxRange->text().toDouble();
    editingDrone.maxSpeed = editMaxSpeed->text().toDouble();
    editingDrone.minSpeed = editMinSpeed->text().toDouble();
    editingDrone.maxAcceleration = editMaxAcceleration->text().toDouble();
    editingDrone.maxDeceleration = editMaxDeceleration->text().toDouble();
    editingDrone.minTurnRadius = editMinTurnRadius->text().toDouble();
    editingDrone.maxWaypointDistance = editMaxWaypointDistance->text().toDouble();
    editingDrone.minWaypointDistance = editMinWaypointDistance->text().toDouble();
    editingDrone.endurance = editEndurance->text().toDouble();
    editingDrone.minAltitude = editMinAltitude->text().toDouble();
    editingDrone.maxAltitude = editMaxAltitude->text().toDouble();
    editingDrone.maxClimbRate = editMaxClimbRate->text().toDouble();
    editingDrone.maxDescentRate = editMaxDescentRate->text().toDouble();

    // Verificar unicidad del nombre (excluyendo el propio dron si estamos editando)
    if (!isDroneNameUnique(editingDrone.nombre, isEditingExistingDrone ? editingDrone.id : -1)) {
        QMessageBox::warning(this, "Error",
                             QString("Ya existe un dron con el nombre '%1'.").arg(editingDrone.nombre));
        return;
    }

    // Guardar en base de datos
    if (saveDroneToDatabase()) {
        QMessageBox::information(this, "Éxito",
                                 QString("Dron '%1' guardado correctamente.").arg(editingDrone.nombre));

        // Recargar lista y volver a pantalla de lista
        loadDronesFromDatabase();
        showDroneListScreen();
    } else {
        QMessageBox::warning(this, "Error",
                             "No se pudo guardar el dron.");
    }
}

void DroneDialog::onCancelEdit()
{
    showDroneListScreen();
}

bool DroneDialog::validateDroneInputs()
{
    // 1. Validar nombre del dron
    QString droneName = editDroneName->text().trimmed();
    if (droneName.isEmpty()) {
        QMessageBox::warning(this, "Validación", "El nombre del dron no puede estar vacío.");
        editDroneName->setFocus();
        return false;
    }

    if (droneName.contains(QRegExp("[\\\\/:\"*?<>|]"))) {
        QMessageBox::warning(this, "Validación",
            "El nombre del dron contiene caracteres inválidos.\n"
            "No se permiten: \\ / : * ? \" < > |");
        editDroneName->setFocus();
        return false;
    }

    // 2. Validar todos los campos numéricos
    struct FieldValidation {
        QLineEdit *field;
        QString name;
        double minValue;
        double maxValue;
        bool allowZero;
    };

    QList<FieldValidation> fields = {
        {editMaxRange, "Autonomía máxima", 0.1, 999999.9, false},
        {editMaxSpeed, "Velocidad máxima", 0.1, 100.0, false},
        {editMinSpeed, "Velocidad mínima", 0.0, 100.0, true}, // puede ser 0
        {editMaxAcceleration, "Aceleración máxima", 0.1, 20.0, false},
        {editMaxDeceleration, "Desaceleración máxima", 0.1, 20.0, false},
        {editMaxClimbRate, "Tasa de ascenso máxima", 0.1, 20.0, false},
        {editMaxDescentRate, "Tasa de descenso máxima", 0.1, 20.0, false},
        {editMinTurnRadius, "Radio mínimo de giro", 0.0, 1000.0, true},
        {editMaxWaypointDistance, "Distancia máxima entre waypoints", 0.1, 10000.0, false},
        {editMinWaypointDistance, "Distancia mínima entre waypoints", 0.1, 10000.0, false},
        {editEndurance, "Endurance", 0.1, 999999.9, false},
        {editMinAltitude, "Altura mínima", 0.1, 5000.0, false},
        {editMaxAltitude, "Altura máxima", 0.1, 5000.0, false}
    };

    for (const auto &field : fields) {
        QString text = field.field->text();
        if (text.isEmpty()) {
            QMessageBox::warning(this, "Validación",
                QString("%1 no puede estar vacío.").arg(field.name));
            field.field->setFocus();
            return false;
        }

        bool ok;
        double value = text.toDouble(&ok);
        if (!ok) {
            QMessageBox::warning(this, "Validación",
                QString("%1 debe ser un número válido.").arg(field.name));
            field.field->setFocus();
            return false;
        }

        if (field.allowZero) {
            if (value < field.minValue) {
                QMessageBox::warning(this, "Validación",
                    QString("%1 debe ser mayor o igual a %2.").arg(field.name).arg(field.minValue));
                field.field->setFocus();
                return false;
            }
        } else {
            if (value <= 0) {
                QMessageBox::warning(this, "Validación",
                    QString("%1 debe ser un número positivo.").arg(field.name));
                field.field->setFocus();
                return false;
            }
        }

        if (value > field.maxValue) {
            QMessageBox::warning(this, "Validación",
                QString("%1 no puede exceder %2.").arg(field.name).arg(field.maxValue));
            field.field->setFocus();
            return false;
        }
    }

    // 3. Validar relación velocidad mínima vs máxima
    double maxSpeed = editMaxSpeed->text().toDouble();
    double minSpeed = editMinSpeed->text().toDouble();
    if (minSpeed > maxSpeed) {
        QMessageBox::warning(this, "Validación",
            "La velocidad mínima no puede ser mayor que la velocidad máxima.");
        editMinSpeed->setFocus();
        return false;
    }

    // 4. Validar distancias entre waypoints
    double maxDist = editMaxWaypointDistance->text().toDouble();
    double minDist = editMinWaypointDistance->text().toDouble();
    if (maxDist <= minDist) {
        QMessageBox::warning(this, "Validación",
            "La distancia máxima entre waypoints debe ser mayor que la distancia mínima.");
        editMaxWaypointDistance->setFocus();
        return false;
    }

    // 5. Validar alturas
    double minAlt = editMinAltitude->text().toDouble();
    double maxAlt = editMaxAltitude->text().toDouble();
    if (maxAlt <= minAlt) {
        QMessageBox::warning(this, "Validación",
            "La altura máxima debe ser mayor que la altura mínima.");
        editMaxAltitude->setFocus();
        return false;
    }

    // 6. Validaciones específicas por tipo de dron
    if (radioFixedWing->isChecked()) {
        // Validaciones para ala fija
        double radius = editMinTurnRadius->text().toDouble();
        if (radius < 10.0) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Confirmación",
                "El radio mínimo de giro para ala fija es muy pequeño (< 10 m).\n"
                "Los valores típicos para ala fija son entre 10 y 100 m.\n"
                "¿Está seguro que este valor es correcto?",
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply == QMessageBox::No) {
                editMinTurnRadius->setFocus();
                return false;
            }
        }

        double speed = editMaxSpeed->text().toDouble();
        if (speed < 10.0) {
            QMessageBox::warning(this, "Validación",
                "La velocidad máxima para ala fija debe ser al menos 10 m/s.");
            editMaxSpeed->setFocus();
            return false;
        }

        // Validar velocidad mínima para ala fija
        double minSpeedValue = editMinSpeed->text().toDouble();
        if (minSpeedValue < 5.0) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Confirmación",
                "La velocidad mínima para ala fija es muy baja (< 5 m/s).\n"
                "Los valores típicos para ala fija son entre 8 y 15 m/s para mantener sustentación.\n"
                "¿Está seguro que este valor es correcto?",
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply == QMessageBox::No) {
                editMinSpeed->setFocus();
                return false;
            }
        }

        // Validar distancia mínima entre waypoints para ala fija
        double minWaypointDist = editMinWaypointDistance->text().toDouble();
        if (minWaypointDist < 50.0) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Confirmación",
                "La distancia mínima entre waypoints para ala fija es muy pequeña (< 50 m).\n"
                "Los valores típicos para ala fija son entre 50 y 500 m.\n"
                "¿Está seguro que este valor es correcto?",
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply == QMessageBox::No) {
                editMinWaypointDistance->setFocus();
                return false;
            }
        }

        // Validar altura mínima para ala fija
        double minAltitude = editMinAltitude->text().toDouble();
        if (minAltitude < 30.0) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Confirmación",
                "La altura mínima para ala fija es muy baja (< 30 m).\n"
                "Los valores típicos para ala fija son entre 30 y 500 m.\n"
                "¿Está seguro que este valor es correcto?",
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply == QMessageBox::No) {
                editMinAltitude->setFocus();
                return false;
            }
        }

        // Validar tasa de ascenso para ala fija
        double climbRate = editMaxClimbRate->text().toDouble();
        if (climbRate > 6.0) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Confirmación",
                "La tasa de ascenso para ala fija es muy alta (> 6 m/s).\n"
                "Los valores típicos para ala fija son entre 2 y 6 m/s.\n"
                "¿Está seguro que este valor es correcto?",
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply == QMessageBox::No) {
                editMaxClimbRate->setFocus();
                return false;
            }
        }

    } else {
        // Validaciones para quadcopter
        double radius = editMinTurnRadius->text().toDouble();
        if (radius > 10.0) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Confirmación",
                "El radio mínimo de giro para quadcopter es muy grande (> 10 m).\n"
                "Los valores típicos para quadcopter son entre 0 y 5 m.\n"
                "¿Está seguro que este valor es correcto?",
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply == QMessageBox::No) {
                editMinTurnRadius->setFocus();
                return false;
            }
        }

        // Validar distancia máxima entre waypoints para quadcopter
        double maxWaypointDist = editMaxWaypointDistance->text().toDouble();
        if (maxWaypointDist > 500.0) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Confirmación",
                "La distancia máxima entre waypoints para quadcopter es muy grande (> 500 m).\n"
                "Los valores típicos para quadcopter son entre 50 y 300 m.\n"
                "¿Está seguro que este valor es correcto?",
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply == QMessageBox::No) {
                editMaxWaypointDistance->setFocus();
                return false;
            }
        }

        // Validar autonomía para quadcopter
        double maxRange = editMaxRange->text().toDouble();
        if (maxRange > 10000.0) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Confirmación",
                "La autonomía máxima para quadcopter es muy grande (> 10,000 m).\n"
                "Los valores típicos para quadcopter son entre 1,000 y 5,000 m.\n"
                "¿Está seguro que este valor es correcto?",
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply == QMessageBox::No) {
                editMaxRange->setFocus();
                return false;
            }
        }

        // Validar altura máxima para quadcopter
        double maxAltitude = editMaxAltitude->text().toDouble();
        if (maxAltitude > 200.0) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Confirmación",
                "La altura máxima para quadcopter es muy grande (> 200 m).\n"
                "Los valores típicos para quadcopter son entre 50 y 150 m.\n"
                "¿Está seguro que este valor es correcto?",
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply == QMessageBox::No) {
                editMaxAltitude->setFocus();
                return false;
            }
        }

        // Quadcopter puede tener velocidad mínima 0 (hovering)
        double minSpeedValue = editMinSpeed->text().toDouble();
        if (minSpeedValue > 5.0) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Confirmación",
                "La velocidad mínima para quadcopter es alta (> 5 m/s).\n"
                "Quadcopter puede hover a velocidad 0.\n"
                "¿Está seguro que este valor es correcto?",
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply == QMessageBox::No) {
                editMinSpeed->setFocus();
                return false;
            }
        }
    }

    // 7. Validar coherencia entre velocidad y autonomía
    double maxRange = editMaxRange->text().toDouble();
    double maxSpeedValue = editMaxSpeed->text().toDouble();
    double endurance = editEndurance->text().toDouble();

    // Calcular autonomía teórica basada en velocidad máxima y endurance
    double theoreticalRange = maxSpeedValue * endurance;
    if (maxRange > theoreticalRange) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Confirmación",
            QString("La autonomía máxima (%1 m) excede la autonomía teórica basada en "
                   "velocidad máxima (%2 m/s) y endurance (%3 s) que sería %4 m.\n"
                   "¿Está seguro que estos valores son correctos?")
                .arg(maxRange, 0, 'f', 0)
                .arg(maxSpeedValue, 0, 'f', 1)
                .arg(endurance, 0, 'f', 0)
                .arg(theoreticalRange, 0, 'f', 0),
            QMessageBox::Yes | QMessageBox::No
        );
        if (reply == QMessageBox::No) {
            editMaxRange->setFocus();
            return false;
        }
    }

    // 8. Validar relación entre radio de giro y velocidad para ala fija
    if (radioFixedWing->isChecked()) {
        double radius = editMinTurnRadius->text().toDouble();
        double speed = editMaxSpeed->text().toDouble();

        // Calcular radio de giro mínimo teórico basado en velocidad
        // Fórmula simplificada: radio mínimo ≈ velocidad² / (g * tan(bank_angle))
        // Asumiendo ángulo de inclinación de 30 grados y g = 9.81 m/s²
        double theoreticalMinRadius = (speed * speed) / (9.81 * 0.577); // tan(30°) ≈ 0.577

        if (radius < theoreticalMinRadius * 0.5) {
            QMessageBox::StandardButton reply = QMessageBox::question(
                this, "Confirmación",
                QString("El radio mínimo de giro (%1 m) parece muy pequeño para la "
                       "velocidad máxima (%2 m/s).\n"
                       "Para un giro seguro a esa velocidad, el radio mínimo teórico "
                       "sería aproximadamente %3 m.\n"
                       "¿Está seguro que estos valores son correctos?")
                    .arg(radius, 0, 'f', 1)
                    .arg(speed, 0, 'f', 1)
                    .arg(theoreticalMinRadius, 0, 'f', 1),
                QMessageBox::Yes | QMessageBox::No
            );
            if (reply == QMessageBox::No) {
                editMinTurnRadius->setFocus();
                return false;
            }
        }
    }

    // 9. Validar aceleración vs desaceleración
    double acceleration = editMaxAcceleration->text().toDouble();
    double deceleration = editMaxDeceleration->text().toDouble();
    if (deceleration < acceleration) {
        QMessageBox::StandardButton reply = QMessageBox::question(
            this, "Confirmación",
            QString("La desaceleración máxima (%1 m/s²) es menor que la aceleración máxima (%2 m/s²).\n"
                   "¿Está seguro que estos valores son correctos?")
                .arg(deceleration, 0, 'f', 1)
                .arg(acceleration, 0, 'f', 1),
            QMessageBox::Yes | QMessageBox::No
        );
        if (reply == QMessageBox::No) {
            editMaxDeceleration->setFocus();
            return false;
        }
    }

    return true;
}

bool DroneDialog::validateRouteInputs()
{
    // Validar nombre de ruta
    QString routeName = editRouteName->text().trimmed();
    if (routeName.isEmpty()) {
        QMessageBox::warning(this, "Validación", "El nombre de la ruta no puede estar vacío.");
        editRouteName->setFocus();
        return false;
    }

    if (routeName.contains(QRegExp("[\\\\/:\"*?<>|]"))) {
        QMessageBox::warning(this, "Validación",
                             "El nombre de la ruta contiene caracteres inválidos.\n"
                             "No se permiten: \\ / : * ? \" < > |");
        editRouteName->setFocus();
        return false;
    }

    // Validar que se haya seleccionado un dron
    QListWidget *routeDroneList = groupRouteConfig->findChild<QListWidget*>();
    if (!routeDroneList || routeDroneList->selectedItems().isEmpty()) {
        QMessageBox::warning(this, "Validación", "Debe seleccionar un dron para la ruta.");
        return false;
    }

    return true;
}

void DroneDialog::updateFormFromDrone(const DroneCharacteristics &drone)
{
    editDroneName->setText(drone.nombre);

    if (drone.type == DroneType::QUADCOPTER) {
        radioQuadcopter->setChecked(true);
    } else {
        radioFixedWing->setChecked(true);
    }

    editMaxRange->setText(QString::number(drone.maxRange));
    editMaxSpeed->setText(QString::number(drone.maxSpeed));
    editMinSpeed->setText(QString::number(drone.minSpeed));
    editMaxAcceleration->setText(QString::number(drone.maxAcceleration));
    editMaxDeceleration->setText(QString::number(drone.maxDeceleration));
    editMaxClimbRate->setText(QString::number(drone.maxClimbRate));
    editMaxDescentRate->setText(QString::number(drone.maxDescentRate));
    editMinTurnRadius->setText(QString::number(drone.minTurnRadius));
    editMaxWaypointDistance->setText(QString::number(drone.maxWaypointDistance));
    editMinWaypointDistance->setText(QString::number(drone.minWaypointDistance));
    editEndurance->setText(QString::number(drone.endurance));
    editMinAltitude->setText(QString::number(drone.minAltitude));
    editMaxAltitude->setText(QString::number(drone.maxAltitude));
}

void DroneDialog::clearDroneForm()
{
    editDroneName->clear();
    radioQuadcopter->setChecked(true);
    editMaxRange->clear();
    editMaxSpeed->clear();
    editMinSpeed->clear();
    editMaxAcceleration->clear();
    editMaxDeceleration->clear();
    editMaxClimbRate->clear();
    editMaxDescentRate->clear();
    editMinTurnRadius->clear();
    editMaxWaypointDistance->clear();
    editMinWaypointDistance->clear();
    editEndurance->clear();
    editMinAltitude->clear();
    editMaxAltitude->clear();
}

bool DroneDialog::saveDroneToDatabase()
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return false;
    }

    QSqlQuery query;

    if (editingDrone.id == -1) {
        // Insertar nuevo dron
        query.prepare("INSERT INTO drones (nombre, tipo, max_range, max_speed, min_speed, "
                      "max_acceleration, max_deceleration, min_turn_radius, "
                      "max_waypoint_distance, min_waypoint_distance, endurance, "
                      "min_altitude, max_altitude, max_climb_rate, max_descent_rate) "
                      "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
    } else {
        // Actualizar dron existente
        query.prepare("UPDATE drones SET nombre = ?, tipo = ?, max_range = ?, max_speed = ?, "
                      "min_speed = ?, max_acceleration = ?, max_deceleration = ?, "
                      "min_turn_radius = ?, max_waypoint_distance = ?, min_waypoint_distance = ?, "
                      "endurance = ?, min_altitude = ?, max_altitude = ?, "
                      "max_climb_rate = ?, max_descent_rate = ? WHERE id = ?");
    }

    query.addBindValue(editingDrone.nombre);
    query.addBindValue(editingDrone.tipoToString());
    query.addBindValue(editingDrone.maxRange);
    query.addBindValue(editingDrone.maxSpeed);
    query.addBindValue(editingDrone.minSpeed);
    query.addBindValue(editingDrone.maxAcceleration);
    query.addBindValue(editingDrone.maxDeceleration);
    query.addBindValue(editingDrone.minTurnRadius);
    query.addBindValue(editingDrone.maxWaypointDistance);
    query.addBindValue(editingDrone.minWaypointDistance);
    query.addBindValue(editingDrone.endurance);
    query.addBindValue(editingDrone.minAltitude);
    query.addBindValue(editingDrone.maxAltitude);
    query.addBindValue(editingDrone.maxClimbRate);
    query.addBindValue(editingDrone.maxDescentRate);

    if (editingDrone.id != -1) {
        query.addBindValue(editingDrone.id);
    }

    return query.exec();
}

bool DroneDialog::deleteDroneFromDatabase(int droneId)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return false;
    }

    QSqlQuery query;
    query.prepare("DELETE FROM drones WHERE id = ?");
    query.addBindValue(droneId);

    return query.exec();
}

int DroneDialog::countRoutesUsingDrone(int droneId)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return 0;
    }

    QSqlQuery query;
    query.prepare("SELECT COUNT(*) FROM rutas_metadata WHERE drone_id = ?");
    query.addBindValue(droneId);

    if (query.exec() && query.next()) {
        return query.value(0).toInt();
    }

    return 0;
}

bool DroneDialog::deleteRoutesUsingDrone(int droneId)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return false;
    }

    // Obtener nombres de rutas asociadas
    QSqlQuery query;
    query.prepare("SELECT nombre_ruta FROM rutas_metadata WHERE drone_id = ?");
    query.addBindValue(droneId);

    QStringList routesToDelete;
    if (query.exec()) {
        while (query.next()) {
            routesToDelete.append(query.value(0).toString());
        }
    }

    // Eliminar metadatos de rutas
    QSqlQuery deleteMetaQuery;
    deleteMetaQuery.prepare("DELETE FROM rutas_metadata WHERE drone_id = ?");
    deleteMetaQuery.addBindValue(droneId);

    if (!deleteMetaQuery.exec()) {
        return false;
    }

    // Eliminar tablas de rutas
    for (const QString &routeName : routesToDelete) {
        QSqlQuery dropQuery;
        dropQuery.exec(QString("DROP TABLE IF EXISTS %1").arg(routeName));
    }

    return true;
}

bool DroneDialog::isDroneNameUnique(const QString &name, int excludeId)
{
    QSqlDatabase db = QSqlDatabase::database();
    if (!db.isOpen()) {
        return false;
    }

    QSqlQuery query;
    if (excludeId == -1) {
        query.prepare("SELECT COUNT(*) FROM drones WHERE nombre = ?");
        query.addBindValue(name);
    } else {
        query.prepare("SELECT COUNT(*) FROM drones WHERE nombre = ? AND id != ?");
        query.addBindValue(name);
        query.addBindValue(excludeId);
    }

    if (query.exec() && query.next()) {
        return query.value(0).toInt() == 0;
    }

    return false;
}

void DroneDialog::onAccept()
{
    // Obtener dron seleccionado
    QListWidget *routeDroneList = groupRouteConfig->findChild<QListWidget*>();
    switch (currentMode) {
    case SELECT_DRONE:
        if (listDrones->selectedItems().isEmpty()) {
            QMessageBox::warning(this, "Validación", "Debe seleccionar un dron.");
            return;
        }
        cancelled = false;
        accept();
        break;

    case CREATE_ROUTE:
        if (!validateRouteInputs()) {
            return;
        }

        if (routeDroneList && !routeDroneList->selectedItems().isEmpty()) {
            int droneId = routeDroneList->currentItem()->data(Qt::UserRole).toInt();

            // Buscar el dron seleccionado
            for (const auto &drone : dronesList) {
                if (drone.id == droneId) {
                    selectedDrone = drone;
                    break;
                }
            }
        }

        cancelled = false;
        accept();
        break;

    case MANAGE_DRONES:
        // En modo gestión, simplemente cerrar
        cancelled = false;
        accept();
        break;
    }
}

void DroneDialog::onCancel()
{
    cancelled = true;
    reject();
}

void DroneDialog::closeEvent(QCloseEvent *event)
{
    cancelled = true;
    QDialog::closeEvent(event);
}

void DroneDialog::onDroneTypeChanged()
{
    // Actualizar límites recomendados
    DroneType currentType = radioQuadcopter->isChecked() ? DroneType::QUADCOPTER : DroneType::FIXED_WING;
    updateRecommendedLimits(currentType);

    // Solo cargar valores por defecto si estamos en modo de creación (no edición)
    if (stackedWidget->currentIndex() == 1 && !isEditingExistingDrone) {
        loadDefaultValues(currentType, true);  // Preservar nombre
    } else if (stackedWidget->currentIndex() == 1) {
        // Si estamos editando, solo actualizar el tipo
        editingDrone.type = currentType;
    }
}

DroneCharacteristics DroneDialog::getSelectedDrone() const
{
    return selectedDrone;
}

QString DroneDialog::getRouteName() const
{
    if (currentMode == CREATE_ROUTE) {
        return editRouteName->text().trimmed();
    }
    return "";
}
