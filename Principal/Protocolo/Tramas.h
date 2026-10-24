#ifndef TRAMAS_H
#define TRAMAS_H

#include "Estructuras/Estructuras.h"

#include <QByteArray>
#include <QVector>

/*!
 * \brief Las tramas que se cruzan la estacion y el dron, en bytes.
 *
 * Es el protocolo de siempre, el que ya hablaba SerialManager con el puerto
 * serie; ahora vive aqui para que lo compartan la estacion (por serie, TCP o
 * UDP) y el simulador, y para poder probarlo. Solo Qt Core: ni red, ni puerto
 * serie, ni interfaz. Se le dan bytes y devuelve estructuras, y al reves.
 *
 * Del dron a la estacion:
 * - ala fija:    "abc" + sTRAMA1 + sTRAMA2
 * - cuadroptero: sCUADROPTEROS (la estructura ya empieza por "abc")
 * - acuses:      "xyz" o "uvw" + un byte ('1' = recibido bien)
 *
 * De la estacion al dron (con suma de 16 bits al final, byte bajo primero):
 * - mandos: "xyz" + sMANDOS + suma
 * - plan:   "uvw" + 10 x sPUNTOXYZ + suma
 *
 * La suma es la de todos los bytes de la trama, cabecera incluida, en 16 bits.
 * Las estructuras van empaquetadas a 1 byte y en el orden de bytes de la
 * maquina (little endian en el PC y en el autopiloto).
 */
namespace Tramas {

//! Puntos que lleva siempre un plan de vuelo.
constexpr int kPuntosPlan = 10;

//! Suma de 16 bits de todos los bytes (la del protocolo).
quint16 suma(const QByteArray &datos);

// --- Lo que manda la estacion ---------------------------------------------
QByteArray mandos(const sMANDOS &m);
QByteArray plan(const sPUNTOXYZ (&puntos)[kPuntosPlan]);

// --- Lo que manda el dron (lo usa el simulador) -----------------------------
QByteArray constante(const sTRAMA1 &t1, const sTRAMA2 &t2);
QByteArray cuadroptero(const sCUADROPTEROS &c);
//! Acuse de recibo: \a cabecera es "xyz" (mandos) o "uvw" (plan).
QByteArray acuse(const char *cabecera, bool recibido);

/*!
 * \brief Lee lo que manda el DRON, en el orden en que llegue y a trozos.
 *
 * Se le va dando lo que llegue (por serie, TCP o UDP) con agregar(); cada
 * trama completa queda en la cola de eventos. La basura entre tramas se
 * descarta buscando la siguiente cabecera. Interpreta igual que el antiguo
 * SerialManager: con dron de ala fija, "abc" trae sTRAMA1 + sTRAMA2 y "xyz" /
 * "uvw" son acuses; con cuadroptero, "abc" es el principio de sCUADROPTEROS y
 * no hay acuses.
 */
class DecodificadorDron
{
public:
    enum class Tipo { Constante, Cuadroptero, AcuseMandos, AcusePlan };

    struct Evento {
        Tipo tipo = Tipo::Constante;
        sTRAMA1 t1;
        sTRAMA2 t2;
        sCUADROPTEROS cuadro;
        bool recibido = false;   //!< En los acuses: el dron respondio '1'.
    };

    //! 0 = ala fija, 1 = cuadroptero (como SerialManager::setTipo_dron).
    void setTipoDron(int tipo);
    int tipoDron() const { return m_tipo; }

    //! Anade bytes y decodifica todas las tramas completas que haya.
    void agregar(const QByteArray &bytes);
    //! Saca los eventos decodificados desde la ultima llamada.
    QVector<Evento> eventos();
    //! Olvida lo que estuviera a medias (al conectar o tras un error).
    void reiniciar();

    //! Lo que queda sin decodificar (una trama a medias). Para pruebas.
    int pendientes() const { return m_buffer.size(); }

    //! Mas de esto sin una trama completa es basura: se descarta.
    static constexpr int kMaxBuffer = 4096;

private:
    bool paso();

    QByteArray m_buffer;
    QVector<Evento> m_eventos;
    int m_tipo = 0;
};

/*!
 * \brief Lee lo que manda la ESTACION (lo usa el simulador): mandos y planes.
 *
 * Comprueba la suma de cada trama; una trama con la suma mal se entrega igual
 * con \c sumaCorrecta = false, para que quien la reciba responda el acuse
 * negativo como haria el dron.
 */
class DecodificadorEstacion
{
public:
    enum class Tipo { Mandos, Plan };

    struct Evento {
        Tipo tipo = Tipo::Mandos;
        sMANDOS mandos;
        sPUNTOXYZ puntos[kPuntosPlan];
        bool sumaCorrecta = false;
    };

    void agregar(const QByteArray &bytes);
    QVector<Evento> eventos();
    void reiniciar();

    static constexpr int kMaxBuffer = 4096;

private:
    bool paso();

    QByteArray m_buffer;
    QVector<Evento> m_eventos;
};

} // namespace Tramas

#endif // TRAMAS_H
