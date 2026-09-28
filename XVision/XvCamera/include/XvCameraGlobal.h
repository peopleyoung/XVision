#ifndef XVCAMERAGLOBAL_H
#define XVCAMERAGLOBAL_H

#include <QtCore/qglobal.h>

#if defined(XVCAMERA_LIBRARY)
#  define XVCAMERA_EXPORT Q_DECL_EXPORT
#else
#  define XVCAMERA_EXPORT Q_DECL_IMPORT
#endif

#endif // XVCAMERAGLOBAL_H
