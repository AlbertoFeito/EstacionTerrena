#include "customplot.h"

CustomPlot::CustomPlot(QWidget *parent)
    : QCustomPlot(parent)
    ,m_isShowTracer(false)
    ,m_xTracer(Q_NULLPTR)
    ,m_yTracer(Q_NULLPTR)
    ,m_dataTracers(QList<XxwTracer *>())
    ,m_lineTracer(Q_NULLPTR)
{
    setSelectionTolerance(3);

    foreach(QCPAxisRect *rect,this->axisRects())
    {
        rect->setRangeDrag(Qt::Horizontal);
        rect->setRangeZoom(Qt::Horizontal);
    }
    /*********************/
    this->setSelectionRectMode(QCP::SelectionRectMode::srmNone);
    this->selectionRect()->setPen(QPen(Qt::blue,1,Qt::DashLine));
    this->selectionRect()->setBrush(QBrush(QColor(0,0,100,50)));

    this->setMultiSelectModifier(Qt::KeyboardModifier::ControlModifier);
    /***********************/
    this->addLayer("AlturaGPS",nullptr,QCustomPlot::limBelow);
    this->layer("AlturaGPS")->setVisible(true);
    this->addLayer("VelocidadGPS",nullptr,QCustomPlot::limBelow);
    this->layer("VelocidadGPS")->setVisible(true);

    this->addLayer("Cursores",nullptr,QCustomPlot::limAbove);
    this->layer("Cursores")->setVisible(true);

    /****************axis time**********************/
    QSharedPointer<QCPAxisTickerTime> timeTicker(new QCPAxisTickerTime);
    timeTicker->setTimeFormat("%s");
    this->xAxis->setTicker(timeTicker);
//    this->axisRect()->setupFullAxesBox();
//    connect( this->xAxis, SIGNAL(rangeChanged(QCPRange)), this->xAxis2, SLOT(setRange(QCPRange)));

    this->legend->setSelectableParts(QCPLegend::spItems ); // legend box shall not be selectable, only legend items
    //make legend wrap text in two columns
    this->legend->setWrap(6);
    this->legend->setRowSpacing(1);
    this->legend->setColumnSpacing(2);
    this->legend->setFillOrder(QCPLayoutGrid::FillOrder::foColumnsFirst,true);
    //    qDebug()<<ui->graficas->legend->brush();
    this->legend->setBrush(Qt::transparent);
    this->legend->setFont( QFont("sans", 12, QFont::Bold));

    // set some pens, brushes and backgrounds:
    this->xAxis->setBasePen(QPen(Qt::white, 1));
    this->yAxis->setBasePen(QPen(Qt::white, 1));
    this->xAxis->setTickPen(QPen(Qt::white, 1));
    this->yAxis->setTickPen(QPen(Qt::white, 1));
    this->xAxis->setSubTickPen(QPen(Qt::white, 1));
    this->yAxis->setSubTickPen(QPen(Qt::white, 1));
    this->xAxis->setTickLabelColor(Qt::white);
    this->yAxis->setTickLabelColor(Qt::white);
    this->xAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    this->yAxis->grid()->setPen(QPen(QColor(140, 140, 140), 1, Qt::DotLine));
    this->xAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    this->yAxis->grid()->setSubGridPen(QPen(QColor(80, 80, 80), 1, Qt::DotLine));
    this->xAxis->grid()->setSubGridVisible(true);
    this->yAxis->grid()->setSubGridVisible(true);
    this->xAxis->grid()->setZeroLinePen(Qt::NoPen);
    this->yAxis->grid()->setZeroLinePen(Qt::NoPen);
    this->xAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    this->yAxis->setUpperEnding(QCPLineEnding::esSpikeArrow);
    QLinearGradient plotGradient;
    plotGradient.setStart(0, 0);
    plotGradient.setFinalStop(0, 350);
    plotGradient.setColorAt(0, QColor(50, 50, 50));
    plotGradient.setColorAt(1, QColor(20, 20, 20));
    this->setBackground(plotGradient);
    QLinearGradient axisRectGradient;
    axisRectGradient.setStart(0, 0);
    axisRectGradient.setFinalStop(0, 350);
    axisRectGradient.setColorAt(0, QColor(50, 50, 50));
    axisRectGradient.setColorAt(1, QColor(0, 0, 0));
    this->axisRect()->setBackground(QColor(50, 50, 50));

    this->rescaleAxes();

    this->yAxis->setRange(0,1000);//se estabelece rango de y
    connect(this, SIGNAL(legendClick(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)), this, SLOT(legendItemClicked(QCPLegend*,QCPAbstractLegendItem*,QMouseEvent*)));
    connect(this, SIGNAL(plottableClick(QCPAbstractPlottable*,int,QMouseEvent*)), this, SLOT(graphClicked(QCPAbstractPlottable*,int)));
}

CustomPlot::~CustomPlot()
{

}

void CustomPlot::setRange(QCPRange range)
{
    myRange = range;
}
void CustomPlot::mousePressEvent(QMouseEvent *event)
{
    switch(event->button())
    {
    case Qt::LeftButton:

        break;
    case Qt::RightButton:
        rescaleAxes();
        this->yAxis->setRange(0,1000);//se estabelece rango de y
        this->xAxis->setRange(myRange);
        replot();
        break;
    case Qt::MiddleButton:
        rescaleAxes();
        this->yAxis->setRange(0,1000);//se estabelece rango de y
        this->xAxis->setRange(myRange);
        replot();
        break;
    default:
        break;
    }

    QCPAbstractItem *item = itemAt(event->localPos());
    if (item && item->selectable()) {
        item->setSelected(true);
        item->layer()->replot();
    }
    QCustomPlot::mousePressEvent(event);
}

void CustomPlot::mouseReleaseEvent(QMouseEvent *event)
{

//    if(rubberBand->isVisible())
//    {
//        const QRect & zoomRect = rubberBand->geometry();
//        int xp1, yp1, xp2, yp2;
//        zoomRect.getCoords(&xp1, &yp1, &xp2, &yp2);

//        // zoom in only if the rect is bigger than 5px x 5px
//        int tol = 5;
//        if(abs(xp2-xp1) > tol && abs(yp2-yp1) > tol)
//        {
//            if(event->button() == Qt::LeftButton)
//            {
//                for(int j = 0; j < axisRectCount(); j++)
//                {
//                    for(int i = 0; i < axisRect(j)->axisCount(QCPAxis::atBottom); i++)
//                    {
//                        auto myAxis = axisRect(j)->axis(QCPAxis::atBottom, i);
//                        auto x1 = myAxis->pixelToCoord(xp1);
//                        auto x2 = myAxis->pixelToCoord(xp2);
//                        myAxis->setRange(x1, x2);
//                    }

//                    for(int i = 0; i < axisRect(j)->axisCount(QCPAxis::atLeft); i++)
//                    {
//                        //                            auto myAxis = axisRect(j)->axis(QCPAxis::atLeft, i);
//                        //                            auto y1 = myAxis->pixelToCoord(yp1);
//                        //                            auto y2 = myAxis->pixelToCoord(yp2);
//                        //                            myAxis->setRange(y1, y2);
//                    }
//                }
//            }
//            else if(event->button() == Qt::RightButton)
//            {

//            }
//        }

//        rubberBand->hide();
//        replot();
//    }
    QCustomPlot::mouseReleaseEvent(event);

    foreach (QCPAbstractItem *item, selectedItems()) {
        item->setSelected(false);
        item->layer()->replot();
    }
}

void CustomPlot::mouseMoveEvent(QMouseEvent *event)
{
    /********zoom***********/
//    if(rubberBand->isVisible())
//    {
//        rubberBand->setGeometry(QRect(origin, event->pos()).normalized());
//    }

    QCustomPlot::mouseMoveEvent(event);
    //cursor
    QCustomPlot::mouseMoveEvent(event);
    int x_pos = event->pos().x();
    int y_pos = event->pos().y();

    float x_val = this->xAxis->pixelToCoord(x_pos);
    float y_val = this->yAxis->pixelToCoord(y_pos);
//    qDebug()<<x_val;
    emit datosCursor(x_val);

    if(m_isShowTracer)
    {
        if(Q_NULLPTR == m_xTracer)
            m_xTracer = new XxwTracer(this, XxwTracer::XAxisTracer);
        m_xTracer->updatePosition("T(s)",x_val, y_val);

        if(Q_NULLPTR == m_yTracer)
            m_yTracer = new XxwTracer(this, XxwTracer::YAxisTracer);
        m_yTracer->updatePosition("Ampl", x_val, y_val);

        int nTracerCount = m_dataTracers.count();
        int nGraphCount = graphCount();
        if(nTracerCount < nGraphCount)
        {
            for(int i = nTracerCount; i < nGraphCount; ++i)
            {
                XxwTracer *tracer = new XxwTracer(this, XxwTracer::DataTracer);
                m_dataTracers.append(tracer);
            }
        }
        else if(nTracerCount > nGraphCount)
        {
            for(int i = nGraphCount; i < nTracerCount; ++i)
            {
                XxwTracer *tracer = m_dataTracers[i];
                if(tracer)
                {
                    tracer->setVisible(false);
                }
            }
        }
        for (int i = 0; i < nGraphCount; ++i)
        {
            XxwTracer *tracer = m_dataTracers[i];
            if(!tracer)
                tracer = new XxwTracer(this, XxwTracer::DataTracer);
            tracer->setVisible(true);
            tracer->setPen(this->graph(i)->pen());
            tracer->setBrush(Qt::NoBrush);
            tracer->setLabelPen(this->graph(i)->pen());
            auto iter = this->graph(i)->data()->findBegin(x_val);
            double value = iter->mainValue();
            auto name = this->graph(i)->name();
            if(this->graph(i)->layer()->visible())
            {
                tracer->setVisible(true);
                tracer->updatePosition(name, x_val, value);
            }
            else
                tracer->setVisible(false);
        }

        if(Q_NULLPTR == m_lineTracer)
            m_lineTracer = new XxwTraceLine(this,XxwTraceLine::Both);
        m_lineTracer->updatePosition(x_val, y_val);

        this->replot();
    }
}

void CustomPlot::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_L:

        if(yAxis->scaleType() == QCPAxis::ScaleType::stLinear)
            yAxis->setScaleType(QCPAxis::ScaleType::stLogarithmic);
        else
            yAxis->setScaleType(QCPAxis::ScaleType::stLinear);

        rescaleAxes();
        replot();

        break;
    default:
        break;
    }

    QCustomPlot::keyPressEvent(event);
}

void CustomPlot::keyReleaseEvent(QKeyEvent *event)
{
    if(event->key() == Qt::Key_Control)
    {

    }

    QCustomPlot::keyReleaseEvent(event);
}

QSize CustomPlot::sizeHint() const
{
    return QSize(400, 300);
}

void CustomPlot::zoommas(QCustomPlot *customPlot)
{
    QCPRange range;
    range = customPlot->xAxis->range();
    customPlot->xAxis->scaleRange(0.85, (range.lower + range.upper) / 2);
//    customPlot->replot();
}

void CustomPlot::zoommenos(QCustomPlot *customPlot)
{
    QCPRange range;

    range = customPlot->xAxis->range();
    customPlot->xAxis->scaleRange(1.17647058823529, (range.lower + range.upper) / 2);

//    customPlot->replot();
}

void CustomPlot::legendItemClicked(QCPLegend *legend, QCPAbstractLegendItem *legendItem, QMouseEvent *event)
{
    Q_UNUSED(legend);
    Q_UNUSED(event);
    for (int i=0; i < this->graphCount(); ++i)//seleccionar grafica
    {
        QCPGraph *graph = this->graph(i);
        if(legendItem == this->legend->itemWithPlottable(graph))
        {
            graph->setSelection (QCPDataSelection(graph->data()->dataRange()));
        }
    }
}

void CustomPlot::graphClicked(QCPAbstractPlottable *plottable, int dataIndex)
{
    Q_UNUSED (dataIndex);
    QString message = QString("A seleccionado la curva '%1'.").arg(plottable->name())/*.arg(dataIndex).arg(dataValue)*/;

    QCPPlottableLegendItem *item = this->legend->itemWithPlottable(plottable);
    item->setSelected(true);
}


