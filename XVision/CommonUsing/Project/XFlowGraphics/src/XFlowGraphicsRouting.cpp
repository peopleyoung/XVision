#include "XFlowGraphicsRouting.h"
#include <QLineF>
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
#include <vector>

namespace {
constexpr qreal Clearance = 18;
QPointF stub(const QPointF &point, const QRectF &bounds)
{
    if(bounds.isEmpty()) return point;
    const QPointF delta=point-bounds.center();
    if(std::abs(delta.x())/bounds.width()>std::abs(delta.y())/bounds.height())
        return QPointF(delta.x()<0 ? bounds.left()-Clearance : bounds.right()+Clearance,point.y());
    return QPointF(point.x(),delta.y()<0 ? bounds.top()-Clearance : bounds.bottom()+Clearance);
}
bool clearSegment(const QPointF &a,const QPointF &b,const QList<QRectF> &obstacles)
{
    for(const QRectF &r:obstacles) {
        if(qFuzzyCompare(a.x()+1,b.x()+1)) {
            if(a.x()>r.left() && a.x()<r.right()
                    && qMax(a.y(),b.y())>r.top() && qMin(a.y(),b.y())<r.bottom()) return false;
        } else if(a.y()>r.top() && a.y()<r.bottom()
                  && qMax(a.x(),b.x())>r.left() && qMin(a.x(),b.x())<r.right()) return false;
    }
    return true;
}
void appendPoint(QVector<QPointF> &points,const QPointF &point)
{
    if(!points.isEmpty() && QLineF(points.last(),point).length()<0.001) return;
    if(points.size()>1) {
        const QPointF a=points[points.size()-2],b=points.last();
        if((qFuzzyCompare(a.x()+1,b.x()+1) && qFuzzyCompare(b.x()+1,point.x()+1))
                || (qFuzzyCompare(a.y()+1,b.y()+1) && qFuzzyCompare(b.y()+1,point.y()+1))) {
            // Preserve reversals (a port may have to leave a node and return).
            if(QPointF::dotProduct(b-a,point-b)>=0) points.removeLast();
        }
    }
    points.append(point);
}
}

QPainterPath makeOrthogonalPath(const QPointF &start,const QPointF &end,
                               const QRectF &sourceBounds,const QRectF &targetBounds)
{
    QPainterPath path(start);
    if(QLineF(start,end).length()<0.001) return path;
    const QPointF a=stub(start,sourceBounds),b=stub(end,targetBounds);
    QList<QRectF> obstacles;
    if(!sourceBounds.isEmpty()) obstacles.append(sourceBounds);
    if(!targetBounds.isEmpty()) obstacles.append(targetBounds);
    QVector<qreal> xs{a.x(),b.x(),(a.x()+b.x())/2},ys{a.y(),b.y(),(a.y()+b.y())/2};
    for(const QRectF &r:obstacles) {
        xs << r.left()-Clearance << r.right()+Clearance;
        ys << r.top()-Clearance << r.bottom()+Clearance;
    }
    auto unique=[](QVector<qreal> &values) {
        std::sort(values.begin(),values.end());
        values.erase(std::unique(values.begin(),values.end()),values.end());
    };
    unique(xs);unique(ys);
    const int width=xs.size(),height=ys.size(),states=width*height*3;
    auto point=[&](int node){return QPointF(xs[node%width],ys[node/width]);};
    const int begin=(ys.indexOf(a.y())*width+xs.indexOf(a.x()))*3;
    const int target=ys.indexOf(b.y())*width+xs.indexOf(b.x());
    QVector<qreal> distance(states,std::numeric_limits<qreal>::infinity());
    QVector<int> previous(states,-1);
    using Entry=std::pair<qreal,int>;
    std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> queue;
    distance[begin]=0;queue.push({0,begin});
    int finish=-1;
    while(!queue.empty()) {
        const auto current=queue.top();queue.pop();
        const int state=current.second,node=state/3,direction=state%3;
        if(current.first>distance[state]) continue;
        if(node==target){finish=state;break;}
        const int x=node%width,y=node/width;
        const int neighbors[]={x>0?node-1:-1,x+1<width?node+1:-1,
                               y>0?node-width:-1,y+1<height?node+width:-1};
        for(int next:neighbors) {
            if(next<0 || !clearSegment(point(node),point(next),obstacles)) continue;
            const int nextDirection=next/width==y?1:2,nextState=next*3+nextDirection;
            const qreal cost=distance[state]+QLineF(point(node),point(next)).length()
                    +(direction && direction!=nextDirection ? 12 : 0);
            if(cost<distance[nextState]) {
                distance[nextState]=cost;previous[nextState]=state;queue.push({cost,nextState});
            }
        }
    }
    QVector<QPointF> points{start};
    appendPoint(points,a);
    if(finish>=0) {
        QVector<QPointF> route;
        for(int state=finish;state>=0;state=previous[state]) route.prepend(point(state/3));
        for(const QPointF &p:route) appendPoint(points,p);
    } else {
        // Overlapping endpoint nodes can prevent an obstacle-free route.
        appendPoint(points,QPointF(a.x(),b.y()));appendPoint(points,b);
    }
    appendPoint(points,end);
    for(int i=1;i<points.size();++i) path.lineTo(points[i]);
    return path;
}
