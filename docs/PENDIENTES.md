# Pendientes

Lo que queda por hacer, con lo que ya se sabe para no repetir el análisis. Al
hacer una, se borra de aquí y se cuenta en `BITACORA.md`.

## Preguntas para quien programa el autopiloto

- **Unidad de la velocidad del plan**: el planificador la muestra en m/s
  (16,25 por defecto), pero `CControladora::onEnviaPlan` la divide por 3,6
  antes de mandarla, como si fuera km/h: el dron recibe 4,5 m/s (16 km/h) y
  el simulador vuela a eso. ¿En qué unidad la espera el autopiloto? Según la
  respuesta, sobra la división o sobra el «(m/s)» del planificador.
- Qué hace en **Circular** (el simulador da vueltas de 300 m sobre el punto al
  que iba). Al terminar la misión **Puntos** vuelve a casa y da vueltas (lo
  decidió el usuario, §6); confirmar que el autopiloto hace lo mismo.
- Qué significa **Distancia de encuentro** (DistEnc): el simulador no la usa.
- Si hay una trama en la que el dron devuelve el plan (`newPlanificacionReceived`
  existe en la estación, pero nada la emite).
- **Pérdida de enlace**: cómo la detecta el autopiloto. La estación solo
  manda algo cuando el operador envía mandos o una ruta (no hay una trama
  periódica de «sigo aquí»), así que tiene que ser por la radio o por tiempo
  sin recibir. Y qué devuelve en la telemetría mientras vuelve a casa sin
  enlace (el simulador sigue diciendo el régimen y la misión de antes), y si
  sale de ahí con unos mandos nuevos, como hace el simulador (§11).

## Mapa

- **Overlays del paquete** (capas fijas y mapa híbrido): hoy se esconden (§3).
  Decidir cuáles se ven y con qué menú.
- **Guardar los polígonos del operador**: con `autosaveFeatures = false` no se
  guardan; se puede hacer a mano con `saveFeaturesTo` / `loadFeaturesFrom`.
- **Lentitud al hacer zoom**: comprobar en el PC si con §3 (sin híbrido) ya va
  fluido; si no, medir qué capa base o qué tamaño de ventana lo frena.

## Fase 2: mejora del código (por orden)

1. ~~Orden del repo~~: hecho en §12.
2. ~~Fallos vistos~~: hechos en §13.
3. ~~Protocolo en un módulo aparte con tests~~: hecho en §4 (`Protocolo/Tramas`).
4. **Partir las clases grandes**: `CControladora` (UI, menús, protocolo,
   gráficas y BD en una) y `Widget_Puntos` (unas 3.900 líneas). Los índices de
   parámetros, a un `enum`.
5. **Código muerto** (no está en el `.pro`): `Indicador/`, `cSerialPort/`,
   `graficar/qtableviewprinter`, `qcheckboxcombo` y `myqbuttongroup`.
   `Reporte/` se compila pero no parece usarse: confirmarlo antes de quitarlo.
6. ~~Rendimiento~~: los `appendLogMessage` de cada trama, quitados en §13.
