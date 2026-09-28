#ifndef XVFLOW_H
#define XVFLOW_H

#include "XvCoreGlobal.h"
#include <QObject>
#include <QSet>
#include "IXvTokenMsgAble.h"
#include "XmlSerializable.h"
#include "XvCoreDef.h"

#include <atomic>

namespace XvCore
{

class XvFunc;
class XvCoreManager;
class XvProject;
class XvFlowPrivate;
class XVCORE_EXPORT XvFlow : public QObject,public IXvTokenMsgAble,public XmlSerializable
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(XvFlow)
    Q_PROPERTY(QString flowId READ flowId)
    Q_PROPERTY(QString flowName READ flowName WRITE setFlowName NOTIFY sgFlowNameChanged)

    friend class XvFunc;
    friend class XvProject;
    friend class XvCoreManager;
public:
    explicit XvFlow(XvProject *project,const QString &name="Flow",QObject *parent = nullptr);
    ~XvFlow();
protected:
    const QScopedPointer<XvFlowPrivate> d_ptr;

 /**********************字段/属性定义**********************/
 //流程基本定义属性 序列号/名称 归属的项目
 /********************************************************/
public:
    ///流程Id
    inline QString flowId() const;
    ///流程名称
    inline QString flowName() const;
    ///设置名称
    void setFlowName(const QString &name);

    ///TokenMsgAble
    QString tokenMsgId() override { return flowId(); }
    ///父项目
    XvProject* parProject() const { return m_parProject; }

    ///获取最后错误信息
    QString lastErrorMsg();
protected:
    ///设置父流程
    void setParProject(XvProject* project);
    ///设置最后错误信息
    void setLastErrorMsg(const QString &msg);
signals:
    ///信号:流程名称更改
    void sgFlowNameChanged(XvFlow* flow);
protected:
    ///流程唯一ID
    QString _flowId;

    ///流程名称
    QString _flowName;

    ///最后错误信息
    QString _lastErrorMsg;
    ///父项目
    XvProject* m_parProject=nullptr;

/**********************算子操作**********************/
//算子增删查-改
/****************************************************/
public:
    ///通过ID获取算子
    XvFunc* getXvFunc(const QString &id);
    ///获取所有算子
    QList<XvFunc*> getXvFuncs() const;
    ///算子数量
    int xvFuncCount() const;
public slots:
//*[增删]*
    ///通过标识符创建算子
    XvFunc* createXvFunc(const QString &role);
protected:
    ///通过标识符和指定ID创建算子(项目恢复使用)
    XvFunc* createXvFunc(const QString &role,const QString &restoredId);
public slots:
    ///通过ID移除算子
    bool removeXvFunc(const QString &id);

protected slots:
    ///所有算子连接更新事件
    void onUpdateAllXvFuncLink();
signals:
    ///算子添加信号
    void sgXvFuncCreated(XvFunc* func);
    ///算子开始删除信号
    void sgRemoveXvFuncStart(XvFunc* func);
    ///算子完成删除信号
    void sgRemoveXvFuncEnd(const QString &funcId);

/**********************算子运行操作及其状态更新**********************/
public:
    ///流程运行接口
    RetXv runOnce();
    ///在线程中仅运行指定算子；保留流程编辑锁直到执行结束
    RetXv runFunctionOnce(const QString &functionId);
    ///流程循环运行
    RetXv runLoop();
    ///流程停止运行
    RetXv stop();
    ///等待流程运行
    /// ms:等待事件 ms=0:无限等待
    RetXv wait(unsigned long ms=0);
    ///是否正在运行
    bool isRunning() const { return _running.load();}
    ///流程及所属项目均未运行时允许结构编辑
    bool isEditAllowed() const;
    ///获取运行信息
    XvFlowRunInfo getXvFuncRunInfo() const
    {
        return _runInfo;
    }

    ///获取运行状态
    EXvFlowRunStatus getXvFuncRunStatus() const
    {
        return _runInfo.runStatus;
    }
protected:
    ///项目顺序调度使用的同步单次运行入口
    RetXv runOnceSynchronously();
    ///检查流程合法性(运行前调用)
    bool checkFlowLegal();
    ///执行一次已验证的动态DAG
    bool runGraphOnce(XvFlowConfig *config);
    ///执行一个入口已选择的DAG子上下文
    bool runGraphContext(const QSet<XvFunc*> &nodes,
                         const QSet<XvFunc*> &entryNodes,
                         XvFlowConfig *config);
    ///流程运行线程
    RetXv startAsync(bool loop,const QString &functionId=QString());
    void _threadRun(bool bLoop=false,const QString &functionId=QString());
signals:
    ///流程运行开始
    void sgFlowRunStart();
    ///流程运行结束
    void sgFlowRunEnd();
    ///流程运行停止
    void sgFlowRunStop();


protected:
    ///流程运行信息
    XvFlowRunInfo _runInfo;
    ///流程是否在运行
    std::atomic_bool _running;
    std::atomic_bool _stopRequested{false};

 /**********************流程操作**********************/
 //流程操作:运行释放
 /****************************************************/

public:
    ///流程资源释放(删除前必需调用进行判断)
    /// Ret_Xv_Success:能删除 !Ret_Xv_Success:不可删除
    RetXv release();

/**********************流程配置**********************/
//流程配置获取和修改
/****************************************************/

public:
    ///流程配置
    XvFlowConfig* getFlowConfig();

/**********************XML序列化**********************/
public:
    QDomElement toXmlElement(QDomDocument &doc) override;
    bool fromXmlElement(QDomElement &xmlEle) override;

};

inline QString XvFlow::flowId() const
{
    return _flowId;
}

inline QString XvFlow::flowName() const
{
    return _flowName;
}
}
#endif // XVFLOW_H
