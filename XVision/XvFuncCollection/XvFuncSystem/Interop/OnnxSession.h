#ifndef ONNXSESSION_H
#define ONNXSESSION_H

#include "XVFuncSystemGlobal.h"

#include <QByteArray>
#include <QList>
#include <QString>
#include <QVector>

#include <memory>

struct XVFUNCSYSTEM_EXPORT XOnnxTensorInfo
{
    QString name;
    QString elementType;
    QVector<qint64> dimensions;
};

struct XVFUNCSYSTEM_EXPORT XOnnxTensorData
{
    QString name;
    QString elementType;
    QVector<qint64> dimensions;
    QByteArray bytes;
};

class XVFUNCSYSTEM_EXPORT OnnxSession
{
public:
    OnnxSession();
    ~OnnxSession();
    OnnxSession(OnnxSession &&) = delete;
    OnnxSession &operator=(OnnxSession &&other) noexcept;
    OnnxSession(const OnnxSession &) = delete;
    OnnxSession &operator=(const OnnxSession &) = delete;

    bool load(const QString &modelPath,QString *error=nullptr);
    void clear();
    bool isLoaded() const;
    QString modelPath() const;
    QList<XOnnxTensorInfo> inputs() const;
    QList<XOnnxTensorInfo> outputs() const;
    bool run(const QList<XOnnxTensorData> &inputs,
             QList<XOnnxTensorData> &outputs,
             QString *error=nullptr);

private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
};

#endif // ONNXSESSION_H
