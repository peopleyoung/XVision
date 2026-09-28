#ifndef NONNXBASE_H
#define NONNXBASE_H

#include "XVFuncSystemGlobal.h"
#include "XvFunc.h"
#include "OnnxOperatorUtils.h"

#include <memory>

namespace XvCore
{
class XVFUNCSYSTEM_EXPORT NOnnxBase:public XvFunc
{
    Q_OBJECT
    Q_PROPERTY(QString modelPath READ modelPath WRITE setModelPath)
    Q_PROPERTY(NOnnxBase::InputLayout inputLayout READ inputLayout WRITE setInputLayout)
    Q_PROPERTY(int inputWidth READ inputWidth WRITE setInputWidth)
    Q_PROPERTY(int inputHeight READ inputHeight WRITE setInputHeight)
    Q_PROPERTY(NOnnxBase::ImageChannelOrder channelOrder READ channelOrder WRITE setChannelOrder)
    Q_PROPERTY(NOnnxBase::ImageResizeMode resizeMode READ resizeMode WRITE setResizeMode)
    Q_PROPERTY(int paddingValue READ paddingValue WRITE setPaddingValue)
    Q_PROPERTY(double pixelScale READ pixelScale WRITE setPixelScale)
    Q_PROPERTY(QString meanValues READ meanValues WRITE setMeanValues)
    Q_PROPERTY(QString stdValues READ stdValues WRITE setStdValues)
public:
    enum InputLayout
    {
        Auto=0,
        Nchw=1,
        Nhwc=2
    };
    Q_ENUM(InputLayout)

    enum ImageChannelOrder
    {
        Rgb=0,
        Bgr=1
    };
    Q_ENUM(ImageChannelOrder)

    enum ImageResizeMode
    {
        Stretch=0,
        Letterbox=1
    };
    Q_ENUM(ImageResizeMode)

    explicit NOnnxBase(QObject *parent=nullptr);
    ~NOnnxBase() override;

    QString modelPath() const { return m_modelPath; }
    void setModelPath(const QString &path);
    InputLayout inputLayout() const { return m_inputLayout; }
    void setInputLayout(InputLayout layout) { m_inputLayout=layout; }
    int inputWidth() const { return m_inputWidth; }
    void setInputWidth(int width) { m_inputWidth=width; }
    int inputHeight() const { return m_inputHeight; }
    void setInputHeight(int height) { m_inputHeight=height; }
    ImageChannelOrder channelOrder() const { return m_channelOrder; }
    void setChannelOrder(ImageChannelOrder order) { m_channelOrder=order; }
    ImageResizeMode resizeMode() const { return m_resizeMode; }
    void setResizeMode(ImageResizeMode mode) { m_resizeMode=mode; }
    int paddingValue() const { return m_paddingValue; }
    void setPaddingValue(int value) { m_paddingValue=value; }
    double pixelScale() const { return m_pixelScale; }
    void setPixelScale(double value) { m_pixelScale=value; }
    QString meanValues() const { return m_meanValues; }
    void setMeanValues(const QString &values) { m_meanValues=values; }
    QString stdValues() const { return m_stdValues; }
    void setStdValues(const QString &values) { m_stdValues=values; }

    bool configureModel(const QString &path,QString &error);
    bool hasConfiguredModel() const;
    QString modelSummary() const;

protected:
    QStringList commonPersistentPropertyNames() const;
    bool appendPersistentData(QDomDocument &doc,QDomElement &functionElement,
                              QString &error) const override;
    bool readPersistentData(const QDomElement &dataElement,QString &error) override;
    bool validateCommonConfiguration(QString &error) const;
    bool prepareImageInput(const QImage &image,XOnnxTensorData &tensor,
                           XvOnnx::ImageTransform &transform,QString &error) const;
    bool executeModel(const QList<XOnnxTensorData> &inputs,
                      QList<XOnnxTensorData> &outputs,QString &error) const;
    QList<XOnnxTensorInfo> modelInputs(QString &error) const;
    QList<XOnnxTensorInfo> modelOutputs(QString &error) const;
    XvOnnx::ImagePreprocessConfig imagePreprocessConfig() const;

private:
    class Private;
    std::unique_ptr<Private> d;
    QString m_modelPath;
    InputLayout m_inputLayout=Auto;
    int m_inputWidth=0;
    int m_inputHeight=0;
    ImageChannelOrder m_channelOrder=Rgb;
    ImageResizeMode m_resizeMode=Stretch;
    int m_paddingValue=114;
    double m_pixelScale=1.0/255.0;
    QString m_meanValues="0";
    QString m_stdValues="1";
};
}

#endif // NONNXBASE_H
