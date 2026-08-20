#ifndef DRONEDIALOG_H
#define DRONEDIALOG_H

#include <QDialog>
#include <QComboBox>
#include <QLineEdit>
#include <QPushButton>
#include <QLabel>
#include <QGridLayout>
#include <QGroupBox>
#include <QRadioButton>
#include <QDoubleValidator>
#include <QMessageBox>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QListWidget>
#include <QStackedWidget>

// Enumeración para tipo de dron
enum class DroneType {
    QUADCOPTER,
    FIXED_WING
};

// Estructura para características del dron (COMPLETAMENTE ACTUALIZADA)
struct DroneCharacteristics {
    int id;  // Para base de datos
    QString nombre;
    DroneType type;
    double maxRange;              // autonomía máxima en metros
    double maxSpeed;              // velocidad máxima en m/s
    double minSpeed;              // NUEVO: velocidad mínima en m/s (especialmente para ala fija)
    double maxAcceleration;       // NUEVO: aceleración máxima en m/s²
    double maxDeceleration;       // NUEVO: desaceleración máxima en m/s²
    double minTurnRadius;         // radio mínimo de giro en metros
    double maxWaypointDistance;   // distancia máxima entre waypoints
    double minWaypointDistance;   // distancia mínima entre waypoints
    double endurance;             // tiempo máximo de vuelo en segundos
    double minAltitude;           // altura mínima en metros
    double maxAltitude;           // altura máxima en metros
    double maxClimbRate;          // NUEVO: tasa máxima de ascenso en m/s
    double maxDescentRate;        // NUEVO: tasa máxima de descenso en m/s

    DroneCharacteristics() {
        id = -1;
        nombre = "";
        type = DroneType::QUADCOPTER;
        maxRange = 5000.0;
        maxSpeed = 20.0;
        minSpeed = 0.0;           // Quadcopter puede hover
        maxAcceleration = 2.0;    // Valor razonable por defecto
        maxDeceleration = 3.0;    // Desaceleración puede ser mayor
        minTurnRadius = 0.0;
        maxWaypointDistance = 100.0;
        minWaypointDistance = 5.0;
        endurance = 1800.0;
        minAltitude = 0.0;
        maxAltitude = 5000.0;
        maxClimbRate = 5.0;       // Tasa de ascenso razonable
        maxDescentRate = 5.0;     // Tasa de descenso razonable
    }

    QString tipoToString() const {
        return (type == DroneType::QUADCOPTER) ? "Quadcopter" : "Ala Fija";
    }

    static DroneType stringToTipo(const QString &str) {
        QString lower = str.toLower();
        if (lower == "quadcopter" || lower == "quadcópter" || lower == "quad")
            return DroneType::QUADCOPTER;
        else
            return DroneType::FIXED_WING;
    }

    // Método para obtener límites recomendados por tipo
    QMap<QString, QPair<double, double>> getRecommendedLimits() const {
        QMap<QString, QPair<double, double>> limits;

        if (type == DroneType::QUADCOPTER) {
            limits["minSpeed"] = QPair<double, double>(0.0, 5.0);       // Puede hover
            limits["maxSpeed"] = QPair<double, double>(10.0, 25.0);
            limits["minTurnRadius"] = QPair<double, double>(0.0, 10.0);
            limits["maxClimbRate"] = QPair<double, double>(3.0, 10.0);
        } else {
            limits["minSpeed"] = QPair<double, double>(8.0, 15.0);      // Necesita velocidad mínima
            limits["maxSpeed"] = QPair<double, double>(15.0, 40.0);
            limits["minTurnRadius"] = QPair<double, double>(10.0, 100.0);
            limits["maxClimbRate"] = QPair<double, double>(2.0, 6.0);
        }

        return limits;
    }
};

class DroneDialog : public QDialog
{
    Q_OBJECT

public:
    enum Mode {
        SELECT_DRONE,    // Seleccionar dron existente para ruta
        CREATE_ROUTE,    // Crear ruta con dron seleccionado
        MANAGE_DRONES    // Gestionar drones (crear, editar, eliminar)
    };

    explicit DroneDialog(Mode mode, QWidget *parent = nullptr);
    ~DroneDialog();

    DroneCharacteristics getSelectedDrone() const;
    QString getRouteName() const;
    Mode getMode() const { return currentMode; }

protected:
    void closeEvent(QCloseEvent *event) override;

private slots:
    void onDroneTypeChanged();
    void onAccept();
    void onCancel();

    // Gestión de drones
    void onNewDrone();
    void onEditDrone();
    void onDeleteDrone();
    void onSaveDrone();
    void onCancelEdit();

    // Lista de drones
    void onDroneListSelectionChanged();
    void onDroneDoubleClicked(QListWidgetItem *item);

private:
    // Métodos de UI
    void setupUI();
    void setupValidators();

    // Métodos de navegación
    void showDroneListScreen();
    void showDroneFormScreen(bool isNew);
    void showRouteFormScreen();

    // Métodos de datos
    void loadDronesFromDatabase();
    bool validateDroneInputs();
    bool validateRouteInputs();
    void loadDefaultValues(DroneType type, bool preserveName = true);
    void updateFormFromDrone(const DroneCharacteristics &drone);
    void clearDroneForm();
    void updateRecommendedLimits(DroneType type);  // NUEVO

    // Métodos de base de datos
    bool saveDroneToDatabase();
    bool deleteDroneFromDatabase(int droneId);
    int countRoutesUsingDrone(int droneId);
    bool deleteRoutesUsingDrone(int droneId);
    bool isDroneNameUnique(const QString &name, int excludeId = -1);

    // Widgets principales
    QStackedWidget *stackedWidget;

    // Pantalla 1: Lista de drones
    QListWidget *listDrones;
    QPushButton *btnNewDrone;
    QPushButton *btnEditDrone;
    QPushButton *btnDeleteDrone;
    QPushButton *btnSelectDrone;
    QPushButton *btnBackFromList;

    // Pantalla 2: Formulario de dron (CAMBIOS AÑADIDOS)
    QGroupBox *groupDroneConfig;
    QRadioButton *radioQuadcopter;
    QRadioButton *radioFixedWing;
    QLineEdit *editDroneName;
    QLineEdit *editMaxRange;
    QLineEdit *editMaxSpeed;
    QLineEdit *editMinSpeed;           // NUEVO
    QLineEdit *editMaxAcceleration;    // NUEVO
    QLineEdit *editMaxDeceleration;    // NUEVO
    QLineEdit *editMinTurnRadius;
    QLineEdit *editMaxWaypointDistance;
    QLineEdit *editMinWaypointDistance;
    QLineEdit *editEndurance;
    QLineEdit *editMinAltitude;
    QLineEdit *editMaxAltitude;
    QLineEdit *editMaxClimbRate;       // NUEVO
    QLineEdit *editMaxDescentRate;     // NUEVO
    QPushButton *btnSaveDrone;
    QPushButton *btnCancelEdit;

    // Pantalla 3: Formulario de ruta
    QGroupBox *groupRouteConfig;
    QLineEdit *editRouteName;
    QPushButton *btnCreateRoute;
    QPushButton *btnBackFromRoute;

    // Botones generales
    QPushButton *btnAccept;
    QPushButton *btnCancel;

    // Variables de estado
    Mode currentMode;
    Mode originalMode;
    DroneCharacteristics selectedDrone;
    DroneCharacteristics editingDrone;
    QList<DroneCharacteristics> dronesList;
    bool cancelled;
    bool isEditingExistingDrone;
};

#endif // DRONEDIALOG_H
