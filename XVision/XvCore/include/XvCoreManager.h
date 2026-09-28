#ifndef XVCOREMANAGER_H
#define XVCOREMANAGER_H


#include "XvCoreGlobal.h"
#include <QObject>
#include "IXvTokenMsgAble.h"

#define XvCoreMgrId  "XvCoreMgr"//核心管理器ID
#define XvCoreMgr XvCore::XvCoreManager::getInstance()

namespace XvCore
{

class XvPluginManager;
class XvFuncAssembly;
class XvProject;
class XvFlow;
class XvCoreManagerPrivate;

class XVCORE_EXPORT XvCoreManager : public QObject,public IXvTokenMsgAble
{
    Q_OBJECT
    Q_DECLARE_PRIVATE(XvCoreManager)
public:
    ///获取单例
    static XvCoreManager *getInstance();
private:
    explicit XvCoreManager(QObject *parent = nullptr);
    ~XvCoreManager();
    static XvCoreManager* s_Instance;
public:
    ///TokenMsgAble
    QString tokenMsgId() override { return XvCoreMgrId; }
//*[公共接口]*
    ///获取插件管理器
    XvPluginManager* getPlgMgr() const;
    ///获取算子合集
    XvFuncAssembly* getXvFuncAsm() const;

public:
//*[初始化操作]*
    ///初始化
    bool init();
    ///反初始化
    bool uninit();

public:
//*[项目操作]*
    ///获取项目，项目全局唯一
    XvProject* getXvProject() const;
    ///项目文件默认扩展名(不含点号)
    static QString projectFileSuffix();
    ///流程文件默认扩展名(不含点号)
    static QString flowFileSuffix();

    ///创建新项目
    XvProject *createNewXvProject(const QString &name);

    ///校验项目文件，不替换当前项目
    bool validateXvProjectFile(const QString &path);
    ///加载项目
    bool loadXvProject(const QString &path);
    ///保存项目
    bool saveXvProject(const QString &path);
    ///校验流程文件，不修改当前项目
    bool validateXvFlowFile(const QString &path);
    ///将流程文件作为新流程导入当前项目
    XvFlow* importXvFlow(const QString &path);
    ///导出指定流程
    bool exportXvFlow(const QString &flowId,const QString &path);
    ///当前项目是否存在运行中的流程
    bool hasRunningXvFlow() const;
    ///获取最后错误信息
    QString lastErrorMsg();

signals:
    ///项目即将替换信号
    void sgXvProjectAboutToReplace(XvProject* project);
    ///项目更新信号
    void sgUpdateXvProject(XvProject* project);


protected:
    const QScopedPointer<XvCoreManagerPrivate> d_ptr;


};
}
#endif // XVCOREMANAGER_H
