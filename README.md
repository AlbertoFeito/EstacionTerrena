# EstacionTerrena

Estación de control terrestre (ECT) para drones de ala fija y cuadrópteros:
telemetría por puerto serie, instrumentos de vuelo, mandos y ganancias del
autopiloto, planificación de rutas y mapa sin conexión.

El mapa es **libmapa** (repositorio `LibMapaStatic`), instalada aparte.

## Qué hace falta

- Qt 5.14 / 5.15 / 6.x con los módulos `sql`, `network`, `positioning`,
  `printsupport`, `serialport` y `svg`. Ya no hace falta `core5compat`.
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

Por este orden:

1. **El primer argumento**: la ruta del `mapa.json` (o de su carpeta).

       EstacionTerrena.exe D:/QtPro/Recursos/mapa.json

   En Qt Creator: *Proyectos → Ejecutar → Argumentos de la línea de órdenes*.
2. `conf.ini` (en la carpeta de trabajo):

       [MAPA]
       paquete=D:/QtPro/Recursos/mapa.json

3. `<carpeta de trabajo>/../Recursos`, como siempre.

Del paquete solo se muestran las capas base (Satelital y OSM): ni mapa híbrido
ni capas fijas, de momento. Si el paquete no trae elevación, se usa la carpeta
de `.hgt` de siempre (`C:/CubaAlt`).

## Enlace con el dron: serie, TCP o UDP

En la pestaña *Configuración*, **Enlace** elige por dónde llega el mismo
protocolo de siempre (el dron no cambia nada):

- **Serie**: el puerto, a 115200 8N1, como siempre.
- **TCP**: la estación se conecta como cliente a `host:puerto` (el simulador o
  un puente de radio hacen de servidor). Por defecto `127.0.0.1:5760`.
- **UDP**: escucha en un puerto (14551) y manda a `host:puerto` (14550). Con el
  host vacío contesta a quien le mande la telemetría.

Lo elegido se guarda en `conf.ini`, sección `[ENLACE]`.

## Simulador (sin dron)

`simulador_dron` (se compila con el resto, queda junto al ejecutable) es un dron
de mentira que habla el mismo protocolo: manda la telemetría de ala fija o de
cuadróptero y contesta a los mandos y al plan con su acuse. Sin opciones abre a
la vez TCP (servidor en 5760) y UDP (escucha en 14550, manda a
127.0.0.1:14551), justo lo que la estación trae por defecto.

    simulador_dron                     ala fija dando vueltas a casa
    simulador_dron --tipo cuadro       cuadróptero
    simulador_dron --regimen 2         sigue el plan en cuanto llega
    simulador_dron --serie COM6        también por puerto serie
    simulador_dron --help              todas las opciones

En la estación: *Selecciona Drone* uno del mismo tipo que el simulado (ala fija
o cuadróptero), *Enlace* TCP o UDP y **Conectar**. Para mandar el plan, la ruta
de 10 puntos en *Rutas*; para que la siga, *Mandos* en régimen automático.

Por puerto serie sin cable hace falta un par de puertos virtuales:
[com0com](https://com0com.sourceforge.net/) en Windows (p. ej. COM5↔COM6: el
simulador en COM6 y la estación en COM5) o `socat` en Linux.

## Documentación

- `docs/BITACORA.md`: qué se ha hecho y por qué.
- `docs/PENDIENTES.md`: lo que queda (fase de mejora del código).
