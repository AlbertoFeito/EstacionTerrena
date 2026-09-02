#ifndef CUSTOMPLOT_H
#define CUSTOMPLOT_H

#include "graficar/qcustomplot.h"
#include "XxwTracer.h"

#include <QPoint>
class QRubberBand;
class QMouseEvent;
class QWidget;
class CustomPlot : public QCustomPlot
{
    Q_OBJECT
public:
    CustomPlot(QWidget *parent = Q_NULLPTR);
    virtual ~CustomPlot() Q_DECL_OVERRIDE;

    void setRange(QCPRange range);
    void showTracer(bool show)
    {
        m_isShowTracer = show;
        if(m_xTracer)
            m_xTracer->setVisible(m_isShowTracer);
        if(m_yTracer)
            m_yTracer->setVisible(m_isShowTracer);
        foreach (XxwTracer *tracer, m_dataTracers)
        {
            if(tracer)
                tracer->setVisible(m_isShowTracer);
        }
        if(m_lineTracer)
            m_lineTracer->setVisible(m_isShowTracer);
    }
    bool isShowTracer()
    {
        return m_isShowTracer;
    }

private slots://zoom
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private:
    bool m_isShowTracer;
    XxwTracer *m_xTracer;
    XxwTracer *m_yTracer;
    QList<XxwTracer *> m_dataTracers;
    XxwTraceLine  *m_lineTracer;

    /******zoom******/

    QCPRange myRange;

Q_SIGNALS:

    void datosCursor(double);

public:
    virtual QSize sizeHint() const Q_DECL_OVERRIDE;

    void zoommas(QCustomPlot *customPlot);
    void zoommenos(QCustomPlot *customPlot);

private slots:
    void legendItemClicked(QCPLegend *legend, QCPAbstractLegendItem *legendItem, QMouseEvent *event);
    void graphClicked(QCPAbstractPlottable *plottable, int dataIndex);
};

#endif // CUSTOMPLOT_H
