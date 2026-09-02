#include "flexibleheaderview.h"

#include <algorithm>



FlexibleHeaderView::FlexibleHeaderView(Qt::Orientation orientation, QWidget *parent) :
    QHeaderView(orientation, parent)
{
    connect(this, &QHeaderView::sectionResized,
            this, &FlexibleHeaderView::handleSectionResize);
}



void FlexibleHeaderView::setSectionProportion(int logicalIndex, double proportion)
{
    if (proportion < 0) {
        proportions.remove(logicalIndex);
    } else {
        proportions[logicalIndex] = proportion;
    }
}



double FlexibleHeaderView::sectionProportion(int logicalIndex) {
    if (proportions.contains(logicalIndex)) {
        return proportions[logicalIndex];
    }

    return -1;
}



void FlexibleHeaderView::resizeEvent(QResizeEvent *event)
{
    fitProportions();

    QHeaderView::resizeEvent(event);
}



void FlexibleHeaderView::fitProportions()
{
    double total = 0;
    for (QHash<int, double>::const_iterator i = proportions.constBegin();
         i != proportions.constEnd(); ++i) {
        if (!isSectionHidden(i.key())) {
            total += i.value();
        }
    }

    if (total == 0) {
        return;
    }

    std::for_each(proportions.begin(), proportions.end(),
                  [ total ] (double &n) { n /= total; });

    int usedSpace = 0;
    for (int i = 0; i < count(); ++i) {
        if (!proportions.contains(i)) {
            usedSpace += sectionSize(i);
        }
    }

    int freeSpace = viewport()->width() - usedSpace;

    blockSignals(true);

    int overflow = 0;
    for (QHash<int, double>::const_iterator i = proportions.constBegin();
         i != proportions.constEnd(); ++i) {

        int newSize = static_cast<int>(i.value() * freeSpace);
        overflow += std::max(0, minimumSectionSize() - newSize);

        resizeSection(i.key(), std::max(minimumSectionSize(), newSize));
    }

    for (QHash<int, double>::const_iterator j = proportions.constBegin();
         overflow > 0 && j != proportions.constEnd(); ++j) {
        int size = sectionSize(j.key());
        int delta = size - minimumSectionSize();

        if (delta > 0) {
            resizeSection(j.key(), size - std::min(delta, overflow));
            overflow -= delta;
        }
    }

    blockSignals(false);

    model()->headerDataChanged(orientation(), 0, count() - 1);
}



void FlexibleHeaderView::handleSectionResize(int logicalIndex, int oldSize, int newSize)
{
    if (newSize == 0 || oldSize == 0) {
        QMetaObject::invokeMethod(this, "fitProportions", Qt::QueuedConnection);
        return;
    }

    blockSignals(true);

    int totalSize = 0;
    for (QHash<int, double>::const_iterator i = proportions.constBegin();
         i != proportions.constEnd(); ++i) {
        totalSize += sectionSize(i.key());
    }

    int nextShift = 1;
    int nextIndex = -1;
    do {
        nextIndex = this->logicalIndex(visualIndex(logicalIndex) + nextShift++);
    } while (isSectionHidden(nextIndex));

    int nextSize = (nextIndex == -1) ? -1 : sectionSize(nextIndex) - (newSize - oldSize);
    if (nextSize >= minimumSectionSize()) {
        if (proportions.contains(nextIndex)) {
            if (proportions.contains(logicalIndex)) {
                double newProportion = newSize * 1.0 / totalSize;

                proportions[nextIndex] += proportions[logicalIndex] - newProportion;
                proportions[logicalIndex] = newProportion;
            } else {
                proportions[nextIndex] = nextSize * 1.0 / totalSize;
            }

            fitProportions();
        }
        else {
            if (proportions.contains(logicalIndex)) {
                resizeSection(nextIndex, nextSize);

                for (int i = 0; i < count(); ++i) {
                    if (!isSectionHidden(i) && proportions.contains(i)) {
                        proportions[i] = sectionSize(i) * 1.0 / totalSize;
                    }
                }

                fitProportions();
            }
            else {
                resizeSection(nextIndex, nextSize);
            }
        }
    }
    else if (totalSize > 0) {
        resizeSection(logicalIndex, oldSize);
    }

    blockSignals(false);
}
