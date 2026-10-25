#ifndef FILAVUELO_H
#define FILAVUELO_H

#include <QByteArray>
#include <QList>

/*!
 * \brief Una fila de un vuelo grabado, como la manda Control de objetivo al
 *        reproducirlo (texto separado por comas).
 *
 * Campos: 0 "=", 1 vuelo, 2 lat, 3 lon, 4 cabeceo, 5 banqueo, 6 velocidad
 * (km/h: se guarda ya pasada), 7 altura, 8 curso, 9 distancia de chequeo, 10 curso deseado, 11
 * regimen, 12 mision, 13 curso IMU, 14 satelites, 15..25 ganancias, 26..29
 * actuadores, 30..31 convergencias (banqueo, cabeceo), 32 segundos desde el
 * principio. Son las columnas de DatosVuelos: tal cual llegaron del dron
 * (grados), salvo la velocidad, que se guarda en km/h.
 *
 * Antes la estacion leia los campos sin mirar cuantos traia la linea (una
 * corta la tumbaba), giraba el avion del mapa con la velocidad (campo 6) en
 * lugar del curso (8) y pasaba el curso deseado de radianes a grados aunque
 * se grabo en grados.
 */
struct FilaVuelo
{
    static constexpr int kCampos = 32;   //!< sin contar el tiempo (32), que puede faltar

    bool valida = false;
    double lat = 0, lon = 0;
    double cabeceo = 0, banqueo = 0, velocidad = 0, altura = 0, curso = 0;
    double distanciaChequeo = 0, cursoDeseado = 0, cursoImu = 0;
    int regimen = 0, mision = 0, satelites = 0;
    double ganancias[11] = {};
    double actuadores[4] = {};
    double convergenciaBanqueo = 0, convergenciaCabeceo = 0;

    //! Lee una fila; con menos campos de los debidos, valida = false.
    static FilaVuelo leer(const QByteArray &linea)
    {
        FilaVuelo f;
        const QList<QByteArray> c = linea.split(',');
        if (c.size() < kCampos)
            return f;
        auto n = [&c](int i) { return c.at(i).trimmed().toDouble(); };
        f.lat = n(2);
        f.lon = n(3);
        f.cabeceo = n(4);
        f.banqueo = n(5);
        f.velocidad = n(6);
        f.altura = n(7);
        f.curso = n(8);
        f.distanciaChequeo = n(9);
        f.cursoDeseado = n(10);
        f.regimen = int(n(11));
        f.mision = int(n(12));
        f.cursoImu = n(13);
        f.satelites = int(n(14));
        for (int i = 0; i < 11; ++i)
            f.ganancias[i] = n(15 + i);
        for (int i = 0; i < 4; ++i)
            f.actuadores[i] = n(26 + i);
        f.convergenciaBanqueo = n(30);
        f.convergenciaCabeceo = n(31);
        f.valida = true;
        return f;
    }
};

#endif // FILAVUELO_H
