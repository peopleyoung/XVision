#pragma once
#include "HardwareBackend.h"
#include "VendorAbi.h"
#include <QLibrary>
#include <QHash>
#include <QDir>
#include <QFileInfo>
#include <QCoreApplication>
#include <QMutexLocker>
#include <algorithm>
#include <cmath>
#include <cstring>

namespace XvCamera { namespace Hardware {
class VendorLibrary
{
public:
    QLibrary library;
    Result load(const char *environment,const QString &base,const QStringList &directories)
    {
        if(library.isLoaded()) return {};
        const QString explicitPath=qEnvironmentVariable(environment);
        QStringList candidates;
        if(!explicitPath.isEmpty())
        {
            if(!QFileInfo(explicitPath).isAbsolute())
                return {EXvCameraError::InvalidArgument,QStringLiteral("SDK 路径必须是绝对路径：")+QString::fromLatin1(environment)};
            candidates.append(explicitPath);
        }
        else
        {
            for(const auto &directory:directories)
                if(!directory.isEmpty()) candidates.append(QDir(directory).filePath(base));
            candidates.append(base); // Installed vendor runtime may be on PATH/ld.so.
        }
        for(const auto &path:candidates)
        { library.setFileName(path); if(library.load()) return {}; }
        return {EXvCameraError::Unsupported,QStringLiteral("未能加载 %1；请安装厂商 64 位 SDK/驱动，或用 %2 指定库的完整路径。%3")
            .arg(base,QString::fromLatin1(environment),library.errorString())};
    }
    template<class T> bool resolve(T &function,const char *name)
    { function=reinterpret_cast<T>(library.resolve(name)); return function!=nullptr; }
};
template<size_t N> QString sdkText(const char (&text)[N])
{ size_t length=0; while(length<N && text[length]) ++length; return QString::fromUtf8(text,int(length)); }
template<size_t N> QString sdkText(const unsigned char (&text)[N])
{ size_t length=0; while(length<N && text[length]) ++length; return QString::fromUtf8(reinterpret_cast<const char*>(text),int(length)); }
inline bool validImageSize(int width,int height,quint64 channels,quint64 bytes)
{
    return width>0 && height>0 && width<=32768 && height<=32768 &&
        quint64(width)*quint64(height)*channels<=bytes && quint64(width)*quint64(height)*channels<=512*1024*1024;
}
XVCAMERA_EXPORT Result copyPackedImage(const void *data,int width,int height,int pixelType,quint64 length,QImage &image);
inline XvCameraParameterDescriptor realParameter(const QString &key,const QString &name,double minimum,double maximum,double current,bool readOnly=false)
{
    XvCameraParameterDescriptor descriptor;
    descriptor.key=key; descriptor.displayName=name; descriptor.type=EXvCameraParameterType::Real;
    descriptor.minimum=minimum; descriptor.maximum=maximum; descriptor.defaultValue=current; descriptor.readOnly=readOnly;
    return descriptor;
}
inline bool numericParameter(const QVariant &value,double &number)
{
    const int type=value.userType();
    if(type!=QMetaType::Double && type!=QMetaType::Float && type!=QMetaType::Int && type!=QMetaType::LongLong && type!=QMetaType::UInt) return false;
    bool ok=false; number=value.toDouble(&ok); return ok && std::isfinite(number);
}
} }
