#include "AlturaWorker.h"



AlturaWorker::AlturaWorker(QObject *parent) :QObject(parent)
{

}

AlturaWorker::~AlturaWorker()
{

}

qint16 AlturaWorker::obtenerAltura(double lat, double lon) {
    // Lógica para obtener la altura desde HGT
//    qint16 altura = obtenerAlturaDesdeHGT(lat, lon);
    qint16 altura = obtenerAlturaDesdeHGT2(lat, lon);
    emit alturaActualizada(altura);
    return  altura;
}

quint16 AlturaWorker::obtenerAlturaDesdeHGT(double lat, double lon)
{
    const int SRTM_SIZE = 1201;

    GMS gmsla = QUtiles::GradosToGms(lat);
    GMS gmslo = QUtiles::GradosToGms(-lon);

    int latArc=(gmsla.M)*60+gmsla.S;
    int lonArc=(gmslo.M)*60+gmslo.S;
    int row = 1200 - int(round(latArc / 3));
    int col = 1200 - int(round(lonArc / 3));

    QString hgtFileName = QString("N%1W%2.hgt").arg (QString::number(floor(lat), 'f',0),2,'0').arg (QString::number (floor(-lon+1), 'f',0),3,'0');


    QString dirCubaAlt;

#ifdef Q_OS_WIN
    dirCubaAlt = QDir::rootPath ()+ "CubaAlt/"+hgtFileName;
#else
    dirCubaAlt = QDir::homePath ()+"/CubaAlt/"+nombreFichero;
#endif
//    qint16 altura = 0;

    static QFile f;
    static QString currentFile;

    if(!f.isOpen () || currentFile != dirCubaAlt)
    {
        if(f.isOpen ())
            f.close ();
        f.setFileName (dirCubaAlt);
        currentFile = dirCubaAlt;
        if(!f.open (QIODevice::ReadOnly))
            return 0;
    }
    if(row >= 0 && row < SRTM_SIZE && col >= 0 && col < SRTM_SIZE)
    {
        int pos = (row * SRTM_SIZE + col) * 2;
        if(f.seek(pos))
        {
            short dato = 0;
            f.read((char*)&dato, 2);
            qSwap(((uchar*)&dato)[0], ((uchar*)&dato)[1]);
            if(dato >= 0 && dato <= 2000)
                return dato;
        }
    }

//    QFile f(dirCubaAlt);
//    if(f.open(QFile::ReadOnly))
//    {

//        int pos = (row * SRTM_SIZE + col) * 2;
//        if(f.seek(pos))
//        {
//            short dato = 0;
//            f.read((char*)&dato, 2);
//            qSwap(((uchar*)&dato)[0], ((uchar*)&dato)[1]);
//            if(dato >= 0 && dato <= 2000)
//                altura = dato;
//        }
//        f.close();
//    }
//    else
//    {
//        altura = 0.0;
//    }
    return 0;
}

int AlturaWorker::obtenerAlturaDesdeHGT2(double lat, double lon)
{

    const int SRTM_SIZE = 1201;
    const QString DIR_HGT = QDir::rootPath ()+ "CubaAlt/";

    QString hgtFileName = QString("N%1W%2.hgt").arg (QString::number(floor(lat), 'f',0),2,'0').arg (QString::number (floor(-lon+1), 'f',0),3,'0');

    QString dirCubaAlt;
    dirCubaAlt = QDir::rootPath ()+ "CubaAlt/"+hgtFileName;

    QFile file(dirCubaAlt);
    if(!file.open (QIODevice::ReadOnly))
    {
        return 0;
    }
    QDataStream in(&file);
    in.setByteOrder (QDataStream::BigEndian);

    GMS gmsla = QUtiles::GradosToGms(lat);
    GMS gmslo = QUtiles::GradosToGms(-lon);

    int latArc=(gmsla.M)*60+gmsla.S;
    int lonArc=(gmslo.M)*60+gmslo.S;
    int row = 1200 - int(round(latArc / 3));
    int col = 1200 - int(round(lonArc / 3));
    int pos = (row * SRTM_SIZE + col) * 2;

    if(!file.seek (pos))
        return 0;
    quint16 altura = 0;
    in >> altura;
    file.close ();
    return altura;
}
