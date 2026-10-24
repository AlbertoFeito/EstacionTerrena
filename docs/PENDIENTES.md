# Pendientes

Lo que queda por hacer, con lo que ya se sabe para no repetir el análisis. Al
hacer una, se borra de aquí y se cuenta en `BITACORA.md`.

## Siguiente PR: borrar la librería antigua

Cuando se haya probado el cambio a libmapa (§1) con el dron y el mapa real:
borrar la carpeta `LibMapaStatic/` (ya no se compila).

## Preguntas para quien programa el autopiloto

- Qué hace en **Circular** (el simulador da vueltas de 300 m sobre el punto al
  que iba). Al terminar la misión **Puntos** vuelve a casa y da vueltas (lo
  decidió el usuario, §6); confirmar que el autopiloto hace lo mismo.
- Qué significa **Distancia de encuentro** (DistEnc): el simulador no la usa.
- Si hay una trama en la que el dron devuelve el plan (`newPlanificacionReceived`
  existe en la estación, pero nada la emite).

## Mapa

- **Overlays del paquete** (capas fijas y mapa híbrido): hoy se esconden (§3).
  Decidir cuáles se ven y con qué menú.
- **Guardar los polígonos del operador**: con `autosaveFeatures = false` no se
  guardan; se puede hacer a mano con `saveFeaturesTo` / `loadFeaturesFrom`.
- **Lentitud al hacer zoom**: comprobar en el PC si con §3 (sin híbrido) ya va
  fluido; si no, medir qué capa base o qué tamaño de ventana lo frena.

## Fase 2: mejora del código (por orden)

1. **Orden del repo**
   - `build/` está en `.gitignore`, pero tiene 911 ficheros commiteados (21 MB):
     ahí viven los `Recursos` (iconos, estilos, `.qrc`). Pasarlos a `Recursos/` en la raíz.
   - **Falta `build/Recursos/estilos.qrc` en el repo** (`Principal.pro` lo pide):
     un clon nuevo no compila. Hay que subirlo, o quitarlo si no se usa (ningún
     `:/` del código apunta a él, salvo quizá `:/icons/…` de `Widget_Puntos`).
   - `Principal/Principal.pro.user` está commiteado.
   - `CONFIG += ordered` está obsoleto: usar `.depends`.
2. **Fallos vistos**
   - `CControladora::recibirDatos` lee `aux.at(31)` sin mirar cuántos campos
     trae la línea: una línea corta tumba la app.
   - En ese mismo `recibirDatos`, el rumbo del avión es el campo 6 (el
     indicador usa el 6 como velocidad y el 8 como rumbo): parece un campo
     cambiado. Se ha mantenido igual al pasar a libmapa.
   - `m_ParametrosValues` tiene 25 valores para 24 nombres de `listaParametros`.
   - Rutas que dependen del directorio de trabajo (`currentPath().cdUp()`).
   - `usbdeviceswatcher` en Linux nunca arranca su bucle de udev.
3. ~~Protocolo en un módulo aparte con tests~~: hecho en §4 (`Protocolo/Tramas`).
4. **Partir las clases grandes**: `CControladora` (UI, menús, protocolo,
   gráficas y BD en una) y `Widget_Puntos` (unas 3.900 líneas). Los índices de
   parámetros, a un `enum`.
5. **Código muerto** (no está en el `.pro`): `Indicador/`, `cSerialPort/`,
   `graficar/qtableviewprinter`, `qcheckboxcombo` y `myqbuttongroup`.
   `Reporte/` se compila pero no parece usarse: confirmarlo antes de quitarlo.
6. **Rendimiento**: quitar los `appendLogMessage` de cada trama.
