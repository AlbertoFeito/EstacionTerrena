// ============================================================================
// Estructuras/E_Punto.h (con comentarios Doxygen añadidos)
// ============================================================================

#ifndef E_PUNTO_H
#define E_PUNTO_H

#include <QGeoCoordinate>
#include <QString>
#include <cmath>
#include <QtMath>

/**
 * @brief Estructura que representa un waypoint o punto de ruta.
 *
 * Contiene toda la información necesaria para definir un punto en la ruta:
 * identificador, nombre, modo, coordenadas geográficas, altitud, velocidad,
 * radio de giro, distancias y estado de validación.
 */
struct E_Punto
{
    int id;                         ///< Identificador único del punto.
    QString nombre;                  ///< Nombre del punto (ej. "P1", "P2").
    int modo;                        ///< Modo de operación (reservado para futuros usos).
    QGeoCoordinate pos;               ///< Posición geográfica (latitud, longitud).
    float altura;                    ///< Altitud sobre el nivel del mar (metros).
    float velocidad;                 ///< Velocidad deseada en el punto (m/s).
    float radio;                     ///< Radio de giro en el punto (metros).
    double distanciaAnterior;         ///< Distancia desde el punto anterior (metros).
    float altitudObjetivo;            ///< Altitud objetivo (puede usarse para planificación).
    float tiempoTransicion;           ///< Tiempo estimado desde el punto anterior (segundos).
    double tasaAscenso;               ///< Tasa de ascenso/descenso calculada (m/s).
    bool puntoCritico;                ///< Indica si el punto se considera crítico.
    QStringList advertencias;         ///< Lista de advertencias asociadas al punto.

    /**
     * @brief Calcula la distancia geodésica a otro punto.
     * @param otro Punto destino.
     * @return Distancia en metros.
     */
    double distanciaA(const E_Punto &otro) const {
        return pos.distanceTo(otro.pos);
    }

    /**
     * @brief Calcula la diferencia de altitud con otro punto.
     * @param otro Punto destino.
     * @return Diferencia de altitud (altura de otro - altura de este).
     */
    double diferenciaAltitud(const E_Punto &otro) const {
        return otro.altura - altura;
    }

    /**
     * @brief Calcula la pendiente media entre este punto y otro.
     * @param otro Punto destino.
     * @return Pendiente en grados (0 si distancia horizontal muy pequeña).
     */
    double calcularPendiente(const E_Punto &otro) const {
        double distanciaHorizontal = distanciaA(otro);
        if (distanciaHorizontal < 0.1) return 0.0;
        double diferenciaAltura = otro.altura - altura;
        return qRadiansToDegrees(atan(diferenciaAltura / distanciaHorizontal));
    }

    /**
     * @brief Calcula la tasa de ascenso/descenso necesaria para llegar a otro punto.
     * @param otro Punto destino.
     * @return Tasa vertical en m/s (positiva para ascenso, negativa para descenso).
     */
    double calcularTasaVertical(const E_Punto &otro) const {
        double distancia = distanciaA(otro);
        if (distancia < 0.1) return 0.0;
        double diferenciaAltura = otro.altura - altura;
        double velocidadPromedio = (velocidad + otro.velocidad) / 2.0;
        if (velocidadPromedio < 0.1) return 0.0;
        double tiempo = distancia / velocidadPromedio;
        return diferenciaAltura / tiempo;
    }

    /**
     * @brief Valida la velocidad del punto contra límites dados.
     * @param minSpeed Velocidad mínima permitida.
     * @param maxSpeed Velocidad máxima permitida.
     * @param error Cadena de salida con el error si la validación falla.
     * @return true si la velocidad es válida.
     */
    bool velocidadValida(double minSpeed, double maxSpeed, QString &error) const {
        if (velocidad > maxSpeed) {
            error = QString("Velocidad %1 m/s excede máximo %2 m/s").arg(velocidad).arg(maxSpeed);
            return false;
        }
        if (velocidad < minSpeed) {
            error = QString("Velocidad %1 m/s menor al mínimo %2 m/s").arg(velocidad).arg(minSpeed);
            return false;
        }
        return true;
    }

    bool operator==(const E_Punto &otro) const {
        return id == otro.id;
    }

    bool operator!=(const E_Punto &otro) const {
        return !(*this == otro);
    }
};

#endif // E_PUNTO_H
