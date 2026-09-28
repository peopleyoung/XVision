#ifndef XROTATERECTROI_H
#define XROTATERECTROI_H

#include "XObject.h"

#define XRotateRectRoiType "XRotateRectRoi"

class XVDATA_EXPORT XRotateRectRoi:public XObject
{
public:
    XRotateRectRoi(const QString &objectName,double centerX=0.0,double centerY=0.0,
                   double length1=1.0,double length2=1.0,double angle=0.0,
                   XObjectSet *parObjectSet=nullptr,const QString &displayName="");
    XRotateRectRoi(double centerX,double centerY,double length1,double length2,
                   double angle=0.0);
    XRotateRectRoi();

    static QString type() { return XRotateRectRoiType; }
    QString typeName() override { return XRotateRectRoiType; }
    XObject *clone() override;
    bool getData(XObject *object) override;
    bool setData(XObject *object) override;

    bool setValue(double centerX,double centerY,double length1,double length2,
                  double angle);
    double centerX() const { return m_centerX; }
    double centerY() const { return m_centerY; }
    double length1() const { return m_length1; }
    double length2() const { return m_length2; }
    double angle() const { return m_angle; }

private:
    double m_centerX=0.0;
    double m_centerY=0.0;
    double m_length1=1.0;
    double m_length2=1.0;
    double m_angle=0.0;
};

#endif // XROTATERECTROI_H
