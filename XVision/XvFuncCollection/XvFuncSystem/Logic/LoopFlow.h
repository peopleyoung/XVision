#ifndef LOOPFLOW_H
#define LOOPFLOW_H

#include "XVFuncSystemGlobal.h"
#include "XImage.h"
#include "XInt.h"
#include "XObjectList.h"
#include "XvFunc.h"

class LoopFlowWdg;

namespace XvCore
{
class LoopFlowParam:public XvBaseParam
{
public:
    LoopFlowParam()
    {
        start=new XInt("start",0,this,"起始值");
        end=new XInt("end",1,this,"结束值");
        step=new XInt("step",1,this,"步长");
        images=new XObjectList("images",XImage::type(),this,"图像集合");
    }

    XInt *start=nullptr;
    XInt *end=nullptr;
    XInt *step=nullptr;
    XObjectList *images=nullptr;
};

class LoopFlowResult:public XvBaseResult
{
public:
    LoopFlowResult()
    {
        iterationIndex=new XInt("iterationIndex",-1,this,"迭代序号");
        currentValue=new XInt("currentValue",0,this,"当前值");
        currentImage=new XImage("currentImage",QImage(),this,"当前图像");
    }

    XInt *iterationIndex=nullptr;
    XInt *currentValue=nullptr;
    XImage *currentImage=nullptr;
};

class XVFUNCSYSTEM_EXPORT LoopFlow:public XvFunc
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
    friend class ::LoopFlowWdg;
public:
    enum Mode { For,ForeachImages };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit LoopFlow(QObject *parent=nullptr);
    ~LoopFlow() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList persistentPropertyNames() const override { return {"mode"}; }
    QStringList outputPorts() const override { return {"body","done"}; }
    XvExecutionDirective executionDirective() const override;
    bool prepareIteration(int iteration,QString &error) override;

public slots:
    void onShowFunc() override;

protected:
    QString defaultOutputPortForNewLink() const override;
    EXvFuncRunStatus run() override;
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }

private:
    LoopFlowParam *m_param=nullptr;
    LoopFlowResult *m_result=nullptr;
    LoopFlowWdg *m_widget=nullptr;
    Mode m_mode=For;
    QList<int> m_values;
    int m_iterationCount=0;
};
}

#endif // LOOPFLOW_H
