#ifndef OPENCVTEMPLATERECTIFICATIONUTILS_H
#define OPENCVTEMPLATERECTIFICATIONUTILS_H

#include "XVFuncSystemGlobal.h"

#include <QByteArray>
#include <QImage>
#include <QPointF>
#include <QSize>
#include <QString>
#include <QVector>

#include <array>

class XRotateRectRoi;
class XTensor;

namespace OpenCvTemplateRectificationUtils
{
constexpr int MaxTemplateAssetBytes=8*1024*1024;
constexpr int MaxTemplateDimension=32768;
constexpr qint64 MaxTemplatePixels=64LL*1024LL*1024LL;

XVFUNCSYSTEM_EXPORT bool encodeTemplatePng(const QImage &image,QByteArray &asset,
                                           QSize &size,QByteArray &sha256,
                                           QString &error);
XVFUNCSYSTEM_EXPORT bool decodeTemplatePng(const QByteArray &asset,QImage &image,
                                           QSize &size,QByteArray &sha256,
                                           QString &error);
XVFUNCSYSTEM_EXPORT bool roiCorners(const XRotateRectRoi &roi,
                                    QVector<QPointF> &corners,QString &error);
XVFUNCSYSTEM_EXPORT bool roiOutputSize(const XRotateRectRoi &roi,int requestedWidth,
                                       int requestedHeight,QSize &size,QString &error);
XVFUNCSYSTEM_EXPORT bool setTransformTensor(const std::array<double,9> &matrix,
                                            XTensor &tensor,QString &error);
}

#endif // OPENCVTEMPLATERECTIFICATIONUTILS_H
