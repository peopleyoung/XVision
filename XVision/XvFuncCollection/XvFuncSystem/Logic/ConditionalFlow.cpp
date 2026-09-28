#include "XLanguage.h"
#include "ConditionalFlow.h"
#include "ConditionalFlowWdg.h"

using namespace XvCore;

ConditionalFlow::ConditionalFlow(QObject *parent)
    :XvFunc(parent),m_param(new ConditionalFlowParam())
{
    _funcRole="ConditionalFlow";
    _funcName=getUiText("Conditional Flow");
    _funcType=EXvFuncType::Logic;
}

ConditionalFlow::~ConditionalFlow()
{
    delete m_widget;
    delete m_param;
}

QString ConditionalFlow::defaultOutputPortForNewLink() const
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

EXvFuncRunStatus ConditionalFlow::run()
{
    if(!m_param || !m_param->condition)
    {
        setRunMsg("Condition parameter is unavailable");
        return EXvFuncRunStatus::Error;
    }
    m_selectedValue=m_param->condition->value();
    return EXvFuncRunStatus::Ok;
}

XvExecutionDirective ConditionalFlow::executionDirective() const
{
    XvExecutionDirective directive;
    directive.kind=XvExecutionDirective::SelectPorts;
    directive.selectedPorts=QStringList{m_selectedValue?QString("true"):QString("false")};
    return directive;
}

void ConditionalFlow::onShowFunc()
{
    if(!m_widget) m_widget=new ConditionalFlowWdg(this);
    m_widget->show();
    m_widget->raise();
}
