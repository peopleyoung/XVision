// Compile the inline data classes inside XvData so MSVC emits their DLL exports.
// Consumers use XVDATA_EXPORT as dllimport and must resolve the same definitions.
#include "XVisionSharedData.h"
#include "XVisionRuntimeData.h"
