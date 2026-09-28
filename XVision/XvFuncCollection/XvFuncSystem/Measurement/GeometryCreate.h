#ifndef GEOMETRYCREATE_H
#define GEOMETRYCREATE_H

#include "GeometryOperatorBase.h"

#include "XPoint2D.h"
#include "XVisionSharedData.h"
#include <XObjectBaseType>

namespace XvCore
{
class GeometryCreateParam:public XvBaseParam
{
public:
    GeometryCreateParam();

    XReal *pointX=nullptr;
    XReal *pointY=nullptr;
    XReal *lineX1=nullptr;
    XReal *lineY1=nullptr;
    XReal *lineX2=nullptr;
    XReal *lineY2=nullptr;
    XReal *circleCenterX=nullptr;
    XReal *circleCenterY=nullptr;
    XReal *circleRadius=nullptr;
    XReal *rectangleX=nullptr;
    XReal *rectangleY=nullptr;
    XReal *rectangleWidth=nullptr;
    XReal *rectangleHeight=nullptr;
};

class GeometryCreateResult:public XvBaseResult
{
public:
    GeometryCreateResult();

    XPoint2D *point=nullptr;
    XLine2D *line=nullptr;
    XCircle2D *circle=nullptr;
    XRect2D *rectangle=nullptr;
    XInt *createdType=nullptr;
    XBool *created=nullptr;
};

class XVFUNCSYSTEM_EXPORT GeometryCreate:public GeometryOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(ShapeType shapeType READ shapeType WRITE setShapeType)
public:
    enum ShapeType { Point=0,Line=1,Circle=2,Rectangle=3 };
    Q_ENUM(ShapeType)

    Q_INVOKABLE explicit GeometryCreate(QObject *parent=nullptr);
    ~GeometryCreate() override;

    ShapeType shapeType() const { return m_shapeType; }
    void setShapeType(ShapeType type) { m_shapeType=type; }
    QString selectorPropertyName() const override { return "shapeType"; }
    QStringList activeParameterNames() const override;
    QStringList persistentPropertyNames() const override { return {"shapeType"}; }

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    EXvFuncRunStatus run() override;

private:
    GeometryCreateParam *m_param=nullptr;
    GeometryCreateResult *m_result=nullptr;
    ShapeType m_shapeType=Point;
};
}

#endif // GEOMETRYCREATE_H
