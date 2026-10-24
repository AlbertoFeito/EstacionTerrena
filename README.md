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

En la pestaña *Enlace*, **Enlace** elige por dónde llega el mismo
protocolo de siempre (el dron no cambia nada):

- **Serie**: el puerto, a 115200 8N1, como siempre.
- **TCP**: la estación se conecta como cliente a `host:puerto` (el simulador o
  un puente de radio hacen de servidor). Por defecto `127.0.0.1:5760`.
- **UDP**: escucha en un puerto (14551) y manda a `host:puerto` (14550). Con el
  host vacío contesta a quien le mande la telemetría.

Lo elegido se guarda en `conf.ini`, sección `[ENLACE]`.

**Estado del enlace**, siempre a la vista en la barra de estado:

| Color | Qué pasa |
|---|---|
| Gris «Sin enlace» | No hay enlace abierto |
| Verde «tcp 127.0.0.1:5760 · 10 tramas/s · última hace 0,1 s» | Llegan datos |
| Ámbar | Conectado esperando la primera trama, o más de 1 s sin datos |
| Rojo «SIN DATOS hace 5 s» | Más de 3 s sin datos: pitido (y otro cada 5 s), aviso en el panel de vuelo y nota en el log |

## Panel de vuelo

Arriba en el mapa, en grande y con unidades: **régimen** (rojo manual, ámbar
estabilizado, verde automático, como el avión), **misión**, **punto** al que va
(«P3 / 10»; «✓ 10/10» con la ruta cumplida), **distancia** al punto, **rumbo**
actual → deseado y **satélites** (rojo por debajo de 6, ámbar por debajo de 8);
debajo, la posición del dron. **Detalle ▾** abre, en el mismo panel, las
ganancias (P / I), los actuadores, el rumbo de la IMU y las convergencias. Se
muestra u oculta en *Estado_Drone → Datos de vuelo* y *Detalle técnico*.

En el mapa, el rumbo actual (azul, solo en automático) y el deseado (verde)
son vectores de 150 px; la línea morada va al punto al que se dirige el dron.

## Pestañas

A la izquierda, en vertical: **Enlace** (dron, enlace y conectar), **Drones**,
**Mandos**, **Ganancias** y **Objetivo**. En *Mandos*, régimen y misión son
botones que se encienden; arriba se ve en qué está el dron de verdad («Dron:
Automático · Puntos», en ámbar si no es lo elegido; al conectar, la pestaña
toma lo que diga el dron). *Usar posición actual del dron como casa* copia la
última posición recibida a la casa, y **Enviar mandos** lo manda todo. Cuando
el dron confirma, la «H» del mapa pasa a la casa enviada.

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

### Probar una misión con el simulador

1. `simulador_dron` y, en la estación, **Conectar** (ala fija, TCP). La primera
   posición que llega fija la casa (la «H»).
2. *Rutas → Configuración de Rutas* → **Nueva**: elegir el dron y el nombre de
   la ruta; después, clic derecho en el mapa en el punto inicial (se generan
   los 10 puntos, que se pueden arrastrar o editar en la tabla) y **Enviar**. Cuando el dron confirma, el botón se pone verde y
   la barra de estado dice «✓ El dron ha confirmado el plan». Sin respuesta en
   3 s, aviso; si lo rechaza, se reenvía hasta 3 veces.
3. Con la ruta en el dron, este sigue dando vueltas a casa: solo la cumple en
   automático. *Mandos*: **Automático**, la misión y **Enviar** (verde y «✓ El dron ha
   confirmado los mandos»). El dron sigue la ruta: líneas de rumbo, puntos
   cumplidos en verde.

Lo que hace el simulador con cada mando:

| Mando | Qué hace |
|---|---|
| Régimen Manual / Estabilizado | Vueltas alrededor de casa (no hay mando de radio) |
| Automático + misión **Puntos** | Recorre la ruta una vez; al acabar, vuelve a casa y da vueltas sobre ella |
| Automático + **Circular** | Vueltas alrededor del punto al que iba |
| Automático + **Repetir misión** | Recorre la ruta una y otra vez |
| Automático + **RTL** | Vuelve a casa a V.RTH y a la altura de casa, y da vueltas sobre ella |
| RPunto | Radio en el que da un punto por cumplido (metros Mercator, como el autopiloto) |
| Casa (lat, lon, alt) y V.RTH | Destino, altura y velocidad del RTL |
| Paracaídas | Se para y baja a 5 m/s hasta el suelo |
| Ganancias (pestaña Ganancias) | Devuelve en la telemetría solo las marcadas para actualizar |

Cada punto de la ruta trae su altura y su velocidad, y el simulador las usa.
La consola del simulador cuenta cada cosa (plan recibido, punto cumplido,
cambio de fase...).

Por puerto serie sin cable hace falta un par de puertos virtuales:
[com0com](https://com0com.sourceforge.net/) en Windows (p. ej. COM5↔COM6: el
simulador en COM6 y la estación en COM5) o `socat` en Linux.

## Documentación

- `docs/BITACORA.md`: qué se ha hecho y por qué.
- `docs/PENDIENTES.md`: lo que queda (fase de mejora del código).
