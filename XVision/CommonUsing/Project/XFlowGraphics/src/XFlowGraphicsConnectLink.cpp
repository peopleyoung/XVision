#include <QGuiApplication>
#include <QApplication>
#include <QPalette>
#include "XFlowGraphicsConnectLink.h"
#include "XFlowGraphicsItem.h"
#include "XFlowGraphicsScene.h"
#include "XFlowGraphicsUtils.h"

#include <QUuid>
#include "XFlowGraphicsRouting.h"
#include <QFontMetricsF>
/*******************************/
//* [XFlowGraphicsConnectLinkPrivate]
/*******************************/
class XFlowGraphicsConnectLinkPrivate
{
    Q_DISABLE_COPY(XFlowGraphicsConnectLinkPrivate)
    Q_DECLARE_PUBLIC(XFlowGraphicsConnectLink)

public:
    XFlowGraphicsConnectLinkPrivate(XFlowGraphicsConnectLink *q):q_ptr(q)
    {
        linkingCirclePen=QPen(QColor(Qt::white));
        linkingCircleBrush=QBrush(QColor(50, 125, 255));
        linkingCircleRadius=4;


        linkingPen=QColor(25, 150, 255);
        linkingPen.setWidth(3);
        linkingPen.setStyle(Qt::SolidLine);

        linkedPen=QColor(25, 150, 255);
        linkedPen.setWidth(3);
        linkedPen.setStyle(Qt::SolidLine);

        linkSelectedPen=QColor("#69C9FF");
        linkSelectedPen.setWidth(5);
        linkSelectedPen.setStyle(Qt::SolidLine);


        selectBoundingRectPen.setColor(QColor(255, 255, 255));
        selectBoundingRectPen.setWidth(1);
        selectBoundingRectPen.setStyle(Qt::DashLine);

        arrowSize=15;

        highLightPen.setColor(QColor("#69C9FF"));
        highLightPen.setWidth(3);
        highLightPen.setStyle(Qt::SolidLine);

        textPen = QPen();
        textPen.setColor(QColor(255, 255, 255));
        textPen.setWidth(1);
        textFont = QFont("YouYuan", 12, 2);
        textFont.setBold(false);
        applyPalette(QApplication::palette());
        QObject::connect(qGuiApp,&QGuiApplication::paletteChanged,q_ptr,[this](const QPalette &palette) {
            applyPalette(palette); q_ptr->update();
        });
    };
    void applyPalette(const QPalette &palette) {
        linkingCirclePen.setColor(palette.color(QPalette::Text));
        linkingCircleBrush.setColor(palette.color(QPalette::Link));
        linkingPen.setColor(palette.color(QPalette::Link));
        linkedPen.setColor(palette.color(QPalette::Link));
        linkSelectedPen.setColor(palette.color(QPalette::Link));
        selectBoundingRectPen.setColor(palette.color(QPalette::Text));
        highLightPen.setColor(palette.color(QPalette::Link));
        textPen.setColor(palette.color(QPalette::Text));
    }
    virtual ~XFlowGraphicsConnectLinkPrivate(){};

    XFlowGraphicsConnectLink                *const q_ptr;

    ///连接拖动圆形画笔
    QPen                                linkingCirclePen;
    ///连接拖动圆形笔刷
    QBrush                              linkingCircleBrush;
    ///连接拖动圆半径
    double                              linkingCircleRadius;


    ///正在连线画笔
    QPen                                linkingPen;

    ///连线完毕画笔
    QPen                                linkedPen;

    ///连线被选择时
    QPen                                linkSelectedPen;

    ///选中时边框画笔
    QPen                                selectBoundingRectPen;

    ///末端箭头大小
    double                              arrowSize=15;

    ///连线高亮时的画笔
    QPen                                highLightPen;

    ///Link文本画笔
    QPen                                textPen;
    ///Link文本字体
    QFont                               textFont;

};

/****************************构建与析构****************************/

XFlowGraphicsConnectLink::XFlowGraphicsConnectLink(QObject *parent)
    :QObject{parent},   m_LinkId(QUuid::createUuid().toString(QUuid::Id128)),
    m_ptFatherStart(QPointF(0,0)),m_ptSonEnd(QPointF(0,0)),d_ptr(new XFlowGraphicsConnectLinkPrivate(this))
{
    if(parent)
    {
        auto scene=qobject_cast<XFlowGraphicsScene*>(parent);
        if(scene)
        {
            m_parScene=scene;
        }
    }
    initConnectLink();
}

XFlowGraphicsConnectLink::XFlowGraphicsConnectLink(QPointF ptStart, QPointF ptEnd,QObject *parent)
    :QObject{parent},   m_LinkId(QUuid::createUuid().toString(QUuid::Id128)),
      m_ptFatherStart(ptStart),m_ptSonEnd(ptEnd),d_ptr(new XFlowGraphicsConnectLinkPrivate(this))
{
    if(parent)
    {
        auto scene=qobject_cast<XFlowGraphicsScene*>(parent);
        if(scene)
        {
            m_parScene=scene;
        }
    }
    initConnectLink();
}

XFlowGraphicsConnectLink::XFlowGraphicsConnectLink(XFlowGraphicsItem *xItemFather, const QString &fatherKey,
                                           XFlowGraphicsItem *xItemSon, const QString &sonKey, QObject *parent)
    :QObject{parent},   m_LinkId(QUuid::createUuid().toString(QUuid::Id128)),d_ptr(new XFlowGraphicsConnectLinkPrivate(this))
{
    if(parent)
    {
        auto scene=qobject_cast<XFlowGraphicsScene*>(parent);
        if(scene)
        {
            m_parScene=scene;
        }
    }

    if(!setFatherXItemKey(xItemFather,fatherKey))
    {
        m_ptFatherStart=QPointF(0,0);
    }
    if(!setSonXItemKey(xItemSon,sonKey))
    {
         m_ptSonEnd=QPointF(0,0);
    }
    initConnectLink();
}

XFlowGraphicsConnectLink::~XFlowGraphicsConnectLink()
{
    if(m_fatherXItem)
    {
        m_fatherXItem->removeSonConnect(this);
    }
    if(m_sonXItem)
    {
        m_sonXItem->removeFatherConnect(this);
    }
}
/***************************属性接口***************************/

QPen XFlowGraphicsConnectLink::linkingCirclePen() const
{
    Q_D(const XFlowGraphicsConnectLink);
    return d->linkingCirclePen;
}

void XFlowGraphicsConnectLink::setLinkingCirclePen(const QPen &pen)
{
    Q_D(XFlowGraphicsConnectLink);
    d->linkingCirclePen=pen;
}


QBrush XFlowGraphicsConnectLink::linkingCircleBrush() const
{
    Q_D(const XFlowGraphicsConnectLink);
    return d->linkingCircleBrush;
}

void XFlowGraphicsConnectLink::setLinkingCircleBrush(const  QBrush &brush)
{
    Q_D(XFlowGraphicsConnectLink);
    d->linkingCircleBrush=brush;
}

double XFlowGraphicsConnectLink::linkingCircleRadius() const
{
    Q_D(const XFlowGraphicsConnectLink);
    return d->linkingCircleRadius;
}

void XFlowGraphicsConnectLink::setLinkingCircleRadius(const double &radius)
{
    Q_D(XFlowGraphicsConnectLink);
    d->linkingCircleRadius=radius;
}

QPen XFlowGraphicsConnectLink::linkingPen() const
{
    Q_D(const XFlowGraphicsConnectLink);
    return d->linkingPen;
}

void XFlowGraphicsConnectLink::setLinkingPen(const QPen &pen)
{
    Q_D(XFlowGraphicsConnectLink);
    d->linkingPen=pen;
}

QPen XFlowGraphicsConnectLink::linkedPen() const
{
    Q_D(const XFlowGraphicsConnectLink);
    return d->linkedPen;
}

void XFlowGraphicsConnectLink::setLinkedPen(const QPen &pen)
{
    Q_D(XFlowGraphicsConnectLink);
    d->linkedPen=pen;
}

QPen XFlowGraphicsConnectLink::linkSelectedPen() const
{
    Q_D(const XFlowGraphicsConnectLink);
    return d->linkSelectedPen;
}

void XFlowGraphicsConnectLink::setLinkSelectedPen(const QPen &pen)
{
    Q_D(XFlowGraphicsConnectLink);
    d->linkSelectedPen=pen;
}


QPen XFlowGraphicsConnectLink::selectBoundingRectPen() const
{
    Q_D(const XFlowGraphicsConnectLink);
    return d->selectBoundingRectPen;
}

void XFlowGraphicsConnectLink::setSelectBoundingRectPen(const QPen &pen)
{
    Q_D(XFlowGraphicsConnectLink);
    d->selectBoundingRectPen=pen;
}

double XFlowGraphicsConnectLink::arrowSize() const
{
    Q_D(const XFlowGraphicsConnectLink);
    return d->arrowSize;
}

void XFlowGraphicsConnectLink::setArrowSize(const double &size)
{
    Q_D(XFlowGraphicsConnectLink);
    d->arrowSize=size;
    updateXLink();
}

QPen XFlowGraphicsConnectLink::highLightPen() const
{
    Q_D(const XFlowGraphicsConnectLink);
    return d->highLightPen;
}

void XFlowGraphicsConnectLink::setHighLightPen(const QPen &pen)
{
    Q_D(XFlowGraphicsConnectLink);
    d->highLightPen=pen;
}

QPen XFlowGraphicsConnectLink::textPen() const
{
    Q_D(const XFlowGraphicsConnectLink);
    return d->textPen;
}

void XFlowGraphicsConnectLink::setTextPen(const QPen &pen)
{
    Q_D(XFlowGraphicsConnectLink);
    d->textPen=pen;
}

QFont XFlowGraphicsConnectLink::textFont() const
{
    Q_D(const XFlowGraphicsConnectLink);
    return d->textFont;
}

void XFlowGraphicsConnectLink::setTextFont(const QFont &font)
{
    Q_D(XFlowGraphicsConnectLink);
    d->textFont=font;
    updateXLink();
}


/***************************初始化接口***************************/
void XFlowGraphicsConnectLink::initConnectLink()
{
    setFlag(QGraphicsItem::ItemIsSelectable, true);
    setFlag(QGraphicsItem::ItemSendsGeometryChanges, true);

    updateXLink();
}


/***************************槽函数***************************/

void XFlowGraphicsConnectLink::onXItemUpdate()
{
    onFatherXItemUpdate();
    onSonXItemUpdate();
}


void XFlowGraphicsConnectLink::onFatherXItemDestroyed()
{
    m_fatherXItem=nullptr;
    emit xLinkError();
}

void XFlowGraphicsConnectLink::onSonXItemDestroyed()
{
    m_sonXItem=nullptr;
    emit xLinkError();
}

void XFlowGraphicsConnectLink::onFatherXItemUpdate()
{
    if(!m_fatherXItem) return;
    SConnectData data;
    if(!m_fatherXItem->getSonConnectData(this,data))
    {
        emit xLinkError();
    }
    m_ptFatherStart=data.pt;
    updateXLink();
}

void XFlowGraphicsConnectLink::onSonXItemUpdate()
{
    if(!m_sonXItem) return;
    SConnectData data;
    if(!m_sonXItem->getFatherConnectData(this,data))
    {
        emit xLinkError();
    }
    m_ptSonEnd=data.pt;
    updateXLink();
}


void XFlowGraphicsConnectLink::updateXLink()
{
    prepareGeometryChange();
    // Keep the legacy endpoint line for consumers; all visible geometry uses the path.
    setLine(QLineF(m_ptSonEnd,m_ptFatherStart));
    const auto bounds=[](XFlowGraphicsItem *node) {
        return node && node->item() ? node->item()->sceneBoundingRect() : QRectF();
    };
    m_connectionPath=makeOrthogonalPath(m_ptFatherStart,m_ptSonEnd,bounds(m_fatherXItem),bounds(m_sonXItem));
    updateArrow();
    const QPointF center=m_connectionPath.pointAtPercent(0.5);
    const QRectF textBounds=QFontMetricsF(textFont()).boundingRect(m_LinkText);
    m_LinkRectText=QRectF(center-QPointF(textBounds.width()/2,textBounds.height()/2),textBounds.size());
    update();
}

/***************************XLink数据***************************/

void XFlowGraphicsConnectLink::setParScene(XFlowGraphicsScene *parScene)
{
    m_parScene=parScene;
}


bool XFlowGraphicsConnectLink::setFatherXItemKey(XFlowGraphicsItem *xItem, const QString &key)
{
    if(xItem==nullptr) return false;
    SConnectData data;
    if(!xItem->getConnectData(key,data))
    {
        return false;//设置失败
    }
    //设置成功 更新startItem
    if(m_fatherXItem)
    {
        m_fatherXItem->removeSonConnect(this);
        disconnect(m_fatherXItem,&XFlowGraphicsItem::destroyed,this,&XFlowGraphicsConnectLink::onFatherXItemDestroyed);
        disconnect(m_fatherXItem,&XFlowGraphicsItem::posChanged,this,&XFlowGraphicsConnectLink::onFatherXItemUpdate);
        disconnect(m_fatherXItem,&XFlowGraphicsItem::shapeChanged,this,&XFlowGraphicsConnectLink::onFatherXItemUpdate);
        m_fatherXItem=nullptr;
    } 

    m_fatherXItem=xItem;
    m_fatherConnKey=key;
    m_ptFatherStart=data.pt;
    m_fatherXItem->addSonConnect(this);
    connect(m_fatherXItem,&XFlowGraphicsItem::destroyed,this,&XFlowGraphicsConnectLink::onFatherXItemDestroyed);
    connect(m_fatherXItem,&XFlowGraphicsItem::posChanged,this,&XFlowGraphicsConnectLink::onFatherXItemUpdate);
    connect(m_fatherXItem,&XFlowGraphicsItem::shapeChanged,this,&XFlowGraphicsConnectLink::onFatherXItemUpdate);
    updateXLink();
    return true;
}

bool XFlowGraphicsConnectLink::setSonXItemKey(XFlowGraphicsItem *xItem, const QString &key)
{
    if(xItem==nullptr) return false;
    SConnectData data;
    if(!xItem->getConnectData(key,data))
    {
        return false;//设置失败
    }
    //设置成功 更新startItem
    if(m_sonXItem)
    {
        m_sonXItem->removeFatherConnect(this);
        disconnect(m_sonXItem,&XFlowGraphicsItem::destroyed,this,&XFlowGraphicsConnectLink::onSonXItemDestroyed);
        disconnect(m_sonXItem,&XFlowGraphicsItem::posChanged,this,&XFlowGraphicsConnectLink::onSonXItemUpdate);
        disconnect(m_sonXItem,&XFlowGraphicsItem::shapeChanged,this,&XFlowGraphicsConnectLink::onSonXItemUpdate);
        m_sonXItem=nullptr;
    }
    m_sonXItem=xItem;
    m_sonConnKey=key;
    m_ptSonEnd=data.pt;
    m_sonXItem->addFatherConnect(this);
    connect(m_sonXItem,&XFlowGraphicsItem::destroyed,this,&XFlowGraphicsConnectLink::onSonXItemDestroyed);
    connect(m_sonXItem,&XFlowGraphicsItem::posChanged,this,&XFlowGraphicsConnectLink::onSonXItemUpdate);
    connect(m_sonXItem,&XFlowGraphicsItem::shapeChanged,this,&XFlowGraphicsConnectLink::onSonXItemUpdate);
    updateXLink();
    return true;
}

bool XFlowGraphicsConnectLink::setFatherStartPos(const QPointF &pt)
{
   if(m_fatherXItem) return false;//存在起始Item不可设置
   m_ptFatherStart=pt;
   updateXLink();
   return true;
}

bool XFlowGraphicsConnectLink::setSonEndPos(const QPointF &pt)
{
    if(m_sonXItem) return false;//存在结束Item不可设置
    m_ptSonEnd=pt;
    updateXLink();
    return true;
}

bool XFlowGraphicsConnectLink::setFatherKey(const QString &key)
{
    if(!m_fatherXItem) return false;//不存在开始Item不可设置
    if(!m_fatherXItem->hasConnectKey(key)) return false; //父Item中不存在该Key连接点 不可设置
    SConnectData data;
    if(!m_fatherXItem->getConnectData(key,data))
    {
        return false;//设置失败
    }
    m_fatherConnKey=key;
    m_ptFatherStart=data.pt;
    updateXLink();
    return true;
}

bool XFlowGraphicsConnectLink::setSonKey(const QString &key)
{
    if(!m_sonXItem) return false;//不存在结束Item不可设置
    if(!m_sonXItem->hasConnectKey(key)) return false; //子Item中不存在该Key连接点 不可设置
    SConnectData data;
    if(!m_sonXItem->getConnectData(key,data))
    {
        return false;//设置失败
    }
    m_sonConnKey=key;
    m_ptSonEnd=data.pt;
    updateXLink();
    return true;
}

void XFlowGraphicsConnectLink::setLinkState(bool isLinked)
{
    m_bLinked=isLinked;
}

void XFlowGraphicsConnectLink::setHighLight(bool highLight, bool bUpdate)
{
    m_bHighLight=highLight;
    if(bUpdate)
    {
        this->update();
    }
}


/***************************重写父类接口***************************/

QRectF XFlowGraphicsConnectLink::boundingRect() const
{
    QRectF bounds=m_connectionPath.boundingRect().united(m_polyArrowHead.boundingRect());
    if(!m_LinkText.isEmpty()) bounds=bounds.united(m_LinkRectText);
    return bounds.adjusted(-16,-16,16,16);
}

QPainterPath XFlowGraphicsConnectLink::shape() const
{
    QPainterPathStroker stroker;
    stroker.setWidth(16);
    stroker.setJoinStyle(Qt::RoundJoin);
    QPainterPath hit=stroker.createStroke(m_connectionPath);
    hit.addPolygon(m_polyArrowHead);
    return hit;
}

void XFlowGraphicsConnectLink::paint(QPainter *painter, const QStyleOptionGraphicsItem *option, QWidget *widget)
{
    Q_D(XFlowGraphicsConnectLink);

    painter->save();
    if(m_bLinked) //已经连接
    {

        if(isSelected())
        {
            painter->setPen(d->linkSelectedPen);
            painter->setBrush(d->linkSelectedPen.color());
        }
        else
        {
            painter->setPen(d->linkedPen);
            painter->setBrush(d->linkedPen.color());
        }
        if(m_bHighLight)
        {
            painter->setPen(d->highLightPen);
            painter->setBrush(d->highLightPen.color());
        }
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(m_connectionPath);
        painter->setBrush(painter->pen().color());
        painter->drawPolygon(m_polyArrowHead);


    }
    else //未连接时
    {
       painter->setPen(d->linkingPen);
       painter->setBrush(d->linkingPen.color());
       painter->setBrush(Qt::NoBrush);
        painter->drawPath(m_connectionPath);
        painter->setBrush(painter->pen().color());
       painter->drawPolygon(m_polyArrowHead);
       painter->setPen(d->linkingCirclePen);
       painter->setBrush(d->linkingCircleBrush);
       double radius=d->linkingCircleRadius;
       painter->drawEllipse(this->sonEndPos(),radius,radius);
    }


    if(!m_LinkText.isEmpty())
    {
        drawLinkText(painter,m_LinkText);
    }

    painter->restore();


}



/***************************内部调用接口***************************/
void XFlowGraphicsConnectLink::updateArrow()
{
    m_polyArrowHead.clear();
    if(m_connectionPath.elementCount()<2) return;
    const auto last=m_connectionPath.elementAt(m_connectionPath.elementCount()-1);
    const auto before=m_connectionPath.elementAt(m_connectionPath.elementCount()-2);
    const QPointF tip(last.x,last.y),previous(before.x,before.y);
    const qreal length=QLineF(previous,tip).length();
    if(length<0.001) return;
    const QPointF direction=(tip-previous)/length,normal(-direction.y(),direction.x());
    const qreal size=qMin(arrowSize(),length);
    m_polyArrowHead << tip << tip-direction*size+normal*size*0.45
                    << tip-direction*size-normal*size*0.45;
}

void XFlowGraphicsConnectLink::drawLinkText(QPainter *painter, const QString &text)
{
    painter->save();
    painter->setFont(textFont());
    QFontMetrics fontMetrics = painter->fontMetrics();
    painter->setPen(textPen());
    QRect rect = fontMetrics.boundingRect(text);
    QPointF pos=m_LinkRectText.center()-QPointF(rect.width()/2,rect.height()/2);
    rect.moveTo(pos.toPoint());
    painter->drawText(rect,text);
    painter->restore();
}

/***************************XLink形状外观***************************/

double XFlowGraphicsConnectLink::linkLength()
{
   return m_connectionPath.length();
}


void XFlowGraphicsConnectLink::refreshThemePalette() {
    Q_D(XFlowGraphicsConnectLink);
    d->applyPalette(QApplication::palette());
    update();
}
