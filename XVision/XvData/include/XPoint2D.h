#ifndef XPOINT2D_H
#define XPOINT2D_H

#include "XObject.h"

#define XPoint2DType "XPoint2D"

class XVDATA_EXPORT XPoint2D:public XObject
{
public:
    XPoint2D(const QString &objectName,double x=0.0,double y=0.0,
             XObjectSet *parObjectSet=nullptr,const QString &displayName="");
    XPoint2D(double x,double y);
    XPoint2D();

    static QString type() { return XPoint2DType; }
    QString typeName() override { return XPoint2DType; }
    XObject *clone() override;
    bool getData(XObject *object) override;
    bool setData(XObject *object) override;

    bool setValue(double x,double y);
    double x() const { return m_x; }
    double y() const { return m_y; }

private:
    double m_x=0.0;
    double m_y=0.0;
};

#endif // XPOINT2D_H
