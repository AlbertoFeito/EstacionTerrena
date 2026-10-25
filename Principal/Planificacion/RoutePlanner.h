#ifndef ROUTEPLANNER_H
#define ROUTEPLANNER_H

// Motor de validacion y analisis de rutas (antes dentro de Widget_Puntos.h).
// Solo Qt Core y Positioning: se prueba sin pantalla (test_planificador).

#include "Estructuras/E_Punto.h"
#include "Planificacion/CaracteristicasDron.h"

#include <QList>
#include <QPair>
#include <QString>
#include <QStringList>
#include <QVariant>

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

#endif // ROUTEPLANNER_H
