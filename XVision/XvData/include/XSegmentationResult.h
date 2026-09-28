#ifndef XSEGMENTATIONRESULT_H
#define XSEGMENTATIONRESULT_H

#include "XObject.h"

#include <QColor>
#include <QStringList>
#include <QVector>

#define XSegmentationResultType "XSegmentationResult"

class XVDATA_EXPORT XSegmentationResult : public XObject
{
public:
    explicit XSegmentationResult(const QString &objectName,
                                 XObjectSet *parObjectSet=nullptr,
                                 const QString &displayName="");
    XSegmentationResult();

    static QString type() { return XSegmentationResultType; }
    QString typeName() override { return XSegmentationResultType; }
    XObject *clone() override;
    bool getData(XObject *object) override;
    bool setData(XObject *object) override;

    bool setValue(int width,int height,
                  const QVector<qint32> &labels,
                  const QVector<float> &confidences,
                  const QVector<qint32> &classIds,
                  const QStringList &classNames,
                  const QVector<QRgb> &classColors);
    void clear();
    bool isEmpty() const { return m_width==0 && m_height==0; }

    int width() const { return m_width; }
    int height() const { return m_height; }
    const QVector<qint32> &labels() const { return m_labels; }
    const QVector<float> &confidences() const { return m_confidences; }
    const QVector<qint32> &classIds() const { return m_classIds; }
    const QStringList &classNames() const { return m_classNames; }
    const QVector<QRgb> &classColors() const { return m_classColors; }

private:
    int m_width=0;
    int m_height=0;
    QVector<qint32> m_labels;
    QVector<float> m_confidences;
    QVector<qint32> m_classIds;
    QStringList m_classNames;
    QVector<QRgb> m_classColors;
};

#endif // XSEGMENTATIONRESULT_H
