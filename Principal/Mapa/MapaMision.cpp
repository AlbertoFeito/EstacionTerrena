#include "MapaMision.h"

#include <libmapa/MapWidget.h>

#include <QDir>
#include <QFileInfo>
#include <QFont>
#include <QPainter>
#include <QResizeEvent>
#include <QtMath>

#include <cmath>
#include <limits>

using namespace libmapa;

namespace {
// El dron es el unico objetivo del mapa: id fijo para reemplazarlo siempre.
const qint64 kIdVehiculo = 1;
// Posiciones de la estela del dron: 10 minutos a 10 tramas por segundo.
const int kEstelaPuntos = 10 * 60 * 10;

const QString kCapaRuta = QStringLiteral("ruta");
const QString kCapaMision = QStringLiteral("mision");
const QString kCapaDibujos = QStringLiteral("dibujos");

// Colores de la libreria antigua, para que el operador vea lo mismo.
const QColor kColorRumboVehiculo(0, 0, 255);
const QColor kColorRumboProximo(120, 0, 255);
const QColor kColorRumboDeseado(0, 255, 0);
// Largo en pantalla de los vectores de rumbo (actual y deseado): fijo, como un
// vector de rumbo, sea cual sea el zoom.
const double kLargoRumboPx = 150.0;
// Separacion minima en pantalla entre dos puntos de la ruta para rotularlos:
// por debajo, «P1»...«P10» se montaban unos encima de otros.
const double kSeparacionRotuloPx = 34.0;
// Mas lejos que esto, el proximo punto que dice el dron no es de esta mision
// (sin plan llega 0,0, que cae en el golfo de Guinea): no se dibuja la linea.
const double kMaxProximoM = 100000.0;
const QColor kColorLineaRuta(255, 150, 10);
const QColor kColorPunto(Qt::red);
const QColor kColorCumplido(Qt::green);
}

// Abre el mapa sobre el paquete como los ejemplos de libmapa (dataDir y sin
// autoguardado), deja solo las capas base, prepara las capas propias y engancha
// las senales de libmapa a las de la estacion. Si el paquete no trae elevacion
// se prueba la carpeta de .hgt de siempre (C:/CubaAlt), para no perder la cota
// del terreno que usan la ruta y el perfil.
MapaMision::MapaMision(const QString &paquete, QWidget *padre)
    : QObject(nullptr)
{
    MapConfig cfg;
    cfg.dataDir = paquete;
    cfg.autosaveFeatures = false;   // no tocar las entidades del paquete
    m_mapa = new MapWidget(cfg, padre);
    // Hijo del mapa: se destruye con el, y el proveedor de simbolos (que
    // captura this) nunca sobrevive al mapa.
    setParent(m_mapa);

    if (std::isnan(m_mapa->elevationAt(m_mapa->center()))
        && m_mapa->elevationDescription().isEmpty()) {
        const QStringList antiguas = {QDir::rootPath() + QStringLiteral("CubaAlt"),
                                      QDir::homePath() + QStringLiteral("/CubaAlt")};
        for (const QString &dir : antiguas) {
            if (QDir(dir).exists()) {
                m_mapa->setElevationDir(dir);
                break;
            }
        }
    }

    ocultarCapasDelPaquete();
    crearCapas();
    ponerCapaBase(CapaBase::Satelital);
    m_mapa->setScaleBarVisible(true);
    // Para no desplazar lo que se ve cuando un panel le quita sitio (eventFilter).
    m_mapa->installEventFilter(this);

    // Estela de los ultimos 10 minutos (a 10 tramas/s, 6000 posiciones). Antes
    // era entera y se redibujaba completa con cada trama: tras horas en
    // «Repetir mision» cada repintado iba mas lento. La ruta completa la sigue
    // guardando libmapa aparte (targetHistory, aclarada para no crecer).
    m_mapa->setTargetTrailLength(kEstelaPuntos);
    m_mapa->setTargetSymbolProvider([this](const MapTarget &t) {
        TargetSymbol s;
        if (t.id == m_vehiculo)
            s.icon = m_simbolo;
        return s;
    });

    connect(m_mapa, &MapWidget::zoomChanged, this, [this](int) {
        refrescarRumbos();
        refrescarRotulosRuta();
    });
    connect(m_mapa, &MapWidget::mouseMoved, this, &MapaMision::posicionRaton);
    connect(m_mapa, &MapWidget::clicked, this,
            [this](const QGeoCoordinate &pos, Qt::MouseButton boton) {
                if (boton == Qt::RightButton)
                    emit clicDerecho(pos);
            });
    connect(m_mapa, &MapWidget::areaSelected, this, [this]() {
        if (m_mapa->activeTool() == MapTool::AreaZoom) {
            m_mapa->setActiveTool(MapTool::None);
            emit zoomAreaTerminado();
        }
    });
    connect(m_mapa, &MapWidget::measurementFinished, this,
            [this](const Measurement &m) { emit medidaTerminada(m.azimuthDegrees, m.distanceMeters); });
    connect(m_mapa, &MapWidget::featureCreated, this, [this]() {
        if (m_mapa->activeTool() == MapTool::DrawPolygon)
            m_mapa->setActiveTool(MapTool::None);
    });

    // Arrastre de un punto de la ruta: la linea lo sigue en vivo y, al soltar,
    // se avisa con la posicion final para que el modelo de la ruta la guarde.
    connect(m_mapa, &MapWidget::featureDragMovedTo, this,
            [this](qint64 id, const QGeoCoordinate &pos) {
                const int i = m_puntos.indexOf(id);
                if (i >= 0 && m_linea >= 0)
                    m_mapa->moveVertex(m_linea, i, pos);
            });
    connect(m_mapa, &MapWidget::featureDragFinished, this, [this](qint64 id) {
        const int i = m_puntos.indexOf(id);
        const auto f = m_mapa->feature(id);
        if (i >= 0 && f && !f->geometry.isEmpty()) {
            emit puntoRutaMovido(i, f->geometry.first());
            refrescarRotulosRuta();
        }
    });
}

// Cuando un panel (rutas abajo, capas a la derecha) quita o devuelve sitio al
// mapa, lo que se ve se queda quieto: se recorta o se amplia por abajo y por la
// derecha. libmapa conserva el centro, y todo se desplazaba medio panel. Si
// cambia la ventana (maximizar, redimensionar), se conserva el centro, como
// siempre. Llega antes que el resizeEvent del mapa; libmapa ya ha ajustado su
// vista al tamano nuevo (center y visibleNorthWest lo sincronizan) con el
// centro viejo.
bool MapaMision::eventFilter(QObject *objeto, QEvent *evento)
{
    if (objeto != m_mapa || evento->type() != QEvent::Resize)
        return QObject::eventFilter(objeto, evento);
    const auto *r = static_cast<QResizeEvent *>(evento);
    const QSize ventana = m_mapa->window()->size();
    const bool porPanel = ventana == m_tamVentana && m_mapa->isVisible()
                          && r->oldSize().width() > 1 && r->oldSize().height() > 1;
    m_tamVentana = ventana;
    const int dw = r->size().width() - r->oldSize().width();
    const int dh = r->size().height() - r->oldSize().height();
    if (!porPanel || (dw == 0 && dh == 0) || r->size().width() <= 1 || r->size().height() <= 1)
        return QObject::eventFilter(objeto, evento);

    // En coordenadas de eje (longitud y Mercator, lineales en pantalla).
    const QPointF c = m_mapa->toAxisCoords(m_mapa->center());
    const QPointF no = m_mapa->toAxisCoords(m_mapa->visibleNorthWest());
    const QPointF se = m_mapa->toAxisCoords(m_mapa->visibleSouthEast());
    const double porPxX = (se.x() - no.x()) / r->size().width();
    const double porPxY = (no.y() - se.y()) / r->size().height();
    // Esquina de arriba a la izquierda fija: el centro va media diferencia
    // hacia la derecha y hacia abajo (hacia arriba y la izquierda si crece).
    m_mapa->setCenter(m_mapa->fromAxisCoords(QPointF(c.x() + dw / 2.0 * porPxX,
                                                     c.y() - dh / 2.0 * porPxY)));
    return QObject::eventFilter(objeto, evento);
}

// El widget de libmapa.
MapWidget *MapaMision::mapa() const
{
    return m_mapa;
}

// Si el mapa arranco con alguna capa base.
bool MapaMision::listo() const
{
    return m_mapa->isReady();
}

// Por que no arranco el mapa.
QString MapaMision::error() const
{
    return m_mapa->lastError();
}

// Avisos del paquete y de la elevacion juntos, para ensenarlos de una vez.
QStringList MapaMision::avisos() const
{
    return m_mapa->dataWarnings() + m_mapa->elevationWarnings();
}

// Al abrir, solo la capa base: el mapa hibrido (las teselas vectoriales que el
// paquete pone encima) y las capas fijas (aguas, corredores...) quedan cargados
// pero ocultos, y el operador los enciende en el panel de capas. Como no se
// carga ninguna base de entidades, las capas que hay al abrir son del paquete.
void MapaMision::ocultarCapasDelPaquete()
{
    if (m_mapa->hasVectorOverlay())
        m_mapa->setVectorOverlayVisible(false);
    for (const LayerInfo &capa : m_mapa->featureLayers())
        m_mapa->setFeatureLayerVisible(capa.id, false);
}

// Las capas que crea la estacion (no vienen del mapa.json).
bool MapaMision::esCapaDeLaEstacion(const QString &id)
{
    return id == kCapaDibujos || id == kCapaRuta || id == kCapaMision;
}

// Capas base del paquete, con la activa marcada.
QVector<CapaMapa> MapaMision::capasBase() const
{
    QVector<CapaMapa> capas;
    const QString activa = m_mapa->baseLayerId();
    for (const BaseLayerInfo &b : m_mapa->availableBaseLayers())
        capas.append({b.id, b.displayName.isEmpty() ? b.id : b.displayName, b.id == activa});
    return capas;
}

// Id de la capa base activa.
QString MapaMision::capaBaseId() const
{
    return m_mapa->baseLayerId();
}

// Pone la capa base por su id del mapa.json.
bool MapaMision::ponerCapaBaseId(const QString &id)
{
    if (id == m_mapa->baseLayerId())
        return true;
    if (!m_mapa->setBaseLayerId(id))
        return false;
    emit capasCambiadas();
    return true;
}

// Pasa los grupos de libmapa (calles, POI...) a los de la estacion.
static QVector<CapaMapa> aCapas(const QVector<BaseLayerGroup> &grupos)
{
    QVector<CapaMapa> capas;
    for (const BaseLayerGroup &g : grupos)
        capas.append({g.id, g.title.isEmpty() ? g.id : g.title, g.visible});
    return capas;
}

// Grupos de una capa base vectorial (vacio si es raster).
QVector<CapaMapa> MapaMision::gruposCapaBase(const QString &base) const
{
    return aCapas(m_mapa->baseLayerGroups(base));
}

// Enciende o apaga un grupo de una capa base vectorial.
bool MapaMision::ponerGrupoCapaBaseVisible(const QString &base, const QString &grupo, bool visible)
{
    if (!m_mapa->setBaseLayerGroupVisible(grupo, visible, base))
        return false;
    emit capasCambiadas();
    return true;
}

// Si el paquete trae mapa hibrido.
bool MapaMision::hayHibrido() const
{
    return m_mapa->hasVectorOverlay();
}

// Nombre del hibrido para el panel: el del fichero, sin extension.
QString MapaMision::nombreHibrido() const
{
    return QFileInfo(m_mapa->vectorOverlayFile()).completeBaseName();
}

bool MapaMision::hibridoVisible() const
{
    return m_mapa->hasVectorOverlay() && m_mapa->isVectorOverlayVisible();
}

// Muestra u oculta el hibrido entero (sus grupos no se olvidan).
void MapaMision::ponerHibridoVisible(bool visible)
{
    if (!m_mapa->hasVectorOverlay() || m_mapa->isVectorOverlayVisible() == visible)
        return;
    m_mapa->setVectorOverlayVisible(visible);
    emit capasCambiadas();
}

QVector<CapaMapa> MapaMision::gruposHibrido() const
{
    return aCapas(m_mapa->vectorOverlayGroups());
}

// Enciende o apaga un grupo del hibrido (nombres de calles, lugares...).
bool MapaMision::ponerGrupoHibridoVisible(const QString &grupo, bool visible)
{
    if (!m_mapa->setVectorOverlayGroupVisible(grupo, visible))
        return false;
    emit capasCambiadas();
    return true;
}

// Capas fijas del paquete, de arriba abajo como se pintan. Son las de solo
// lectura: las editables son las de la estacion y la de entidades que libmapa
// crea por defecto.
QVector<CapaMapa> MapaMision::capasFijas() const
{
    QVector<CapaMapa> capas;
    const QVector<LayerInfo> todas = m_mapa->featureLayers();
    for (auto it = todas.crbegin(); it != todas.crend(); ++it) {
        if (!it->editable && !esCapaDeLaEstacion(it->id))
            capas.append({it->id, it->displayName.isEmpty() ? it->id : it->displayName, it->visible});
    }
    return capas;
}

// Muestra u oculta una capa fija del paquete (nunca una de la estacion ni otra
// editable).
bool MapaMision::ponerCapaFijaVisible(const QString &id, bool visible)
{
    bool fija = false;
    for (const CapaMapa &c : capasFijas())
        fija = fija || c.id == id;
    if (!fija || !m_mapa->setFeatureLayerVisible(id, visible))
        return false;
    emit capasCambiadas();
    return true;
}

// Ids de los grupos apagados.
static QStringList ocultos(const QVector<CapaMapa> &grupos)
{
    QStringList ids;
    for (const CapaMapa &g : grupos)
        if (!g.visible)
            ids.append(g.id);
    return ids;
}

// Estado de las capas para conf.ini.
QVariantMap MapaMision::estadoCapas() const
{
    QVariantMap e;
    e.insert(QStringLiteral("base"), capaBaseId());
    e.insert(QStringLiteral("hibrido"), hibridoVisible());
    e.insert(QStringLiteral("hibrido_ocultos"), ocultos(gruposHibrido()));
    QStringList fijas;
    for (const CapaMapa &c : capasFijas())
        if (c.visible)
            fijas.append(c.id);
    e.insert(QStringLiteral("fijas"), fijas);
    for (const CapaMapa &b : capasBase()) {
        const QVector<CapaMapa> grupos = gruposCapaBase(b.id);
        if (!grupos.isEmpty())
            e.insert(QStringLiteral("ocultos_") + b.id, ocultos(grupos));
    }
    return e;
}

// Aplica un estado guardado. Avisa una sola vez al final, no por cada capa.
void MapaMision::restaurarCapas(const QVariantMap &estado)
{
    {
        const QSignalBlocker sinAvisos(this);
        const QString base = estado.value(QStringLiteral("base")).toString();
        if (!base.isEmpty())
            ponerCapaBaseId(base);
        for (const CapaMapa &b : capasBase()) {
            const QString clave = QStringLiteral("ocultos_") + b.id;
            if (!estado.contains(clave))
                continue;
            const QStringList apagados = estado.value(clave).toStringList();
            for (const CapaMapa &g : gruposCapaBase(b.id))
                ponerGrupoCapaBaseVisible(b.id, g.id, !apagados.contains(g.id));
        }
        if (estado.contains(QStringLiteral("hibrido_ocultos"))) {
            const QStringList apagados = estado.value(QStringLiteral("hibrido_ocultos")).toStringList();
            for (const CapaMapa &g : gruposHibrido())
                ponerGrupoHibridoVisible(g.id, !apagados.contains(g.id));
        }
        ponerHibridoVisible(estado.value(QStringLiteral("hibrido")).toBool());
        const QStringList fijas = estado.value(QStringLiteral("fijas")).toStringList();
        for (const CapaMapa &c : capasFijas())
            ponerCapaFijaVisible(c.id, fijas.contains(c.id));
    }
    emit capasCambiadas();
}

// Capas de la estacion: la ruta (puntos arrastrables), la mision (la casa, que
// no se selecciona) y los dibujos del operador.
void MapaMision::crearCapas()
{
    m_mapa->addFeatureLayer(kCapaDibujos, tr("Dibujos"), 40);
    m_mapa->addFeatureLayer(kCapaRuta, tr("Ruta"), 50);
    m_mapa->setFeatureLayerTransient(kCapaRuta);
    m_mapa->addFeatureLayer(kCapaMision, tr("Misión"), 60);
    m_mapa->setFeatureLayerTransient(kCapaMision);
    m_mapa->setFeatureLayerSelectable(kCapaMision, false);
}

// Id de la capa base en el mapa.json del paquete.
QString MapaMision::idCapa(CapaBase capa)
{
    return capa == CapaBase::OSM ? QStringLiteral("osm") : QStringLiteral("satelital");
}

// Cambia la capa base por su id; si el paquete usa otro id, busca una capa
// cuyo nombre lo recuerde ("Satelital", "Open Street Map").
bool MapaMision::ponerCapaBase(CapaBase capa)
{
    if (ponerCapaBaseId(idCapa(capa)))
        return true;
    const QString pista = capa == CapaBase::OSM ? QStringLiteral("street") : QStringLiteral("sat");
    for (const BaseLayerInfo &info : m_mapa->availableBaseLayers()) {
        if (info.displayName.contains(pista, Qt::CaseInsensitive)
            || info.id.contains(pista, Qt::CaseInsensitive))
            return ponerCapaBaseId(info.id);
    }
    return false;
}

// Capa base activa, en los terminos de la estacion.
MapaMision::CapaBase MapaMision::capaBase() const
{
    return m_mapa->baseLayerId() == idCapa(CapaBase::OSM) ? CapaBase::OSM : CapaBase::Satelital;
}

// Reemplaza el objetivo del dron entero (posicion, rumbo, color y rotulo):
// libmapa conserva su traza al reemplazarlo, y asi el color de la traza sigue
// al modo de vuelo como en la libreria antigua. El rotulo conserva donde lo
// haya dejado el operador (se puede arrastrar).
void MapaMision::actualizarVehiculo(const QGeoCoordinate &pos, double rumboDeg,
                                    const QColor &colorTraza, const QPixmap &simbolo,
                                    const QString &rotulo)
{
    if (!pos.isValid())
        return;
    m_simbolo = simbolo;

    MapTarget t;
    t.id = kIdVehiculo;
    t.position = pos;
    t.headingDeg = rumboDeg;
    t.label = rotulo;
    t.kind = QStringLiteral("dron");
    t.color = colorTraza;
    if (const auto anterior = m_mapa->target(kIdVehiculo))
        t.labelOffsetPx = anterior->labelOffsetPx;
    m_vehiculo = m_mapa->addTarget(t);
}

// Guarda los rumbos pedidos y redibuja las tres lineas.
void MapaMision::ponerRumbos(double rumboVehiculoDeg, double rumboDeseadoDeg,
                             const QGeoCoordinate &proximoPunto, bool automatico)
{
    m_rumboVehiculo = rumboVehiculoDeg;
    m_rumboDeseado = rumboDeseadoDeg;
    m_proximo = proximoPunto;
    m_automatico = automatico;
    m_hayRumbos = true;
    refrescarRumbos();
}

// Metros que miden \a px pixeles de pantalla a la escala actual.
double MapaMision::metrosEnPantalla(double px) const
{
    return m_mapa->metersPerPixel() * px;
}

// Pone, actualiza o quita cada linea de rumbo segun el modo. Se llama con cada
// trama y al cambiar de zoom (el largo en metros depende de la escala). Los
// rumbos actual y deseado son vectores cortos de largo fijo en pantalla; antes
// cruzaban toda la vista y, con la linea al proximo punto, se repintaban miles
// de kilometros de trazo en cada trama: la estacion no daba abasto y la
// telemetria se iba retrasando. El rotulo de cada linea se queda donde lo haya
// arrastrado el operador: si se rehiciera con el desplazamiento a cero en cada
// trama, no se podria mover.
void MapaMision::refrescarRumbos()
{
    if (!m_hayRumbos || m_vehiculo < 0)
        return;

    auto poner = [this](qint64 &id, bool mostrar, BearingLine linea) {
        if (!mostrar) {
            if (id >= 0)
                m_mapa->removeRangeMark(id);
            id = -1;
            return;
        }
        if (id >= 0) {
            if (const auto anterior = m_mapa->bearingLine(id))
                linea.labelOffsetPx = anterior->labelOffsetPx;
            if (m_mapa->updateBearingLine(id, linea))
                return;
        }
        id = m_mapa->addBearingLine(linea);
    };

    const double largo = metrosEnPantalla(kLargoRumboPx);

    BearingLine vehiculo;
    vehiculo.fromTarget = m_vehiculo;
    vehiculo.bearingDeg = m_rumboVehiculo;
    vehiculo.lengthMeters = largo;
    vehiculo.color = kColorRumboVehiculo;
    vehiculo.label = false;
    poner(m_lineaVehiculo, m_automatico, vehiculo);

    BearingLine proximo;
    proximo.fromTarget = m_vehiculo;
    proximo.to = m_proximo;
    proximo.color = kColorRumboProximo;
    const auto vehiculoActual = m_mapa->target(m_vehiculo);
    const bool proximoValido = m_proximo.isValid() && vehiculoActual
        && vehiculoActual->position.distanceTo(m_proximo) < kMaxProximoM;
    poner(m_lineaProximo, m_automatico && proximoValido, proximo);

    BearingLine deseado;
    deseado.fromTarget = m_vehiculo;
    deseado.bearingDeg = m_rumboDeseado;
    deseado.lengthMeters = largo;
    deseado.color = kColorRumboDeseado;
    deseado.label = false;
    poner(m_lineaDeseado, true, deseado);
}

// Dibuja la "H" de la casa: circulo verde de 40 px con la letra en blanco.
QPixmap MapaMision::simboloHome()
{
    QPixmap px(40, 40);
    px.fill(Qt::transparent);
    QPainter p(&px);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(QPen(Qt::green, 3));
    p.setBrush(Qt::NoBrush);
    p.drawEllipse(QRectF(2, 2, 36, 36));
    QFont f;
    f.setPixelSize(26);
    f.setBold(true);
    p.setFont(f);
    p.setPen(Qt::white);
    p.drawText(px.rect(), Qt::AlignCenter, QStringLiteral("H"));
    return px;
}

// Pone la casa o la mueve si ya estaba.
void MapaMision::ponerHome(const QGeoCoordinate &pos)
{
    if (!pos.isValid())
        return;
    if (m_home >= 0) {
        if (auto f = m_mapa->feature(m_home)) {
            MapFeature casa = *f;
            casa.geometry = {pos};
            m_mapa->updateFeature(casa);
            return;
        }
    }
    MapFeature casa;
    casa.layerId = kCapaMision;
    casa.kind = GeometryKind::Point;
    casa.type = QStringLiteral("home");
    casa.name = QStringLiteral("Home");
    casa.geometry = {pos};
    casa.selectable = false;
    casa.style.icon = simboloHome();
    casa.style.labelVisible = false;
    m_home = m_mapa->addFeature(casa);
}

// Quita del mapa todo lo del vuelo; la ruta se queda.
void MapaMision::limpiarVuelo()
{
    for (qint64 *id : {&m_lineaVehiculo, &m_lineaProximo, &m_lineaDeseado}) {
        if (*id >= 0)
            m_mapa->removeRangeMark(*id);
        *id = -1;
    }
    m_hayRumbos = false;
    if (m_vehiculo >= 0)
        m_mapa->removeTarget(m_vehiculo);
    m_vehiculo = -1;
    if (m_home >= 0)
        m_mapa->removeFeature(m_home);
    m_home = -1;
}

// Rehace la ruta desde cero: la linea, un punto arrastrable por vertice con su
// rotulo y los circulos de acercamiento. Se rehace entera porque cambia poco
// (al editarla), no con cada trama.
void MapaMision::pintarRuta(const QVector<PuntoRutaMapa> &puntos, bool visible)
{
    for (qint64 id : qAsConst(m_anillos))
        m_mapa->removeRangeMark(id);
    m_anillos.clear();
    m_mapa->clearFeatureLayer(kCapaRuta);
    m_linea = -1;
    m_puntos.clear();
    m_cumplidos.clear();

    if (!visible || puntos.isEmpty())
        return;

    if (puntos.size() >= 2) {
        MapFeature linea;
        linea.layerId = kCapaRuta;
        linea.kind = GeometryKind::Polyline;
        linea.type = QStringLiteral("ruta");
        linea.selectable = false;
        linea.style.lineColor = kColorLineaRuta;
        linea.style.lineWidth = 3.0;
        linea.style.labelVisible = false;
        for (const PuntoRutaMapa &p : puntos)
            linea.geometry.append(p.pos);
        m_linea = m_mapa->addFeature(linea);
    }

    for (int i = 0; i < puntos.size(); ++i) {
        MapFeature punto;
        punto.layerId = kCapaRuta;
        punto.kind = GeometryKind::Point;
        punto.type = QStringLiteral("punto_ruta");
        punto.name = puntos.at(i).nombre;
        punto.geometry = {puntos.at(i).pos};
        punto.attributes.insert(QStringLiteral("indice"), i);
        punto.draggable = true;
        punto.style.lineColor = kColorPunto;
        punto.style.fillColor = kColorPunto;
        punto.style.pointRadiusPx = 4.0;
        punto.style.labelColor = Qt::black;
        m_puntos.append(m_mapa->addFeature(punto));
    }
    m_cumplidos.fill(false, m_puntos.size());
    refrescarAnillos();
    refrescarRotulosRuta();
}

// Rotulos de los puntos solo si caben: con dos puntos a menos de
// kSeparacionRotuloPx en pantalla se rotula solo P1 (donde empieza); al
// acercarse vuelven todos.
void MapaMision::refrescarRotulosRuta()
{
    if (m_puntos.size() < 2)
        return;
    QVector<QGeoCoordinate> pos;
    for (qint64 id : qAsConst(m_puntos)) {
        const auto f = m_mapa->feature(id);
        if (f && !f->geometry.isEmpty())
            pos.append(f->geometry.first());
    }
    double minimo = std::numeric_limits<double>::max();
    for (int i = 0; i < pos.size(); ++i)
        for (int j = i + 1; j < pos.size(); ++j)
            minimo = std::min(minimo, pos.at(i).distanceTo(pos.at(j)));
    const double mpp = m_mapa->metersPerPixel();
    const bool caben = mpp > 0.0 && minimo / mpp >= kSeparacionRotuloPx;
    for (int i = 0; i < m_puntos.size(); ++i) {
        const auto f = m_mapa->feature(m_puntos.at(i));
        const bool quiere = caben || i == 0;
        if (!f || f->style.labelVisible == quiere)
            continue;
        MapFeature punto = *f;
        punto.style.labelVisible = quiere;
        punto.draggable = true;   // no se persiste: hay que volver a ponerlo
        m_mapa->updateFeature(punto);
    }
}

// Ajusta la vista para que se vea la ruta entera, con un margen alrededor.
void MapaMision::encuadrarRuta()
{
    double norte = -90.0, sur = 90.0, este = -180.0, oeste = 180.0;
    int n = 0;
    for (qint64 id : qAsConst(m_puntos)) {
        const auto f = m_mapa->feature(id);
        if (!f || f->geometry.isEmpty())
            continue;
        const QGeoCoordinate &c = f->geometry.first();
        norte = std::max(norte, c.latitude());
        sur = std::min(sur, c.latitude());
        este = std::max(este, c.longitude());
        oeste = std::min(oeste, c.longitude());
        ++n;
    }
    if (n == 0)
        return;
    // Margen del 25 % (y al menos unos 300 m): los anillos de acercamiento y
    // los rotulos no quedan pegados al borde.
    const double mLat = std::max((norte - sur) * 0.25, 0.003);
    const double mLon = std::max((este - oeste) * 0.25, 0.003);
    m_mapa->fitBounds(QGeoCoordinate(norte + mLat, oeste - mLon),
                      QGeoCoordinate(sur - mLat, este + mLon));
}

// Pinta de verde un punto ya cumplido: el punto y su circulo.
void MapaMision::marcarPuntoCumplido(int indice)
{
    if (indice < 0 || indice >= m_puntos.size() || m_cumplidos.at(indice))
        return;
    m_cumplidos[indice] = true;
    if (auto f = m_mapa->feature(m_puntos.at(indice))) {
        MapFeature punto = *f;
        punto.style.lineColor = kColorCumplido;
        punto.style.fillColor = kColorCumplido;
        punto.draggable = true;   // no se persiste: hay que volver a ponerlo
        m_mapa->updateFeature(punto);
    }
    refrescarAnillos();
}

// Cambia el radio de acercamiento de todos los puntos.
void MapaMision::ponerRadioAcercamiento(double metrosMercator)
{
    m_radio = metrosMercator;
    refrescarAnillos();
}

// Radio de acercamiento actual, en metros Mercator.
double MapaMision::radioAcercamiento() const
{
    return m_radio;
}

// Un anillo por punto de la ruta, que lo sigue si se arrastra. El radio llega
// en metros MERCATOR (los del autopiloto, y la antigua libreria los dibujaba
// asi); libmapa dibuja metros sobre el terreno, que a la latitud del punto son
// el radio por cos(lat): asi el circulo sale del mismo tamano que antes.
void MapaMision::refrescarAnillos()
{
    for (qint64 id : qAsConst(m_anillos))
        m_mapa->removeRangeMark(id);
    m_anillos.clear();
    if (m_radio <= 0.0)
        return;

    for (int i = 0; i < m_puntos.size(); ++i) {
        const auto f = m_mapa->feature(m_puntos.at(i));
        if (!f || f->geometry.isEmpty())
            continue;
        RangeRings anillo;
        anillo.featureId = m_puntos.at(i);
        anillo.spacingMeters = m_radio * std::cos(qDegreesToRadians(f->geometry.first().latitude()));
        anillo.count = 1;
        anillo.labels = false;
        anillo.color = m_cumplidos.value(i) ? kColorCumplido : kColorPunto;
        m_anillos.append(m_mapa->addRangeRings(anillo));
    }
}

// Herramienta Medir (distancia y marcacion entre dos clics) o Navegar.
void MapaMision::medirDistancia(bool activa)
{
    m_mapa->setActiveTool(activa ? MapTool::Measure : MapTool::None);
}

// Herramienta de ampliar por recuadro; al terminar vuelve sola a Navegar.
void MapaMision::zoomArea(bool activa)
{
    m_mapa->setActiveTool(activa ? MapTool::AreaZoom : MapTool::None);
}

// Un nivel mas de zoom.
void MapaMision::zoomMas()
{
    m_mapa->zoomIn();
}

// Un nivel menos de zoom.
void MapaMision::zoomMenos()
{
    m_mapa->zoomOut();
}

// Dibujar un poligono en la capa de dibujos (doble clic o Enter lo cierra).
void MapaMision::dibujarPoligono()
{
    m_mapa->setActiveFeatureLayer(kCapaDibujos);
    m_mapa->setActiveTool(MapTool::DrawPolygon);
}

// Centra la vista en una posicion (doble clic en un punto de la tabla de ruta).
void MapaMision::centrarEn(const QGeoCoordinate &pos)
{
    if (pos.isValid())
        m_mapa->setCenter(pos);
}

// Cota del terreno del origen de elevacion del mapa (NaN si no hay).
double MapaMision::alturaTerreno(const QGeoCoordinate &pos) const
{
    return m_mapa->elevationAt(pos);
}
