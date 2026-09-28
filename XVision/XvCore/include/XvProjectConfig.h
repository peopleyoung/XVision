#ifndef XVPROJECTCONFIG_H
#define XVPROJECTCONFIG_H

#include "XvCoreGlobal.h"

#include <QList>
#include <QString>

namespace XvCore
{

enum class EXvProjectFlowErrorPolicy
{
    Stop=0,
    Continue=1
};

struct XVCORE_EXPORT XvProjectFlowEntry
{
    XvProjectFlowEntry()=default;
    explicit XvProjectFlowEntry(const QString &id,bool isEnabled=true)
        :flowId(id),enabled(isEnabled) {}

    bool operator==(const XvProjectFlowEntry &other) const
    {
        return flowId==other.flowId && enabled==other.enabled;
    }
    bool operator!=(const XvProjectFlowEntry &other) const
    {
        return !(*this==other);
    }

    QString flowId;
    bool enabled=true;
};

struct XVCORE_EXPORT XvProjectConfig
{
    static constexpr unsigned int DefaultLoopInterval=1000;
    static constexpr unsigned int MaximumLoopInterval=99999;

    bool operator==(const XvProjectConfig &other) const
    {
        return mainFlows==other.mainFlows
                && loopInterval==other.loopInterval
                && flowErrorPolicy==other.flowErrorPolicy;
    }
    bool operator!=(const XvProjectConfig &other) const
    {
        return !(*this==other);
    }

    QList<XvProjectFlowEntry> mainFlows;
    unsigned int loopInterval=DefaultLoopInterval;
    EXvProjectFlowErrorPolicy flowErrorPolicy=EXvProjectFlowErrorPolicy::Stop;
};

}

#endif // XVPROJECTCONFIG_H
