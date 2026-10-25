//#include "Ci_Principal.h"
#include "CControladora.h"
#include <QtWidgets/QApplication>
#include <QDebug>
#include <QDir>
#include "Database/ConnectionManager.h"

#include "logger/LogHandler.h"
#include "Rutas.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    a.setStyle("fusion");

    LogHandler& logHandler = LogHandler::getInstance();
    logHandler.appendLogMessage("Control");
    logHandler.installMessageHandler();

    Database::ConnectionManager *mgr = Database::ConnectionManager::createInstance();
    mgr->setType("QSQLITE");

    const QString dirFich = Rutas::carpetaDatos();
    logHandler.appendLogMessage("Configuración: " + Rutas::confIni());
    logHandler.appendLogMessage("Datos: " + dirFich);
    //    if(QFile::exists(dirFich + "/DronDB.db"))

    mgr->setDatabaseName(dirFich + "/DronDB.db");

    CControladora controladora;
    //    Ci_Principal w;
    //    w.show();
    //    return a.exec();

    logHandler.uninstallMessageHandler();
    qDebug() << "........"; // 不写入日志
    logHandler.installMessageHandler();

    int ret = a.exec();

    Database::ConnectionManager::destroyInstance();

    logHandler.uninstallMessageHandler();
    return ret;
}
