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

## 4. Enlace por serie, TCP o UDP y simulador del dron

Sin dron para probar, hacía falta un simulador, y poder hablar con él (o con un
puente de radio) por red además de por el puerto serie. El protocolo **no
cambia**: lo define quien programa el autopiloto, así que los bytes son los de
siempre; lo que se ha mejorado es cómo se leen.

- **`Protocolo/Tramas`** (solo Qt Core): arma y lee las tramas de los dos lados
  (telemetría `abc`, acuses `xyz`/`uvw`; mandos y plan con su suma de 16 bits).
  Los `static_assert` fijan el tamaño de cada estructura (32, 91, 126, 79 y 29
  bytes): si alguien toca una, no compila en vez de mandar bytes corridos.
- **La lectura (máquina de estados) mejorada**, con el mismo significado:
  - Se decodifica en cuanto llegan bytes, todas las tramas que haya. Antes un
    temporizador de 500 ms daba un paso por vez y, con una trama a medias, se
    volvía a llamar sin parar (CPU al 100 % hasta que llegaba el resto).
  - No se consume la cabecera hasta tener la trama entera: si llega basura, se
    resincroniza en la siguiente cabecera sin perder la buena.
  - En modo cuadróptero, un `xyz`/`uvw` suelto dejaba la lectura atascada para
    siempre (encontraba esa cabecera una y otra vez sin quitarla); ahora en ese
    modo solo se busca `abc`.
  - La basura acumulada sin ninguna trama se recorta (no crece sin límite).
- **`SerialManager`** sigue siendo el enlace con el dron y con las mismas
  señales, pero ahora abre puerto serie, TCP (cliente, con tope de 5 s para
  conectar) o UDP (puerto local; remoto fijo o el último que habló). En
  `CControladora`, los mandos y el plan se arman con `Tramas` en vez de las dos
  copias a mano (un test comprueba que salen los mismos bytes).
- **`Widget_Conexion`**: selector de enlace con los campos de cada uno,
  guardados en `conf.ini` (`[ENLACE]`).
- **`simulador/` (`simulador_dron`)**: dron de mentira, de consola, con las
  mismas tramas (comparte `Tramas` y `ProyeccionMercator` con la estación).
  Ala fija o cuadróptero; da vueltas a casa, o en régimen automático sigue el
  plan recibido (cumple cada punto dentro del radio de acercamiento, en metros
  Mercator) y devuelve en `Cumplir_Punto` el punto tal como llegó, para que la
  estación pinte los cumplidos. Aplica los mandos (régimen, radio, casa) y
  devuelve las ganancias. Por TCP (servidor), UDP y serie a la vez.
- **Tests**: `test_tramas` (15 casos; cada trama armada campo a campo con
  `QDataStream` sin usar las estructuras, a trozos, con basura...) y
  `test_enlace` (la estación contra el simulador por TCP, UDP y, con `socat`,
  por un par de puertos serie virtuales; y la posición del cuadróptero
  reconstruida como en `CControladora`).
- Comprobado además con la aplicación entera en una pantalla virtual: con el
  simulador abierto, *Enlace* TCP y **Conectar**, el indicador se mueve y el dron
  vuela en el mapa con su traza y su rumbo.

## 5. Planificar una ruta, mandarla al simulador y que obedezca los mandos

Con el simulador ya conectado, el siguiente paso era el circuito completo:
planificar la ruta en la estación, mandarla, y que el simulador la cumpla y
obedezca la pantalla de Mandos.

**En la estación (lo que fallaba al probarlo):**
- *Enviar* en la ventana de rutas decía «✓ Ruta enviada exitosamente» y ponía
  el botón en verde AL MOMENTO, aunque no hubiera enlace (entonces no se
  mandaba nada). Ahora, sin enlace, avisa y no envía; y el verde llega con el
  acuse del dron (`uvw1`), que ya estaba conectado a ese botón.
- Se esperan los acuses con un tope de 3 s: plan sin confirmar → aviso; mandos
  sin confirmar → aviso en la barra de estado. Confirmados, se dice en la barra.
- Si el dron rechaza el plan (`uvw0`) se reenvía, pero como mucho 3 veces
  (antes, sin límite).
- Con el plan confirmado se repinta la ruta y vuelve a contarse desde el
  principio qué puntos están cumplidos.
- Los mandos de las pantallas de Mandos y de Ganancias salen por una sola
  función (`enviarMandos`), en vez de dos copias.

**En el simulador (obedece los mandos como los presenta la pantalla de
Mandos):** régimen (Manual y Estabilizado dan vueltas a casa, porque no hay
mando de radio; Automático sigue la misión), misión (Puntos: una vez y vueltas
sobre el último; Circular: vueltas sobre el punto al que iba; Repetir; RTL: a
casa a V.RTH y a la altura de casa), RPunto, casa y paracaídas (baja a 5 m/s).
Las ganancias se devuelven solo si llegan marcadas con «Actualizar». Cada punto
del plan trae su altura y su velocidad. `--mision` fija la misión inicial.

Lo que hace al terminar la misión Puntos (vueltas sobre el último punto) y en
Circular es una suposición razonable: hay que confirmar con quien programa el
autopiloto qué hace el de verdad (anotado en pendientes).

**Test nuevo `test_simulador`** (9 casos, sin red): cada misión, manual,
paracaídas, ganancias, acuses y el punto a cumplir en la telemetría.

## 6. El último punto cumplido y el final de la misión

Probando la ruta con el simulador, **el último punto nunca se ponía verde**. La
estación marcaba como cumplidos los puntos ANTERIORES al que el dron dice que
va a cumplir (`Cumplir_Punto`); tras el último no hay "siguiente", así que ese
no se marcaba nunca, hiciera lo que hiciera el dron después.

- `Protocolo/SeguimientoCumplidos` (solo Qt Core) lleva la cuenta: la regla de
  siempre y, además, el punto al que va queda cumplido en cuanto el dron entra
  en su radio de acercamiento (RPunto, con 5 m de margen por la precisión).
  No depende de lo que mande el autopiloto al terminar. Sustituye los dos
  bloques repetidos de `CControladora`; el plan se le da al confirmarlo el dron
  y, si la ruta se repinta, los cumplidos se vuelven a pintar.
- El simulador, al terminar la misión Puntos, **vuelve a casa y da vueltas
  sobre ella** (como pidió el usuario), igual que con RTL: a V.RTH y a la altura
  de casa.
- `test_simulador`: `laEstacionMarcaTodosLosPuntos` aplica la lógica de la
  estación a las tramas del simulador y exige los tres puntos; sin la regla del
  radio salen dos (comprobado), que es el fallo que se vio.

## 7. Los rótulos del mapa se pueden arrastrar

El rótulo de la línea al próximo punto («095° · 149 m») no se dejaba mover, y
tapaba el del dron. La causa: `MapaMision` rehacía en cada trama (10 por
segundo) la línea de rumbo y el objetivo del dron con el desplazamiento del
rótulo a cero, así que lo que arrastraba el operador se borraba al instante.
Ahora se conserva el desplazamiento que tengan (libmapa ya los deja arrastrar).
`test_mapamision::rotulosArrastradosSeQuedan` lo comprueba (sin el arreglo, el
rótulo vuelve a (0,0) con la siguiente trama).
