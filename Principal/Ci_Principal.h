#ifndef CI_PRINCIPAL_H
#define CI_PRINCIPAL_H

#include <QtWidgets/QMainWindow>

#include <QTime>

#include <QtWidgets/QTabWidget>

QT_BEGIN_NAMESPACE
namespace Ui { class Ci_Principal; }
QT_END_NAMESPACE

class Ci_Principal : public QMainWindow
{
    Q_OBJECT

public:
    Ci_Principal(QWidget *parent = nullptr);
    ~Ci_Principal();

    QWidget *wMapa() const;
    void setWMapa(QWidget *wMapa);



    QTabWidget *tabWidget() const;
    void setTabWidget(QTabWidget *tabWidget);


    void resizeEvent(QResizeEvent* event);


public slots:
    void ponerTema(bool claro);

protected:
    void timerEvent(QTimerEvent *event);
private slots:
signals :
    void visibleLog(bool);
    void resize();
    void move();
private:
    Ui::Ci_Principal *ui;
    QWidget *m_wMapa;

    //    WidgetPFD *m_indicadorPFD;

    bool m_Conectado = false;
    int TimerId;
    QTime start_time_;
    int vueloID;
    int dronID;
    QString DronID = "";




    // QWidget interface
protected:
    void moveEvent(QMoveEvent *event);
};
#endif // CI_PRINCIPAL_H
