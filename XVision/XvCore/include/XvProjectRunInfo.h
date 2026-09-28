#ifndef XVPROJECTRUNINFO_H
#define XVPROJECTRUNINFO_H

#include "XvCoreGlobal.h"
#include "XvError.h"

#include <QString>
#include <QStringList>

namespace XvCore
{

enum class EXvProjectRunStatus
{
    Init=0,
    Running=1,
    Ok=2,
    Fail=3,
    Error=4,
    Stopped=5
};

struct XVCORE_EXPORT XvProjectRunInfo
{
    unsigned int runIdx=0;
    EXvProjectRunStatus runStatus=EXvProjectRunStatus::Init;
    RetXv runCode=Ret_Xv_Success;
    QString runMsg;
    double runElapsed=0.0;
    QString currentFlowId;
    QStringList failedFlowIds;
};

}

#endif // XVPROJECTRUNINFO_H
