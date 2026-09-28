#ifndef HALCONIMAGEINTEROP_H
#define HALCONIMAGEINTEROP_H

#include "XVFuncSystemGlobal.h"

#include <QImage>
#include <QString>
#include <QVector>

#include "HalconCpp.h"

namespace XvHalconImageInterop
{

XVFUNCSYSTEM_EXPORT bool toHalcon(const QImage &source,
                                 HalconCpp::HImage &target,
                                 QString *error=nullptr);
XVFUNCSYSTEM_EXPORT bool toQImage(const HalconCpp::HImage &source,
                                 QImage &target,
                                 QString *error=nullptr);
XVFUNCSYSTEM_EXPORT bool toInt32Pixels(const HalconCpp::HImage &source,
                                      QVector<qint32> &target,
                                      int &width,int &height,
                                      QString *error=nullptr);
XVFUNCSYSTEM_EXPORT bool toFloatPixels(const HalconCpp::HImage &source,
                                      QVector<float> &target,
                                      int &width,int &height,
                                      QString *error=nullptr);

}

#endif // HALCONIMAGEINTEROP_H
