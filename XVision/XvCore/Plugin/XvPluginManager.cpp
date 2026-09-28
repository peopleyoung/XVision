#include "XvPluginManager.h"
#include <QMutexLocker>
#include <QtGlobal>
#include <QtCore>
#include <QPluginLoader>

#include "IXvFactoryPlugin.h"
#include "XvFunc.h"
#include "XvFuncAssembly.h"
#include "LangDef.h"

using namespace XvCore;


XvPluginManager::XvPluginManager(QObject *parent)
    : QObject{parent}
{

}





XvPluginManager *XvPluginManager::s_Instance = NULL;
XvPluginManager *XvPluginManager::getInstance() {
  if (!s_Instance) {
     QMutex s_Mutex;
    QMutexLocker locker(&s_Mutex);
    if (!s_Instance) {
      s_Instance = new XvPluginManager();
    }
  }
  return s_Instance;
}

bool XvPluginManager::init()
{
    QString dllPath=qApp->applicationDirPath()+"/XvFuncCollection";
    QDir pluginsDir(dllPath);
    for (const QString& filename : pluginsDir.entryList(QDir::Files))
    {
        QFileInfo fileinfo(filename);
        if (fileinfo.suffix() != "so" && fileinfo.suffix() != "dll" &&
            fileinfo.suffix() != "dylib")
        {
          continue;
        }
        QString path=pluginsDir.absoluteFilePath(filename);
        QPluginLoader pluginLoader(path);

        auto plugin=pluginLoader.instance();
        if(plugin && dynamic_cast<IXvFactoryPlugin*>(plugin))
        {
             IXvFactoryPlugin *vFuncplg = qobject_cast<IXvFactoryPlugin*>(plugin);
             if(vFuncplg)
             {
                QString plgName=vFuncplg->name();
                if(!vFuncplg->init())//初始化失败
                {
                    Log_Error(QString("#%1#%2").arg(plgName).arg(getLang(Core_Plg_InitFail,"插件初始化失败")));
                }
                else
                {
                   const QList<QMetaObject> functions=vFuncplg->getPlgXvFunc();
                   const QList<XvFuncPreset> presets=vFuncplg->getPlgXvFuncPresets();
                   QString registrationError;
                   if(!XvFuncAsm->registerPlugin(functions,presets,&registrationError))
                   {
                       vFuncplg->uninit();
                       Log_Error(QString("%1:%2 %3")
                                 .arg(plgName,registrationError,
                                      getLang(Core_XvFunc_RegFail,"算子注册失败")));
                   }
                   else
                   {
                       m_lstPlg.append(vFuncplg);
                       QStringList registeredNames;
                       for(const QMetaObject &meta:functions)
                           registeredNames.append(meta.className());
                       for(const XvFuncPreset &preset:presets)
                           registeredNames.append(preset.alias);
                       Log_Event(QString("%1:<%2> %3")
                                 .arg(plgName,registeredNames.join("><"),
                                      getLang(Core_XvFunc_RegSuccess,"算子注册成功")));
                   }
                }
             }
        }
    }

    return false;
}

bool XvPluginManager::uninit()
{
    foreach (auto plg, m_lstPlg)
    {
        plg->uninit();
    }
    return false;
}

bool XvPluginManager::plgRegisterXvFunc(const QMetaObject &funcMeta)
{
    return XvFuncAsm->registerXvFunc(funcMeta);
}
