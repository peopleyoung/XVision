#ifndef OPENCVIMAGEOPERATORBASE_H
#define OPENCVIMAGEOPERATORBASE_H

#include "XVFuncSystemGlobal.h"
#include "XImage.h"
#include "XvFunc.h"

class OpenCvImageOperatorWdg;

namespace XvCore
{
class OpenCvImageParamBase:public XvBaseParam
{
public:
    OpenCvImageParamBase()
    {
        inputImage=new XImage("inputImage",QImage(),this,"输入图像");
    }

    XImage *inputImage=nullptr;
};

class OpenCvImageResultBase:public XvBaseResult
{
public:
    OpenCvImageResultBase()
    {
        outputImage=new XImage("outputImage",QImage(),this,"输出图像");
    }

    XImage *outputImage=nullptr;
};

class XVFUNCSYSTEM_EXPORT OpenCvImageOperatorBase:public XvFunc
{
    Q_OBJECT
    friend class ::OpenCvImageOperatorWdg;
public:
    explicit OpenCvImageOperatorBase(QObject *parent=nullptr);
    ~OpenCvImageOperatorBase() override;

    QStringList persistentPropertyNames() const override { return {"mode"}; }
    virtual QStringList activeParameterNames() const=0;

public slots:
    void onShowFunc() override;

protected:

    EXvFuncRunStatus run() final;

    virtual bool processImage(const QImage &source,QImage &candidate,QString &error)=0;
    virtual bool commitAdditionalResults(QString &error);
    virtual void clearResultsAfterFailure();

private:
    OpenCvImageOperatorWdg *m_widget=nullptr;
};
}

#endif // OPENCVIMAGEOPERATORBASE_H
