#include "OpenCvTemplateRectificationUtils.h"

#include "XRotateRectRoi.h"
#include "XVisionRuntimeData.h"

#include <QBuffer>
#include <QCryptographicHash>
#include <QImageReader>
#include <QtMath>

#include <cstring>
#include <limits>

namespace
{
bool validateImageSize(const QSize &size,QString &error)
{
    if(!size.isValid() || size.width()>OpenCvTemplateRectificationUtils::MaxTemplateDimension
            || size.height()>OpenCvTemplateRectificationUtils::MaxTemplateDimension
            || qint64(size.width())*qint64(size.height())
                >OpenCvTemplateRectificationUtils::MaxTemplatePixels)
    {
        error="Template image dimensions exceed the supported limit";
        return false;
    }
    return true;
}
}

bool OpenCvTemplateRectificationUtils::encodeTemplatePng(
        const QImage &image,QByteArray &asset,QSize &size,QByteArray &sha256,
        QString &error)
{
    if(image.isNull() || !validateImageSize(image.size(),error))
    {
        if(error.isEmpty()) error="Template image is empty";
        return false;
    }
    QByteArray candidate;
    QBuffer buffer(&candidate);
    if(!buffer.open(QIODevice::WriteOnly) || !image.save(&buffer,"PNG"))
    {
        error="Template image could not be encoded as PNG";
        return false;
    }
    if(candidate.isEmpty() || candidate.size()>MaxTemplateAssetBytes)
    {
        error=QString("Template PNG must be at most %1 bytes")
                .arg(MaxTemplateAssetBytes);
        return false;
    }
    asset=candidate;
    size=image.size();
    sha256=QCryptographicHash::hash(candidate,QCryptographicHash::Sha256).toHex();
    error.clear();
    return true;
}

bool OpenCvTemplateRectificationUtils::decodeTemplatePng(
        const QByteArray &asset,QImage &image,QSize &size,QByteArray &sha256,
        QString &error)
{
    static const QByteArray pngSignature("\x89PNG\r\n\x1a\n",8);
    if(asset.isEmpty() || asset.size()>MaxTemplateAssetBytes
            || !asset.startsWith(pngSignature))
    {
        error=asset.size()>MaxTemplateAssetBytes
                ?QString("Template PNG exceeds %1 bytes").arg(MaxTemplateAssetBytes)
                :QString("Template asset is not a PNG image");
        return false;
    }

    QBuffer buffer;
    buffer.setData(asset);
    if(!buffer.open(QIODevice::ReadOnly))
    {
        error="Template PNG could not be opened";
        return false;
    }
    QImageReader reader(&buffer,"png");
    reader.setAutoTransform(false);
    if(!reader.canRead())
    {
        error=reader.errorString().isEmpty()
                ?QString("Template PNG could not be decoded")
                :reader.errorString();
        return false;
    }
    const QSize candidateSize=reader.size();
    if(!validateImageSize(candidateSize,error)) return false;
    const QImage candidate=reader.read();
    if(candidate.isNull() || candidate.size()!=candidateSize)
    {
        error=reader.errorString().isEmpty()
                ?QString("Template PNG could not be decoded")
                :reader.errorString();
        return false;
    }
    image=candidate.copy();
    size=candidateSize;
    sha256=QCryptographicHash::hash(asset,QCryptographicHash::Sha256).toHex();
    error.clear();
    return true;
}

bool OpenCvTemplateRectificationUtils::roiCorners(
        const XRotateRectRoi &roi,QVector<QPointF> &corners,QString &error)
{
    const double centerX=roi.centerX();
    const double centerY=roi.centerY();
    const double length1=roi.length1();
    const double length2=roi.length2();
    const double angle=roi.angle();
    if(!qIsFinite(centerX) || !qIsFinite(centerY) || !qIsFinite(length1)
            || !qIsFinite(length2) || !qIsFinite(angle)
            || length1<=0.0 || length2<=0.0)
    {
        error="Rotated rectangle contains invalid geometry";
        return false;
    }
    const QPointF center(centerX,centerY);
    const QPointF axis1(qCos(angle),-qSin(angle));
    const QPointF axis2(qSin(angle),qCos(angle));
    QVector<QPointF> candidate;
    candidate.reserve(4);
    candidate << center-axis1*length1-axis2*length2
              << center+axis1*length1-axis2*length2
              << center+axis1*length1+axis2*length2
              << center-axis1*length1+axis2*length2;
    for(const QPointF &point:candidate)
    {
        if(!qIsFinite(point.x()) || !qIsFinite(point.y()))
        {
            error="Rotated rectangle corners are not finite";
            return false;
        }
    }
    corners=candidate;
    error.clear();
    return true;
}

bool OpenCvTemplateRectificationUtils::roiOutputSize(
        const XRotateRectRoi &roi,int requestedWidth,int requestedHeight,
        QSize &size,QString &error)
{
    if((requestedWidth==0)!=(requestedHeight==0) || requestedWidth<0
            || requestedHeight<0 || requestedWidth>MaxTemplateDimension
            || requestedHeight>MaxTemplateDimension)
    {
        error="Output width and height must both be zero or both be positive";
        return false;
    }
    const double rawWidth=requestedWidth>0?requestedWidth:roi.length1()*2.0;
    const double rawHeight=requestedHeight>0?requestedHeight:roi.length2()*2.0;
    if(!qIsFinite(rawWidth) || !qIsFinite(rawHeight) || rawWidth<1.0
            || rawHeight<1.0 || rawWidth>MaxTemplateDimension
            || rawHeight>MaxTemplateDimension)
    {
        error="Rotated rectangle output dimensions are outside the supported range";
        return false;
    }
    const QSize candidate(qMax(1,qRound(rawWidth)),qMax(1,qRound(rawHeight)));
    if(!validateImageSize(candidate,error)) return false;
    size=candidate;
    error.clear();
    return true;
}

bool OpenCvTemplateRectificationUtils::setTransformTensor(
        const std::array<double,9> &matrix,XTensor &tensor,QString &error)
{
    QByteArray bytes(int(sizeof(double)*matrix.size()),'\0');
    for(double value:matrix)
    {
        if(!qIsFinite(value))
        {
            error="Transform matrix contains a non-finite value";
            return false;
        }
    }
    std::memcpy(bytes.data(),matrix.data(),size_t(bytes.size()));
    if(!tensor.setValue("float64",{3,3},bytes))
    {
        error="Transform tensor could not be created";
        return false;
    }
    error.clear();
    return true;
}
