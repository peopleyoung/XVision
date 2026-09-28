#ifndef XVOPENCVIMAGEINTEROP_H
#define XVOPENCVIMAGEINTEROP_H

#include "XVFuncSystemGlobal.h"

#include <QImage>
#include <QString>

namespace cv
{
class Mat;
}

namespace XvOpenCvImageInterop
{
XVFUNCSYSTEM_EXPORT bool toMat(const QImage &source,cv::Mat &target,QString *error=nullptr);
XVFUNCSYSTEM_EXPORT bool toQImage(const cv::Mat &source,QImage &target,QString *error=nullptr);
}

#endif // XVOPENCVIMAGEINTEROP_H
