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

#include "Planificacion/CaracteristicasDron.h"

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
