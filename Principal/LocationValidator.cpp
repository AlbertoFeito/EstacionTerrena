#include "LocationValidator.h"
#include "MyQSettings/myqsetting.h"

#include <QDir>


LocationValidator::LocationValidator(int threshold) : m_threshold(threshold)
{
    MyQSetting settings;
    QString  fileName = QDir::currentPath() + "/conf.ini";
    coordenadas.MIN_LAT = settings.cargarSetting(fileName,"MIN_LAT","COORDENADAS", 17.099f).value<double>();
    coordenadas.MAX_LAT = settings.cargarSetting(fileName,"MAX_LAT","COORDENADAS", 31.541f).value<double>();
    coordenadas.MIN_LON = settings.cargarSetting(fileName,"MIN_LON","COORDENADAS",-91.846f).value<double>();
    coordenadas.MAX_LON = settings.cargarSetting(fileName,"MAX_LON","COORDENADAS",-68.071f).value<double>();
}

bool LocationValidator::validateLocation(double lat, double lon) {
    bool currentValid = isInWorkArea(lat, lon);

    m_buffer.enqueue(currentValid);
    if(m_buffer.size() > m_threshold) m_buffer.dequeue();

    return currentValid;
}

void LocationValidator::reset() {
    m_buffer.clear();
}

bool LocationValidator::isInWorkArea(double lat, double lon) {
    // Área de Cuba con márgenes de seguridad


    return (lat >= coordenadas.MIN_LAT) && (lat <= coordenadas.MAX_LAT) &&
            (lon >= coordenadas.MIN_LON) && (lon <= coordenadas.MAX_LON);
}

bool LocationValidator::shouldTriggerAlert() {
    if(m_buffer.size() < m_threshold) return true; // Considerar válido hasta tener suficientes datos

    int invalidCount = std::count_if(m_buffer.begin(), m_buffer.end(), [](bool v) { return !v; });
    return invalidCount < 2; // Máximo 1 inválida en las últimas 3 lecturas
}

COORDENADAS LocationValidator::getCoordenadas() const
{
    return coordenadas;
}

void LocationValidator::setCoordenadas(const COORDENADAS &value)
{
    coordenadas = value;
}
