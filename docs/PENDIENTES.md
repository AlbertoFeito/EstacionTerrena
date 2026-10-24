# Pendientes

Lo que queda por hacer, con lo que ya se sabe para no repetir el análisis. Al
hacer una, se borra de aquí y se cuenta en `BITACORA.md`.

## Siguiente PR: borrar la librería antigua

Cuando se haya probado el cambio a libmapa (§1) con el dron y el mapa real:
borrar la carpeta `LibMapaStatic/` (ya no se compila).

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
   - `Widget_Mandos::on_pB_Enviar_clicked` mete en la trama la DIRECCIÓN del
     puntero (`&mandos`, que ya es un puntero). No hace daño porque
     `CControladora` rehace la trama, pero sobra.
   - Rutas que dependen del directorio de trabajo (`currentPath().cdUp()`).
   - `usbdeviceswatcher` en Linux nunca arranca su bucle de udev.
3. **Protocolo serie en un módulo aparte** (solo Qt Core), con tests de las
   tramas `abc`/`xyz`/`uvw` y su suma de comprobación. El empaquetado de los
   mandos está copiado dos veces en `CControladora`.
4. **Partir las clases grandes**: `CControladora` (UI, menús, protocolo,
   gráficas y BD en una) y `Widget_Puntos` (unas 3.900 líneas). Los índices de
   parámetros, a un `enum`.
5. **Código muerto** (no está en el `.pro`): `Indicador/`, `cSerialPort/`,
   `graficar/qtableviewprinter`, `qcheckboxcombo` y `myqbuttongroup`.
   `Reporte/` se compila pero no parece usarse: confirmarlo antes de quitarlo.
6. **Rendimiento**: quitar los `appendLogMessage` de cada trama.
