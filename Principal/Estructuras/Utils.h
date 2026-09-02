#ifndef UTILS_H
#define UTILS_H

#include <QtCore>
#include <QtGlobal>

/**
 * @brief Clase utilitaria para funciones comunes del proyecto.
 * centralized common utilities to avoid code duplication (DRY principle).
 */
class Utils
{
public:
    Utils() = delete;

    /**
     * @brief Calculate checksum for serial protocol.
     * Suma todos los bytes y retorna el checksum de 16 bits.
     * @param data Byte array a procesar
     * @return Checksum de 16 bits
     */
    static quint16 calculateChecksum(const QByteArray &data) {
        quint16 checksum = 0;
        for (char byte : data) {
            checksum += static_cast<quint8>(byte);
        }
        return checksum;
    }
};

#endif // UTILS_H