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

## 8. Estado del enlace y panel de vuelo

Lo primero que pedía la vista: si se perdía la telemetría, el dron se quedaba
quieto en el mapa y nada lo decía; y el panel de datos de arriba no se leía
(verde oscuro sobre fondo oscuro, letra pequeña, ganancias que no cambian en
vuelo y columnas «Reserva»).

- **`Protocolo/MonitorEnlace`** (solo Qt Core, la hora se le pasa para poder
  probarlo): cuenta las tramas de telemetría y dice si el enlace está sin
  abrir, esperando, bien, lento (> 1 s sin datos) o perdido (> 3 s). En la barra
  de estado, un indicador con color y texto («tcp 127.0.0.1:5760 · 10 tramas/s ·
  última hace 0,1 s»); perdido: rojo, pitido al empezar y cada 5 s, aviso en el
  panel de vuelo y nota en el log (y otra al recuperarse).
- **`Widgets/PanelVuelo`**: régimen (con el color del avión), misión, punto al
  que va («P3 / 10», «✓ 10/10»), distancia, rumbo actual → deseado y satélites
  (rojo < 6, ámbar < 8), en grande y con unidades. Cada columna nace con el
  ancho de su texto más largo: el panel no cambia de tamaño ni corta valores.
  El panel técnico (`Widget_Parametros`) sigue en *Estado_Drone → Detalle
  técnico*; las gráficas, que ocupan el mismo sitio, esconden los dos.
- Al caerse el enlace TCP ahora se desconecta **antes** de avisar: el aviso es
  un diálogo modal y, mientras estaba abierto, la estación seguía diciendo
  «Desconectar».
- `test_estado` (10 casos): estados y textos del monitor con la hora a mano, y
  textos y colores del panel.
- Visto con la aplicación entera en una pantalla virtual contra el simulador:
  indicador verde en vuelo, panel con los datos, y al cerrar el simulador, el
  enlace se cierra y luego se avisa.

## 9. La telemetría ya no se atasca al mandar, un solo panel de vuelo y pestaña de Mandos nueva

**El atasco.** Al mandar un régimen (sobre todo Automático) la telemetría
parecía pararse. Medido con `strace` en la estación contra el simulador: antes
de mandar, una lectura de 126 bytes (una trama) cada 100 ms; después, las
lecturas se iban espaciando (0,15 s, 0,25 s… 1,1 s) y cada una traía más tramas
atrasadas (hasta 1386 bytes): la estación no daba abasto. Con `gdb`, el hilo
principal estaba siempre dentro de `MapaMision::refrescarRumbos` dibujando
marcaciones. Dos causas:

- En automático, sin plan, el dron manda `Cumplir_Punto = 0,0`, que en Mercator
  es lat 0, lon 0: la línea morada «al próximo punto» iba de Cuba al golfo de
  Guinea, miles de kilómetros de trazo grueso repintados tres veces por trama.
- Las líneas azul y verde medían todo el ancho de la vista.

Ahora los rumbos actual (azul) y deseado (verde) son **vectores de 150 px en
pantalla** a cualquier zoom, y la línea morada solo se pinta si el próximo
punto está a menos de 100 km (si no, no es de esta misión). Medido otra vez:
después de mandar Automático, todas las lecturas siguen siendo una trama cada
100 ms. `test_mapamision::rumbosDeLargoFijoEnPantalla` comprueba el largo a dos
zooms y que con 0,0 no hay línea morada. De paso, `PanelVuelo` solo toca una
etiqueta si cambia su texto o su color (rehacer su hoja de estilo diez veces
por segundo no sale gratis).

**Un solo panel de vuelo.** El detalle técnico era otro panel aparte, debajo
del de vuelo, que no se leía. Ahora va **dentro** del panel de vuelo, plegado:
«Detalle ▾» lo abre (o *Estado_Drone → Detalle técnico*) con el mismo estilo,
en tres bloques: ganancias P / I (cabeceo, banqueo, velocidad, altura, curso,
rumbo), actuadores (estabilizadores, alerones, motor, rumbo; con cuadróptero,
los cuatro motores y sin ganancias, que no las manda) y otros (rumbo de la IMU
y convergencias). Lo que ya está arriba (posición, régimen, satélites…) no se
repite. `Widget_Parametros` se ha borrado. También se arregla «Estabilizado»,
que salía cortado: el ancho de la columna se mide con la fuente real.

**Pestaña de Mandos.** Las cajas de Régimen y Navegación estaban casi vacías y
las pestañas no cabían («Ganacias» cortada y con flechas):

- Régimen y misión son **botones grandes** que se encienden con el color del
  avión (rojo, ámbar, verde) o en azul; navegación y casa, en dos líneas cada
  una; **«Usar posición actual del dron como casa»** pone en Lat/Lon la última
  posición recibida (se manda con Enviar); «Enviar mandos» abajo, a lo ancho.
- Lat y Lon de la casa con 6 decimales (con 4 la casa se mandaba redondeada a
  unos 11 m) y rango de todo el mundo (antes la longitud solo iba de -100 a -60).
- Cuando el dron confirma los mandos, la «H» del mapa se mueve a la casa que
  ha aceptado.
- Las pestañas van a la izquierda, en vertical y con nombres cortos (Enlace,
  Drones, Mandos, Ganancias, Objetivo; el nombre largo en el *tooltip*).
- **V.RTH crecía en cada arranque**: se guardaba en km/h y al leerla se
  multiplicaba otra vez por 3,6 (72 → 259 → 300, el tope). Ahora se guarda en
  m/s, como se lee. Si en `conf.ini` ya quedó un valor inflado, hay que
  corregirlo una vez a mano en la pestaña.
- La trama que emite la pestaña sale de `Tramas::mandos` (antes copiaba los
  bytes del puntero a la estructura; la estación no la usaba, pero era basura).

`test_estado` suma el detalle (plegado, botón, formatos, cuadróptero): 65
pruebas en total.

## 10. La ruta solo se cumple en automático, y la pestaña de Mandos lo deja claro

Aviso del usuario: el dron no debe ir a la ruta hasta estar en automático;
primero se le manda la ruta (la guarda) y solo la cumple en automático.

Comprobado contra el simulador recién arrancado (en manual) mandándole un plan
por TCP: lo guarda («se sigue en régimen automático») y sigue dando vueltas a
casa; la estación tampoco manda nada más junto con el plan. El fallo estaba en
lo que se veía: el simulador, como el autopiloto, **recuerda su régimen**
entre sesiones de la estación, y la pestaña de Mandos siempre arrancaba
diciendo «Manual». Si el dron se había quedado en automático de una prueba
anterior, la ruta nueva se cumplía al llegar aunque la pestaña dijera «Manual».

- La pestaña de Mandos enseña arriba **en qué está el dron** según su
  telemetría («Dron: Automático · Puntos»), en ámbar si no coincide con lo
  elegido abajo: lo elegido no le llega hasta pulsar «Enviar mandos».
- Con la **primera trama de cada conexión**, la pestaña adopta el régimen y la
  misión del dron, así nunca empieza mostrando otra cosa.
- `test_simulador::laRutaSoloSeSigueEnAutomatico`: plan, manual y
  estabilizado (vueltas a casa, sin acercarse a la ruta ni punto a cumplir) y
  luego automático (va al primer punto).
- `test_mandos` (nuevo, 4 casos): adopta el estado del dron, avisa si no
  coincide, la trama sale con lo elegido y V.RTH no crece al arrancar. 70
  pruebas en total.

## 11. Sin enlace, el dron vuelve a casa

El usuario repitió cómo es un vuelo: se conecta en manual, se manda la ruta (el
dron no hace nada), estabilizado (el autopiloto ayuda, pero manda el operador),
automático (cumple la ruta) y, al acabar, a casa a dar vueltas a la velocidad y
la altura de casa. **Si se corta la transmisión, el dron vuelve a casa y da
vueltas igual**. En manual y estabilizado manda el operador; como aquí no hay
mando de radio, el simulador da vueltas a casa.

Lo que ya hacía el simulador se ha fijado con una prueba de la secuencia
entera (`secuenciaDelOperadorHastaCasa`, con V.RTH y altura de casa en los
mandos). Lo nuevo es la pérdida de enlace:

- `SimuladorDron::perderEnlace()` → fase **«ENLACE PERDIDO»**: en cualquier
  régimen, a casa a V.RTH y a la altura de casa, y vueltas sobre ella (lo mismo
  que al terminar la misión y que con RTL). Solo unos **mandos nuevos** lo
  sacan de ahí (el operador vuelve a mandar); una ruta, no.
- `EnlacesSimulador` avisa de que no hay enlace cuando se va la última
  estación TCP y con un **corte de radio simulado**: `--corte 120:20` corta a
  los 120 s durante 20 s (no sale ni entra nada por ninguna vía). La estación lo
  ve como «SIN DATOS» (rojo y pitido), y sirve también con UDP y serie.
- Probado con la app: con `--corte 15:8`, la estación pasa a «SIN DATOS hace
  5 s» y el simulador escribe «Ahora: ENLACE PERDIDO…» y vuelve a casa.
- Pruebas nuevas en `test_simulador`: la secuencia entera, sin enlace en
  automático y en manual, y el aviso de las vías (corte y TCP). 73 en total.

Queda una pregunta para quien programa el autopiloto (en `PENDIENTES.md`): la
estación no manda nada periódico, así que el autopiloto real tiene que
detectar la pérdida por la radio o por tiempo; y qué dice en la telemetría
mientras vuelve sin enlace.

## 12. Repo ordenado: fuera la librería antigua y los recursos sueltos

Un clon nuevo no compilaba: `Principal.pro` pedía `build/Recursos/estilos.qrc`,
que no estaba en el repo, y `build/` (que está en `.gitignore`) tenía 913
ficheros commiteados: iconos, logotipos y dos juegos de hojas de estilo qss.
El usuario no tiene `estilos.qrc` ni necesita nada de eso: «borra toda
referencia; si hace falta algún icono, lo inventas tú».

- Se borran `build/` del repo (vuelve a ser solo la salida de compilación),
  la carpeta `LibMapaStatic/` (la librería antigua, que ya no se compilaba
  desde §1) y `Principal.pro.user`.
- **Iconos dibujados en código** (`Principal/Iconos`): el avión (rojo, ámbar,
  verde según el régimen) y el cuadróptero del mapa, los de la barra (regla,
  lupa de recuadro, acercar, alejar, polígono) y play/pausa y stop del control
  de objetivo. Sin ficheros ni plugins de imagen, y se ven en claro y oscuro.
  Los de avisos de la tabla de puntos usan los del estilo de Qt (los
  `:/icons/...` que pedía no existían en ningún `.qrc`); el informe impreso
  sale sin logotipo.
- **Tema** (*Tema → Claro / Oscuro*): paletas de Fusion en lugar de las hojas
  qss. Al cambiar, se repulen los widgets que llevan hoja de estilo propia (la
  pestaña de Mandos), que si no se quedaban con la paleta anterior.
- `EstacionTierra3.pro`: `Principal.depends = usbdeviceswatcher` en lugar del
  obsoleto `CONFIG += ordered`.
- Compilado desde cero en una carpeta de build nueva y pasan las 73 pruebas;
  visto con la app en los dos temas.

## 13. Fallos conocidos y el log de cada trama

El punto 2 de la fase de mejora, más lo aprovechable del PR #1 (que ya no se
podía fusionar: tocaba `LibMapaStatic/` y `estilos.qrc`, borrados en §12).

- **Reproducción de vuelos** (Control de objetivo → `recibirDatos`): leía los
  campos de la fila sin mirar cuántos traía (una fila corta tumbaba la
  estación), giraba el avión del mapa con la **velocidad** (campo 6) en lugar
  del **curso** (campo 8), pasaba el curso deseado de radianes a grados aunque
  se grabó en grados. Ahora la fila la lee `Protocolo/FilaVuelo.h` (con su prueba) y la
  reproducción se ve como en vivo: instrumentos, panel, detalle técnico y el
  avión o el cuadróptero del color del régimen. Una fila incompleta se ignora
  y se anota en el log.
- `listaParametros` (24 nombres para 25 valores) no lo usaba nadie: fuera.
- **Rutas**: `conf.ini` y `Recursos/data` se buscaban siempre respecto a la
  carpeta de trabajo; lanzada de otra forma, la estación creaba en silencio
  una base de datos vacía en otro sitio. Ahora `Principal/Rutas.h` elige una
  carpeta base una vez (la de trabajo si tiene los datos, si no la del
  ejecutable, si no la de trabajo como antes), todo pasa por ahí y el log dice
  al arrancar cuál usa. Donde ya funcionaba, sigue igual.
- **`usbdeviceswatcher`** se borra: en Linux no arrancaba su bucle, pero es que
  la estación no lo usaba en ningún sistema (se enlazaba y nada lo llamaba; el
  enchufar y desenchufar puertos lo lleva `DeviceHotplug`). Fuera también
  `libudev`.
- **Log**: cada trama escribía 7 líneas en la ventana del log (70 por segundo,
  sin límite: en un vuelo largo, cientos de miles de líneas y memoria). Ahora
  solo los sucesos (casa, posición válida o fuera del área al cambiar, enlace
  perdido o recuperado, acuses que no llegan...) y la ventana guarda las
  últimas 5000 líneas.
- Del PR #1: los combos de Control de objetivo usaban
  `currentIndexChanged(QString)`, que en Qt 6 no existe: no se llamaban nunca
  (no se podía elegir dron ni vuelo). Pasan a `currentTextChanged`.
- **Pestañas**: al acortar sus nombres (§9) dejaron de cumplirse las
  comparaciones con el texto de la pestaña que cargan datos al abrirla:
  Control de objetivo no llenaba los combos de dron y vuelo (lo vio el
  usuario), Enlace no refrescaba la lista de drones, Drones no refrescaba la
  tabla y Ganancias no se activaba con el enlace. Ahora cada pestaña se
  reconoce por el widget que contiene. Probado con la app: vuelo guardado con
  el simulador, combos llenos y reproducción.
- **Lo que se graba** (casilla «Guardar»): el cabeceo se guardaba en la
  columna del banqueo y al revés, la posición con 4 decimales (unos 11 m) y,
  con cuadróptero, los datos de la última trama de ala fija. Ahora cada
  manejador de trama le da a `guardaDatosDron` lo suyo. Los vuelos grabados
  antes de esto tienen el cabeceo y el banqueo cambiados.
- **Gráficas de un vuelo grabado**: pedían columnas que no existen
  («Cabeceo», «Altura»...), así que todo salía a cero (la línea plana a 500
  que vio el usuario: cabeceo y banqueo a 0 en la escala de 0 a 1000). Y aun
  con datos, todas las series compartían una escala fija (la altura de 0 a
  5000 m): 100 m de diferencia no se veían. Ahora cada serie va en su franja
  (altura, velocidad, motor, cabeceo y banqueo, de arriba abajo) estirada entre
  su mínimo y su máximo, con el rango real en la leyenda («Altura 100–162 m»),
  que va debajo de la gráfica y en blanco. «RPM», que el dron no manda, pasa a
  ser el control del motor. La gráfica en vivo usa lo mismo, y la del
  cuadróptero ya grafica sus datos y no los de ala fija.
- **Combo de vuelos**: un vuelo recién grabado no salía hasta reiniciar.
  Solo se refrescaba al abrir la pestaña, y nunca si se había empezado una
  reproducción y se había dejado en pausa. Ahora se refresca también al
  desconectar y al dejar de grabar, elige el vuelo más reciente y, durante una
  reproducción, se queda con el que se ve. Las respuestas que llegan tarde (de
  un dron o vuelo que ya no es el elegido) se descartan: el combo podía salir
  con vuelos repetidos.
- Probado con la app: vuelo grabado con alturas de 100 y 250 m, un segundo
  vuelo grabado con el primero en pausa (aparece sin reiniciar y no se pierde
  el que se veía) y la gráfica con la altura subiendo y bajando.
- Pruebas: `test_tramas` lee una fila grabada y una incompleta. 75 en total.

## 14. Grabación de vuelos nueva: sin escalones y sin perder el principio

La traza de un vuelo reproducido salía en escalones. Venía de redondear la
posición a 4 decimales (unos 11 m en latitud y 10 m en longitud) en dos
sitios: al guardar (como texto) y al reproducir, porque Control de objetivo
vuelve a escribir cada fila con `QByteArray::number(Lat, 'f', 4)`. Además, la
grabación tenía otros defectos:

- el número de vuelo se pedía y se apuntaba en `DronTable` de forma
  asíncrona: las primeras tramas tras marcar «Guardar» iban al vuelo
  anterior;
- cada trama lanzaba un `INSERT` asíncrono con la consulta compartida
  `_aQuery`, preparada y con los valores pasados a texto cada vez;
- `DatosVuelos` no tenía índice, y todas las consultas de Control de objetivo
  buscan por dron y vuelo.

Por eso se sustituye por **`Principal/Vuelos/GrabadorVuelo`** (solo Qt Core +
Sql, con su propia conexión SQLite al mismo fichero):

- `empezar(dron)` da de alta el vuelo en el momento (número del dron + 1,
  guardado en `DronTable.Vuelos`), antes de aceptar ninguna muestra; si el
  dron no existe, «Guardar» se desmarca y la barra de estado dice por qué;
- `anotar()` solo guarda la muestra en memoria (a 10 Hz no se toca el disco);
- cada segundo, y al terminar, las pendientes se escriben de una vez en una
  transacción con una consulta preparada, con los números tal cual (REAL, sin
  pasar por texto), y la fecha con milisegundos;
- crea `DatosVuelos` (las mismas columnas de siempre) y el índice
  `DatosVuelos_dron_vuelo` si no existen.

La reproducción pasa la posición con 7 decimales. La precisión que queda es la
del protocolo, que manda latitud y longitud como `float` de 32 bits (unos
0,2 m en latitud y 0,8 m en longitud a esta latitud): no se ve en el mapa.

- `test_grabador` (4 casos, base de datos temporal): vuelo nuevo al empezar,
  muestras sin redondear y cada una en su columna, escritura solo al volcar y
  al terminar, dron que no existe. 79 pruebas en total.
- Probado con la app contra el simulador: 628 filas en 63 s (10 por segundo),
  la altura de 100 a 159 m, el índice creado y la reproducción del vuelo nuevo.

## 15. Reproducir un vuelo como fue: ruta, cumplidos, rumbos y tiempo real

Lo pidió el usuario: que la reproducción muestre la ruta del vuelo y las
flechas del movimiento, lo más real posible, y que se borre el módulo
`Database` si ya no se usa.

**Qué se graba ahora** (`GrabadorVuelo`):

- en cada muestra, el **punto al que va el dron** (`Cumplir_Punto`: columnas
  `Punto_Norte`, `Punto_Este`, `Punto_Altura`; en una base de datos vieja se
  añaden con `ALTER TABLE`);
- cada **plan que confirma el dron**, punto a punto y con la hora, en la
  tabla nueva `PlanesVuelo`: el que ya tenía al empezar a grabar (el último
  confirmado en la sesión) y cada uno nuevo durante el vuelo.

**`Vuelos/ReproductorVuelo`** (solo Qt Core + Sql, con pruebas):

- el tiempo es el de la grabación (la fecha de cada muestra), por la
  velocidad elegida (×½ a ×32); antes, una fila cada 200 ms fuera cual fuera
  el tiempo real;
- **interpola** entre muestras (posición, altura, velocidad, actitud; los
  rumbos por el lado corto) a 25 fotogramas por segundo: el dron se mueve
  suave; si hay un corte de más de 2 s, no se inventa el tramo;
- con cada fotograma va el plan que tenía el dron en ese momento.

**En el mapa**, cada fotograma se pinta como en vivo
(`CControladora::mostrarMuestraReproducida`): la ruta del plan grabado, los
puntos que va cumpliendo en verde, el vector de rumbo y el deseado, la línea
morada al punto al que iba con su rótulo, el panel («P3 / 10», distancia,
rumbos) con el detalle técnico, los instrumentos y el avión o cuadróptero del
color del régimen con su traza. Al acabar se queda en el último momento con
la traza entera; Stop limpia y vuelve la ruta de planificación.

**Control de objetivo** se rehace sobre el reproductor: el título del grupo
dice «Reproducir 01:23 / 03:57», el deslizador de velocidad va de ×½ a ×32
(×1 = tiempo real) y el de avanzar salta a cualquier momento. Graficar usa las
muestras ya leídas.

**Fuera `Database/`** (AsyncQuery, AsyncQueryResult, AsyncQueryModel y
ConnectionManager, unas 1100 líneas): lanzaban cada consulta en otro hilo y
devolvían el resultado más tarde, lo que obligaba a encadenar callbacks y
dejaba carreras. Lo sustituye `Vuelos/BaseDatos`, una conexión directa a
`DronDB.db` (consultas pequeñas, en milisegundos). `Widget_Drones` usa un
`QSqlQueryModel`. También se borra `Protocolo/FilaVuelo.h`: la reproducción
ya no pasa por una línea de texto.

- `test_grabador` (10 casos): además de la grabación, lo grabado se lee igual
  al reproducir (con su plan), la interpolación, un plan que cambia a mitad
  del vuelo, un vuelo de una base de datos vieja (sin ruta) y la reproducción
  a ×16 en el tiempo que toca. 82 pruebas en total.
- Probado con la app contra el simulador: ruta de 10 puntos creada en el
  planificador y enviada antes de grabar, vuelo en automático grabado (2376
  muestras, 2348 con punto a cumplir, el plan guardado al empezar) y
  reproducido a ×32 con la ruta, la línea al próximo punto y «P3 / 10».

Visto al probar (sin cambiar, pendiente de confirmar): el planificador
muestra la velocidad de cada punto en m/s, pero al enviar el plan se divide
por 3,6 como si fuera km/h (16,25 → 4,5 m/s). Apuntado en `PENDIENTES.md`.

## 16. Grabar sin frenar la pantalla

Aviso del usuario: grabando un vuelo (al menos en debug) el dron «se detiene y
luego acelera». Cada segundo, `GrabadorVuelo` escribía las muestras
pendientes en el hilo de la interfaz, y cerrar la transacción espera al disco.
Aquí (Linux, en un contenedor) son 1–2 ms, pero en Windows cada `COMMIT` hace
varias esperas al disco (diario y base de datos) que, en debug o con el
antivirus mirando el fichero, llegan a decenas o cientos de ms. En ese rato
no se pinta nada, las tramas se acumulan y luego salen todas de golpe.

- La escritura pasa a un **hilo propio** (`GrabadorVuelo`): `volcar()` solo
  entrega el lote y vuelve; el hilo lo escribe en una transacción, en orden
  con los planes. `terminar()` espera a que todo esté en disco (la lista de
  vuelos lo lee justo después), y `esperar()` lo hace a petición.
- Esa conexión usa **WAL con `synchronous=NORMAL`**: una espera al disco por
  transacción en lugar de varias, y sigue a salvo si se cae la estación (solo
  se perdería lo del último segundo si se va la luz). El fichero de la base de
  datos pasa a tener al lado `DronDB.db-wal` y `DronDB.db-shm`, que son suyos.
- `test_grabador::volcarNoEsperaAlDisco`: con la base de datos bloqueada por
  otra conexión (como un disco lento), `volcar()` vuelve en menos de 100 ms y,
  al soltarse el bloqueo, se escribe todo. Antes esperaba hasta 5 s en el
  hilo de la interfaz. 83 pruebas en total.
- Visto con la app grabando: todas las esperas al disco (`fsync`) las hace el
  hilo «GrabadorVuelo» y el de la interfaz lee la telemetría trama a trama.

## 17. Fuera el código muerto

Punto 5 de la fase de mejora. Se buscó lo que no se compila y lo que, aun
compilándose, no usa nadie:

- **Sin compilar**: `Indicador/` (instrumentos antiguos con sus 61 imágenes; el
  que se usa es `qfi/`), `cSerialPort/` (comentado en el `.pro`; el enlace lo
  lleva `SerialManager`) y, en `graficar/`, `qtableviewprinter`,
  `qcheckboxcombo` y `myqbuttongroup`.
- **Compilado sin uso**:
  - de `qfi/` solo se usa el EADI (el horizonte artificial con velocidad,
    altura y rumbo); fuera los otros siete instrumentos (EHSI, AI, HI, VSI,
    ASI, ALT y TC) y sus imágenes, que iban metidas en el ejecutable;
  - `Estructuras/Utils.h` (una suma de comprobación que ya está en
    `Protocolo/Tramas` y que nadie incluía);
  - tres copias de `calculateChecksum` (Conexión, Ganancias y Mandos) que
    nadie llamaba, y `Widget_Graficas::Map`, que dejó de usarse en §13.
- `Reporte/` se queda: lo usa el botón «Imprimir» de los datos de un vuelo.

El ejecutable pasa de 5,8 a 5,5 MB. Compilado desde cero y pasan las 83
pruebas; el EADI se ve igual con datos en vivo.

## 18. Enviar mandos ya no congela la pantalla

Al pulsar «Enviar mandos», la pantalla se quedaba 1 o 2 s sin pintar la
telemetría. La trama sale en un instante; lo que tardaba era guardar en
`conf.ini` los valores de la pestaña, **uno a uno y en el hilo de la
interfaz**: 4 con Mandos, 13 con Ganancias y 6 al conectar. Cada
`QSettings` lee el fichero entero y, al escribir, crea un fichero de
bloqueo y uno temporal y lo renombra. En Windows, con el antivirus
revisando cada fichero nuevo, eso son decenas o cientos de ms por valor, y
mientras tanto no se leía el enlace ni se pintaba nada.

`MyQSetting` gana `guardarSettings(fichero, grupo, valores)`:

- **Todo de una vez**: un solo `QSettings` y una sola escritura por grupo.
- **En otro hilo**: un `QThreadPool` de un solo hilo, así que las
  escrituras van en orden. `cargarSetting` espera a que acabe lo pendiente
  para no leer un valor viejo, y al cerrar el programa se espera a que
  termine.
- **Solo si algo cambió**: si se mandan los mismos valores que la última
  vez, no se toca el disco.

`guardarSetting` (un valor) sigue existiendo y pasa por lo mismo. Lo usan
Mandos, Ganancias y la conexión (`[ENLACE]`); las claves y los valores de
`conf.ini` no cambian.

De paso, Ganancias mandaba "xyz" seguido de los 8 bytes **del puntero** a
la estructura, y sin suma: el autopiloto no podía aceptar esa trama. Ahora
sale de `Tramas::mandos`, igual que la de la pestaña Mandos. El protocolo
es el mismo; lo que se corrige es que antes no se cumplía.

## 19. Ventana maximizada de verdad y tiempo de conexión

**Maximizada.** La ventana ya se abría con `showMaximized()`, pero en el PC
no salía maximizada. Medido: la ventana pedía **799 px de alto como
mínimo**, porque la pestaña más alta de la izquierda (Mandos, Ganancias…)
iba debajo del horizonte artificial, que mide 300 px fijos. En una pantalla
de 768 px, o en una de 1080 con la escala de Windows al 125–150 %, no cabe;
Windows no puede maximizar una ventana más alta que la pantalla y la deja
como está. Ahora cada pestaña va dentro de un `QScrollArea` (con barra
vertical si no cabe y sin horizontal, porque el ancho es fijo) y el mínimo
baja a 515 px. Las pestañas se siguen reconociendo por el widget que
contienen (`isAncestorOf`), así que el desplazable no las confunde.

**Tiempo de conexión.** En la barra de estado, junto al indicador del
enlace, se muestra «Conectado 00:12:34» mientras el enlace está abierto y,
al cerrarlo, «Última conexión 00:12:34». Además se anota en la ventana del
log y en `log.txt`: «Enlace cerrado (tcp 127.0.0.1:5760) tras 00:12:34
conectado». Lo calcula `MonitorEnlace` (`msConectado`, `duracionUltima`,
`textoTiempo`), que recibe la hora como parámetro, así que `test_estado`
lo prueba sin esperar (caso `tiempoDeConexion`).

## 20. La velocidad del plan llega en m/s

Respondida la pregunta de la unidad (estaba en `PENDIENTES.md`): **la
velocidad va siempre en m/s** y solo se pasa a km/h para mostrarla. Si la
ruta dice 16,5 m/s, eso es lo que tiene que volar el dron, y en pantalla
se ve 59,4 km/h.

`CControladora::onEnviaPlan` dividía la velocidad de cada punto por 3,6
antes de mandarla, como si el planificador estuviera en km/h: con 16,5 m/s
en la ruta, el dron recibía 4,6 m/s, y el simulador volaba a eso. Ahora se
manda tal cual. El resto ya era coherente:

- la telemetría (`Realimentacion_Velocidad`) llega en m/s, y el horizonte,
  el rótulo del avión en el mapa y la grabación la multiplican por 3,6;
- la velocidad de vuelta a casa (V.RTH) se escribe en km/h en la pestaña
  Mandos y se manda dividida por 3,6, es decir, en m/s;
- el simulador usa la velocidad del plan en m/s, sin convertir.

## 21. Horas en «Repetir misión» sin que se vuelva lenta

Pregunta del usuario: ¿cuánto aguanta la estación recibiendo y pintando una
ruta en «Repetir misión»? Para medirlo, el simulador en automático y
«Repetir» a 100 tramas/s (diez veces el dron real) y la estación 10 minutos
contra él: unas 42.000 tramas, como unos 70 minutos de vuelo de verdad. La
memoria pasó de 78 a 85 MB, unos 6 MB por hora de vuelo. Eso no tumba nada,
pero había dos cosas que crecían sin tope y que cada trama recorría entera:

- **La estela del dron en el mapa** era entera (`setTargetTrailLength(-1)`)
  y se redibujaba completa en cada repintado: 36.000 puntos por hora,
  860.000 en un día. Ahora es de **10 minutos** (6000 posiciones a 10
  tramas/s). La ruta completa la sigue guardando libmapa aparte
  (`targetHistory`, con 5000 puntos como mucho y aclarando los
  intermedios), y los puntos cumplidos siguen marcados en el plan.
- **Las gráficas en vivo** guardaban todo desde la conexión y, abiertas,
  lo redibujaban con cada trama. Ahora guardan la **última media hora**; se
  recorta de minuto en minuto, no en cada trama. Las gráficas de un vuelo
  grabado no cambian: entran por otro camino.

Lo demás ya estaba acotado: el log se queda en sus últimas 5000 líneas, los
puntos cumplidos se marcan una vez por punto (no una vez por vuelta) y la
grabación va a disco desde su propio hilo.

Repetida la misma medida de 10 minutos: la memoria va de 79 a 83 MB en los
primeros 6 minutos, mientras se llenan la estela y el historial, y se queda
en 83 MB los 4 últimos.

## 22. Panel de capas como en la demo, y el panel de rutas abajo

**Capas.** Hasta ahora el mapa solo enseñaba las dos capas base, con dos
acciones fijas en el menú (Satelital y OSM), y quitaba el mapa híbrido y las
capas fijas del paquete (§3). Ahora todo sale del `mapa.json`:

- **Menú Capas**: una acción por capa base del paquete (se mantienen Ctrl+S
  para la satelital y Ctrl+O para OSM; las demás, Ctrl+1, Ctrl+2…) y
  «Panel de capas» (Ctrl+L).
- **Panel de capas** (`Mapa/PanelCapas`), a la derecha y como el árbol de la
  demo de libmapa: «Mapa base» (se marca una, como un radio; si es vectorial,
  sus grupos cuelgan de ella) y «Superpuestas» (el híbrido con sus grupos
  —carreteras, nombres de calles, lugares, POI…— y las capas fijas). Las
  capas de la estación (ruta, misión, dibujos) y la de entidades que libmapa
  crea por defecto no salen: no son del paquete.
- El híbrido ya no se descarga: queda cargado y **oculto** al arrancar, igual
  que las capas fijas, y se enciende desde el panel.
- Lo que se deja puesto se guarda en `conf.ini`, en `[CAPAS]`: la base, el
  híbrido, sus grupos apagados y las capas fijas visibles. Las listas van
  unidas con «;», para distinguir «ninguno» de «nunca guardado». Si el panel
  estaba abierto, también se recuerda, en `[VENTANAS] capas`.
- El panel no habla con libmapa: `MapaMision` gana lo necesario
  (`capasBase`, `gruposHibrido`, `capasFijas`, `estadoCapas` /
  `restaurarCapas`… y la señal `capasCambiadas`).

**Rutas.** La ventana de rutas era flotante, sin marco, y tapaba la esquina
del mapa (había que recolocarla a mano cada vez que se movía o redimensionaba
la ventana). Ahora es un **panel abajo**, a todo lo ancho, en un
`QDockWidget`. Se muestra u oculta con *Rutas → Panel de rutas* (Ctrl+R) o
con su botón de cerrar, y `conf.ini` (`[VENTANAS] rutas`) recuerda si se
dejó abierto. El diseño va en tres columnas:

- **izquierda**: qué ruta (combo), el dron y la gestión (Nueva, Guardar,
  Restablecer y Eliminar);
- **centro**: la tabla, con columnas a su medida, filas alternas y el estado
  como un solo icono (antes salían dos: «✓✓»);
- **derecha**: el resumen en grande (distancia, tiempo y autonomía que queda,
  con barra y estado) y las acciones: «Ver en el mapa» (verde cuando está
  dibujada), Perfil, Análisis, Guía y un **«Enviar al dron»** grande.

Cambios de funcionamiento:

- **«Enviar al dron»** solo se puede pulsar con la ruta de 10 puntos lista y
  con enlace; sin enlace dice «Enviar al dron (sin enlace)». Antes se pulsaba
  y salía un aviso.
- **«Ver en el mapa» y «Análisis»** salían desactivados al arrancar aunque
  hubiera una ruta válida: el constructor los apagaba al final. Ahora quedan
  según la ruta cargada.
- **Doble clic en un punto de la tabla** centra el mapa en él
  (`MapaMision::centrarEn`).

Los paneles cambian el tamaño del mapa sin cambiar el de la ventana, así que
el panel de vuelo y las gráficas se recolocan cuando cambia el mapa, no solo
cuando cambia la ventana. `test_mapamision` suma tres casos: las capas del
paquete, el panel de capas y guardar y restaurar el estado.

## 23. Repaso de diseño: panel de rutas con perfil, mapa y pestaña Enlace

El usuario pidió una opinión sincera sobre cómo se ve la estación, con sus
capturas a pantalla completa (1920×1080). Lo que fallaba:

- **Panel de rutas**: la tabla se estiraba a 1400 px y los números quedaban
  a un palmo unos de otros. Había mucho hueco y nada útil en él. Las cifras
  traían decimales que no dicen nada («300.00», «16.25») y el estado salía
  dos veces.
- **Mapa**: al mostrar la ruta, aparecía diminuta en mitad de La Habana y
  había que buscarla. A esa escala, los rótulos P1…P10 se montaban unos
  encima de otros y no se leía ninguno.
- **Pestaña Enlace**: el dron se elegía abajo del todo, después de conectar
  (al revés de como se usa), y entre el puerto y «Conectar» quedaba un hueco
  enorme.
- Detalles: el interruptor «Quitar del mapa», relleno de verde, gritaba más
  que la acción principal («Enviar al dron»). «Válida (con advertencias)» no
  decía cuáles.

Lo que cambia:

- **Perfil de altitud dentro del panel**, en el hueco que dejaba la tabla.
  Muestra la ruta en el naranja del mapa, con P1…P10, sobre el terreno
  relleno, y marca en rojo donde el terreno queda a menos de 10 m. Sigue el
  tema claro u oscuro, y un clic en el perfil selecciona ese punto en la
  tabla. «Perfil» pasa a ser «Ampliar perfil», que abre otro igual en una
  ventana grande. De paso: la leyenda va en su fila (dentro tapaba la ruta),
  un clic elige el punto más cercano (antes la cuenta estaba mal) y dos
  puntos en el mismo sitio ya no dividen por cero.
- **Tabla al ancho justo**: columnas a su medida, cabeceras cortas
  («Dist. (m)», «Alt. (m)», «Vel. (m/s)») con el nombre largo en el tooltip,
  números a la derecha, metros sin decimales y la velocidad con uno.
  **Editar una celda ya trae su valor**: no había `EditRole` y el editor
  salía vacío.
- **Estado como pastilla** de color (verde, ámbar o rojo); el tooltip dice
  qué hay que mirar.
- **«Ver en el mapa» encuadra la ruta** (`MapaMision::encuadrarRuta`, con un
  margen alrededor). Si dos puntos quedan a menos de 34 px en pantalla, solo
  se rotula P1 (`refrescarRotulosRuta`, al cambiar el zoom o mover un
  punto); al acercarse vuelven todos.
- **El interruptor «Ver en el mapa»**, encendido, va con borde y texto
  verdes, sin relleno.
- **Pestaña Enlace en el orden de uso**: Dron, Enlace (tipo, host, puerto),
  Conectar (más alto, y en verde mientras hay conexión) y «Grabar el vuelo»
  (antes «Guardar»), todo arriba y sin el hueco.

El rectángulo oscuro arriba a la izquierda del mapa, en la captura, es una
zona sin teselas en el paquete; no viene de la estación.

## 24. Abrir un panel ya no desplaza el mapa, y la tabla no crece con los errores

- **El mapa no se corre.** Al abrir el panel de rutas (o el de capas), el
  mapa encogía por abajo, pero libmapa conserva el centro: todo lo que se veía
  subía medio panel. Ahora `MapaMision` vigila el redimensionado del mapa y,
  si la ventana no ha cambiado (lo ha encogido o ampliado un panel, o el
  operador arrastrando su borde), deja fija la esquina de arriba a la
  izquierda: se recorta o se amplía por abajo y por la derecha. Al maximizar
  o redimensionar la ventana se sigue conservando el centro. La cuenta va en
  coordenadas de eje de libmapa (longitud y Mercator), que son lineales en
  pantalla. Prueba: `panelNoDesplazaElMapa`.
- **La tabla no se ensancha con filas no válidas.** Las filas con errores o
  advertencias se pintaban en negrita, con un `QFont()` que además cambiaba
  la letra de la tabla por la de la aplicación. Las columnas crecían y la
  tabla le comía sitio al perfil. Fuera esa negrita: ya marcan el error el
  fondo rojo y el icono.

## 25. Sin saltos: el mapa al ocultar el panel, «Enviar mandos» a la vista y la barra de estado quieta

- **El salto al ocultar el panel de rutas.** Grabada la pantalla a 60 fps
  (ffmpeg contra la pantalla virtual) y medida la posición de la cuadrícula
  fotograma a fotograma: al ocultar el panel salía **un fotograma con el mapa
  viejo estirado** al tamaño nuevo (líneas cada 104 px en vez de 64) y en el
  siguiente volvía a su sitio. Ahora `MapaMision` no deja pintar el mapa
  mientras se redimensiona: lo vuelve a permitir dos vueltas del bucle de
  eventos después, cuando libmapa ya ha repintado con el tamaño nuevo.
  Medido igual al ocultar y al mostrar el panel: ningún fotograma
  intermedio.
- **«Enviar mandos» siempre a la vista.** Con el panel de rutas abierto, a la
  columna de la izquierda le queda poco alto y el botón quedaba abajo del
  desplazamiento. Ahora el botón de enviar de cada pestaña (Mandos,
  Ganancias) va fijo debajo, fuera de lo que se desplaza. Al sacarlo de su
  widget sigue su estado: desactivado sin enlace y oculto cuando la pestaña
  muestra los mandos del cuadróptero.
- **La barra de estado no baila.** El indicador del enlace (cambia cuatro
  veces por segundo), el tiempo de conexión y la posición del ratón tenían el
  ancho de su texto: con cada cambio de cifras se recolocaba toda la barra.
  Ahora cada uno tiene un ancho fijo, el de su texto más largo con la letra
  de la pantalla.

## 26. Los paneles flotan encima del mapa, como en QGroundControl

Al usuario seguía sin gustarle abrir y cerrar el panel de rutas, y preguntó
lo justo: ¿por qué hay que repintar si solo cambia el tamaño? ¿Y si el
panel no afectara al mapa? Mirado cómo lo hacen otras estaciones:

- **QGroundControl**: el mapa ocupa toda la ventana. El editor de misión
  (plegable, a la derecha) y el perfil de terreno o las estadísticas (abajo)
  **flotan encima del mapa**, que nunca cambia de tamaño.
- **Mission Planner**: reparte la pantalla en zonas (HUD, pestañas, mapa).
  Cambiar una de tamaño cambia el mapa, igual que nos pasaba con los
  `QDockWidget`.

Los dos arreglos anteriores (§24 fijaba la esquina del mapa y §25 no lo
dejaba pintar mientras se redimensionaba) trataban el síntoma: el mapa
cambiaba de tamaño y libmapa tenía que rehacer la vista, pedir teselas y
repintar. Ahora **el mapa no cambia de tamaño**:

- `Widgets/PanelFlotante`: un panel hijo del mapa, con título, cruz de
  cerrar y el fondo del tema casi opaco, con las esquinas redondeadas. El
  ratón y la rueda sobre él son suyos (no arrastran ni acercan el mapa de
  debajo).
- El **panel de rutas** flota abajo, a todo lo ancho del mapa, y el de
  **capas** a la derecha, encima del de rutas. Se abren y cierran igual que
  antes (Ctrl+R, Ctrl+L o su cruz) y `conf.ini` lo recuerda. Al cerrarlos,
  el mapa solo vuelve a pintar el trozo que tapaban, desde la imagen que ya
  tiene.
- Medido a 60 fps al cerrar el panel de rutas: en los 167 fotogramas, la
  parte del mapa que no tapa el panel no cambia ni un píxel.
- Fuera los dos arreglos de §24 y §25 en `MapaMision` (el filtro de
  redimensionado), que ya no hacen falta.
- Lo que tapan los paneles se le dice al mapa (`ponerZonaTapada`), para que
  «Ver en el mapa» encuadre la ruta en la parte visible y el doble clic en un
  punto lo lleve al centro de lo que se ve, no debajo del panel. La prueba
  `zonaTapadaPorLosPaneles` sustituye a la de §24.
- El panel de vuelo se centra en lo que deja libre el panel de capas. En un
  mapa estrecho, el perfil del panel de rutas se oculta si le quedan menos de
  220 px, en vez de quedar aplastado.
- Con el panel de rutas abierto, la barra de escala del mapa (abajo a la
  izquierda) queda tapada: libmapa no deja moverla.

## 27. La telemetría sale de CControladora

Primer paso del punto 4 de la fase de mejora (partir las clases grandes).
`CControladora` tenía tres copias casi iguales del mismo trabajo: la trama
del ala fija, la del cuadróptero y la muestra de un vuelo grabado. Cada una
leía campo a campo, rellenaba un vector de 25 posiciones con índices sueltos
(`m_ParametrosValues[19]` era el primer actuador...) y repartía los datos por
el horizonte, el panel de vuelo, el mapa, las gráficas y la grabación. Un
cambio había que hacerlo tres veces, y ya se habían colado diferencias.

Ahora:

- **`Protocolo/Telemetria`** (solo Qt Core y Positioning) convierte la trama
  en una `MuestraVuelo`, lo que se sabe del dron en un instante: la misma
  estructura que ya se grababa y se reproducía. `desdeAlaFija` y
  `desdeCuadroptero` llevan los mismos arreglos que antes: velocidad a km/h,
  curso deseado en 0..360 y, en el cuadróptero, la posición en metros
  Mercator con el desplazamiento fijo de siempre y los cuatro motores como
  actuadores. Prueba nueva: `test_telemetria` (7 casos, campo a campo).
- **`VistaTelemetria`** pinta una `MuestraVuelo`: horizonte, panel de vuelo
  con su detalle (sin ganancias en el cuadróptero), el dron en el mapa del
  color del régimen con sus rumbos, y las gráficas en vivo.
- **`CControladora`** se queda con lo que no es pintar, en
  `procesarTelemetria`, común a los dos tipos de dron: validar la posición,
  fijar la casa con la primera válida, marcar los puntos cumplidos, la
  pestaña de Mandos y grabar. Grabar es ahora `m_grabador->anotar(m)`: la
  misma muestra que se pinta, sin copiarla otra vez campo a campo.
- **El vuelo reproducido** usa la misma `VistaTelemetria`: lo que se ve en vivo
  y reproducido sale del mismo código.
- `MuestraVuelo` y `PuntoPlan` pasan a `Vuelos/MuestraVuelo.h`, para que la
  telemetría no dependa del grabador (y de QtSql).
- Fuera de `CControladora`: `updateParameters`, `actualizarPanel`,
  `guardaDatosDron`, `loadInitialData`, el vector `m_ParametrosValues`, su
  `m_detalle` y `rumboProxPunto`, que nadie leía. `CControladora.cpp` pasa de
  unas 1.870 líneas a 1.490.

Se conserva tal cual una rareza: en el cuadróptero en vivo, las líneas de
rumbo actual y deseado se pasan al mapa cambiadas, como hacía la librería
antigua (va a las preguntas para quien programa el autopiloto). Cambios que
sí se ven:

- al desconectar, el detalle técnico sale con guiones en vez de ceros;
- el punto al que va el dron, en el panel, es el de la trama que acaba de
  llegar y no el de la anterior;
- el mensaje de la casa en el log es uno solo para los dos tipos de dron.

Visto con la app contra el simulador (ala fija, automático y «Repetir»):

- el horizonte, el panel con su detalle, el dron y sus rumbos en vivo;
- el vuelo grabado con «Grabar el vuelo», cada valor en su columna;
- la reproducción de ese vuelo, por el mismo camino.

El cuadróptero queda cubierto por `test_telemetria`, pero sin verlo en
pantalla: en la base de datos de prueba no hay un dron de ese tipo.

## 28. Widget_Puntos partido: planificador, modelo y panel

Segundo paso del punto 4 (partir las clases grandes). `Widget_Puntos.cpp`
tenía unas 3.900 líneas con tres clases dentro: el motor que valida y analiza
las rutas (`RoutePlanner`), el modelo de la tabla con su base de datos
(`MiModelo`) y el panel visible (`Widget_Puntos`). Ahora cada una tiene su
fichero, en la carpeta nueva `Principal/Planificacion/`:

- **`CaracteristicasDron.h`**: `DroneCharacteristics` y `DroneType`, que
  estaban en `dronedialog.h` junto a un diálogo. `dronedialog.h` los sigue
  dando, porque lo incluye.
- **`RoutePlanner.h/.cpp`** (unas 760 líneas): la validación y el análisis.
  Solo Qt Core y Positioning, sin interfaz.
- **`ModeloRuta.h/.cpp`** (unas 1.480 líneas): `MiModelo`, los 10 puntos y su
  guardado en SQLite (tablas `drones`, `rutas_metadata` y una por ruta).
- **`Widgets/Widget_Puntos`** se queda con el panel: unas 1.730 líneas.

Los nombres de las clases no cambian y `Widget_Puntos.h` sigue incluyendo los
otros dos, así que nadie más ha tenido que tocarse. La base de datos tampoco
cambia.

Lo que gana de verdad: **`RoutePlanner` ya tiene pruebas**. Hasta ahora las
reglas con las que se acepta o rechaza una ruta antes de mandarla al dron no
tenían ninguna. `test_planificador` (9 casos) prueba:

- la distancia entre puntos (mínima y máxima);
- la altura y la velocidad, incluida la mínima del ala fija, mientras que el
  cuadróptero puede ir parado;
- las aceleraciones y las tasas de subida, con cuentas a mano;
- el radio de giro, contra la fórmula de un giro coordinado con 30° de
  alabeo, r = v² / (g·tan 30°): 70,7 m a 20 m/s;
- una ruta de 10 puntos válida, con su distancia total y su tiempo;
- una ruta que se pasa de la autonomía.

De paso, `RoutePlanner` usaba `abs()` sin `std::` con valores con decimales.
Según el compilador eso puede acabar en `int abs(int)` y redondear: una
pendiente de 30,5° pasaría por 30° y se daría por buena. Con GCC no ocurre (la
prueba `pendienteNoSeRedondea` lo comprueba), pero ahora es `std::abs` y no
depende del compilador.

Visto con la app: el panel de rutas carga la ruta, la valida y la dibuja como
antes.

## 29. Los puntos de la ruta: un símbolo, un solo repintado y la selección se queda

El usuario vio algo raro con los puntos: el rótulo P3 suelto de su vértice,
puntos verdes que no venían a cuento y la sensación de que la ruta se repintaba
demasiado. Pidió un centro con una circunferencia alrededor, menos repintado y
poder seleccionar un solo vértice sin que afectara al resto.

Lo que pasaba:

- **Cada arrastre rehacía la ruta tres veces.** `Widget_Puntos::onPointMoved`
  cambiaba la latitud, la longitud y la altura con tres `setData`. Cada uno
  guardaba en la base, revalidaba la ruta entera y emitía `dataChanged`, y la
  estación redibujaba la ruta desde cero. Además, el primero dejaba el punto
  un momento con la latitud nueva y la longitud vieja.
- **Rehacer desde cero cambiaba los ids.** `MapaMision::pintarRuta` borraba la
  capa y creaba la línea y los puntos otra vez. La selección y el arrastre de
  libmapa se quedaban apuntando a entidades que ya no existían.
- **Los verdes eran de otro plan.** Los puntos cumplidos se volvían a marcar
  por número en cada repintado, aunque la ruta ya no fuera la que el dron
  había confirmado.

Lo hecho:

- **`MiModelo::moverPunto(fila, pos, altura)`** cambia la posición y la altura
  de una vez, con un guardado y una revalidación (un solo `dataChanged`).
  `onPointMoved` lo usa.
- **`pintarRuta` actualiza en el sitio** cuando la ruta tiene los mismos puntos
  (`actualizarRuta`). Solo se mueven los puntos que cambiaron, y el vértice de
  la línea se comprueba aparte, para que nunca quede despegado del punto. Cada
  punto conserva su id, así que la selección no se pierde, y los anillos lo
  siguen solos (van atados al id). Si cambia el número de puntos, se rehace.
- **Los cumplidos solo se marcan sobre el plan que confirmó el dron.** Si la
  ruta que se ve es otra (se ha movido un punto o se ha abierto otra ruta),
  `olvidarCumplidos` la vuelve a poner en rojo.
- **El símbolo del punto** es ahora un centro marcado y una circunferencia de
  9 px, con borde oscuro para que se vea sobre el satélite: rojo si está
  pendiente y verde si está cumplido. Se dibuja una vez (a doble resolución,
  nítido en pantallas con escala) y se reutiliza. El rótulo va junto a la
  circunferencia.

Pruebas nuevas en `test_mapamision`:

- `repintarMueveSoloElPuntoCambiado`: los ids y la selección se mantienen, la
  línea sigue al punto y P1 ni se toca.
- `cumplidosSeOlvidan`.
- `puntoConCentroYCircunferencia`: comprueba el centro, el hueco y el anillo
  en los píxeles del icono.

Son 112 pruebas en total.

## 30. La revisión de la ruta: qué pasa, por qué y cómo arreglarlo

El usuario pidió revisar el análisis y la validación de las rutas: «creo debe
quedar mejor explicado los problemas y las soluciones», y que se mirara cómo se
hace y qué se le dice al operador.

### Lo que había

Se miró `RoutePlanner`, `MiModelo` y el panel. Había más fallos de fondo que de
redacción:

- **Se paraba en el primer problema.** `validarRuta` devolvía el primer error
  que encontraba. Con tres problemas se veía uno; al arreglarlo salía otro.
- **Lo mismo era error en un sitio y aviso en otro.** Al radio de giro y al
  cambio de velocidad la tabla los marcaba como advertencia, pero bastaban para
  que el estado dijera «No válida». La tabla podía estar toda en ámbar y la
  ruta, inválida, sin saber por qué.
- **Se comprobaba algo que el dron no recibe.** La columna Radio se validaba
  contra el radio de giro teórico, pero no viaja en la trama del plan
  (`sPUNTOXYZ` no la lleva) y el acercamiento es el radio de Mandos, uno para
  todos.
- **El terreno no se comprobaba.** El perfil marcaba en rojo dónde se acercaba
  la ruta al terreno, pero la validación no lo miraba: una ruta que atravesaba
  una loma salía «Válida».
- **Cifras sin base.** Mostraba como datos de seguridad cosas inventadas:
  - un «margen de seguridad» que restaba puntos a ojo;
  - un «estrés de ruta»;
  - un «factor de carga» que no era el de un giro;
  - un consumo en Wh con potencias de 150 y 300 W sacadas de la nada;
  - una «eficiencia» que comparaba con la línea recta de P1 a P10.
  Parecían medidas y no lo eran.
- **Los giros se medían mal.** El ángulo salía de restar grados de latitud y
  longitud: a 60° de latitud, un ángulo recto daba unos 63°.
- **El arreglo, si lo había, era vago.** «Considere suavizar la trayectoria»,
  «Aumente el radio». La ventana de puntos críticos calculaba explicaciones y
  soluciones, pero no las enseñaba.
- **Repartido y repetido.** Había reglas de «punto crítico» con umbrales
  distintos en el modelo y en el planificador. El estado se calculaba en dos
  sitios. Cada cambio rehacía el análisis varias veces.

### Lo que hay ahora

**`RoutePlanner::revisar`** devuelve una `RevisionRuta`: todos los hallazgos de
una vez, ordenados (errores, avisos y notas) y cada uno con cuatro cosas:

- dónde («P4», «P7 → P8», «Ruta»);
- un título corto;
- la explicación, con los números y el límite del dron;
- **cómo arreglarlo, con el valor que hace falta**: «Baja P9 a 105 m, o separa
  los puntos hasta 813 m», «Baja la velocidad en P6 a 16.8 m/s o menos».

Las tres gravedades:

- **Error**: incumple un límite del dron o pasa por el terreno. Solo los errores
  hacen la ruta «No válida».
- **Aviso**: el dron la vuela, pero no como está planeada.
- **Nota**: algo que no se ha podido comprobar.

Lo que se revisa, y por qué así:

- **Altura y velocidad de cada punto** contra el dron (error). Por debajo de la
  mínima del ala fija se explica qué es entrar en pérdida.
- **Distancia entre puntos**, mínima y máxima (error).
- **Velocidad**: si le da para acelerar o frenar entre dos puntos, con
  v² = v₀² ± 2·a·d, que es como cambia de velocidad. Antes se usaba el tiempo a
  velocidad media. Es aviso, porque llega más despacio pero llega. El arreglo da
  la velocidad que alcanza y la distancia que haría falta.
- **Subidas y bajadas** con su tasa máxima (aviso). Si no le da, llega más bajo
  y **el tramo siguiente empieza ahí**: se simula la altura a la que va de
  verdad. Antes era error, pero un dron que no llega a la altura sigue
  volando; el peligro es el terreno, y eso tiene su propia comprobación.
- **Terreno**: se mira cada 20 m de cada tramo, con esa altura real y no con la
  del plan:
  - a menos de 10 m es error (atraviesa o roza);
  - a menos de 30 m, aviso;
  - el arreglo da la altura que libra lo más alto del tramo con margen.

  Si la zona no tiene elevación, una nota lo dice: antes se suponía terreno a
  0 m sin avisar. La cota la da `AlturaWorker::cota`, que devuelve NaN sin dato
  y no emite una señal por cada consulta.
- **Giros del ala fija**: el radio sale de la velocidad, r = v²/(g·tan 30°), y
  nunca es menor que el radio mínimo del dron. Si el punto siguiente queda
  dentro del círculo de giro (d < 2·r·sen θ), no puede ir directo: se pasa y da
  una vuelta. Es aviso. El arreglo da la velocidad con la que cabe, a qué
  distancia alejar el punto o cuánto abrir el giro. El ángulo se mide ahora con
  acimutes geodésicos.
- **Autonomía**, en distancia y en tiempo: pasarse es error; usar más del 80 %,
  aviso, porque además hay que ir hasta P1 y volver.

Lo que se quitó:

- las cifras sin base (margen, estrés, factor de carga, consumo, eficiencia);
- la validación de la columna Radio;
- las comprobaciones sueltas (`validarAltitud`, `validarRadioGiro`...);
- `advertencias` y `puntoCritico` de `E_Punto`, y `erroresPorPunto` del modelo.

La revisión vive en un solo sitio, `MiModelo::revision()`. Se rehace sola
cuando cambia algo que la afecta: compara una firma con las posiciones,
alturas, velocidades y nombres de los puntos, el dron y si ya hay mapa.

### Lo que ve el operador

- **Tabla**: el icono y un tinte translúcido de la gravedad en cada fila. Un
  fondo claro fijo dejaba la letra clara del tema oscuro sin contraste; el
  tinte se lee con los dos temas. Con el ratón encima, el detalle de ese punto:
  cada problema con su porqué y su arreglo.
- **Estado** (abajo): «No válida: 2 errores y 1 aviso». En el tooltip, cuáles
  son; al pulsarlo, se abre el análisis.
- **Análisis** (`Widgets/VentanaInforme`, con el texto de
  `Planificacion/InformeRuta`): sustituye al cuadro de texto con emojis. Trae:
  - el estado y las cifras de la ruta;
  - «Qué revisar»;
  - la tabla de tramos (distancia, tiempo, qué pide en vertical, a qué altura
    llega de verdad y cuánto pasa sobre el terreno);
  - «Cómo se revisa»: lo que se comprueba y lo que se supone, para que se sepa
    de dónde sale cada cosa.

  No es modal: se deja abierto mientras se corrige y se rehace solo. Pinchar en
  un problema o en un tramo selecciona su punto en la tabla. **Exportar** guarda
  el mismo informe en texto.
- **Enviar** con errores: dice cuáles y ofrece «Ver el análisis», «Enviar
  igualmente» o «Cancelar». Antes era un Sí/No/Cancelar en el que «No» abría el
  análisis.
- La **guía** explica las tres gravedades, dónde se ve cada cosa y la misma
  lista de «Cómo se revisa» que el informe (sale de
  `InformeRuta::comoSeRevisa`, una sola vez).

### Pruebas

`test_planificador` pasa a 13 casos, cada uno con números hechos a mano:

- una recta sin problemas;
- límites de altura y velocidad;
- que salen todos los problemas, no solo el primero;
- la separación entre puntos;
- la aceleración, con v² = v₀² ± 2·a·d;
- la subida que no llega y arrastra al tramo siguiente;
- el terreno: una loma que se choca, se roza, se libra justa o se libra, y un
  caso en que el plan la libra pero la altura real no;
- la nota cuando no hay terreno;
- el giro cerrado y la velocidad que cabe;
- el ángulo a 60° de latitud;
- el radio de giro;
- la autonomía;
- el informe.

En total pasan 117 pruebas.

Con la app, sobre una ruta con P4 bajo el mínimo, un tramo de 612 m y una
subida de 150 m en 300 m:

- el estado dice «No válida: 2 errores y 1 aviso»;
- las filas se marcan;
- el análisis explica cada cosa y su arreglo;
- un enlace selecciona su punto;
- al subir P4 a 60 m en la tabla, el informe abierto se rehace sin ese error.

Se ha mirado con los dos temas.

## 31. Altura sobre el terreno: la ruta nueva ya no atraviesa las lomas

El usuario preguntó cómo maneja una ruta nueva la altura del terreno y la altura
sobre el terreno, y mandó una captura: terreno de 470 a 660 m y la ruta plana a
500 m, metida en las lomas, con «No válida: 9 errores».

### Lo que pasaba

Cada punto tenía una sola altura, sobre el nivel del mar. Al crear la ruta se
calculaba así: terreno del punto + 50 m, recortada entre la altura mínima y la
máxima del dron, y nunca menos de 50 m. Esto tenía varios fallos:

- **Mezclaba referencias.** La altura mínima y la máxima del dron (50 y 500 m
  en el ala fija estándar) son alturas sobre el terreno, pero se aplicaban a la
  altura sobre el mar. Con terreno a 600 m, 600 + 50 = 650 se recortaba a 500,
  **100 m bajo tierra**, sin aviso. Es la captura.
- **Solo miraba el terreno en el punto**, no entre puntos.
- **Sin elevación, suponía terreno a 0 m** sin decirlo.
- **Arrastrar un punto pisaba la altura puesta a mano** (volvía a +50).
- **«Restablecer» llevaba la ruta a un punto fijo** (23° N, 82° O), con otra
  separación.

### Lo que hay ahora

- **Los límites de altura del dron cuentan sobre el terreno.** La revisión
  resta a la altura de cada punto la cota de debajo (`RevisionRuta::cotas`). Sin
  cota, cuenta desde el nivel del mar y lo dice. El arreglo da las dos cifras:
  «Sube P4 a 650 m sobre el mar o más (50 m sobre el terreno)». Hay medio metro
  de tolerancia porque las alturas se guardan en `float`: un punto puesto justo
  en el límite salía como error, con 49,99 m.
- **El panel tiene «Sobre el terreno»** (50 m por defecto; se guarda en
  `conf.ini`, `[RUTAS] sobre_terreno`). Sirve al crear la ruta con el clic
  derecho y al restablecerla.
- **La ruta nueva libra el terreno en toda su longitud, no solo en los puntos**
  (`RoutePlanner::generarRuta`). Cada punto queda por encima de lo más alto
  del terreno en los tramos que lo tocan (mirado cada 20 m). Como los dos
  extremos de cada tramo libran su punto más alto, la recta entre ellos nunca
  baja de esa altura sobre el terreno. Con la cota solo en cada punto, la
  primera prueba en la app seguía chocando entre puntos.
- **Sin elevación**, la ruta se genera desde el nivel del mar y un aviso dice
  que esas alturas no se han podido comprobar.
- **Arrastrar conserva la altura sobre el terreno** del punto
  (`MiModelo::moverPunto`): si iba a 30 m sobre una loma, sigue a 30 m sobre lo
  que haya en el sitio nuevo. Sin cota en alguno de los dos sitios, se queda la
  altura sobre el mar.
- **La tabla muestra las dos alturas.** «Alt.» es sobre el mar (la que se manda
  al dron) y la columna nueva «S/terr.», sobre el terreno, con «—» si no hay
  dato. Editar «S/terr.» cambia «Alt.». El tooltip de cada fila trae las dos, y
  el informe dice entre qué alturas sobre el terreno van los puntos.
- **«Restablecer»** rehace los 10 puntos desde el P1 que hay, igual que el clic
  derecho. Se quitaron `generarPuntosPorDefecto`, `calcularAltitudVuelo` y
  `ajustarAPuntosFijos` (este no lo usaba nadie).

Lo que se manda al dron no cambia: la altura sobre el mar, como hasta ahora.
Queda en `PENDIENTES.md` confirmar que es lo que espera el autopiloto en `z`.

### Pruebas

`test_planificador` pasa a 15 casos:

- `alturaSobreElTerreno`: la meseta a 600 m, con un punto bajo, uno bajo
  tierra (el de la captura), uno demasiado alto y uno bien;
- `generarRutaSobreElTerreno`: un pico entre P3 y P4 deja esos dos a 50 m
  sobre el pico y los demás a 50 m sobre el llano; sin elevación, a 50 m sobre
  el mar.

En total pasan 119 pruebas.

En la app, con un `.hgt` sintético de lomas de 300 m añadido al paquete de
prueba:

- la ruta vieja, plana a 50 m, sale con alturas negativas sobre el terreno y
  19 errores;
- «Restablecer» deja «Válida, con 2 avisos», pasa a 50 m del terreno como
  mínimo, y los avisos son de bajadas que el dron hace más despacio, llegando
  más alto;
- editar «S/terr.» de P1 a 120 m cambia «Alt.» de 672 a 570 m, y la revisión
  avisa de que así el tramo P1 → P2 ya no libra la cresta.

## 32. Cuando el dron no le da para subir, y la trayectoria probable

El usuario mandó la ruta «hhh» (ala fija, 16,3 m/s, puntos a 100–206 m sobre el
terreno) con «No válida: 1 error y 8 avisos» y pidió que se le explicara.

El terreno subía de unos 510 a 1.085 m en 2,7 km, una pendiente media del
21 %. El dron, a 16,3 m/s y subiendo a 3 m/s como mucho, sube al 18 %. Tramo
a tramo se quedaba por debajo del plan (17, 11… 177 m al llegar a P9), y en
P9 → P10 acababa 21 m dentro de la ladera, aunque los puntos estuvieran bien
altos.

La revisión lo detectaba, pero **el arreglo que daba no servía**: «Sube P9 y
P10 hasta 1.115 m». Subir los puntos del plan no hace que el dron suba más
rápido. El usuario pidió las dos mejoras propuestas y, además, que se dibujara
la trayectoria posible según las características del dron.

### El choque dice de dónde viene

Cuando el dron atraviesa o roza el terreno (o pasa con poco margen), la
revisión mira también la recta del plan en ese sitio:

- **Si el plan va bajo**, el arreglo sigue siendo subir los puntos.
- **Si el plan libra el terreno pero el dron no llega a su altura**, el error lo
  dice: «El plan pasa ahí a 101 m sobre el terreno, pero el dron no llega a esa
  altura: desde P1 no le da para subir lo que pide la ruta (va 98 m por debajo
  del plan). Subir los puntos no lo arregla.» Y propone lo que sí funciona:
  - **volar más despacio** desde el último punto al que llegó a la altura del
    plan: la velocidad media con la que le da, y a qué pendiente sube así;
  - **empezar la subida más alto**: ese punto, con los metros que le faltan;
  - **hacer la subida más larga**: puntos más separados, o en zigzag u otro
    rumbo en vez de ir de frente contra la ladera.

### Aviso de ladera

Un aviso de ruta, «El terreno sube más de lo que puede subir el dron», sale
cuando, en un trozo de varios puntos, el terreno sube más empinado (más de
50 m) que tasa de subida ÷ velocidad. Además el dron tiene que quedarse de
verdad por debajo del plan en algún tramo: si la ruta ya va por encima, no
hace falta seguir la ladera. En la primera prueba avisaba por eso en una ruta
correcta.

Se queda con el trozo en el que más metros pierde: la subida entera, no el
pedazo más empinado, que es lo que elegía al principio. Da las pendientes, los
metros que se quedará por debajo y los mismos tres arreglos, con la velocidad
que le basta («Vuela de P1 a P10 a 12 m/s o menos, así sube al 25 %»). Si ni a
su velocidad mínima puede, lo dice.

### El generador ya no hace rutas que el dron no puede subir

`RoutePlanner::generarRuta`, después de librar el terreno:

- baja la velocidad de los dos puntos de cada tramo que sube más de lo que le
  da su tasa, hasta la mínima del ala fija;
- si ni así le da, sube los puntos de antes (recorriendo la ruta de atrás
  adelante) para que empiece la subida más alto.

Solo sube alturas: el terreno queda igual de librado. Las bajadas que no le dan
se dejan, porque llega más alto y es seguro.

En una ladera sintética al 25 %, «Restablecer» deja la ruta a 12 m/s y
«Válida», sin nada que revisar.

### La trayectoria probable

- **En el perfil** (el del panel y «Ampliar perfil»): una línea amarilla
  discontinua con la altura a la que va de verdad, limitada por sus tasas.
  Sale de `RevisionRuta::perfilReal`, una muestra cada 20 m. Solo se dibuja si
  se aparta del plan. Las cruces rojas de «terreno a menos de 10 m» miran ahora
  esa altura, no la del plan.
- **En el mapa**: otra línea amarilla discontinua, no seleccionable
  (`MapaMision::ponerTrayectoria`). Sale de
  `RoutePlanner::trayectoriaProbable`, que simula el vuelo en planta:
  - a la velocidad de cada tramo;
  - el ala fija gira con el radio de su velocidad (alabeo de 30°); el
    cuadróptero, con su radio mínimo;
  - el rumbo va siempre hacia el punto al que se dirige;
  - pasa al siguiente al entrar en el radio de acercamiento de Mandos (el que
    usa el autopiloto, pasado a metros sobre el terreno).

  Se ve dónde se pasa de una esquina (con 20 m de acercamiento y 70 m de
  radio, unos 50 m) y dónde da vueltas sin alcanzar un punto que queda dentro
  de su círculo de giro. Se rehace al cambiar la ruta o el radio de
  acercamiento. Es una estimación: sin viento ni la ley de guiado real del
  autopiloto, y así lo dice «Cómo se revisa».

El aviso de giro cerrado decía «dará una vuelta para alcanzarlo». La
simulación muestra que, girando al máximo, puede no alcanzarlo nunca, y ahora
lo dice así.

### Pruebas

Dos casos nuevos en `test_planificador`:

- `laderaQueElDronNoSube`: la ladera al 25 % con la ruta a 100 m sobre ella.
  El choque dice que subir los puntos no sirve y da la velocidad y la altura de
  P1. El aviso de ladera da 25 % contra 19 % y 12 m/s. Con un plan bajo, el
  arreglo sigue siendo subir los puntos. La ruta generada sale sin choques ni
  subidas imposibles.
- `trayectoriaProbable`: en recta mide lo que la ruta; en un giro de 90° se
  pasa unos 50 m; con el punto dentro del círculo de giro da vueltas; y el
  perfil real acaba a la altura a la que llega.

En total pasan 121 pruebas.

En la app, con la ladera sintética:

- el perfil enseña la trayectoria probable metiéndose en la ladera mientras la
  ruta la libra;
- el mapa enseña cómo se pasa de P7 en el giro;
- el análisis da la causa real y los arreglos que sirven;
- «Restablecer» deja una ruta válida a 12 m/s.

## 33. Proponer ruta: de P1 a P10 por el terreno, y el operador elige

El usuario preguntó si, a partir de la ruta inicial en línea, con el punto
inicial y el final colocados, la estación podía poner sola el resto de puntos,
conociendo el terreno y las características del dron, para tener una ruta
óptima. Primero pidió solo la explicación (dónde está el límite: 10 puntos y
tramos entre la distancia mínima y la máxima del dron; qué es «óptima»; por
fases). Después eligió «una mezcla, pero si se puede calcula las tres y que el
operador decida».

### Cómo busca (`RoutePlanner::proponerRutas`)

- **Pasillo.** La recta de P1 a P10 se parte en 9 tramos iguales. En cada una
  de las 8 estaciones intermedias se prueban 21 desplazamientos a un lado y a
  otro, hasta el ancho del pasillo (1 km por defecto). Así la ruta puede ir por
  un valle o rodear una loma en vez de subirla.
- **Coste por tramos y por giros.** Cuentan:
  - el largo, que es tiempo;
  - lo que sube lo más alto del terreno de un tramo al siguiente, que es lo que
    tendrá que subir el dron;
  - mucho más caro, lo que pase de lo que le da su tasa de subida a su
    velocidad (ahí se quedaría por debajo, §32);
  - lo accidentado del terreno bajo el tramo;
  - los giros, y mucho más si el punto siguiente queda dentro del círculo de
    giro del ala fija;
  - pasarse de la altura máxima del dron sobre el terreno.

  Los tramos fuera de la distancia mínima o máxima no se admiten.
- **Programación dinámica (Viterbi).** Como el coste se reparte así, y un giro
  depende de tres puntos seguidos, la mejor combinación sale recorriendo las
  estaciones con parejas de desplazamientos, sin probar los 21⁸ casos. Las
  tres prioridades salen en una fracción de segundo.
- **Tres prioridades**, que solo cambian los pesos:
  - **Segura** prima las subidas suaves, no quedarse por debajo y el terreno
    tranquilo, aunque sea más larga;
  - **Rápida** prima el largo;
  - **Equilibrada** es una mezcla.
- **Alturas y velocidades** como en el generador, ahora en una función común
  (`puntosPorPosiciones`): cada punto sobre lo más alto de sus tramos, más
  despacio donde hace falta subir y más alto antes si ni así le da.
- **Cada propuesta se revisa entera** (`revisar`), con la trayectoria real.
  También la **recta**, para comparar.
- **Si es imposible**, lo dice. Por ejemplo, P10 más lejos de lo que caben 9
  tramos: «caben 4500 m; acerca P10 a P1».

### Lo que ve el operador

**Proponer ruta…** está en el panel y usa el P1 y el P10 de la ruta, que el
operador coloca antes arrastrándolos. La ventana trae:

- una fila por propuesta (Segura, Equilibrada, Rápida y Recta) con distancia,
  tiempo, altura máxima, subida total, el margen mínimo sobre el terreno a la
  altura real y el estado de la revisión;
- la elegida en el perfil (con su trayectoria probable) y en el mapa como
  vista previa: cian, a trazos y punto, en la capa de la misión para que no se
  borre al repintar la ruta;
- **Usar esta ruta**, que la pasa a la tabla, y el ancho del pasillo con
  **Calcular** para repetir.

### Pruebas

- `proponerRutas` en `test_planificador`:
  - en llano, las cuatro salen rectas;
  - con una cresta de 300 m que cruza la recta y un paso a un lado, la Segura
    va por el paso (menos de 200 m de altura frente a más de 400, válida y sin
    subidas imposibles) y la Rápida no es más larga que la Segura;
  - con P10 a 10 km lo dice.
- En total pasan 122 pruebas.

En la app, con una cresta sintética de 400 m y un paso a 400–700 m al norte:

| Ruta | Distancia | Sube en total | Estado |
|---|---|---|---|
| Recta | 2.700 m | 269 m (hasta 738 m) | 4 avisos |
| Segura | 2.890 m | 0 m (por el paso) | Válida |
| Equilibrada y Rápida | 2.858 m | 3 m | Válida |

La vista previa se curva hacia el paso y «Usar esta ruta» deja la tabla
«Válida».

Lo que queda para después está en `PENDIENTES.md`: mover las estaciones a lo
largo, afinar, A* para desvíos grandes, zonas a evitar y pesos ajustados con
vuelos.

## 34. Cuánto puede medir una ruta, dicho claro

El usuario intentó «Proponer ruta» con P10 a 5.087 m de P1. Salió «No se pudo»
y el mensaje «caben 4500 m». Preguntó si significaba que la distancia máxima
entre puntos es de 500 m, y pidió que quedara claro en la documentación.

La cuenta:

- Una ruta son siempre 10 puntos, porque lo pide la trama del plan. Entre P1
  y P10 hay, por tanto, 9 tramos.
- Cada tramo tiene que medir entre la distancia mínima y la máxima entre
  puntos del dron elegido. Ese límite no lo pone ni el protocolo ni el
  algoritmo: es el campo «Dist. máx entre waypoints» de los datos del dron, y
  en «Ala Fija Standard» vale 500 m.
- Así que la ruta mide como mucho 9 × 500 = 4.500 m. A 5.087 m, P10 no cabe
  ni en línea recta. En su ruta, el último tramo medía 1.680 m y la revisión
  lo marcaba en rojo.

Dónde queda escrito ahora:

- **Guía de la app**: una sección nueva, «Cuánto puede medir una ruta», con
  la cuenta, el ejemplo de 500 m, qué hacer (acercar P10 dejando margen para
  los desvíos, o subir la distancia máxima del dron si el dron real lo admite:
  con 600 m caben 5.400 m) y el mínimo (9 × distancia mínima).
- **Mensajes**: el de «Proponer ruta» y el de «Puntos demasiado separados»
  dicen de dónde sale el límite («su “Dist. máx entre waypoints”») y los dos
  arreglos.
- **README**: el mismo párrafo, en «Probar una misión».
- **PENDIENTES**: la pregunta para quien programa el autopiloto. ¿Es 500 m un
  límite real (alcance de la radio, guiado) o un valor por defecto?

## 35. El pasillo de «Proponer ruta»: saltos finos y dibujado en el mapa

El usuario probó «Proponer ruta» con P10 a 4.158 m de P1 y 5.000 m de pasillo.
Las cuatro propuestas salieron iguales: la recta.

**Por qué.** Los desplazamientos laterales eran saltos de una décima del
pasillo, así que con 5.000 m el más pequeño era de 500 m. Pero cada tramo en
recta ya medía 462 m y la distancia máxima entre puntos es 500 m, de modo que
un tramo solo podía apartarse √(500² − 462²) ≈ 191 m. Ningún desvío cabía y
solo quedaba la recta: ensanchar el pasillo había empeorado el resultado, al
revés de lo esperado.

**Arreglo.**

- **Saltos finos y fijos.** Se calcula primero lo que cabe de verdad. Un
  tramo entre dos estaciones con diferencia lateral D mide √(a² + D²), con
  a = largo / 9, y no puede pasar de la distancia máxima. El desvío crece como
  mucho eso por tramo desde P1 y se cierra igual hacia P10. En la estación k
  el límite es mín(pasillo, D·mín(k, 9 − k)), y en el centro, 4·D. Los saltos
  son de 50 m (o la mitad de lo que cabe por tramo si es menos), con 20 por
  lado como mucho. Las posiciones fuera del límite de cada estación ni se
  prueban.
- **La ventana dice cuánto se puede apartar**: «Puede apartarse hasta 764 m
  de la recta, en el centro…». Si se ha pedido más, añade que pedir más
  pasillo no cambia nada y que, para tener sitio, hay que acercar P10. Si no
  cabe ningún desvío, lo dice.
- **El pasillo se dibuja en el mapa** como un polígono cian translúcido, con
  el borde suave y la propuesta encima: estrecho junto a P1 y P10, ancho en el
  centro. Lo devuelve `proponerRutas` en `PasilloRuta` (contorno, desvío
  posible y largo de tramo) y lo pone `MapaMision::ponerPasillo` en la capa de
  la misión. Se quita al cerrar la ventana.
- La guía lo explica en «Cuánto puede medir una ruta».

**Pruebas.** `pasilloQueCabe` reproduce el caso del usuario: P10 a 4.158 m,
500 m de máximo y 5.000 m de pasillo.

- El desvío posible sale 4·√(500² − 462²) ≈ 764 m.
- El contorno empieza en P1 sin ancho.
- Con una loma que cruza y un paso a 300–600 m al norte, la Segura lo usa
  (menos de 300 m de altura frente a más de 500 de la recta), sin pasarse de
  500 m en ningún tramo.

En total pasan 123 pruebas. En la app, con la cresta sintética: con 1.000 m de
pasillo se ve el sombreado recortado a 1.000 m; con 5.000 m, «hasta 1.600 m…
pedir más pasillo no cambia nada», y las propuestas cruzan por el paso.

**Después.** El usuario preguntó cuál era el pasillo mínimo y pidió que se viera
en la ventana. Ahora hay una línea fija en gris bajo el campo del pasillo:

- el campo va de 100 a 5.000 m a cada lado;
- junto a P1 y P10 el pasillo mide 0 y se ensancha hacia el centro;
- si P10 está cerca de 9 × la distancia máxima, cabe poco o nada a los lados;
- para ir justo por la recta, está la fila «Recta».

El tooltip del campo dice el rango y el valor por defecto (1.000 m).
