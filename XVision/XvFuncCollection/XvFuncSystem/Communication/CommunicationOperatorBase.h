#ifndef COMMUNICATIONOPERATORBASE_H
#define COMMUNICATIONOPERATORBASE_H

#include "XVFuncSystemGlobal.h"
#include "XvFunc.h"

#include <atomic>

namespace XvCore
{

class XVFUNCSYSTEM_EXPORT CommunicationOperatorBase:public XvFunc
{
public:
    explicit CommunicationOperatorBase(QObject *parent=nullptr);
    EXvFuncRunStatus runXvFunc() override;
    bool release() override;
    virtual QStringList activeParameterNames() const=0;
    virtual QStringList communicationModeNames() const=0;

protected:
    bool communicationCancelled() const;
    const std::atomic_bool *communicationCancellationFlag() const;
    EXvFuncRunStatus communicationCancelledStatus();
    void setCommunicationError(const QString &message);

private:
    std::atomic_bool m_cancelRequested{false};
    std::atomic_bool m_communicationRunning{false};
};

}

#endif // COMMUNICATIONOPERATORBASE_H
