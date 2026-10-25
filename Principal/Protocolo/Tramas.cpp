#include "Tramas.h"

#include <cstring>

// Tamanos del protocolo: si alguien cambia una estructura (o el empaquetado),
// esto deja de compilar en vez de mandar al dron bytes corridos.
static_assert(sizeof(sTRAMA1) == 32, "sTRAMA1 tiene que ocupar 32 bytes");
static_assert(sizeof(sTRAMA2) == 91, "sTRAMA2 tiene que ocupar 91 bytes");
static_assert(sizeof(sCUADROPTEROS) == 126, "sCUADROPTEROS tiene que ocupar 126 bytes");
static_assert(sizeof(sMANDOS) == 79, "sMANDOS tiene que ocupar 79 bytes");
static_assert(sizeof(sPUNTOXYZ) == 29, "sPUNTOXYZ tiene que ocupar 29 bytes");

namespace Tramas {

namespace {
// Anade la suma de 16 bits al final, byte bajo primero.
QByteArray conSuma(QByteArray trama)
{
    const quint16 s = suma(trama);
    trama.append(char(s & 0xFF));
    trama.append(char(s >> 8));
    return trama;
}

// Cabecera seguida de los bytes de una estructura empaquetada.
template <typename T>
QByteArray cabeceraY(const char *cabecera, const T &estructura)
{
    QByteArray b(cabecera, 3);
    b.append(reinterpret_cast<const char *>(&estructura), int(sizeof(T)));
    return b;
}

// Posicion de la primera cabecera de \a cabeceras en \a b (y cual), o -1.
int primera(const QByteArray &b, std::initializer_list<const char *> cabeceras, QByteArray *cual)
{
    int mejor = -1;
    for (const char *c : cabeceras) {
        const int pos = b.indexOf(c);
        if (pos >= 0 && (mejor < 0 || pos < mejor)) {
            mejor = pos;
            *cual = QByteArray(c, 3);
        }
    }
    return mejor;
}

// Si se acumula basura sin ninguna trama, se queda solo la cola (podria ser
// el principio de una trama).
void recortar(QByteArray &b, int maximo)
{
    if (b.size() > maximo)
        b = b.right(256);
}
}

// Suma de todos los bytes como enteros sin signo, en 16 bits.
quint16 suma(const QByteArray &datos)
{
    quint16 s = 0;
    for (char c : datos)
        s += static_cast<quint8>(c);
    return s;
}

// "xyz" + sMANDOS + suma.
QByteArray mandos(const sMANDOS &m)
{
    return conSuma(cabeceraY("xyz", m));
}

// "uvw" + los 10 puntos + suma.
QByteArray plan(const sPUNTOXYZ (&puntos)[kPuntosPlan])
{
    QByteArray b("uvw", 3);
    b.append(reinterpret_cast<const char *>(puntos), int(sizeof(sPUNTOXYZ) * kPuntosPlan));
    return conSuma(b);
}

// "abc" + sTRAMA1 + sTRAMA2, sin suma (como la manda el autopiloto).
QByteArray constante(const sTRAMA1 &t1, const sTRAMA2 &t2)
{
    QByteArray b = cabeceraY("abc", t1);
    b.append(reinterpret_cast<const char *>(&t2), int(sizeof(sTRAMA2)));
    return b;
}

// sCUADROPTEROS tal cual: ya empieza por "abc". Se fuerza la cabecera por si
// alguien la ha tocado.
QByteArray cuadroptero(const sCUADROPTEROS &c)
{
    QByteArray b(reinterpret_cast<const char *>(&c), int(sizeof(sCUADROPTEROS)));
    b.replace(0, 3, QByteArray("abc", 3));
    return b;
}

// "xyz"/"uvw" + '1' si se recibio bien, '0' si no.
QByteArray acuse(const char *cabecera, bool recibido)
{
    QByteArray b(cabecera, 3);
    b.append(recibido ? '1' : '0');
    return b;
}

// ------------------------------------------------------- DecodificadorDron --

// Cambia el tipo de dron; lo que hubiera a medias ya no vale.
void DecodificadorDron::setTipoDron(int tipo)
{
    if (tipo != m_tipo)
        reiniciar();
    m_tipo = tipo;
}

// Anade lo recibido y decodifica mientras salgan tramas completas.
void DecodificadorDron::agregar(const QByteArray &bytes)
{
    m_buffer.append(bytes);
    while (paso()) {
    }
    recortar(m_buffer, kMaxBuffer);
}

// Entrega y vacia la cola de eventos.
QVector<DecodificadorDron::Evento> DecodificadorDron::eventos()
{
    QVector<Evento> salida;
    salida.swap(m_eventos);
    return salida;
}

// Descarta lo que estuviera a medias.
void DecodificadorDron::reiniciar()
{
    m_buffer.clear();
}

// Busca la siguiente cabecera y, si su trama esta completa, la decodifica y la
// quita del buffer. Devuelve true si consumio una trama (puede haber otra).
// Con cuadroptero solo se busca "abc": el antiguo SerialManager no sabia que
// hacer con "xyz"/"uvw" en ese modo (se quedaba atascado en ellos).
bool DecodificadorDron::paso()
{
    QByteArray cabecera;
    const int pos = m_tipo == 0 ? primera(m_buffer, {"abc", "xyz", "uvw"}, &cabecera)
                                : primera(m_buffer, {"abc"}, &cabecera);
    if (pos < 0) {
        // Sin cabecera: basura, salvo los dos ultimos bytes (media cabecera).
        if (m_buffer.size() > 2)
            m_buffer = m_buffer.right(2);
        return false;
    }
    m_buffer.remove(0, pos);   // lo anterior a la cabecera es basura

    Evento e;
    if (m_tipo != 0) {
        if (m_buffer.size() < int(sizeof(sCUADROPTEROS)))
            return false;
        std::memcpy(&e.cuadro, m_buffer.constData(), sizeof(sCUADROPTEROS));
        m_buffer.remove(0, int(sizeof(sCUADROPTEROS)));
        e.tipo = Tipo::Cuadroptero;
    } else if (cabecera == "abc") {
        const int largo = 3 + int(sizeof(sTRAMA1) + sizeof(sTRAMA2));
        if (m_buffer.size() < largo)
            return false;
        std::memcpy(&e.t1, m_buffer.constData() + 3, sizeof(sTRAMA1));
        std::memcpy(&e.t2, m_buffer.constData() + 3 + sizeof(sTRAMA1), sizeof(sTRAMA2));
        m_buffer.remove(0, largo);
        e.tipo = Tipo::Constante;
    } else {
        if (m_buffer.size() < 4)
            return false;
        e.recibido = m_buffer.at(3) == '1';
        e.tipo = cabecera == "xyz" ? Tipo::AcuseMandos : Tipo::AcusePlan;
        m_buffer.remove(0, 4);
    }
    m_eventos.append(e);
    return true;
}

// --------------------------------------------------- DecodificadorEstacion --

// Anade lo recibido y decodifica mientras salgan tramas completas.
void DecodificadorEstacion::agregar(const QByteArray &bytes)
{
    m_buffer.append(bytes);
    while (paso()) {
    }
    recortar(m_buffer, kMaxBuffer);
}

// Entrega y vacia la cola de eventos.
QVector<DecodificadorEstacion::Evento> DecodificadorEstacion::eventos()
{
    QVector<Evento> salida;
    salida.swap(m_eventos);
    return salida;
}

// Descarta lo que estuviera a medias.
void DecodificadorEstacion::reiniciar()
{
    m_buffer.clear();
}

// Busca "xyz" (mandos) o "uvw" (plan); con la trama entera, comprueba la suma
// y la entrega. Devuelve true si consumio una trama.
bool DecodificadorEstacion::paso()
{
    QByteArray cabecera;
    const int pos = primera(m_buffer, {"xyz", "uvw"}, &cabecera);
    if (pos < 0) {
        if (m_buffer.size() > 2)
            m_buffer = m_buffer.right(2);
        return false;
    }
    m_buffer.remove(0, pos);

    const bool esMandos = cabecera == "xyz";
    const int cuerpo = esMandos ? int(sizeof(sMANDOS)) : int(sizeof(sPUNTOXYZ)) * kPuntosPlan;
    const int largo = 3 + cuerpo + 2;
    if (m_buffer.size() < largo)
        return false;

    Evento e;
    e.tipo = esMandos ? Tipo::Mandos : Tipo::Plan;
    if (esMandos)
        std::memcpy(&e.mandos, m_buffer.constData() + 3, sizeof(sMANDOS));
    else
        std::memcpy(e.puntos, m_buffer.constData() + 3, sizeof(sPUNTOXYZ) * kPuntosPlan);
    const quint16 recibida = quint16(quint8(m_buffer.at(largo - 2)))
                           | quint16(quint8(m_buffer.at(largo - 1)) << 8);
    e.sumaCorrecta = recibida == suma(m_buffer.left(3 + cuerpo));
    m_buffer.remove(0, largo);
    m_eventos.append(e);
    return true;
}

} // namespace Tramas
