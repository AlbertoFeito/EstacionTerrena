# CLAUDE.md — convenciones de EstacionTerrena

Las mismas reglas que en LibMapaStatic (su `CLAUDE.md` manda en lo que no
diga este).

## Rama y commits
- Una rama por tarea, nunca `main`: `claude/<tema>` desde `main` actualizado
  (o la rama que traiga la sesión). Cada rama acaba en un PR en borrador; el
  usuario lo prueba y lo fusiona. Claude nunca fusiona.
- Mensajes de commit en español, explicando el porqué. Cierra cada commit con
  `Co-Authored-By: Claude <modelo> <noreply@anthropic.com>` (y en la nube su
  línea `Claude-Session:`). Fuera de esa línea, ningún identificador de modelo.

## Código
- C++17, Qt 5.14 / 5.15 / 6.x (MinGW, MSVC, GCC). Sigue el estilo del fichero.
- Comentario `//` en español encima de cada función nueva (qué hace y, si
  importa, por qué).
- **El mapa solo se toca desde `Principal/Mapa/MapaMision`**: el resto de la
  app no llama a libmapa; como mucho usa `MapaMision::mapa()` como `QWidget`
  (para el layout y para colgarle overlays).
- **Las coordenadas que van al dron pasan por
  `Principal/Protocolo/ProyeccionMercator`** y no se cambia su fórmula: el
  autopiloto trabaja en esos metros (norte primero en las tramas).
- **El protocolo con el dron no se cambia**: lo define quien programa el
  autopiloto. Los bytes (cabeceras, estructuras de `Estructuras.h`, orden y
  suma) son los que son; se arman y se leen solo en `Principal/Protocolo/Tramas`,
  que comparten la estación y el simulador (`simulador/`). Se puede mejorar
  cómo se leen, no qué se manda.
- libmapa se usa INSTALADA (`C:/libmapa/qt6`, `LIBMAPA_PREFIX`). QCustomPlot
  ya viene compilada dentro: aquí solo la cabecera, nunca `qcustomplot.cpp`.

## Verificación
- Compila y pasa los tests (`make check` en la carpeta de build;
  `QT_QPA_PLATFORM=offscreen` sin pantalla).
- En la nube: `sudo apt-get install -y qt6-base-dev qt6-positioning-dev
  qt6-serialport-dev qt6-svg-dev libqt6sql6-sqlite qmake6 socat` (socat,
  para el test del puerto serie); compilar e instalar libmapa con
  `qmake6 <repo LibMapaStatic>/qmake/libmapa/libmapa.pro PREFIX=<prefijo>` (con
  QCustomPlot 2.1.1 en su `third_party/qcustomplot/`) y luego
  `qmake6 EstacionTierra3.pro LIBMAPA_PREFIX=<prefijo>`. Un clon nuevo
  compila tal cual: no hay ficheros de recursos externos.
- Iconos nuevos: se dibujan en código en `Principal/Iconos` (nada de PNG ni
  `.qrc` sueltos). `build/` es solo la salida de compilación (ignorada).

## Documentación
- Cada cambio, en `docs/BITACORA.md` (siguiente `## N.`), explicando el porqué.
- Lo aparcado, en `docs/PENDIENTES.md`; al hacerlo, se borra de ahí.
- README al día.
