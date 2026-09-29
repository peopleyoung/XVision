#ifndef XFLOWGRAPHICSROUTING_H
#define XFLOWGRAPHICSROUTING_H
#include "XFlowGraphicsGlobal.h"
#include <QRectF>
// Routes around the two endpoint nodes; unrelated nodes are not obstacles.
XFLOWGRAPHICS_PUBLIC QPainterPath makeOrthogonalPath(
        const QPointF &start, const QPointF &end,
        const QRectF &sourceBounds = QRectF(), const QRectF &targetBounds = QRectF());
#endif
