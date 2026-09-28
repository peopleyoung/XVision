#include "OnnxOperatorUtils.h"

#include <QColor>
#include <QPainter>
#include <QtMath>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <limits>
#include <numeric>

namespace
{
bool isFiniteValue(double value)
{
    return std::isfinite(value);
}

bool product(const QVector<qint64> &dimensions,qint64 &count,QString &error)
{
    count=1;
    for(qint64 dimension:dimensions)
    {
        if(dimension<=0 || count>std::numeric_limits<qint64>::max()/dimension)
        {
            error="tensor dimensions are invalid or overflow";
            return false;
        }
        count*=dimension;
    }
    return true;
}

bool parseValues(const QString &text,int channels,bool requirePositive,
                 QVector<double> &values,QString &error)
{
    const QStringList parts=text.split(',');
    if(parts.size()!=1 && parts.size()!=channels)
    {
        error=QString("expected one or %1 comma-separated values").arg(channels);
        return false;
    }
    QVector<double> candidate;
    candidate.reserve(channels);
    for(const QString &part:parts)
    {
        bool ok=false;
        const double value=part.trimmed().toDouble(&ok);
        if(part.trimmed().isEmpty() || !ok || !isFiniteValue(value)
                || (requirePositive && value<=0.0))
        {
            error=QString("invalid normalization value '%1'").arg(part.trimmed());
            return false;
        }
        candidate.append(value);
    }
    if(candidate.size()==1)
        while(candidate.size()<channels) candidate.append(candidate.first());
    values=candidate;
    return true;
}

QString className(int classId,const QStringList &names)
{
    if(classId>=0 && classId<names.size() && !names.at(classId).trimmed().isEmpty())
        return names.at(classId).trimmed();
    return QString("Class %1").arg(classId);
}

QRgb classColor(int classId)
{
    if(classId==0) return qRgba(0,0,0,0);
    const quint32 seed=static_cast<quint32>(classId)*2654435761u;
    return qRgba(48+static_cast<int>((seed>>16)&0x9f),
                 48+static_cast<int>((seed>>8)&0x9f),
                 48+static_cast<int>(seed&0x9f),190);
}

double intersectionOverUnion(const XvOnnx::DetectionValue &left,
                             const XvOnnx::DetectionValue &right)
{
    const double x1=qMax(left.x,right.x);
    const double y1=qMax(left.y,right.y);
    const double x2=qMin(left.x+left.width,right.x+right.width);
    const double y2=qMin(left.y+left.height,right.y+right.height);
    const double intersection=qMax(0.0,x2-x1)*qMax(0.0,y2-y1);
    const double total=left.width*left.height+right.width*right.height-intersection;
    return total>0.0?intersection/total:0.0;
}

bool outputRows(const XOnnxTensorData &tensor,int expectedColumns,
                bool minimumColumns,int &rows,int &columns,
                QVector<double> &numbers,QString &error)
{
    if(tensor.dimensions.size()!=2 && tensor.dimensions.size()!=3)
    {
        error=QString("output '%1' must have rank 2 or 3").arg(tensor.name);
        return false;
    }
    int offset=0;
    if(tensor.dimensions.size()==3)
    {
        if(tensor.dimensions.at(0)!=1)
        {
            error=QString("output '%1' batch must be 1").arg(tensor.name);
            return false;
        }
        offset=1;
    }
    if(tensor.dimensions.at(offset)>std::numeric_limits<int>::max()
            || tensor.dimensions.at(offset+1)>std::numeric_limits<int>::max())
    {
        error=QString("output '%1' dimensions are too large").arg(tensor.name);
        return false;
    }
    rows=static_cast<int>(tensor.dimensions.at(offset));
    columns=static_cast<int>(tensor.dimensions.at(offset+1));
    if((minimumColumns && columns<expectedColumns)
            || (!minimumColumns && columns!=expectedColumns))
    {
        error=QString("output '%1' has %2 columns; expected %3%4")
                .arg(tensor.name).arg(columns)
                .arg(minimumColumns?"at least ":"").arg(expectedColumns);
        return false;
    }
    return XvOnnx::tensorToDoubles(tensor,numbers,error)
            && static_cast<qint64>(numbers.size())
            ==static_cast<qint64>(rows)*columns;
}

bool resizeSegmentation(const QVector<qint32> &sourceLabels,
                        const QVector<float> &sourceConfidence,
                        int sourceWidth,int sourceHeight,
                        int targetWidth,int targetHeight,
                        QVector<qint32> &targetLabels,
                        QVector<float> &targetConfidence)
{
    if(sourceWidth<=0 || sourceHeight<=0 || targetWidth<=0 || targetHeight<=0
            || sourceLabels.size()!=sourceWidth*sourceHeight
            || sourceConfidence.size()!=sourceLabels.size()) return false;
    targetLabels.resize(targetWidth*targetHeight);
    targetConfidence.resize(targetWidth*targetHeight);
    for(int y=0;y<targetHeight;++y)
    {
        const double sourceY=(static_cast<double>(y)+0.5)*sourceHeight/targetHeight-0.5;
        const int nearestY=qBound(0,qRound(sourceY),sourceHeight-1);
        const int y0=qBound(0,static_cast<int>(std::floor(sourceY)),sourceHeight-1);
        const int y1=qMin(y0+1,sourceHeight-1);
        const double fy=qBound(0.0,sourceY-y0,1.0);
        for(int x=0;x<targetWidth;++x)
        {
            const double sourceX=(static_cast<double>(x)+0.5)*sourceWidth/targetWidth-0.5;
            const int nearestX=qBound(0,qRound(sourceX),sourceWidth-1);
            const int x0=qBound(0,static_cast<int>(std::floor(sourceX)),sourceWidth-1);
            const int x1=qMin(x0+1,sourceWidth-1);
            const double fx=qBound(0.0,sourceX-x0,1.0);
            const int targetIndex=y*targetWidth+x;
            targetLabels[targetIndex]=sourceLabels.at(nearestY*sourceWidth+nearestX);
            const double top=sourceConfidence.at(y0*sourceWidth+x0)*(1.0-fx)
                    +sourceConfidence.at(y0*sourceWidth+x1)*fx;
            const double bottom=sourceConfidence.at(y1*sourceWidth+x0)*(1.0-fx)
                    +sourceConfidence.at(y1*sourceWidth+x1)*fx;
            targetConfidence[targetIndex]=static_cast<float>(top*(1.0-fy)+bottom*fy);
        }
    }
    return true;
}
}

QStringList XvOnnx::parseClassNames(const QString &text)
{
    QString normalized=text;
    normalized.replace("\r\n","\n");
    normalized.replace('\r','\n');
    QStringList names=normalized.split('\n');
    for(QString &name:names) name=name.trimmed();
    while(!names.isEmpty() && names.last().isEmpty()) names.removeLast();
    return names;
}

bool XvOnnx::tensorToDoubles(const XOnnxTensorData &tensor,
                             QVector<double> &values,QString &error)
{
    qint64 count=0;
    if(!product(tensor.dimensions,count,error)
            || count>std::numeric_limits<int>::max()) return false;
    int elementSize=0;
    if(tensor.elementType=="float32" || tensor.elementType=="int32"
            || tensor.elementType=="uint32") elementSize=4;
    else if(tensor.elementType=="float64" || tensor.elementType=="int64"
            || tensor.elementType=="uint64") elementSize=8;
    else if(tensor.elementType=="uint8" || tensor.elementType=="int8") elementSize=1;
    else
    {
        error=QString("tensor '%1' type %2 cannot be decoded as numbers")
                .arg(tensor.name,tensor.elementType);
        return false;
    }
    if(count*elementSize!=tensor.bytes.size())
    {
        error=QString("tensor '%1' byte count does not match its shape")
                .arg(tensor.name);
        return false;
    }
    QVector<double> candidate(static_cast<int>(count));
    const char *source=tensor.bytes.constData();
    for(int index=0;index<candidate.size();++index)
    {
        double value=0.0;
        if(tensor.elementType=="float32")
        {
            float number=0.0f;
            std::memcpy(&number,source+index*4,4);
            value=number;
        }
        else if(tensor.elementType=="float64")
            std::memcpy(&value,source+index*8,8);
        else if(tensor.elementType=="int32")
        {
            qint32 number=0;
            std::memcpy(&number,source+index*4,4);
            value=number;
        }
        else if(tensor.elementType=="uint32")
        {
            quint32 number=0;
            std::memcpy(&number,source+index*4,4);
            value=number;
        }
        else if(tensor.elementType=="int64")
        {
            qint64 number=0;
            std::memcpy(&number,source+index*8,8);
            value=static_cast<double>(number);
        }
        else if(tensor.elementType=="uint64")
        {
            quint64 number=0;
            std::memcpy(&number,source+index*8,8);
            value=static_cast<double>(number);
        }
        else if(tensor.elementType=="uint8")
            value=static_cast<quint8>(source[index]);
        else
            value=static_cast<qint8>(source[index]);
        if(!isFiniteValue(value))
        {
            error=QString("tensor '%1' contains a non-finite value").arg(tensor.name);
            return false;
        }
        candidate[index]=value;
    }
    values=candidate;
    error.clear();
    return true;
}

bool XvOnnx::softmax(const QVector<double> &values,QVector<double> &scores,
                     QString &error)
{
    if(values.isEmpty())
    {
        error="softmax input is empty";
        return false;
    }
    const double maximum=*std::max_element(values.cbegin(),values.cend());
    QVector<double> candidate(values.size());
    double total=0.0;
    for(int index=0;index<values.size();++index)
    {
        if(!isFiniteValue(values.at(index)))
        {
            error="softmax input contains a non-finite value";
            return false;
        }
        candidate[index]=std::exp(values.at(index)-maximum);
        total+=candidate.at(index);
    }
    if(!isFiniteValue(total) || total<=0.0)
    {
        error="softmax normalization failed";
        return false;
    }
    for(double &score:candidate) score/=total;
    scores=candidate;
    error.clear();
    return true;
}

bool XvOnnx::decodeAge(const XOnnxTensorData &tensor,double &age,QString &error)
{
    QVector<double> numbers;
    if(!tensorToDoubles(tensor,numbers,error) || numbers.isEmpty()) return false;
    double candidate=0.0;
    if(numbers.size()==1) candidate=numbers.first();
    else
    {
        QVector<double> scores;
        if(!softmax(numbers,scores,error)) return false;
        for(int index=0;index<scores.size();++index)
            candidate+=index*scores.at(index);
    }
    if(!isFiniteValue(candidate) || candidate<0.0)
    {
        error="age output must decode to a finite non-negative value";
        return false;
    }
    age=candidate;
    error.clear();
    return true;
}

bool XvOnnx::preprocessImage(const QImage &image,const XOnnxTensorInfo &inputInfo,
                             const ImagePreprocessConfig &config,
                             XOnnxTensorData &tensor,ImageTransform &transform,
                             QString &error)
{
    if(image.isNull() || inputInfo.dimensions.size()!=4)
    {
        error=image.isNull()?"ONNX image input is empty":"ONNX image input must have rank 4";
        return false;
    }
    if(inputInfo.elementType!="float32" && inputInfo.elementType!="uint8")
    {
        error=QString("ONNX image input type %1 is unsupported").arg(inputInfo.elementType);
        return false;
    }
    if(inputInfo.dimensions.at(0)>0 && inputInfo.dimensions.at(0)!=1)
    {
        error="ONNX image input batch must be 1";
        return false;
    }
    TensorLayout layout=config.layout;
    if(layout==TensorLayout::Auto)
    {
        const bool nchw=inputInfo.dimensions.at(1)==1 || inputInfo.dimensions.at(1)==3;
        const bool nhwc=inputInfo.dimensions.at(3)==1 || inputInfo.dimensions.at(3)==3;
        if(nchw==nhwc)
        {
            error="ONNX image layout is ambiguous; select NCHW or NHWC";
            return false;
        }
        layout=nchw?TensorLayout::Nchw:TensorLayout::Nhwc;
    }
    if(layout!=TensorLayout::Nchw && layout!=TensorLayout::Nhwc)
    {
        error="ONNX image layout is invalid";
        return false;
    }
    if(config.channelOrder!=ChannelOrder::Rgb
            && config.channelOrder!=ChannelOrder::Bgr)
    {
        error="ONNX image channel order is invalid";
        return false;
    }
    const int channelIndex=layout==TensorLayout::Nchw?1:3;
    const int heightIndex=layout==TensorLayout::Nchw?2:1;
    const int widthIndex=layout==TensorLayout::Nchw?3:2;
    const qint64 declaredChannels=inputInfo.dimensions.at(channelIndex);
    if(declaredChannels!=1 && declaredChannels!=3)
    {
        error="ONNX image input must declare one or three channels";
        return false;
    }
    const int channels=static_cast<int>(declaredChannels);
    const auto resolvedSize=[&](int index,int configured,const char *name,int &result)
    {
        const qint64 declared=inputInfo.dimensions.at(index);
        if(declared>std::numeric_limits<int>::max())
        {
            error=QString("ONNX image %1 is too large").arg(name);
            return false;
        }
        if(declared>0)
        {
            if(configured>0 && configured!=declared)
            {
                error=QString("configured ONNX image %1 does not match the model").arg(name);
                return false;
            }
            result=static_cast<int>(declared);
            return true;
        }
        if(configured<=0)
        {
            error=QString("dynamic ONNX image %1 requires an explicit value").arg(name);
            return false;
        }
        result=configured;
        return true;
    };
    int targetWidth=0;
    int targetHeight=0;
    if(!resolvedSize(widthIndex,config.inputWidth,"width",targetWidth)
            || !resolvedSize(heightIndex,config.inputHeight,"height",targetHeight)) return false;
    if(config.paddingValue<0 || config.paddingValue>255 || !isFiniteValue(config.pixelScale))
    {
        error="ONNX image padding or scale is invalid";
        return false;
    }
    QVector<double> means;
    QVector<double> standardDeviations;
    if(!parseValues(config.meanValues,channels,false,means,error)
            || !parseValues(config.stdValues,channels,true,standardDeviations,error)) return false;
    if(inputInfo.elementType=="uint8")
    {
        const bool meansZero=std::all_of(means.cbegin(),means.cend(),[](double value){return value==0.0;});
        const bool stdsOne=std::all_of(standardDeviations.cbegin(),standardDeviations.cend(),[](double value){return value==1.0;});
        if(config.pixelScale!=1.0 || !meansZero || !stdsOne)
        {
            error="uint8 ONNX image input requires identity normalization";
            return false;
        }
    }

    QImage prepared;
    ImageTransform candidateTransform;
    candidateTransform.sourceWidth=image.width();
    candidateTransform.sourceHeight=image.height();
    candidateTransform.modelWidth=targetWidth;
    candidateTransform.modelHeight=targetHeight;
    if(config.resizeMode==ResizeMode::Stretch)
    {
        prepared=image.scaled(targetWidth,targetHeight,Qt::IgnoreAspectRatio,
                              Qt::SmoothTransformation);
        candidateTransform.scaleX=static_cast<double>(targetWidth)/image.width();
        candidateTransform.scaleY=static_cast<double>(targetHeight)/image.height();
    }
    else if(config.resizeMode==ResizeMode::Letterbox)
    {
        const double scale=qMin(static_cast<double>(targetWidth)/image.width(),
                                static_cast<double>(targetHeight)/image.height());
        const int scaledWidth=qMax(1,qRound(image.width()*scale));
        const int scaledHeight=qMax(1,qRound(image.height()*scale));
        const QImage scaled=image.scaled(scaledWidth,scaledHeight,Qt::IgnoreAspectRatio,
                                         Qt::SmoothTransformation);
        prepared=QImage(targetWidth,targetHeight,QImage::Format_RGB888);
        prepared.fill(QColor(config.paddingValue,config.paddingValue,config.paddingValue));
        const int left=(targetWidth-scaledWidth)/2;
        const int top=(targetHeight-scaledHeight)/2;
        QPainter painter(&prepared);
        painter.drawImage(left,top,scaled);
        painter.end();
        candidateTransform.scaleX=static_cast<double>(scaledWidth)/image.width();
        candidateTransform.scaleY=static_cast<double>(scaledHeight)/image.height();
        candidateTransform.padX=left;
        candidateTransform.padY=top;
    }
    else
    {
        error="ONNX image resize mode is invalid";
        return false;
    }
    if(prepared.isNull())
    {
        error="ONNX image resize failed";
        return false;
    }

    const qint64 count=static_cast<qint64>(targetWidth)*targetHeight*channels;
    const int bytesPerElement=inputInfo.elementType=="float32"?4:1;
    if(count<=0 || count>std::numeric_limits<int>::max()/bytesPerElement)
    {
        error="ONNX image tensor is too large";
        return false;
    }
    XOnnxTensorData candidate;
    candidate.name=inputInfo.name;
    candidate.elementType=inputInfo.elementType;
    candidate.dimensions=layout==TensorLayout::Nchw
            ?QVector<qint64>{1,channels,targetHeight,targetWidth}
           :QVector<qint64>{1,targetHeight,targetWidth,channels};
    candidate.bytes.resize(static_cast<int>(count*bytesPerElement));
    // Normalize the 256 possible channel values once, keeping the original
    // double arithmetic and float32 rounding. Check only pixels actually used.
    float normalizedValues[3][256]{};
    bool validValues[3][256]{};
    const bool floatingPoint=inputInfo.elementType=="float32";
    if(floatingPoint)
    {
        for(int channel=0;channel<channels;++channel)
            for(int pixel=0;pixel<256;++pixel)
            {
                const double value=(pixel*config.pixelScale-means.at(channel))
                        /standardDeviations.at(channel);
                validValues[channel][pixel]=isFiniteValue(value)
                        && qAbs(value)<=std::numeric_limits<float>::max();
                if(validValues[channel][pixel]) normalizedValues[channel][pixel]=float(value);
            }
    }
    // Premultiplied/high-depth formats use QColor's original rounding rules.
    // Conversion routines can round their unpremultiplication differently.
    const bool fastPixels=prepared.format()==QImage::Format_ARGB32
        || prepared.format()==QImage::Format_RGB32 || prepared.format()==QImage::Format_RGB888
        || prepared.format()==QImage::Format_Grayscale8 || prepared.format()==QImage::Format_Indexed8;
    if(fastPixels) prepared=prepared.convertToFormat(QImage::Format_ARGB32);
    if(prepared.isNull()) { error="ONNX image conversion failed";return false; }
    char *destination=candidate.bytes.data();
    const qint64 plane=static_cast<qint64>(targetWidth)*targetHeight;
    for(int y=0;y<targetHeight;++y)
    {
        const auto row=reinterpret_cast<const QRgb*>(prepared.constScanLine(y));
        for(int x=0;x<targetWidth;++x)
        {
            const QRgb color=fastPixels?row[x]:prepared.pixelColor(x,y).rgb();
            const int rgb[3]={qRed(color),qGreen(color),qBlue(color)};
            const qint64 spatial=static_cast<qint64>(y)*targetWidth+x;
            for(int channel=0;channel<channels;++channel)
            {
                const int pixel=channels==1?qGray(color)
                    :rgb[config.channelOrder==ChannelOrder::Bgr?2-channel:channel];
                const qint64 index=layout==TensorLayout::Nchw
                    ?channel*plane+spatial:spatial*channels+channel;
                if(!floatingPoint) destination[index]=static_cast<char>(pixel);
                else
                {
                    if(!validValues[channel][pixel])
                    { error="ONNX image normalization overflowed float32";return false; }
                    std::memcpy(destination+index*4,&normalizedValues[channel][pixel],4);
                }
            }
        }
    }
    tensor=candidate;
    transform=candidateTransform;
    error.clear();
    return true;
}

bool XvOnnx::decodeClassification(const XOnnxTensorData &tensor,
                                  bool applySoftmax,int topK,double minScore,
                                  const QStringList &classNames,
                                  QVector<ClassificationValue> &values,
                                  QString &error)
{
    if(topK<=0 || !isFiniteValue(minScore) || minScore<0.0 || minScore>1.0
            || (tensor.dimensions.size()!=1 && tensor.dimensions.size()!=2)
            || (tensor.dimensions.size()==2 && tensor.dimensions.first()!=1))
    {
        error="classification parameters or output shape are invalid";
        return false;
    }
    QVector<double> numbers;
    if(!tensorToDoubles(tensor,numbers,error) || numbers.isEmpty()) return false;
    QVector<double> scores;
    if(applySoftmax)
    {
        if(!softmax(numbers,scores,error)) return false;
    }
    else
    {
        for(double score:numbers)
        {
            if(!isFiniteValue(score) || score<0.0 || score>1.0)
            {
                error="classification identity scores must be in [0,1]";
                return false;
            }
        }
        scores=numbers;
    }
    QVector<ClassificationValue> candidate;
    for(int classId=0;classId<scores.size();++classId)
        if(scores.at(classId)>=minScore)
            candidate.append({classId,className(classId,classNames),scores.at(classId)});
    std::stable_sort(candidate.begin(),candidate.end(),[](const ClassificationValue &left,
                                                          const ClassificationValue &right)
    {
        if(left.score!=right.score) return left.score>right.score;
        return left.classId<right.classId;
    });
    if(candidate.size()>topK) candidate.resize(topK);
    values=candidate;
    error.clear();
    return true;
}

bool XvOnnx::decodeDetections(const QList<XOnnxTensorData> &outputs,
                              DetectionDecoder decoder,const ImageTransform &transform,
                              bool normalizedCoordinates,double confidenceThreshold,
                              double iouThreshold,bool classAgnostic,int maximumDetections,
                              const QStringList &classNames,QVector<DetectionValue> &values,
                              QString &error)
{
    if(outputs.isEmpty() || transform.sourceWidth<=0 || transform.sourceHeight<=0
            || transform.modelWidth<=0 || transform.modelHeight<=0
            || !isFiniteValue(transform.scaleX) || transform.scaleX<=0.0
            || !isFiniteValue(transform.scaleY) || transform.scaleY<=0.0
            || !isFiniteValue(transform.padX) || !isFiniteValue(transform.padY)
            || !isFiniteValue(confidenceThreshold) || confidenceThreshold<0.0
            || confidenceThreshold>1.0 || !isFiniteValue(iouThreshold)
            || iouThreshold<0.0 || iouThreshold>1.0 || maximumDetections<=0)
    {
        error="detection inputs or parameters are invalid";
        return false;
    }
    QVector<DetectionValue> candidates;
    int sequence=0;
    QVector<int> sequences;
    for(const XOnnxTensorData &output:outputs)
    {
        int rows=0;
        int columns=0;
        QVector<double> numbers;
        const bool generic=decoder==DetectionDecoder::GenericXyxy;
        const int expected=decoder==DetectionDecoder::Yolov5Face?16:(generic?6:6);
        if(!outputRows(output,expected,!generic && decoder!=DetectionDecoder::Yolov5Face,
                       rows,columns,numbers,error)) return false;
        if(decoder==DetectionDecoder::Yolov5Face && columns!=16)
        {
            error=QString("output '%1' must have 16 columns for YOLOv5Face").arg(output.name);
            return false;
        }
        for(int row=0;row<rows;++row)
        {
            const double *item=numbers.constData()+row*columns;
            for(int column=0;column<columns;++column)
                if(!isFiniteValue(item[column]))
                {
                    error=QString("output '%1' contains a non-finite detection").arg(output.name);
                    return false;
                }
            double x1=0.0;
            double y1=0.0;
            double x2=0.0;
            double y2=0.0;
            double score=0.0;
            int classId=0;
            if(generic)
            {
                x1=item[0]; y1=item[1]; x2=item[2]; y2=item[3]; score=item[4];
                const double rounded=std::round(item[5]);
                if(item[5]!=rounded || rounded<0.0
                        || rounded>std::numeric_limits<int>::max())
                {
                    error="generic detection class ID is invalid";
                    return false;
                }
                classId=static_cast<int>(rounded);
            }
            else
            {
                const double centerX=item[0];
                const double centerY=item[1];
                const double width=item[2];
                const double height=item[3];
                x1=centerX-width/2.0; y1=centerY-height/2.0;
                x2=centerX+width/2.0; y2=centerY+height/2.0;
                if(decoder==DetectionDecoder::Yolov5Face)
                {
                    if(item[4]<0.0 || item[4]>1.0 || item[15]<0.0 || item[15]>1.0)
                    {
                        error="YOLOv5Face confidence values must be in [0,1]";
                        return false;
                    }
                    score=item[4]*item[15];
                }
                else
                {
                    const int classCount=columns-5;
                    if(item[4]<0.0 || item[4]>1.0)
                    {
                        error="YOLO objectness must be in [0,1]";
                        return false;
                    }
                    int bestClass=0;
                    double bestScore=item[5];
                    for(int classIndex=0;classIndex<classCount;++classIndex)
                    {
                        const double classScore=item[5+classIndex];
                        if(classScore<0.0 || classScore>1.0)
                        {
                            error="YOLO class scores must be in [0,1]";
                            return false;
                        }
                        if(classIndex>0 && classScore>bestScore)
                        {
                            bestScore=classScore;
                            bestClass=classIndex;
                        }
                    }
                    classId=bestClass;
                    score=item[4]*bestScore;
                }
            }
            if(score<0.0 || score>1.0)
            {
                error="detection score must be in [0,1]";
                return false;
            }
            if(score<confidenceThreshold) continue;
            if(normalizedCoordinates)
            {
                x1*=transform.modelWidth; x2*=transform.modelWidth;
                y1*=transform.modelHeight; y2*=transform.modelHeight;
            }
            x1=(x1-transform.padX)/transform.scaleX;
            x2=(x2-transform.padX)/transform.scaleX;
            y1=(y1-transform.padY)/transform.scaleY;
            y2=(y2-transform.padY)/transform.scaleY;
            x1=qBound(0.0,x1,static_cast<double>(transform.sourceWidth));
            x2=qBound(0.0,x2,static_cast<double>(transform.sourceWidth));
            y1=qBound(0.0,y1,static_cast<double>(transform.sourceHeight));
            y2=qBound(0.0,y2,static_cast<double>(transform.sourceHeight));
            if(x2<=x1 || y2<=y1)
            {
                error="detection box is empty after mapping to the source image";
                return false;
            }
            candidates.append({x1,y1,x2-x1,y2-y1,classId,
                               className(classId,classNames),score});
            sequences.append(sequence++);
        }
    }
    QVector<int> order(candidates.size());
    std::iota(order.begin(),order.end(),0);
    std::stable_sort(order.begin(),order.end(),[&](int left,int right)
    {
        const DetectionValue &a=candidates.at(left);
        const DetectionValue &b=candidates.at(right);
        if(a.score!=b.score) return a.score>b.score;
        if(a.classId!=b.classId) return a.classId<b.classId;
        return sequences.at(left)<sequences.at(right);
    });
    QVector<DetectionValue> accepted;
    for(int index:order)
    {
        const DetectionValue &candidate=candidates.at(index);
        bool suppressed=false;
        for(const DetectionValue &existing:accepted)
            if((classAgnostic || existing.classId==candidate.classId)
                    && intersectionOverUnion(existing,candidate)>iouThreshold)
            {
                suppressed=true;
                break;
            }
        if(!suppressed) accepted.append(candidate);
        if(accepted.size()>=maximumDetections) break;
    }
    values=accepted;
    error.clear();
    return true;
}

bool XvOnnx::decodeSegmentation(const XOnnxTensorData &output,
                                TensorLayout outputLayout,int sourceWidth,
                                int sourceHeight,double binaryThreshold,
                                const QStringList &classNames,
                                SegmentationValue &value,QString &error)
{
    if(sourceWidth<=0 || sourceHeight<=0 || classNames.size()>100000
            || static_cast<qint64>(sourceWidth)*sourceHeight
            >std::numeric_limits<int>::max() || !isFiniteValue(binaryThreshold)
            || binaryThreshold<0.0 || binaryThreshold>1.0)
    {
        error="segmentation dimensions or threshold are invalid";
        return false;
    }
    QVector<double> numbers;
    if(!tensorToDoubles(output,numbers,error)) return false;
    int width=0;
    int height=0;
    int channels=1;
    bool labelTensor=false;
    TensorLayout layout=outputLayout;
    if(output.dimensions.size()==3)
    {
        if(output.dimensions.at(0)!=1)
        {
            error="segmentation batch must be 1";
            return false;
        }
        if(output.dimensions.at(1)<=0 || output.dimensions.at(2)<=0
                || output.dimensions.at(1)>std::numeric_limits<int>::max()
                || output.dimensions.at(2)>std::numeric_limits<int>::max())
        {
            error="segmentation output dimensions are invalid";
            return false;
        }
        height=static_cast<int>(output.dimensions.at(1));
        width=static_cast<int>(output.dimensions.at(2));
        labelTensor=output.elementType=="int8" || output.elementType=="uint8"
                || output.elementType=="int32" || output.elementType=="uint32"
                || output.elementType=="int64" || output.elementType=="uint64";
    }
    else if(output.dimensions.size()==4)
    {
        if(output.dimensions.at(0)!=1)
        {
            error="segmentation batch must be 1";
            return false;
        }
        for(int index=1;index<4;++index)
            if(output.dimensions.at(index)<=0
                    || output.dimensions.at(index)>std::numeric_limits<int>::max())
            {
                error="segmentation output dimensions are invalid";
                return false;
            }
        if(layout==TensorLayout::Auto)
        {
            const qint64 first=output.dimensions.at(1);
            const qint64 second=output.dimensions.at(2);
            const qint64 third=output.dimensions.at(3);
            const int namedClasses=qMax(2,static_cast<int>(classNames.size()));
            const bool nchw=first>0 && ((first<=namedClasses)
                    || (first<second && first<third));
            const bool nhwc=third>0 && ((third<=namedClasses)
                    || (third<first && third<second));
            if(nchw==nhwc)
            {
                error="segmentation output layout is ambiguous";
                return false;
            }
            layout=nchw?TensorLayout::Nchw:TensorLayout::Nhwc;
        }
        if(layout==TensorLayout::Nchw)
        {
            channels=static_cast<int>(output.dimensions.at(1));
            height=static_cast<int>(output.dimensions.at(2));
            width=static_cast<int>(output.dimensions.at(3));
        }
        else if(layout==TensorLayout::Nhwc)
        {
            height=static_cast<int>(output.dimensions.at(1));
            width=static_cast<int>(output.dimensions.at(2));
            channels=static_cast<int>(output.dimensions.at(3));
        }
        else
        {
            error="segmentation output layout is invalid";
            return false;
        }
    }
    else
    {
        error="segmentation output must have rank 3 or 4";
        return false;
    }
    if(width<=0 || height<=0 || channels<=0
            || static_cast<qint64>(width)*height*channels!=numbers.size())
    {
        error="segmentation output dimensions are invalid";
        return false;
    }

    QVector<qint32> labels(width*height);
    QVector<float> confidences(width*height);
    int classCount=channels;
    if(labelTensor)
    {
        int maximumClass=0;
        for(int index=0;index<labels.size();++index)
        {
            const double rounded=std::round(numbers.at(index));
            if(numbers.at(index)!=rounded || rounded<0.0 || rounded>=100000.0)
            {
                error="segmentation label is not a non-negative integer";
                return false;
            }
            labels[index]=static_cast<qint32>(rounded);
            maximumClass=qMax(maximumClass,static_cast<int>(labels.at(index)));
            confidences[index]=1.0f;
        }
        classCount=qMax(maximumClass+1,static_cast<int>(classNames.size()));
    }
    else if(channels==1)
    {
        classCount=qMax(2,static_cast<int>(classNames.size()));
        for(int index=0;index<labels.size();++index)
        {
            const double probability=1.0/(1.0+std::exp(-numbers.at(index)));
            labels[index]=probability>=binaryThreshold?1:0;
            confidences[index]=static_cast<float>(labels.at(index)==1
                                                   ?probability:1.0-probability);
        }
    }
    else
    {
        QVector<double> pixelLogits(channels);
        QVector<double> pixelScores;
        for(int y=0;y<height;++y)
        {
            for(int x=0;x<width;++x)
            {
                for(int channel=0;channel<channels;++channel)
                {
                    const qint64 index=layout==TensorLayout::Nchw
                            ?static_cast<qint64>(channel)*height*width+y*width+x
                           :(static_cast<qint64>(y)*width+x)*channels+channel;
                    pixelLogits[channel]=numbers.at(static_cast<int>(index));
                }
                if(!softmax(pixelLogits,pixelScores,error)) return false;
                const auto best=std::max_element(pixelScores.cbegin(),pixelScores.cend());
                const int outputIndex=y*width+x;
                labels[outputIndex]=static_cast<qint32>(
                            std::distance(pixelScores.cbegin(),best));
                confidences[outputIndex]=static_cast<float>(*best);
            }
        }
    }

    QVector<qint32> resizedLabels;
    QVector<float> resizedConfidences;
    if(!resizeSegmentation(labels,confidences,width,height,sourceWidth,sourceHeight,
                           resizedLabels,resizedConfidences))
    {
        error="segmentation resize failed";
        return false;
    }
    SegmentationValue candidate;
    candidate.width=sourceWidth;
    candidate.height=sourceHeight;
    candidate.labels=resizedLabels;
    candidate.confidences=resizedConfidences;
    for(int classId=0;classId<classCount;++classId)
    {
        candidate.classIds.append(classId);
        candidate.classNames.append(className(classId,classNames));
        candidate.classColors.append(classColor(classId));
    }
    value=candidate;
    error.clear();
    return true;
}
