#ifndef LOCATIONVALIDATOR_H
#define LOCATIONVALIDATOR_H

#include <QQueue>
#include <QDebug>
#include <algorithm>

struct COORDENADAS
{
    double MIN_LAT =  17.099f;
    double MAX_LAT =  31.541f;
    double MIN_LON = -91.846f;
    double MAX_LON = -68.071f;
};
class LocationValidator
{
public:
    LocationValidator(int threshold = 3);

    bool validateLocation(double lat, double lon);
    void reset();
    COORDENADAS getCoordenadas() const;
    void setCoordenadas(const COORDENADAS &value);

private:
    bool isInWorkArea(double lat, double lon);
    bool shouldTriggerAlert();
    QQueue<bool> m_buffer;
    int m_threshold;
    COORDENADAS coordenadas;
};

#endif // LOCATIONVALIDATOR_H
