#include "XvFuncAssembly.h"
#include <QMutexLocker>
#include <QMetaEnum>
#include <QMetaProperty>
#include <QSet>

#include <memory>

#include "LangDef.h"
#include "XBool.h"
#include "XvFunc.h"
#include "XInt.h"
#include "XReal.h"
#include "XString.h"

using namespace XvCore;

namespace
{
XvFunc *createFunction(const QMetaObject &meta)
{
    QObject *parent=nullptr;
    QObject *object=meta.newInstance(Q_ARG(QObject*,parent));
    XvFunc *function=qobject_cast<XvFunc*>(object);
    if(!function) delete object;
    return function;
}

bool makeFunctionInfo(const QMetaObject &meta,XvFuncInfo &info,QString &error)
{
    if(!meta.inherits(&XvFunc::staticMetaObject))
    {
        error=QString("metaobject '%1' does not inherit XvFunc").arg(meta.className());
        return false;
    }
    std::unique_ptr<XvFunc> function(createFunction(meta));
    if(!function)
    {
        error=QString("metaobject '%1' cannot construct an XvFunc").arg(meta.className());
        return false;
    }
    if(function->funcRole().trimmed().isEmpty()
            || function->funcRole()!=function->funcRole().trimmed())
    {
        error=QString("metaobject '%1' has an empty or untrimmed role")
                .arg(meta.className());
        return false;
    }
    info=XvFuncInfo(function->funcRole(),function->funcType(),function->funcName(),
                    function->funcIcon(),meta);
    return true;
}

bool variantsEqual(const QVariant &left,const QVariant &right,int type)
{
    if(type==QMetaType::Bool) return left.toBool()==right.toBool();
    if(type==QMetaType::Int) return left.toInt()==right.toInt();
    if(type==QMetaType::Double) return left.toDouble()==right.toDouble();
    if(type==QMetaType::QString) return left.toString()==right.toString();
    return left==right;
}

bool applyPreset(XvFunc *function,const XvFuncPreset &preset,QString &error)
{
    if(!function)
    {
        error="preset target function is null";
        return false;
    }
    const QStringList persistentPropertyNames=function->persistentPropertyNames();
    QSet<QString> persistentProperties;
    for (const QString &name : persistentPropertyNames)
    {
        persistentProperties.insert(name);
    }
    for(auto iterator=preset.properties.constBegin();
        iterator!=preset.properties.constEnd();++iterator)
    {
        const QString name=iterator.key();
        if(name.isEmpty() || !persistentProperties.contains(name))
        {
            error=QString("preset '%1' property '%2' is not persistent")
                    .arg(preset.alias,name);
            return false;
        }
        const QMetaObject *meta=function->metaObject();
        const int index=meta->indexOfProperty(name.toUtf8().constData());
        if(index<0)
        {
            error=QString("preset '%1' property '%2' does not exist")
                    .arg(preset.alias,name);
            return false;
        }
        const QMetaProperty property=meta->property(index);
        if(!property.isWritable())
        {
            error=QString("preset '%1' property '%2' is not writable")
                    .arg(preset.alias,name);
            return false;
        }

        QVariant value=iterator.value();
        int expectedType=property.userType();
        if(property.isEnumType())
        {
            if(value.userType()!=QMetaType::Int
                    || !property.enumerator().valueToKey(value.toInt()))
            {
                error=QString("preset '%1' property '%2' has an invalid enum value")
                        .arg(preset.alias,name);
                return false;
            }
            expectedType=QMetaType::Int;
        }
        else if(value.userType()!=expectedType
                || (expectedType!=QMetaType::Bool && expectedType!=QMetaType::Int
                    && expectedType!=QMetaType::Double
                    && expectedType!=QMetaType::QString))
        {
            error=QString("preset '%1' property '%2' has an incompatible type")
                    .arg(preset.alias,name);
            return false;
        }
        if(!property.write(function,value)
                || !variantsEqual(property.read(function),value,expectedType))
        {
            error=QString("preset '%1' property '%2' cannot be applied")
                    .arg(preset.alias,name);
            return false;
        }
    }

    for(auto iterator=preset.parameters.constBegin();
        iterator!=preset.parameters.constEnd();++iterator)
    {
        XObject *parameter=function->getParamsByName(iterator.key());
        const QVariant value=iterator.value();
        bool applied=false;
        if(auto target=dynamic_cast<XBool*>(parameter))
        {
            if(value.userType()==QMetaType::Bool)
            {
                target->setValue(value.toBool());
                applied=target->value()==value.toBool();
            }
        }
        else if(auto target=dynamic_cast<XInt*>(parameter))
        {
            if(value.userType()==QMetaType::Int)
            {
                target->setValue(value.toInt());
                applied=target->value()==value.toInt();
            }
        }
        else if(auto target=dynamic_cast<XReal*>(parameter))
        {
            if(value.userType()==QMetaType::Double)
            {
                target->setValue(value.toDouble());
                applied=target->value()==value.toDouble();
            }
        }
        else if(auto target=dynamic_cast<XString*>(parameter))
        {
            if(value.userType()==QMetaType::QString)
            {
                target->setValue(value.toString());
                applied=target->value()==value.toString();
            }
        }
        if(!applied)
        {
            error=QString("preset '%1' parameter '%2' is missing or has an incompatible type")
                    .arg(preset.alias,iterator.key());
            return false;
        }
    }
    return true;
}

XvFuncInfo presetInfo(const XvFuncPreset &preset,const XvFuncInfo &canonical)
{
    XvFuncInfo info=canonical;
    info.role=preset.alias;
    info.canonicalRole=preset.canonicalRole;
    info.name=preset.displayName.isEmpty()?preset.alias:preset.displayName;
    info.preset=true;
    return info;
}
}

XvFuncAssembly::XvFuncAssembly(QObject *parent)
    : QObject{parent}
{
    m_mapXvFuncInfo.clear();
}


XvFuncAssembly *XvFuncAssembly::s_Instance = NULL;
XvFuncAssembly *XvFuncAssembly::getInstance() {
  if (!s_Instance) {
     QMutex s_Mutex;
    QMutexLocker locker(&s_Mutex);
    if (!s_Instance) {
      s_Instance = new XvFuncAssembly();
    }
  }
  return s_Instance;
}

bool XvFuncAssembly::registerXvFunc(XvFunc* func)
{
    if(func==nullptr) return false;
    return registerPlugin({*func->metaObject()},{},nullptr);
}

bool XvFuncAssembly::registerXvFunc(const QMetaObject &funcMeta)
{
    return registerPlugin({funcMeta},{},nullptr);
}

bool XvFuncAssembly::registerPlugin(const QList<QMetaObject> &funcMetas,
                                    const QList<XvFuncPreset> &presets,
                                    QString *error)
{
    QMap<QString,XvFuncInfo> nextFunctions=m_mapXvFuncInfo;
    QMap<QString,XvFuncPreset> nextPresets=m_mapXvFuncPreset;
    QList<XvFuncInfo> addedFunctions;
    QList<XvFuncInfo> addedAliases;
    QString failure;

    for(const QMetaObject &meta:funcMetas)
    {
        XvFuncInfo info;
        if(!makeFunctionInfo(meta,info,failure)
                || nextFunctions.contains(info.role)
                || nextPresets.contains(info.role))
        {
            if(failure.isEmpty())
                failure=QString("operator role '%1' is already registered").arg(info.role);
            m_lastErrorMsg=failure;
            if(error) *error=failure;
            return false;
        }
        nextFunctions.insert(info.role,info);
        addedFunctions.append(info);
    }

    for(const XvFuncPreset &input:presets)
    {
        XvFuncPreset preset=input;
        if(!preset.isValid() || preset.alias!=preset.alias.trimmed()
                || preset.canonicalRole!=preset.canonicalRole.trimmed()
                || nextFunctions.contains(preset.alias)
                || nextPresets.contains(preset.alias))
        {
            failure=QString("preset alias '%1' is empty, untrimmed, or already registered")
                    .arg(preset.alias);
            m_lastErrorMsg=failure;
            if(error) *error=failure;
            return false;
        }
        const XvFuncInfo canonical=nextFunctions.value(preset.canonicalRole);
        if(!canonical.isValid())
        {
            failure=QString("preset '%1' targets unknown role '%2'")
                    .arg(preset.alias,preset.canonicalRole);
            m_lastErrorMsg=failure;
            if(error) *error=failure;
            return false;
        }
        std::unique_ptr<XvFunc> candidate(createFunction(canonical.meta));
        if(!candidate || !applyPreset(candidate.get(),preset,failure))
        {
            if(failure.isEmpty())
                failure=QString("preset '%1' cannot construct its target").arg(preset.alias);
            m_lastErrorMsg=failure;
            if(error) *error=failure;
            return false;
        }
        if(preset.displayName.trimmed().isEmpty()) preset.displayName=preset.alias;
        else if(preset.displayName!=preset.displayName.trimmed())
        {
            failure=QString("preset '%1' has an untrimmed display name").arg(preset.alias);
            m_lastErrorMsg=failure;
            if(error) *error=failure;
            return false;
        }
        nextPresets.insert(preset.alias,preset);
        addedAliases.append(presetInfo(preset,canonical));
    }

    m_mapXvFuncInfo=nextFunctions;
    m_mapXvFuncPreset=nextPresets;
    m_lastErrorMsg.clear();
    if(error) error->clear();
    for(const XvFuncInfo &info:addedFunctions) emit sgRegisterNewXvFunc(info);
    for(const XvFuncInfo &info:addedAliases) emit sgRegisterNewXvFunc(info);
    return true;
}

XvFunc *XvFuncAssembly::createNewXvFunc(QString role)
{
    QString canonical=role;
    XvFuncPreset preset;
    const bool fromPreset=m_mapXvFuncPreset.contains(role);
    if(fromPreset)
    {
        preset=m_mapXvFuncPreset.value(role);
        canonical=preset.canonicalRole;
    }
    if(m_mapXvFuncInfo.contains(canonical))
    {
        const XvFuncInfo info=m_mapXvFuncInfo.value(canonical);
        std::unique_ptr<XvFunc> function(createFunction(info.meta));
        QString error;
        if(!function || (fromPreset && !applyPreset(function.get(),preset,error)))
        {
            m_lastErrorMsg=error.isEmpty()
                    ?QString("operator role '%1' cannot be constructed").arg(canonical):error;
            return nullptr;
        }
        m_lastErrorMsg.clear();
        return function.release();
    }
    m_lastErrorMsg=QString("operator role or preset '%1' is not registered").arg(role);
    return nullptr;
}

XvFuncInfo XvFuncAssembly::getXvFuncInfo(QString role)
{
    if(m_mapXvFuncInfo.contains(role))
    {
        return m_mapXvFuncInfo[role];
    }
    if(m_mapXvFuncPreset.contains(role))
    {
        const XvFuncPreset preset=m_mapXvFuncPreset.value(role);
        return presetInfo(preset,m_mapXvFuncInfo.value(preset.canonicalRole));
    }
    return XvFuncInfo();
}

QString XvFuncAssembly::canonicalRole(const QString &role) const
{
    if(m_mapXvFuncInfo.contains(role)) return role;
    return m_mapXvFuncPreset.value(role).canonicalRole;
}

QList<XvFuncPreset> XvFuncAssembly::getXvFuncPresets() const
{
    return m_mapXvFuncPreset.values();
}

QString XvFuncAssembly::lastErrorMsg()
{
    const QString result=m_lastErrorMsg;
    m_lastErrorMsg.clear();
    return result;
}

QList<XvFuncInfo> XvFuncAssembly::getXvFuncInfos()
{
    QList<XvFuncInfo> lst;
    foreach (auto info, m_mapXvFuncInfo)
    {
        lst.append(info);
    }
    for(const XvFuncPreset &preset:m_mapXvFuncPreset)
        lst.append(presetInfo(preset,m_mapXvFuncInfo.value(preset.canonicalRole)));
    return lst;
}


QList<XvFuncInfo> XvFuncAssembly::getXvFuncInfos(const EXvFuncType &type)
{
    QList<XvFuncInfo> lst;
    foreach (auto info, m_mapXvFuncInfo)
    {
        if(info.type==type)
        {
            lst.append(info);
        }
    }
    for(const XvFuncPreset &preset:m_mapXvFuncPreset)
    {
        const XvFuncInfo canonical=m_mapXvFuncInfo.value(preset.canonicalRole);
        if(canonical.type==type) lst.append(presetInfo(preset,canonical));
    }
    return lst;
}

QMap<EXvFuncType, QList<XvFuncInfo>> XvFuncAssembly::getMapXvFuncTypeInfo()
{
    QMap<EXvFuncType, QList<XvFuncInfo>> map;
    auto lstXvTypes=getXvFuncTypeInfos();
    foreach (auto tInfo, lstXvTypes)
    {
      QList<XvCore::XvFuncInfo> lst=getXvFuncInfos(tInfo.type);
      map.insert(tInfo.type,lst);
    }
    return map;
}



QList<XvFuncTypeInfo> XvFuncAssembly::getXvFuncTypeInfos()
{
    QList<XvFuncTypeInfo> lst;
    lst.append(XvFuncTypeInfo(EXvFuncType::Null,getLang(Core_XvFuncType_Null,"无"),QPixmap(":/image/XvFuncType_Null.svg")));
    lst.append(XvFuncTypeInfo(EXvFuncType::ImageAcquisition,getLang(Core_XvFuncType_ImageAcquisition,"图像采集"),QPixmap(":/image/XvFuncType_ImageAcquisition.svg")));
    lst.append(XvFuncTypeInfo(EXvFuncType::Calibration,getLang(Core_XvFuncType_Calibration,"校正标定"),QPixmap(":/image/XvFuncType_Calibration.svg")));
    lst.append(XvFuncTypeInfo(EXvFuncType::Contraposition,getLang(Core_XvFuncType_Contraposition,"对位"),QPixmap(":/image/XvFuncType_Contraposition.svg")));
    lst.append(XvFuncTypeInfo(EXvFuncType::Location,getLang(Core_XvFuncType_Location,"定位"),QPixmap(":/image/XvFuncType_Location.svg")));
    lst.append(XvFuncTypeInfo(EXvFuncType::ImageProcessing,getLang(Core_XvFuncType_ImageProcessing,"图像处理"),QPixmap(":/image/XvFuncType_ImageProcessing.svg")));
    lst.append(XvFuncTypeInfo(EXvFuncType::Measurement,getLang(Core_XvFuncType_Measurement,"测量"),QPixmap(":/image/XvFuncType_Measurement.svg")));
    lst.append(XvFuncTypeInfo(EXvFuncType::DefectDetection,getLang(Core_XvFuncType_DefectDetection,"检测"),QPixmap(":/image/XvFuncType_DefectDetection.svg")));
    lst.append(XvFuncTypeInfo(EXvFuncType::Recognition,getLang(Core_XvFuncType_Recognition,"识别"),QPixmap(":/image/XvFuncType_Recognition.svg")));
    lst.append(XvFuncTypeInfo(EXvFuncType::Communication,getLang(Core_XvFuncType_Communication,"通讯"),QPixmap(":/image/XvFuncType_Communication.svg")));
    lst.append(XvFuncTypeInfo(EXvFuncType::MachineLearning,getLang(Core_XvFuncType_MachineLearning,"机器学习"),QPixmap(":/image/XvFuncType_MachineLearning.svg")));
    lst.append(XvFuncTypeInfo(EXvFuncType::DataProcessing,getLang(Core_XvFuncType_DataProcessing,"数据处理"),QPixmap(":/image/XvFuncType_DataProcessing.svg")));
    lst.append(XvFuncTypeInfo(EXvFuncType::Logic,getLang(Core_XvFuncType_Logic,"逻辑"),QPixmap(":/image/XvFuncType_Logic.svg")));
    lst.append(XvFuncTypeInfo(EXvFuncType::Special,getLang(Core_XvFuncType_Special,"特殊"),QPixmap(":/image/XvFuncType_Special.svg")));
    lst.append(XvFuncTypeInfo(EXvFuncType::Other,getLang(Core_XvFuncType_Other,"其他"),QPixmap(":/image/XvFuncType_Other.svg")));
    lst.append(XvFuncTypeInfo(EXvFuncType::User,getLang(Core_XvFuncType_User,"用户"),QPixmap(":/image/XvFuncType_User.svg")));
    return lst;
}

XvFuncTypeInfo XvFuncAssembly::getXvFuncTypeInfo(const EXvFuncType &type)
{
    auto lst=getXvFuncTypeInfos();
    foreach (auto info, lst)
    {
        if(info.type==type)
        {
            return info;
        }
    }
    return XvFuncTypeInfo();
}
