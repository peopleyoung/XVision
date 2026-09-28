#include "CommunicationOperatorBase.h"

using namespace XvCore;

CommunicationOperatorBase::CommunicationOperatorBase(QObject *parent)
    :XvFunc(parent)
{
}

EXvFuncRunStatus CommunicationOperatorBase::runXvFunc()
{
    m_cancelRequested.store(false,std::memory_order_relaxed);
    m_communicationRunning.store(true,std::memory_order_release);
    const EXvFuncRunStatus status=XvFunc::runXvFunc();
    m_communicationRunning.store(false,std::memory_order_release);
    return status;
}

bool CommunicationOperatorBase::release()
{
    if(m_communicationRunning.load(std::memory_order_acquire))
    {
        m_cancelRequested.store(true,std::memory_order_relaxed);
        return false;
    }
    return XvFunc::release();
}

bool CommunicationOperatorBase::communicationCancelled() const
{
    return m_cancelRequested.load(std::memory_order_relaxed);
}

const std::atomic_bool *CommunicationOperatorBase::communicationCancellationFlag() const
{
    return &m_cancelRequested;
}

EXvFuncRunStatus CommunicationOperatorBase::communicationCancelledStatus()
{
    setRunMsg("Communication operation cancelled");
    return EXvFuncRunStatus::Fail;
}

void CommunicationOperatorBase::setCommunicationError(const QString &message)
{
    setRunMsg(message.isEmpty()?QString("Communication operation failed"):message);
}
