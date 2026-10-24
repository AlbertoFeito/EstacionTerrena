#include "MapaMision.h"

#include <libmapa/MapWidget.h>

#include <QDir>
#include <QFont>
#include <QPainter>
#include <QtMath>

#include <cmath>

using namespace libmapa;

namespace {
// El dron es el unico objetivo del mapa: id fijo para reemplazarlo siempre.
const qint64 kIdVehiculo = 1;

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

    soloCapasBase();
    crearCapas();
    ponerCapaBase(CapaBase::Satelital);
    m_mapa->setScaleBarVisible(true);

    m_mapa->setTargetTrailLength(-1);   // la traza entera, como antes (TODAS)
    m_mapa->setTargetSymbolProvider([this](const MapTarget &t) {
        TargetSymbol s;
        if (t.id == m_vehiculo)
            s.icon = m_simbolo;
        return s;
    });

    connect(m_mapa, &MapWidget::zoomChanged, this, [this](int) { refrescarRumbos(); });
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
        if (i >= 0 && f && !f->geometry.isEmpty())
            emit puntoRutaMovido(i, f->geometry.first());
    });
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

// Fuera el mapa hibrido (las teselas vectoriales que el paquete pone encima de
// la base) y las capas fijas del paquete (aguas, corredores...): de momento la
// estacion solo muestra las capas base, satelital y OSM. Como no se carga
// ninguna base de entidades, las capas que hay al abrir son todas del paquete.
void MapaMision::soloCapasBase()
{
    if (m_mapa->hasVectorOverlay())
        m_mapa->clearVectorOverlay();
    for (const LayerInfo &capa : m_mapa->featureLayers())
        m_mapa->setFeatureLayerVisible(capa.id, false);
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
    if (m_mapa->setBaseLayerId(idCapa(capa)))
        return true;
    const QString pista = capa == CapaBase::OSM ? QStringLiteral("street") : QStringLiteral("sat");
    for (const BaseLayerInfo &info : m_mapa->availableBaseLayers()) {
        if (info.displayName.contains(pista, Qt::CaseInsensitive)
            || info.id.contains(pista, Qt::CaseInsensitive))
            return m_mapa->setBaseLayerId(info.id);
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

// Cota del terreno del origen de elevacion del mapa (NaN si no hay).
double MapaMision::alturaTerreno(const QGeoCoordinate &pos) const
{
    return m_mapa->elevationAt(pos);
}
