#include "LoopFlow.h"
#include "LoopFlowWdg.h"

#include <QtGlobal>

using namespace XvCore;

LoopFlow::LoopFlow(QObject *parent)
    :XvFunc(parent),m_param(new LoopFlowParam()),m_result(new LoopFlowResult())
{
    _funcRole="LoopFlow";
    _funcName="Loop Flow";
    _funcType=EXvFuncType::Logic;
}

LoopFlow::~LoopFlow()
{
    delete m_widget;
    delete m_param;
    delete m_result;
}

QString LoopFlow::defaultOutputPortForNewLink() const
{
    for(const QString &port:outputPorts())
    {
        bool used=false;
        for(XvFunc *son:linkedSonFuncs())
        {
            if(sonFuncPort(son)==port)
            {
                used=true;
                break;
            }
        }
        if(!used) return port;
    }
    return outputPorts().first();
}

EXvFuncRunStatus LoopFlow::run()
{
    m_values.clear();
    m_iterationCount=0;
    if(!m_param || !m_result || (m_mode!=For && m_mode!=ForeachImages))
    {
        setRunMsg("Loop mode or data is invalid");
        return EXvFuncRunStatus::Error;
    }

    if(m_mode==For)
    {
        const qint64 start=m_param->start->value();
        const qint64 end=m_param->end->value();
        const qint64 step=m_param->step->value();
        if(step==0)
        {
            setRunMsg("Loop step cannot be zero");
            return EXvFuncRunStatus::Error;
        }
        if((step>0 && start<end) || (step<0 && start>end))
        {
            const qint64 distance=step>0?end-start:start-end;
            const qint64 stride=step>0?step:-step;
            const qint64 count=(distance+stride-1)/stride;
            if(count>XvExecutionDirective::MaximumIterationCount)
            {
                setRunMsg("Loop iteration count exceeds the configured maximum");
                return EXvFuncRunStatus::Error;
            }
            m_iterationCount=static_cast<int>(count);
            m_values.reserve(m_iterationCount);
            qint64 value=start;
            for(int index=0;index<m_iterationCount;++index,value+=step)
                m_values.append(static_cast<int>(value));
        }
    }
    else
    {
        const qsizetype count=m_param->images->count();
        if(count>XvExecutionDirective::MaximumIterationCount)
        {
            setRunMsg("Image iteration count exceeds the configured maximum");
            return EXvFuncRunStatus::Error;
        }
        m_iterationCount=static_cast<int>(count);
    }

    m_result->iterationIndex->setValue(-1);
    m_result->currentImage->setValue(QImage());
    return EXvFuncRunStatus::Ok;
}

XvExecutionDirective LoopFlow::executionDirective() const
{
    XvExecutionDirective directive;
    directive.kind=getXvFuncRunStatus()==EXvFuncRunStatus::Ok
            ?XvExecutionDirective::Repeat:XvExecutionDirective::Error;
    directive.bodyPort="body";
    directive.donePort="done";
    directive.iterationCount=m_iterationCount;
    return directive;
}

bool LoopFlow::prepareIteration(int iteration,QString &error)
{
    if(iteration<0 || iteration>=m_iterationCount)
    {
        error=QString("Loop iteration index %1 is out of range").arg(iteration);
        return false;
    }
    m_result->iterationIndex->setValue(iteration);
    if(m_mode==For)
    {
        m_result->currentValue->setValue(m_values.at(iteration));
        m_result->currentImage->setValue(QImage());
        return true;
    }
    auto image=dynamic_cast<XImage*>(m_param->images->value(iteration));
    if(!image)
    {
        error=QString("Loop image at index %1 is invalid").arg(iteration);
        return false;
    }
    m_result->currentValue->setValue(iteration);
    m_result->currentImage->setValue(image->value());
    return true;
}

void LoopFlow::onShowFunc()
{
    if(!m_widget) m_widget=new LoopFlowWdg(this);
    m_widget->show();
    m_widget->raise();
}
