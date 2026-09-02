#ifndef FLEXIBLEHEADERVIEW_H
#define FLEXIBLEHEADERVIEW_H

#include <QHeaderView>
#include <QHash>
#include <QResizeEvent>



class FlexibleHeaderView : public QHeaderView
{
    Q_OBJECT

public:
    explicit FlexibleHeaderView(Qt::Orientation orientation, QWidget *parent = Q_NULLPTR);

    void setSectionProportion(int logicalIndex, double proportion);
    double sectionProportion(int logicalIndex);

protected:
    void resizeEvent(QResizeEvent *event);

protected slots:
    void fitProportions();
    void handleSectionResize(int logicalIndex, int oldSize, int newSize);

private:
    QHash<int, double> proportions;
};

#endif // FLEXIBLEHEADERVIEW_H
