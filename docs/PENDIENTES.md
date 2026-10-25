# Pendientes

Lo que queda por hacer, con lo que ya se sabe para no repetir el análisis. Al
hacer una, se borra de aquí y se cuenta en `BITACORA.md`.

## Preguntas para quien programa el autopiloto

- Qué hace en **Circular** (el simulador da vueltas de 300 m sobre el punto al
  que iba). Al terminar la misión **Puntos** vuelve a casa y da vueltas (lo
  decidió el usuario, §6); confirmar que el autopiloto hace lo mismo.
- Qué significa **Distancia de encuentro** (DistEnc): el simulador no la usa.
- En el **cuadróptero**, la estación pasa al mapa el rumbo actual y el deseado
  cambiados (como la librería antigua, §27). ¿Es a propósito o un error?
- Si hay una trama en la que el dron devuelve el plan (`newPlanificacionReceived`
  existe en la estación, pero nada la emite).
- **Pérdida de enlace**: cómo la detecta el autopiloto. La estación solo
  manda algo cuando el operador envía mandos o una ruta (no hay una trama
  periódica de «sigo aquí»), así que tiene que ser por la radio o por tiempo
  sin recibir. Y qué devuelve en la telemetría mientras vuelve a casa sin
  enlace (el simulador sigue diciendo el régimen y la misión de antes), y si
  sale de ahí con unos mandos nuevos, como hace el simulador (§11).

## Mapa

- **Guardar los polígonos del operador**: con `autosaveFeatures = false` no se
  guardan; se puede hacer a mano con `saveFeaturesTo` / `loadFeaturesFrom`.

## Fase 2: mejora del código (por orden)

1. ~~Orden del repo~~: hecho en §12.
2. ~~Fallos vistos~~: hechos en §13.
3. ~~Protocolo en un módulo aparte con tests~~: hecho en §4 (`Protocolo/Tramas`).
4. **Partir las clases grandes**: `CControladora` (UI, menús, protocolo,
   gráficas y BD en una) y `Widget_Puntos` (unas 3.900 líneas).
   - ~~Telemetría~~: sacada en §27 (`Protocolo/Telemetria` y `VistaTelemetria`;
     con ella se fue el vector de índices `m_ParametrosValues`).
   - Menús y paneles de `CControladora`.
   - ~~`Widget_Puntos`~~: partido en §28 (`Planificacion/RoutePlanner`,
     `ModeloRuta` y `CaracteristicasDron`).
5. ~~Código muerto~~: quitado en §17 (`Reporte/` se queda: lo usa «Imprimir»).
6. ~~Rendimiento~~: los `appendLogMessage` de cada trama, quitados en §13.
