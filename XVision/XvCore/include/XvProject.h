#ifndef XVPROJECT_H
#define XVPROJECT_H

#include "XvCoreGlobal.h"
#include <QObject>
#include "IXvTokenMsgAble.h"
#include "XvError.h"
#include "XvProjectConfig.h"
#include "XvProjectRunInfo.h"
#include "XmlSerializable.h"

namespace XvCore
{
class XvFlow;
class XvCoreManager;
class XvProjectPrivate;
class XVCORE_EXPORT XvProject : public QObject,public IXvTokenMsgAble,public XmlSerializable
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(XvProject)
    Q_PROPERTY(QString projectId READ projectId)
    Q_PROPERTY(QString projectName READ projectName WRITE setProjectName NOTIFY projectNameChanged)
    friend class XvCoreManager;
public:
    explicit XvProject(const QString &name="Project",QObject *parent = nullptr);
    ~XvProject();

public:
//*[项目操作]*
    ///进行释放(删除前必需调用进行判断)
    /// Ret_Xv_Success:能删除 !Ret_Xv_Success:不可删除
    RetXv release();
    ///项目单次运行
    RetXv runOnce();
    ///项目循环运行
    RetXv runLoop();
    ///停止项目运行
    RetXv stop();
    ///等待项目运行结束，0表示无限等待
    RetXv wait(unsigned long ms=0);
    ///项目是否正在运行
    bool isRunning() const;
    ///项目运行信息快照
    XvProjectRunInfo projectRunInfo() const;


 /**********************流程操作**********************/
 //流程增删查
 /***************************************************/
public:
    ///通过ID获取流程
    XvFlow* getXvFlow(const QString &id);
    ///通过名称获取流程列表
    QList<XvFlow*> getXvFlows(const QString &name) const;
    ///获取所有流程
    QList<XvFlow*> getXvFlows() const;
    ///流程数量
    int xvFlowCount() const;
public slots:
    ///创建流程
    XvFlow* createXvFlow(const QString &name="Flow");
protected:
    ///通过指定ID创建流程(项目恢复使用)
    XvFlow* createXvFlow(const QString &name,const QString &restoredId);
    ///接纳已完整校验的流程(Core文件导入使用)
    bool adoptXvFlow(XvFlow *flow);
public slots:
    ///通过ID移除流程
    bool removeXvFlow(const QString &id);
    ///通过名称移除多个流程
    /// Rt:移除的流程个数
    int removeXvFlows(const QString &name);
 signals:
    ///流程添加完成信号
    void sgXvFlowCreated(XvFlow* flow);
    ///流程开始删除信号
    void sgRemoveXvFlowStart(XvFlow* flow);
    ///流程完成删除信号
    void sgRemoveXvFlowEnd(const QString &flowId);
    ///项目运行开始/结束/停止
    void sgProjectRunStart();
    void sgProjectRunEnd();
    void sgProjectRunStop();
    ///项目调度单个流程开始/结束
    void sgProjectFlowRunStart(XvFlow *flow);
    void sgProjectFlowRunEnd(XvFlow *flow);

/**********************字段/属性定义**********************/
//项目基本定义属性 序列号/名称
/********************************************************/
public:
    ///返回项目运行配置快照
    XvProjectConfig projectConfig() const;
    ///完整校验后原子替换项目运行配置
    bool setProjectConfig(const XvProjectConfig &config);

    ///流程Id
    inline QString projectId() const;
    ///流程名称
    inline QString projectName() const;
    ///设置名称
    void setProjectName(const QString &name);
    ///TokenMsgAble
    QString tokenMsgId() override { return projectId();}

    ///获取最后错误信息
    QString lastErrorMsg();
protected:
    ///设置错误信息
    void setLastErrorMsg(const QString &msg);
signals:
    void projectNameChanged(const QString &name);
    void projectConfigChanged();

/**********************XML序列化**********************/
public:
    QDomElement toXmlElement(QDomDocument &doc) override;
    bool fromXmlElement(QDomElement &xmlEle) override;
protected:
    RetXv startRun(bool loop);
    void runProjectThread(bool loop,const XvProjectConfig &config);

    ///项目唯一ID
    QString _projectId;

    ///项目名称
    QString _projectName;

    ///最后错误信息
    QString _lastErrorMsg;
protected:
    const QScopedPointer<XvProjectPrivate> d_ptr;

};

inline QString XvProject::projectId() const
{
    return _projectId;
}

inline QString XvProject::projectName() const
{
    return _projectName;
}
}
#endif // XVPROJECT_H
