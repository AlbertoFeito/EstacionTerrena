#ifndef MODELORUTA_H
#define MODELORUTA_H

// Modelo de la tabla de puntos de la ruta (MiModelo) y su base de datos (antes
// dentro de Widget_Puntos.h): los 10 puntos, su validacion con RoutePlanner y
// el guardado en SQLite (tablas drones, rutas_metadata y una por ruta).

#include "AlturaWorker.h"
#include "Estructuras/E_Punto.h"
#include "Planificacion/RoutePlanner.h"

#include <QAbstractTableModel>
#include <QGeoCoordinate>
#include <QList>
#include <QMap>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

// ============================================================================
// CLASE MiModelo – Modelo de tabla (SIN OPTIMIZACIÓN)
// ============================================================================

/**
 * @brief Modelo de tabla que gestiona los puntos de la ruta y su persistencia en base de datos.
 *
 * Hereda de QAbstractTableModel y proporciona un modelo editable con 10 columnas.
 * Incluye validación, análisis y almacenamiento en SQLite.
 */
class MiModelo : public QAbstractTableModel
{
    Q_OBJECT

public:
    /**
     * @brief Roles personalizados para acceder a datos específicos.
     */
    enum Role {
        IdRole = Qt::UserRole + 1,       ///< ID del punto.
        ErrorRole = Qt::UserRole + 2,    ///< Errores del punto.
        WarningRole = Qt::UserRole + 3,   ///< Advertencias del punto.
        CriticoRole = Qt::UserRole + 4,   ///< Indica si el punto es crítico.
        AnalisisRole = Qt::UserRole + 5   ///< Datos de análisis del punto (tasa, distancia, etc.).
    };

    explicit MiModelo(QObject *parent = nullptr);
    ~MiModelo();

    // Drones
    /**
     * @brief Carga la lista de drones desde la base de datos.
     * @return true si se cargaron correctamente (al menos uno), false en caso contrario.
     */
    bool cargarDronesDesdeBD();

    /**
     * @brief Obtiene la lista de drones cargados.
     * @return Lista de características de drones.
     */
    QList<DroneCharacteristics> getDronesList() const;

    /**
     * @brief Guarda un dron en la base de datos (inserta o actualiza).
     * @param drone Características del dron a guardar.
     * @return true si la operación fue exitosa, false en caso contrario.
     */
    bool guardarDroneEnBD(const DroneCharacteristics &drone);

    // Rutas
    /**
     * @brief Configura una nueva ruta con un dron específico.
     * @param drone Características del dron.
     * @param routeName Nombre de la ruta.
     */
    void configurarRuta(const DroneCharacteristics &drone, const QString &routeName);

    /**
     * @brief Guarda los metadatos de una ruta en la tabla `rutas_metadata`.
     * @param nombreRuta Nombre de la ruta.
     * @param drone Características del dron asociado.
     * @return true si la operación fue exitosa.
     */
    bool guardarMetadatosRuta(const QString &nombreRuta, const DroneCharacteristics &drone);

    /**
     * @brief Carga los metadatos de una ruta y completa la estructura de dron.
     * @param nombreRuta Nombre de la ruta.
     * @param drone Referencia a la estructura que se llenará con los datos.
     * @return true si se encontró la ruta y se cargaron los datos.
     */
    bool cargarMetadatosRuta(const QString &nombreRuta, DroneCharacteristics &drone);

    /**
     * @brief Obtiene las características del dron actualmente configurado.
     * @return Estructura DroneCharacteristics.
     */
    DroneCharacteristics getDroneCharacteristics() const;

    /**
     * @brief Obtiene el nombre de la ruta actual.
     * @return Nombre de la ruta (cadena vacía si no hay ninguna).
     */
    QString getCurrentRouteName() const;

    /**
     * @brief Indica si hay un dron configurado (es decir, existe un planificador).
     * @return true si hay dron configurado.
     */
    bool isDroneConfigured() const;

    // Validación y análisis
    /**
     * @brief Valida la ruta actual usando el planificador.
     * @return Par (válido, mensaje).
     */
    QPair<bool, QString> validarRuta() const;

    /**
     * @brief Obtiene métricas básicas de la ruta (distancia, tiempo, porcentajes, etc.).
     * @return Mapa con las métricas.
     */
    QVariantMap obtenerMetricasRuta() const;

    /**
     * @brief Realiza un análisis detallado de la ruta (delega en RoutePlanner).
     * @return Mapa con todos los datos del análisis.
     */
    QVariantMap analizarRutaDetallada() const;

    /**
     * @brief Obtiene una lista de recomendaciones basadas en el análisis de la ruta.
     * @return Lista de cadenas con recomendaciones.
     */
    QStringList obtenerRecomendaciones() const;

    // Gestión de puntos – CON RESTRICCIÓN DE 10 PUNTOS
    /**
     * @brief Valida y agrega un nuevo punto a la ruta.
     * ...
     *
     * \startuml
     * actor Usuario
     * participant "MiModelo" as Model
     * participant "RoutePlanner" as Planner
     * database "Base de datos" as DB
     *
     * Usuario -> Model: validarYAgregarElemento(punto)
     * activate Model
     *
     * Model -> Planner: validarAltitud(punto)
     * Planner --> Model: ok/error
     *
     * Model -> Planner: validarVelocidadPunto(punto)
     * Planner --> Model: ok/error
     *
     * Model -> Planner: validarRadioGiro(punto)
     * Planner --> Model: ok/error
     *
     alt hay punto anterior
     *   Model -> Planner: validarDistanciaWaypoint(anterior, punto)
     *   Planner --> Model: ok/error
     *   Model -> Planner: validarTransicionVelocidad(...)
     *   Planner --> Model: ok/error
     *   Model -> Planner: validarTransicionAltitud(...)
     *   Planner --> Model: ok/error
     * end
     *
     * Model -> Model: beginInsertRows()
     * Model -> Model: append punto
     * Model -> Model: endInsertRows()
     *
     * Model -> DB: insertarElementosEnTabla()
     * DB --> Model: éxito
     *
     * Model --> Usuario: resultado
     * deactivate Model
     * \enduml
     */
    QPair<bool, QString> validarYAgregarElemento(const E_Punto &elemento);

    /**
     * @brief Agrega un punto a la ruta sin retornar mensaje (usa validarYAgregarElemento internamente).
     * @param elemento Punto a agregar.
     */
    void agregarElemento(const E_Punto &elemento);

    /**
     * @brief Valida una lista de puntos (sin agregarlos) contra las restricciones del dron.
     * @param puntos Lista de puntos a validar.
     * @return Par (válido, mensaje).
     */
    QPair<bool, QString> validarYActualizarPuntos(const QList<E_Punto> &puntos);

    /**
     * @brief Actualiza la posición de puntos existentes en la base de datos y revalida la ruta.
     * @param nombreTabla Nombre de la tabla de la ruta.
     * @param elementos Lista de puntos con los nuevos datos (solo se actualizan los que coinciden por id).
     * @return true si todas las actualizaciones fueron exitosas.
     */
    bool actualizarElementosEnTabla(const QString &nombreTabla, const QList<E_Punto> &elementos);

    /**
     * @brief Elimina todos los puntos de la ruta actual.
     */
    void borraTodosLosElementos();

    /**
     * @brief Reinicia el modelo: borra puntos, elimina el planificador y limpia el nombre de ruta.
     */
    void resetearModelo();

    /**
     * @brief Obtiene la lista de puntos actual.
     * @return Referencia constante a la lista interna.
     */
    const QList<E_Punto> &obtenerLista() const;

    // Garantizar 10 puntos
    /**
     * @brief Ajusta el número de puntos de la ruta a un número fijo (por defecto 10), añadiendo o eliminando según sea necesario.
     * @param numRequerido Número de puntos deseado.
     */
    void ajustarAPuntosFijos(int numRequerido = 10);

    /**
     * @brief Genera puntos por defecto para una nueva ruta, teniendo en cuenta el dron configurado y el terreno si está disponible.
     * @param count Número de puntos a generar.
     * @param base Coordenada base para el primer punto (si no se especifica, usa un punto por defecto).
     * @param distancia Distancia entre puntos (si es <=0, se usa la distancia óptima).
     * @param azimut Dirección de avance en grados.
     * @return Lista de puntos generados.
     */
    QList<E_Punto> generarPuntosPorDefecto(int count = 10,
                                           const QGeoCoordinate &base = QGeoCoordinate(23.0, -82.0),
                                           double distancia = 100.0,
                                           double azimut = 90.0) const;

    // Persistencia
    /**
     * @brief Guarda la ruta actual en un archivo CSV.
     * @param nombreFile Nombre del archivo (debe terminar en .csv).
     * @return true si se guardó correctamente.
     */
    bool guardarCSV(const QString &nombreFile);

    /**
     * @brief Crea una nueva tabla en la base de datos para almacenar una ruta.
     * @param nombreTabla Nombre de la tabla.
     * @param nombresColumnas Lista de nombres de columnas (sin incluir id).
     * @return true si la tabla se creó correctamente.
     */
    bool crearNuevaTabla(const QString &nombreTabla, const QStringList &nombresColumnas);

    /**
     * @brief Inserta un punto en la tabla de la ruta.
     * @param nombreTabla Nombre de la tabla.
     * @param elemento Punto a insertar.
     * @return true si la inserción fue exitosa.
     */
    bool insertarElementosEnTabla(const QString &nombreTabla, const E_Punto &elemento);

    /**
     * @brief Actualiza un punto existente en la tabla de la ruta.
     * @param nombreTabla Nombre de la tabla.
     * @param elemento Punto con los nuevos datos (se identifica por id).
     * @return true si la actualización fue exitosa.
     */
    bool actualizarElementoEnTabla(const QString &nombreTabla, const E_Punto &elemento);

    /**
     * @brief Carga una ruta desde la base de datos, incluyendo sus puntos y el dron asociado.
     * @param nombreRuta Nombre de la ruta (tabla).
     */
    void CargarRutaDesdeBaseDatos(const QString &nombreRuta);

    /**
     * @brief Elimina un punto específico de la ruta por su ID.
     * @param nombreTabla Nombre de la tabla.
     * @param id ID del punto a eliminar.
     * @return true si se eliminó correctamente.
     */
    bool eliminarRutaPorId(const QString &nombreTabla, int id);

    /**
     * @brief Elimina completamente una tabla de ruta y sus metadatos.
     * @param nombreTabla Nombre de la tabla a eliminar.
     * @return true si la operación fue exitosa.
     */
    bool eliminarTablaRuta(const QString &nombreTabla);

    /**
     * @brief Limpia todos los puntos de la tabla de ruta (los elimina, pero conserva la tabla y metadatos).
     * @param nombreTabla Nombre de la tabla.
     * @return true si la operación fue exitosa.
     */
    bool limpiaRutaCompleta(const QString &nombreTabla);

    // Interfaz QAbstractTableModel
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    bool setData(const QModelIndex &index, const QVariant &value, int role = Qt::EditRole) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

    bool insertRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;
    bool removeRows(int row, int count, const QModelIndex &parent = QModelIndex()) override;

    // Revalidación
    /**
     * @brief Revalida completamente la ruta: recalcula distancias, errores, advertencias y puntos críticos.
     *        Emite dataChanged para toda la tabla.
     */
    void revalidarRutaCompleta();

    // Utilidades
    /**
     * @brief Obtiene las coordenadas de todos los puntos de la ruta como pares (lat, lon).
     * @return Lista de pares.
     */
    QList<QPair<double, double>> obtenerCoordenadasRuta();

    // AlturaWorker (opcional para validaciones)
    /**
     * @brief Establece el worker de alturas para obtener elevación del terreno.
     * @param worker Puntero al worker.
     */
    void setAlturaWorker(AlturaWorker *worker) { m_alturaWorker = worker; }

signals:
    /**
     * @brief Señal emitida cuando la ruta ha sido modificada (puntos, orden, etc.).
     */
    void rutaModificada();

    /**
     * @brief Señal emitida cuando se configura una nueva ruta.
     * @param tipoDron Tipo de dron configurado.
     * @param nombreRuta Nombre de la ruta.
     */
    void rutaConfigurada(const QString &tipoDron, const QString &nombreRuta);

    /**
     * @brief Señal emitida cuando se actualiza el análisis de la ruta.
     * @param analisis Mapa con el análisis detallado.
     */
    void analisisActualizado(const QVariantMap &analisis);

private:
    QList<E_Punto> listaDatos;                 ///< Lista de puntos de la ruta.
    DroneCharacteristics droneChars;            ///< Características del dron actual.
    RoutePlanner *planner;                      ///< Planificador/validador.
    QString currentRouteName;                   ///< Nombre de la ruta actual.
    QList<DroneCharacteristics> dronesList;     ///< Lista de drones cargados.
    QMap<int, QString> erroresPorPunto;         ///< Errores asociados a cada punto (por ID).
    AlturaWorker *m_alturaWorker = nullptr;     ///< Worker para obtener alturas del terreno.

    /**
     * @brief Inicializa la base de datos SQLite y crea las tablas necesarias si no existen.
     */
    void inicializarBaseDeDatos();

    /**
     * @brief Limpia los errores y advertencias de un punto y sus adyacentes.
     * @param puntoId ID del punto central.
     */
    void limpiarErroresAdyacentes(int puntoId);
};

#endif // MODELORUTA_H
