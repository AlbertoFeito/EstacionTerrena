#include "myqsetting.h"
#include <QVariant>
#include <QSettings>
#include <QDir>
#include <QDebug>
MyQSetting::MyQSetting(QObject *parent) : QObject(parent)
{

}

void MyQSetting::guardarSetting(const QString &fileName, const QString &llave, const QVariant &valor, const QString &grupo)
{
//    qDebug()<<fileName;
    QSettings S(fileName,QSettings::IniFormat,nullptr);
    S.beginGroup(grupo);
    S.setValue(llave,valor);
    S.endGroup();
}

QVariant MyQSetting::cargarSetting(const QString &fileName, const QString &llave, const QString &grupo,const QVariant &valDefecto)
{
//    qDebug()<<fileName <<"  g--" <<grupo <<"  ll--" <<llave << "  v--"<< valDefecto;
    QVariant v;
    QSettings S(fileName,QSettings::IniFormat,nullptr);
    S.beginGroup(grupo);
    v = S.value(llave,valDefecto);
    S.endGroup();
    return v;
}
