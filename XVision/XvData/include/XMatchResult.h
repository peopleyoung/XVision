#ifndef XMATCHRESULT_H
#define XMATCHRESULT_H

#include "XObject.h"

#define XMatchResultType "XMatchResult"

class XVDATA_EXPORT XMatchResult:public XObject
{
public:
    XMatchResult(const QString &objectName,double x=0.0,double y=0.0,
                 double angle=0.0,double score=0.0,
                 XObjectSet *parObjectSet=nullptr,const QString &displayName="");
    XMatchResult(double x,double y,double angle,double score);
    XMatchResult();

    static QString type() { return XMatchResultType; }
    QString typeName() override { return XMatchResultType; }
    XObject *clone() override;
    bool getData(XObject *object) override;
    bool setData(XObject *object) override;

    bool setValue(double x,double y,double angle,double score);
    double x() const { return m_x; }
    double y() const { return m_y; }
    double angle() const { return m_angle; }
    double score() const { return m_score; }

private:
    double m_x=0.0;
    double m_y=0.0;
    double m_angle=0.0;
    double m_score=0.0;
};

#endif // XMATCHRESULT_H
