#include "OpenCvImageOperatorBase.h"
#include "OpenCvImageOperatorWdg.h"
#include <XObjectBaseType>

#include <QtGlobal>

using namespace XvCore;

OpenCvImageOperatorBase::OpenCvImageOperatorBase(QObject *parent)
    :XvFunc(parent)
{
    _funcType=EXvFuncType::ImageProcessing;
}

OpenCvImageOperatorBase::~OpenCvImageOperatorBase()
{
    delete m_widget;
}

QPixmap OpenCvImageOperatorBase::funcIcon()
{
    return QPixmap(":/images/OpenCvImage.svg");
}

EXvFuncRunStatus OpenCvImageOperatorBase::run()
{
    auto source=getParamsByName("inputImage");
    auto output=getResultsByName("outputImage");
    auto inputImage=source?dynamic_cast<XImage*>(source):nullptr;
    auto outputImage=output?dynamic_cast<XImage*>(output):nullptr;
    if(!inputImage || !outputImage)
    {
        clearResultsAfterFailure();
        setRunMsg("Image operator input/output contract is unavailable");
        return EXvFuncRunStatus::Error;
    }
    if(inputImage->value().isNull())
    {
        clearResultsAfterFailure();
        setRunMsg("Input image is empty");
        return EXvFuncRunStatus::Error;
    }
    for(const QString &name:activeParameterNames())
    {
        auto value=dynamic_cast<XReal*>(getParamsByName(name));
        if(value && !qIsFinite(value->value()))
        {
            clearResultsAfterFailure();
            setRunMsg(QString("Parameter '%1' must be finite").arg(name));
            return EXvFuncRunStatus::Error;
        }
    }

#if !defined(XVISION_ENABLE_OPENCV)
    clearResultsAfterFailure();
    setRunMsg("OpenCV backend is disabled; configure with XVISION_ENABLE_OPENCV=ON");
    return EXvFuncRunStatus::Error;
#else
    QImage candidate;
    QString error;
    if(!processImage(inputImage->value(),candidate,error) || candidate.isNull())
    {
        clearResultsAfterFailure();
        setRunMsg(error.isEmpty()?QString("OpenCV image operation failed"):error);
        return EXvFuncRunStatus::Error;
    }
    if(!commitAdditionalResults(error))
    {
        clearResultsAfterFailure();
        setRunMsg(error.isEmpty()?QString("OpenCV result commit failed"):error);
        return EXvFuncRunStatus::Error;
    }
    outputImage->setValue(candidate);
    setRunMsg(QString());
    return EXvFuncRunStatus::Ok;
#endif
}

bool OpenCvImageOperatorBase::commitAdditionalResults(QString &error)
{
    error.clear();
    return true;
}

void OpenCvImageOperatorBase::clearResultsAfterFailure()
{
}

void OpenCvImageOperatorBase::onShowFunc()
{
    if(!m_widget) m_widget=new OpenCvImageOperatorWdg(this);
    m_widget->show();
    m_widget->raise();
}
