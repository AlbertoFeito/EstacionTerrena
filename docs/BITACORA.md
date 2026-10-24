# Bitácora de EstacionTerrena

Secciones numeradas en orden cronológico. Cada una cuenta qué se hizo y por qué.

## 1. Del mapa antiguo (LibMapaStatic) a libmapa

**Qué había.** La estación compilaba su propia copia de la librería de mapa
antigua (`LibMapaStatic/`, estática, con QCustomPlot dentro) y la usaba casi
toda desde `CControladora`: `libMapa->getOMapa()->…` para el dron
(`nuevoPunto`), la casa (`pintaElipseHome`), las líneas de rumbo
(`pintaRumbosVehiculo`), la ruta (`pinta_PuntoRuta`, `rellenaProximoPunto`,
`setRadioAcercamiento`) y las herramientas. Además, `Widget_Puntos`
convertía a mano los metros de un punto arrastrado y `Widget_Mandos` y
`Widget_Ganancias_Control` recibían la proyección del mapa.

**Qué se ha hecho.**

- `Principal/Mapa/MapaMision`: el único sitio que habla con libmapa.
  Traduce lo que entiende la estación a lo de libmapa:

  | Antes | Ahora (libmapa) |
  |---|---|
  | `nuevoPunto(VehiculoTerrestre, …)` y traza `TODAS` | un objetivo (`addTarget`) con su icono girado por rumbo y la traza entera |
  | `pintaElipseHome` | punto «Home» con una H en la capa temporal `mision` |
  | `pintaRumbosVehiculo` | tres marcaciones (`BearingLine`) que salen del dron; la del próximo punto rotula rumbo y distancia |
  | `pinta_PuntoRuta`, `rellenaProximoPunto` | línea y puntos arrastrables en la capa temporal `ruta`; los cumplidos, en verde |
  | `setRadioAcercamiento` | un anillo (`RangeRings`) por punto, que lo sigue al arrastrarlo |
  | `pointMoved` + `inverse(metros)` | `featureDragFinished`: el punto llega ya en grados |
  | zoom área/más/menos, polígono, medir | herramientas de `MapWidget` |

  Las capas `ruta` y `mision` son temporales (no se guardan en la base de
  entidades): la ruta vive en la base de rutas y el vuelo se rehace con la
  telemetría. Los polígonos del operador sí se guardan, en
  `Recursos/data/dibujos.db`.

- `Principal/Protocolo/ProyeccionMercator`: la proyección Web Mercator del
  autopiloto, sacada de la librería antigua. El plan (`uvw`), la casa de los
  mandos (`xyz`) y el punto a cumplir van en metros de esta proyección; la de
  libmapa es privada y, sobre todo, no se puede arriesgar que cambie un
  decimal. Se comprobó contra `CGoogleMercator` con dos millones de puntos al
  azar: **cero diferencias bit a bit**. Se conservan también las rarezas del
  protocolo (norte primero, metros truncados a enteros al armar el plan).

- **El radio de acercamiento** llega en metros Mercator (los del autopiloto,
  y así lo dibujaba la librería antigua). libmapa dibuja metros sobre el
  terreno, así que el anillo usa `radio · cos(lat)`: sale del mismo tamaño
  que antes.

- `AlturaWorker` pregunta la cota al mapa (`elevationAt`) en vez de leer a
  mano los `.hgt` de `C:/CubaAlt`. Si el paquete no trae elevación, el mapa
  usa esa misma carpeta. Mantiene su interfaz: `Widget_Puntos` y
  `PerfilAltitudWidget` no cambian.

- La posición del ratón y la cota van a la barra de estado (antes se
  escribían en el log en cada movimiento del ratón), y la medida de distancia
  también.

- **QCustomPlot una sola vez.** libmapa ya la lleva compilada (la misma
  2.1.1); compilar también `graficar/qcustomplot.cpp` duplicaba los
  símbolos. Las gráficas usan solo la cabecera.

- El paquete del mapa se busca donde siempre (`<trabajo>/../Recursos`, que
  en el PC es `D:\QtPro\Recursos`) o donde diga `conf.ini` (`[MAPA]
  paquete=`). Si no se abre, se avisa con la causa.

- La carpeta `LibMapaStatic/` sale del build pero sigue en el árbol: se
  borra en un PR aparte, después de probar éste con el dron.

**Para poder comprobarlo fuera de Windows** se arreglaron cosas pequeñas que
impedían compilar en Linux sin cambiar nada en Windows: `DeviceHotplug` y
`SerialPortComboBox` ponen lo de Windows bajo `Q_OS_WIN`, un include con la
mayúscula equivocada (`PerfilAltitudWidget.h`), `QRegExp` →
`QRegularExpression` en `dronedialog.cpp` (en Qt 6 `QString::contains`
ya no acepta `QRegExp`), y `usbdeviceswatcher` declara su bucle de udev y
enlaza `-ludev`.

**Tests nuevos** (`tests/`, `make check`):
- `test_proyeccion`: valores publicados de EPSG:3857, La Habana con la
  librería antigua e ida y vuelta por toda Cuba.
- `test_mapamision`: sobre un paquete mínimo que se fabrica el propio test,
  comprueba la ruta, los anillos, los puntos cumplidos, el dron con su traza
  y sus rumbos, limpiar el vuelo y que arrastrar un punto con el ratón emite
  su nueva posición.

**Comprobado en la nube** (Qt 6.4, GCC): libmapa instalada con qmake, la
estación compila y enlaza contra ella, los 15 casos de test pasan y la
aplicación arranca sin pantalla sobre un paquete de prueba (abre las dos capas
y pinta teselas) y, sin paquete, avisa sin caerse. Falta la prueba con el dron
real y el mapa de verdad en el PC.

## 2. El mapa por argumento y fuera core5compat

- **Ruta del mapa por argumento.** `EstacionTerrena D:/QtPro/Recursos/mapa.json`
  (o la carpeta) abre ese paquete sin depender de la carpeta de trabajo, que
  cambia según se lance desde Qt Creator, un acceso directo o el despliegue.
  Manda sobre `conf.ini` (`[MAPA] paquete=`), y éste sobre la ruta de siempre
  (`<trabajo>/../Recursos`). Si no se abre, el aviso explica las tres vías.
- **Sin `core5compat`.** Solo lo pedía `LogHandler`, que pasaba a GB2312 (la
  codificación del autor original del logger) lo que escribía en la consola
  de Windows: eso estropeaba tildes y eñes, y obligaba a instalar el módulo
  «Qt 5 Compatibility» en Qt 6. Ahora usa la codificación local del sistema
  (`toLocal8Bit`); el fichero de log sigue en UTF-8.

## 3. El mapa como en los ejemplos: solo capas base

Con el mapa real en el PC se veía, pero iba lento al hacer zoom, y el paquete
ponía encima el **mapa híbrido** (las teselas vectoriales de calles que
`mapa.json` declara en `vectorOverlay`), que hay que dibujar en cada zoom.

- `MapaMision` crea el mapa como los ejemplos de libmapa: `MapConfig::dataDir`
  y `autosaveFeatures = false` (no toca la base de entidades del paquete) y
  la barra de escala visible.
- Solo las capas base que ya teníamos (Satelital y OSM): se quita el mapa
  híbrido y se esconden las capas fijas del paquete (aguas, corredores...),
  aunque el `mapa.json` las pida visibles. Los overlays se verán más adelante.
- La cota del ratón se pide a la base de elevación solo cuando el ratón se para
  (150 ms), no en cada movimiento.
- El aviso `QLayout: Attempting to add QLayout "" to QWidget "", which already
  has a layout` venía de `Widget_ControlObjetivo`: creaba el layout de los
  botones del reporte con la ventana como padre, que ya tenía otro. Se crea sin
  padre (luego entra con `addLayout`).
- `test_mapamision` fabrica ahora un MBTiles vectorial mínimo y una capa fija
  visible en su paquete: comprueba que un `MapWidget` a pelo pone el híbrido y
  que `MapaMision` lo quita y esconde la capa fija.
- Los polígonos que dibuja el operador ya no se guardan (antes iban a
  `Recursos/data/dibujos.db`); anotado en pendientes.
