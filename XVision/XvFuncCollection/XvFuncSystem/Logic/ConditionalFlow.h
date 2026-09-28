#ifndef CONDITIONALFLOW_H
#define CONDITIONALFLOW_H

#include "XVFuncSystemGlobal.h"
#include "XBool.h"
#include "XvFunc.h"

class ConditionalFlowWdg;

namespace XvCore
{
class ConditionalFlowParam:public XvBaseParam
{
public:
    ConditionalFlowParam()
    {
        condition=new XBool("condition",false,this,"条件");
    }

    XBool *condition=nullptr;
};

class XVFUNCSYSTEM_EXPORT ConditionalFlow:public XvFunc
{
    Q_OBJECT
    friend class ::ConditionalFlowWdg;
public:
    Q_INVOKABLE explicit ConditionalFlow(QObject *parent=nullptr);
    ~ConditionalFlow() override;

    QStringList outputPorts() const override { return {"true","false"}; }
    XvExecutionDirective executionDirective() const override;

public slots:
    void onShowFunc() override;

protected:
    QString defaultOutputPortForNewLink() const override;
    EXvFuncRunStatus run() override;
    XvBaseParam *getParam() const override { return m_param; }

private:
    ConditionalFlowParam *m_param=nullptr;
    ConditionalFlowWdg *m_widget=nullptr;
    bool m_selectedValue=false;
};
}

#endif // CONDITIONALFLOW_H
