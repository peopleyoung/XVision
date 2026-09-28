#include "XSegmentationResult.h"

#include <QSet>

#include <cmath>
#include <limits>

namespace
{
bool validValue(int width,int height,
                const QVector<qint32> &labels,
                const QVector<float> &confidences,
                const QVector<qint32> &classIds,
                const QStringList &classNames,
                const QVector<QRgb> &classColors)
{
    if(width==0 && height==0)
    {
        return labels.isEmpty() && confidences.isEmpty() && classIds.isEmpty()
                && classNames.isEmpty() && classColors.isEmpty();
    }
    if(width<=0 || height<=0) return false;

    const qint64 pixelCount=qint64(width)*qint64(height);
    if(pixelCount<=0 || pixelCount>std::numeric_limits<int>::max()
            || labels.size()!=pixelCount || confidences.size()!=pixelCount)
    {
        return false;
    }
    if(classIds.isEmpty() || classNames.size()!=classIds.size()
            || classColors.size()!=classIds.size())
    {
        return false;
    }

    QSet<qint32> validClassIds;
    for(qsizetype index=0;index<classIds.size();++index)
    {
        if(validClassIds.contains(classIds.at(index))
                || classNames.at(index).trimmed().isEmpty())
        {
            return false;
        }
        validClassIds.insert(classIds.at(index));
    }
    for(qint32 label:labels)
    {
        if(!validClassIds.contains(label)) return false;
    }
    for(float confidence:confidences)
    {
        if(!std::isfinite(double(confidence))
                || confidence<0.0f || confidence>1.0f)
        {
            return false;
        }
    }
    return true;
}
}

XSegmentationResult::XSegmentationResult(const QString &objectName,
                                         XObjectSet *parObjectSet,
                                         const QString &displayName)
    :XObject(objectName,parObjectSet,displayName)
{
}

XSegmentationResult::XSegmentationResult()
    :XObject()
{
}

XObject *XSegmentationResult::clone()
{
    auto result=new XSegmentationResult(objectName(),nullptr,dispalyName());
    result->setTips(tips());
    if(!result->setValue(m_width,m_height,m_labels,m_confidences,
                         m_classIds,m_classNames,m_classColors))
    {
        delete result;
        return nullptr;
    }
    return result;
}

bool XSegmentationResult::getData(XObject *object)
{
    if(!object || object->typeName()!=typeName()) return false;
    auto target=dynamic_cast<XSegmentationResult*>(object);
    return target && target->setValue(m_width,m_height,m_labels,m_confidences,
                                      m_classIds,m_classNames,m_classColors);
}

bool XSegmentationResult::setData(XObject *object)
{
    if(!object || object->typeName()!=typeName()) return false;
    auto source=dynamic_cast<XSegmentationResult*>(object);
    return source && setValue(source->width(),source->height(),source->labels(),
                              source->confidences(),source->classIds(),
                              source->classNames(),source->classColors());
}

bool XSegmentationResult::setValue(int width,int height,
                                   const QVector<qint32> &labels,
                                   const QVector<float> &confidences,
                                   const QVector<qint32> &classIds,
                                   const QStringList &classNames,
                                   const QVector<QRgb> &classColors)
{
    if(!validValue(width,height,labels,confidences,classIds,classNames,classColors))
        return false;

    m_width=width;
    m_height=height;
    m_labels=labels;
    m_confidences=confidences;
    m_classIds=classIds;
    m_classNames=classNames;
    m_classColors=classColors;
    return true;
}

void XSegmentationResult::clear()
{
    m_width=0;
    m_height=0;
    m_labels.clear();
    m_confidences.clear();
    m_classIds.clear();
    m_classNames.clear();
    m_classColors.clear();
}
