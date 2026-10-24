# EstacionTerrena

Estación de control terrestre (ECT) para drones de ala fija y cuadrópteros:
telemetría por puerto serie, instrumentos de vuelo, mandos y ganancias del
autopiloto, planificación de rutas y mapa sin conexión.

El mapa es **libmapa** (repositorio `LibMapaStatic`), instalada aparte.

## Qué hace falta

- Qt 5.14 / 5.15 / 6.x con los módulos `sql`, `network`, `positioning`,
  `printsupport`, `serialport` y `svg` (y `core5compat` en Qt 6).
- **libmapa instalada con el mismo Qt**: por defecto en `C:/libmapa/qt6`
  (o `qt5`). Ver `docs/DESPLIEGUE.md` de LibMapaStatic. Trae dentro QCustomPlot
  2.1.1; la estación solo usa su cabecera (`Principal/graficar/qcustomplot.h`).
- El **paquete de datos del mapa** (la carpeta con `mapa.json` y las
  `.sqlitedb`): en el PC de desarrollo, `D:\QtPro\Recursos`.

## Compilar

Abrir `EstacionTierra3.pro` en Qt Creator con el kit de Qt con el que se
instaló libmapa y compilar. Si libmapa está en otra carpeta:

    qmake EstacionTierra3.pro LIBMAPA_PREFIX=D:/otra/carpeta

Los tests (`tests/`) se lanzan con `make check` (en Linux sin pantalla,
`QT_QPA_PLATFORM=offscreen make check`).

## Dónde busca el mapa

En `<carpeta de trabajo>/../Recursos`, como siempre. Otra carpeta se indica
en `conf.ini` (junto al ejecutable, en la carpeta de trabajo):

    [MAPA]
    paquete=D:/QtPro/Recursos

Si el paquete no trae elevación, se usa la carpeta de `.hgt` de siempre
(`C:/CubaAlt`). Los polígonos que dibuja el operador se guardan en
`Recursos/data/dibujos.db`, junto a la base de drones.

## Documentación

- `docs/BITACORA.md`: qué se ha hecho y por qué.
- `docs/PENDIENTES.md`: lo que queda (fase de mejora del código).
