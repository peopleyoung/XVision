#ifndef OIMAGEARITHMETIC_H
#define OIMAGEARITHMETIC_H

#include "OpenCvImageOperatorBase.h"
#include <XObjectBaseType>

namespace XvCore
{
class OImageArithmeticParam:public OpenCvImageParamBase
{
public:
    OImageArithmeticParam()
    {
        value=new XReal("value",1.0,this,"Value");
        useAbsolute=new XBool("useAbsolute",false,this,"Use absolute difference");
    }

    XReal *value=nullptr;
    XBool *useAbsolute=nullptr;
};

class XVFUNCSYSTEM_EXPORT OImageArithmetic:public OpenCvImageOperatorBase
{
    Q_OBJECT
    Q_PROPERTY(OImageArithmetic::Mode mode READ mode WRITE setMode)
public:
    enum Mode { AddSubtract=0,BitwiseNot=1,MultiplyDivide=2,Pow=3 };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit OImageArithmetic(QObject *parent=nullptr);
    ~OImageArithmetic() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList activeParameterNames() const override;

protected:
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool processImage(const QImage &source,QImage &candidate,QString &error) override;

private:
    OImageArithmeticParam *m_param=nullptr;
    OpenCvImageResultBase *m_result=nullptr;
    Mode m_mode=AddSubtract;
};
}

#endif // OIMAGEARITHMETIC_H
