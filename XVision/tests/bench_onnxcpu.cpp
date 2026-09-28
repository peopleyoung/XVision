#include <QApplication>
#include <QCryptographicHash>
#include <QElapsedTimer>
#include <QFileInfo>
#include <QFile>
#include <QThread>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>
#include "NInference.h"
#include "OnnxSession.h"
#include "OnnxOperatorUtils.h"
#include "XLanguage.h"

class InferenceProbe:public XvCore::NInference
{
public:
    using XvCore::NOnnxBase::executeModel;
};

template<class F> QJsonObject timings(F function,int repeats=15)
{
    for(int i=0;i<3;++i) if(!function()) return {{"error",true}};
    std::vector<double> values;
    for(int i=0;i<repeats;++i)
    {
        QElapsedTimer timer;timer.start();
        if(!function()) return {{"error",true}};
        values.push_back(timer.nsecsElapsed()/1000000.0);
    }
    std::sort(values.begin(),values.end());
    return {{"median_ms",values[values.size()/2]},
            {"p95_ms",values[std::min(values.size()-1,size_t(values.size()*0.95))]},
            {"repeats",repeats}};
}
int main(int argc,char **argv)
{
    QApplication application(argc,argv);XLang->init();
    if(argc!=2 && argc!=3) return 2;
    const QString path=QFileInfo(QString::fromLocal8Bit(argv[1])).absoluteFilePath();
    InferenceProbe operation;
    QString error;
    QElapsedTimer timer;timer.start();
    if(!operation.configureModel(path,error)) { qCritical().noquote()<<error;return 3; }
    const double configureMs=timer.nsecsElapsed()/1000000.0;
    OnnxSession session;
    if(!session.load(path,&error)) { qCritical().noquote()<<error;return 4; }
    XOnnxTensorData input;input.name="input";input.elementType="float32";
    input.dimensions={1,16,128,128};input.bytes.resize(16*128*128*int(sizeof(float)));
    for(int i=0;i<16*128*128;++i)
    {
        const float value=float(i%127)*0.01f;
        std::memcpy(input.bytes.data()+i*sizeof(float),&value,sizeof(value));
    }
    QList<XOnnxTensorData> inputs{input},outputs,directOutputs;
    const QJsonObject full=timings([&] { return operation.executeModel(inputs,outputs,error); });
    const QJsonObject direct=timings([&] { return session.run(inputs,directOutputs,&error); });
    if(full.contains("error") || direct.contains("error") || outputs.isEmpty()
            || outputs.first().bytes!=directOutputs.first().bytes)
    { qCritical().noquote()<<"Benchmark failed:"<<error;return 5; }
    double sum=0.0;
    for(int i=0;i<outputs.first().bytes.size()/int(sizeof(float));++i)
    {
        float value;std::memcpy(&value,outputs.first().bytes.constData()+i*sizeof(float),sizeof(value));
        if(!std::isfinite(value)) return 6;
        sum+=value;
    }
    QImage image(1920,1080,QImage::Format_RGB888);image.fill(QColor(71,127,219));
    XOnnxTensorInfo info;info.name="image";info.elementType="float32";info.dimensions={1,3,640,640};
    XvOnnx::ImagePreprocessConfig config;config.layout=XvOnnx::TensorLayout::Nchw;
    XOnnxTensorData preprocessed;XvOnnx::ImageTransform transform;
    const QJsonObject preprocessing=timings([&] { return XvOnnx::preprocessImage(image,info,config,preprocessed,transform,error); });
    if(preprocessing.contains("error")) { qCritical().noquote()<<error;return 7; }
    if(argc==3)
    {
        QFile dump(QString::fromLocal8Bit(argv[2]));
        if(!dump.open(QIODevice::WriteOnly) || dump.write(outputs.first().bytes)!=outputs.first().bytes.size()) return 8;
    }
    const QJsonObject result{{"cpu_threads",qEnvironmentVariable("XVISION_ONNX_THREADS",QString::number(qMin(4,qMax(1,QThread::idealThreadCount()-1))))},
        {"model",QFileInfo(path).fileName()},
        {"model_bytes",double(QFileInfo(path).size())},{"configure_ms",configureMs},
        {"operator",full},{"runtime",direct},{"preprocess_1080p_to_640",preprocessing},
        {"output_sum",sum},{"output_bytes",outputs.first().bytes.size()},
        {"output_sha256",QString::fromLatin1(QCryptographicHash::hash(outputs.first().bytes,QCryptographicHash::Sha256).toHex())}};
    QTextStream(stdout)<<QJsonDocument(result).toJson(QJsonDocument::Compact)<<'\n';
    return 0;
}
