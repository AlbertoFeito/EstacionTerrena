// ============================================================================
// Widget_Puntos.h
// ============================================================================

#ifndef WIDGET_PUNTOS_H
#define WIDGET_PUNTOS_H

#include "dronedialog.h"
#include "AlturaWorker.h"
#include "Estructuras/E_Punto.h"
#include "PerfilAltitudWidget.h"

#include <QGeoCoordinate>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QSqlError>
#include <QDebug>
#include <QtWidgets>
#include <cmath>
#include <QtMath>
#include <QAbstractTableModel>
#include <QList>
#include <QTimer>
#include <QPair>
#include <QMessageBox>
#include <QFileDialog>
#include <QTextStream>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QClipboard>
#include <QApplication>
#include <QProgressDialog>
#include <QSet>

// ============================================================================
// CLASE RoutePlanner – Motor de validación (SIN OPTIMIZACIÓN)
// ============================================================================

/**
 * @brief Clase encargada de validar rutas y puntos según las características del dron.
 *
 * Proporciona métodos para verificar distancias, altitudes, velocidades, transiciones,
 * radios de giro, y genera análisis detallados de la ruta.
 */
class RoutePlanner {
public:
    /**
     * @brief Constructor.
     * @param chars Características del dron que se usarán para las validaciones.
     */
    RoutePlanner(const DroneCharacteristics &chars);

    /**
     * @brief Valida que la distancia entre dos waypoints esté dentro de los límites del dron.
     * @param wp1 Primer waypoint.
     * @param wp2 Segundo waypoint.
     * @param errorMsg Cadena de salida con el mensaje de error si la validación falla.
     * @return true si la distancia es válida, false en caso contrario.
     */
    bool validarDistanciaWaypoint(const E_Punto &wp1, const E_Punto &wp2, QString &errorMsg) const;

    /**
     * @brief Valida que la altitud de un punto esté dentro del rango permitido.
     * @param punto Punto a validar.
     * @param errorMsg Cadena de salida con el mensaje de error si la validación falla.
     * @return true si la altitud es válida, false en caso contrario.
     */
    bool validarAltitud(const E_Punto &punto, QString &errorMsg) const;

    /**
     * @brief Valida que la velocidad en un punto esté dentro de los límites del dron.
     * @param punto Punto a validar.
     * @param errorMsg Cadena de salida con el mensaje de error si la validación falla.
     * @return true si la velocidad es válida, false en caso contrario.
     */
    bool validarVelocidadPunto(const E_Punto &punto, QString &errorMsg) const;

    /**
     * @brief Valida la transición de velocidad entre dos waypoints, comprobando que la aceleración no supere los límites.
     * @param actual Waypoint actual.
     * @param siguiente Siguiente waypoint.
     * @param distancia Distancia entre ellos.
     * @param errorMsg Cadena de salida con el mensaje de error si la validación falla.
     * @return true si la transición es válida, false en caso contrario.
     */
    bool validarTransicionVelocidad(const E_Punto &actual, const E_Punto &siguiente,
                                    double distancia, QString &errorMsg) const;

    /**
     * @brief Valida la transición de altitud entre dos waypoints, comprobando pendiente y tasa vertical.
     * @param actual Waypoint actual.
     * @param siguiente Siguiente waypoint.
     * @param distancia Distancia entre ellos.
     * @param errorMsg Cadena de salida con el mensaje de error si la validación falla.
     * @return true si la transición es válida, false en caso contrario.
     */
    bool validarTransicionAltitud(const E_Punto &actual, const E_Punto &siguiente,
                                  double distancia, QString &errorMsg) const;

    /**
     * @brief Valida que el radio de giro en un punto sea suficiente para la velocidad y tipo de dron.
     * @param punto Punto a validar.
     * @param errorMsg Cadena de salida con el mensaje de error si la validación falla.
     * @return true si el radio es válido, false en caso contrario.
     */
    bool validarRadioGiro(const E_Punto &punto, QString &errorMsg) const;

    /**
     * @brief Calcula el radio de giro mínimo teórico para una velocidad dada (solo para ala fija).
     * @param velocidad Velocidad en m/s.
     * @return Radio mínimo teórico en metros.
     */
    double calcularRadioMinimoTeorico(double velocidad) const;

    /**
     * @brief Calcula el radio de giro recomendado (1.5 veces el mínimo teórico).
     * @param velocidad Velocidad en m/s.
     * @return Radio recomendado en metros.
     */
    double calcularRadioGiroRecomendado(double velocidad) const;

    /**
     * @brief Calcula la distancia total de la ruta sumando las distancias entre waypoints consecutivos.
     * @param puntos Lista de puntos de la ruta.
     * @return Distancia total en metros.
     */
    double calcularDistanciaTotal(const QList<E_Punto> &puntos) const;

    /**
     * @brief Calcula el tiempo total de vuelo y la energía estimada para la ruta.
     * @param puntos Lista de puntos de la ruta.
     * @return Par (tiempo en segundos, energía en julios).
     */
    QPair<double, double> calcularTiempoVuelo(const QList<E_Punto> &puntos) const;

    /**
     * @brief Estima la potencia requerida para un segmento de vuelo.
     * @param p1 Punto de inicio.
     * @param p2 Punto de fin.
     * @param distancia Distancia del segmento.
     * @return Potencia estimada en vatios.
     */
    double estimarPotencia(const E_Punto &p1, const E_Punto &p2, double distancia) const;

    /**
     * @brief Estima el consumo de energía total de la ruta en vatios-hora, con un factor de seguridad.
     * @param puntos Lista de puntos de la ruta.
     * @return Consumo estimado en Wh.
     */
    double estimarConsumoEnergia(const QList<E_Punto> &puntos) const;

    /**
     * @brief Valida la ruta completa.
     * ...
     *
     * \startuml
     * start
     * :Recibir lista de puntos;
     * if (cantidad < 2) then (sí)
     *   :Retornar error;
     *   stop
     * endif
     *
     * :Para cada punto;
     * while (puntos por validar)
     *   :validarAltitud;
     *   if (falla) then (sí)
     *     :Retornar error;
     *     stop
     *   endif
     *   :validarVelocidad;
     *   if (falla) then (sí)
     *     :Retornar error;
     *     stop
     *   endif
     *   :validarRadioGiro;
     *   if (falla) then (sí)
     *     :Retornar error;
     *     stop
     *   endif
     * endwhile
     *
     * :Para cada par de puntos consecutivos;
     * while (pares por validar)
     *   :Calcular distancia;
     *   :validarDistanciaWaypoint;
     *   if (falla) then (sí)
     *     :Retornar error;
     *     stop
     *   endif
     *   :validarTransicionVelocidad;
     *   if (falla) then (sí)
     *     :Retornar error;
     *     stop
     *   endif
     *   :validarTransicionAltitud;
     *   if (falla) then (sí)
     *     :Retornar error;
     *     stop
     *   endif
     * endwhile
     *
     * :Calcular distancia total;
     * if (distancia > maxRange) then (sí)
     *   :Retornar error;
     *   stop
     * endif
     *
     * :Calcular tiempo de vuelo;
     * if (tiempo > endurance) then (sí)
     *   :Retornar error;
     *   stop
     * endif
     *
     * :Retornar éxito;
     * stop
     * \enduml
     */
    QPair<bool, QString> validarRuta(const QList<E_Punto> &puntos) const;

    /**
     * @brief Realiza validaciones adicionales sobre el perfil completo de la ruta (cambios bruscos, etc.).
     * @param puntos Lista de puntos de la ruta.
     * @return Cadena de error vacía si todo es correcto, o mensaje descriptivo.
     */
    QString validarPerfilCompleto(const QList<E_Punto> &puntos) const;

    /**
     * @brief Realiza un análisis detallado de la ruta, devolviendo múltiples métricas y puntos críticos.
     * @param puntos Lista de puntos de la ruta.
     * @return Mapa con todos los datos del análisis (distancia, tiempo, energía, puntos críticos, etc.).
     */
    QVariantMap analizarRutaDetallada(const QList<E_Punto> &puntos) const;

    /**
     * @brief Detecta puntos críticos en la ruta (giros cerrados, cambios bruscos, etc.).
     * @param puntos Lista de puntos de la ruta.
     * @return Lista de mapas con información detallada de cada punto crítico.
     */
    QVariantList detectarPuntosCriticos(const QList<E_Punto> &puntos) const;

    /**
     * @brief Obtiene el color hexadecimal correspondiente al nivel de criticidad.
     * @param nivel Nivel de criticidad ("CRÍTICO", "ALTO", "MODERADO", "BAJO").
     * @return Código de color en formato #RRGGBB.
     */
    QString obtenerColorCriticidad(const QString &nivel) const;

    /**
     * @brief Clasifica el nivel de criticidad de un punto basado en el número de problemas, ángulo y cambio de velocidad.
     * @param numProblemas Número de problemas detectados.
     * @param angulo Ángulo de giro.
     * @param cambioVelocidad Cambio de velocidad en el segmento.
     * @return Cadena con el nivel ("CRÍTICO", "ALTO", "MODERADO", "BAJO").
     */
    QString clasificarCriticidad(int numProblemas, double angulo, double cambioVelocidad) const;

    /**
     * @brief Clasifica el nivel de seguridad según el margen calculado.
     * @param margen Margen de seguridad en porcentaje.
     * @return Cadena con el nivel ("MUY ALTO", "ALTO", "MODERADO", "BAJO", "MUY BAJO").
     */
    QString clasificarNivelSeguridad(double margen) const;

    /**
     * @brief Analiza cada segmento de la ruta y devuelve métricas por segmento.
     * @param puntos Lista de puntos de la ruta.
     * @return Lista de mapas con datos de cada segmento.
     */
    QVariantList analizarSegmentos(const QList<E_Punto> &puntos) const;

    /**
     * @brief Calcula el ángulo de giro entre tres puntos consecutivos.
     * @param p1 Punto anterior.
     * @param p2 Punto actual.
     * @param p3 Punto siguiente.
     * @return Ángulo en grados (0-180).
     */
    double calcularAnguloGiro(const E_Punto &p1, const E_Punto &p2, const E_Punto &p3) const;

    /**
     * @brief Calcula el margen de seguridad de la ruta en porcentaje.
     * @param puntos Lista de puntos de la ruta.
     * @return Margen de seguridad (0-100).
     */
    double calcularMargenSeguridad(const QList<E_Punto> &puntos) const;

    /**
     * @brief Calcula el factor de carga máximo experimentado por el dron en la ruta.
     * @param puntos Lista de puntos de la ruta.
     * @return Factor de carga máximo (adimensional, g's).
     */
    double calcularFactorCargaMaximo(const QList<E_Punto> &puntos) const;

    /**
     * @brief Calcula un índice de estrés estimado para la ruta (0-100).
     * @param puntos Lista de puntos de la ruta.
     * @return Índice de estrés.
     */
    double calcularEstresRuta(const QList<E_Punto> &puntos) const;

private:
    DroneCharacteristics droneChars; ///< Características del dron.
    double g;                        ///< Aceleración de la gravedad (m/s²).
    double densidadAire;              ///< Densidad del aire (kg/m³) para cálculos aerodinámicos.
};

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

// ============================================================================
// CLASE Widget_Puntos – INTERFAZ GRÁFICA PRINCIPAL (VERSIÓN COMPACTA, SIN .ui)
// ============================================================================

/**
 * @brief Widget principal para la planificación y gestión de rutas.
 *
 * Contiene la tabla de puntos, métricas, botones de acción y se comunica con el modelo
 * y el mapa. Permite crear, cargar, editar, validar y enviar rutas de 10 puntos.
 */
class Widget_Puntos : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructor.
     * @param alturaWorker Worker de alturas (para obtener elevación del terreno).
     * @param parent Widget padre.
     */
    explicit Widget_Puntos(AlturaWorker *alturaWorker, QWidget *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~Widget_Puntos();

    /**
     * @brief Obtiene el puntero al modelo de datos.
     * @return MiModelo*.
     */
    MiModelo *getModelo() const;

    /**
     * @brief Obtiene el combo box de selección de rutas.
     * @return QComboBox*.
     */
    QComboBox *getBoxNombreRuta() const;

    // Control de visibilidad de la ruta en el mapa
    /**
     * @brief Indica si la ruta está actualmente visible en el mapa.
     * @return true si visible.
     */
    bool isRouteVisible() const;

    /**
     * @brief Establece la visibilidad de la ruta en el mapa.
     * @param visible true para mostrar, false para ocultar.
     */
    void setRouteVisible(bool visible);

    /**
     * @brief Establece la ruta actual en el combo box (por nombre real).
     * @param routeName Nombre real de la ruta.
     * @param blockSignals Si true, bloquea las señales del combo durante la operación.
     */
    void setCurrentRouteInCombo(const QString &routeName, bool blockSignals = true);

    // Actualización de interfaz
    /**
     * @brief Actualiza el combo box de rutas consultando las tablas existentes en la base de datos.
     */
    void actualizarComboBoxRutas();

    /**
     * @brief Actualiza las etiquetas de métricas y el estado de la ruta en la interfaz.
     */
    void mostrarMetricasRuta();

    /**
     * @brief Muestra un cuadro de diálogo con el análisis detallado de la ruta.
     */
    void mostrarAnalisisDetallado();

    /**
     * @brief Exporta el análisis de la ruta a un archivo de texto.
     * @param analisis Mapa con los datos del análisis.
     */
    void exportarAnalisis(const QVariantMap &analisis);

signals:
    // Señales para comunicación con otros módulos (principalmente CControladora)
    void sI_pintaRuta(bool visible);                            ///< Solicita pintar/ocultar la ruta en el mapa.
    void sI_Ruta(QList<QPair<double, double>>);                 ///< Emite la ruta como lista de pares (lat, lon).
    void sI_Ruta2(QList<E_Punto>);                              ///< Emite la ruta como lista de objetos E_Punto.
    void sI_RutaValidada(bool valida, const QString &mensaje); ///< Emite resultado de validación.
    void sI_DronConfigurado(const QString &tipoDron, const QString &nombreRuta); ///< Dron configurado.
    void sI_AnalisisActualizado(const QVariantMap &analisis); ///< Análisis actualizado.
    void sI_iniciarSeleccionPuntoInicial();                    ///< Inicia modo selección de punto inicial.
    void sI_cancelarSeleccionPuntoInicial();                   ///< Cancela modo selección.
    void sI_rutasModificadas();                                 ///< Lista de rutas modificada.
    void sI_rutaSeleccionada(const QString &routeName);        ///< Ruta seleccionada.
    void sI_visibilidadCambiada(const QString &routeName, bool visible); ///< Cambió visibilidad de una ruta.
    void closed();                                              ///< El widget se cerró.

public slots:
    /**
     * @brief Agrega un nuevo punto a la ruta (llamado desde el mapa o desde otros módulos).
     * @param punto Punto a agregar.
     */
    void sL_agregarNuevoPunto(const E_Punto &punto);

    /**
     * @brief Actualiza puntos existentes en la ruta (llamado desde el mapa).
     * @param puntos Lista de puntos modificados.
     */
    void sL_actualizaPunto(const QList<E_Punto> &);

    /**
     * @brief Selecciona la ruta cuyo nombre real está en el combo (usado internamente).
     * @param nombreRuta No usado;
     */
    void seleccionarRuta(const QString &);

    /**
     * @brief Muestra un acuse de recibo visual en el botón de enviar.
     */
    void on_AcuseRecibo();

    /**
     * @brief Restaura el estilo original del botón de enviar.
     */
    void on_restoreStyle();

    /**
     * @brief Slot llamado cuando el modelo emite rutaConfigurada.
     * @param tipoDron Tipo de dron configurado.
     * @param nombreRuta Nombre de la ruta configurada.
     */
    void onRutaConfigurada(const QString &tipoDron, const QString &nombreRuta);

    /**
     * @brief Slot llamado cuando el modelo emite analisisActualizado.
     * @param analisis Mapa con el análisis actualizado.
     */
    void onAnalisisActualizado(const QVariantMap &analisis);

    /**
     * @brief Maneja el movimiento de un punto en el mapa, actualizando coordenadas y altitud.
     * @param index Índice del punto movido.
     */
    void onPointMoved(int index);

    /**
     * @brief Inicia el modo de selección del punto inicial (P1) haciendo clic derecho en el mapa.
     */
    void iniciarSeleccionPuntoInicial();

    /**
     * @brief Cancela el modo de selección de punto inicial.
     */
    void cancelarSeleccionPuntoInicial();

    /**
     * @brief Agrega el punto inicial seleccionado y genera los 9 puntos restantes automáticamente.
     * @param coordenada Coordenada del punto P1.
     */
    void agregarPuntoInicialYGenerarResto(const QGeoCoordinate &coordenada);

    /**
     * @brief Carga una ruta desde la base de datos y actualiza la interfaz.
     * @param routeName Nombre de la ruta a cargar.
     */
    void cargarRuta(const QString &routeName);

private slots:
    // Slots de los botones de la interfaz
    void on_pb_guardar_clicked();
    void on_pB_CrearRuta_clicked();
    void on_pB_EliminaRuta_clicked();
    void on_pB_LimpiaRuta_clicked();
    void on_pB_EnviaRuta_clicked();
    void on_pb_Mostrar_toggled(bool checked);
    void on_pB_AnalisisDetallado_clicked();
    void on_closeButtonClicked();
    void mostrarPerfil();
    void mostrarGuia();

private:
    // Miembros de datos
    QSqlDatabase *mdb;                         ///< Conexión a la base de datos.
    MiModelo *modelo;                           ///< Modelo de datos de la tabla.
    QTimer m_timer;                             ///< Temporizador para el acuse de recibo.
    QString m_originalStyle;                     ///< Estilo original del botón enviar.
    bool m_modoSeleccionPuntoInicial;            ///< Indica si estamos en modo selección de P1.
    AlturaWorker *m_alturaWorker;                ///< Worker de alturas.
    PerfilAltitudWidget *m_perfilWidget;         ///< Widget de perfil de altitud.
    QPushButton *m_btnPerfil;                    ///< Botón "Perfil".
    QPoint m_dragPosition;                       ///< Posición para arrastrar la ventana.
    bool m_dragging;                             ///< Indica si se está arrastrando la ventana.
    QPushButton *m_closeButton;                  ///< Botón de cierre de la ventana.
    QWidget *titleBar;                           ///< Barra de título personalizada.

    // Widgets de la interfaz
    QComboBox *cB_Rutas;                         ///< Combo de selección de rutas.
    QTableView *tw_puntosRuta;                   ///< Tabla de puntos.
    QLabel *label_Distancia;                     ///< Etiqueta de distancia total.
    QLabel *label_AutonomiaRest;                  ///< Etiqueta de autonomía restante (%).
    QLabel *label_Tiempo;                         ///< Etiqueta de tiempo estimado.
    QProgressBar *progressAutonomia;              ///< Barra de progreso de autonomía.
    QLabel *label_Estado;                         ///< Etiqueta de estado de la ruta.
    QLabel *labelInfoDron;                        ///< Etiqueta de información del dron.
    QLabel *label_PuntosFijos;                    ///< Etiqueta informativa de puntos fijos.

    QPushButton *pB_CrearRuta;                    ///< Botón "Nueva".
    QPushButton *pB_EliminaRuta;                  ///< Botón "Eliminar".
    QPushButton *pB_LimpiaRuta;                   ///< Botón "Restablecer".
    QPushButton *pb_guardar;                      ///< Botón "Guardar".
    QPushButton *pB_EnviaRuta;                    ///< Botón "Enviar".
    QPushButton *pb_Mostrar;                      ///< Botón "Mostrar/Ocultar" (toggle).
    QPushButton *pB_AnalisisDetallado;            ///< Botón "Análisis".
    QPushButton *pB_CancelarSeleccion;            ///< Botón "Cancelar" (modo selección P1).

    // Botones ocultos (por compatibilidad con código existente)
    QPushButton *pB_EliminaPunto;
    QPushButton *pB_Cargar;
    QPushButton *pB_ExportarAnalisis;

    // Métodos auxiliares
    double calcularAltitudVuelo(double terreno, const DroneCharacteristics &drone) const;
    void generarPuntosPorDefecto();
    void updateUIForDroneConfigured(bool configured);
    void mostrarInfoDronActual();
    void limpiarTodo();
    void habilitarBotonesRuta(bool habilitar);
    QString obtenerNombreTablaLimpio(const QString &nombreTablaConFormato);
    void mostrarRecomendaciones(const QStringList &recomendaciones);
    void mostrarPuntosCriticosDetalles(const QVariantList &puntosCriticos);
    void actualizarPanelAnalisis(const QVariantMap &analisis);

protected:
    void closeEvent(QCloseEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
};

#endif // WIDGET_PUNTOS_H
