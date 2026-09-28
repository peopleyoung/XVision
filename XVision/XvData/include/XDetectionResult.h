#ifndef XDETECTIONRESULT_H
#define XDETECTIONRESULT_H

#include "XObject.h"

#define XDetectionResultType "XDetectionResult"

class XVDATA_EXPORT XDetectionResult : public XObject
{
public:
    XDetectionResult(const QString &objectName,double x=0.0,double y=0.0,
                     double width=1.0,double height=1.0,int classId=0,
                     const QString &className="Class 0",double score=0.0,
                     XObjectSet *parObjectSet=nullptr,
                     const QString &displayName="");
    XDetectionResult(double x,double y,double width,double height,int classId,
                     const QString &className,double score);
    XDetectionResult();

    static QString type() { return XDetectionResultType; }
    QString typeName() override { return XDetectionResultType; }
    XObject *clone() override;
    bool getData(XObject *object) override;
    bool setData(XObject *object) override;

    bool setValue(double x,double y,double width,double height,int classId,
                  const QString &className,double score);
    double x() const { return m_x; }
    double y() const { return m_y; }
    double width() const { return m_width; }
    double height() const { return m_height; }
    int classId() const { return m_classId; }
    QString className() const { return m_className; }
    double score() const { return m_score; }

private:
    double m_x=0.0;
    double m_y=0.0;
    double m_width=1.0;
    double m_height=1.0;
    int m_classId=0;
    QString m_className="Class 0";
    double m_score=0.0;
};

#endif // XDETECTIONRESULT_H
