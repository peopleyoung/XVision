#include "HalconImageInterop.h"

#include <QByteArray>
#include <QtGlobal>

#include <cstring>
#include <cmath>
#include <exception>
#include <limits>
#include <type_traits>

namespace
{
void setError(QString *error,const QString &message)
{
    if(error) *error=message;
}

QString halconError(const HalconCpp::HException &exception)
{
    const QString message=QString::fromLocal8Bit(exception.ErrorMessage().TextA());
    return message.isEmpty()
            ?QString("Halcon error %1").arg(exception.ErrorCode())
            :message;
}

bool validDimensions(Hlong width,Hlong height)
{
    return width>0 && height>0
            && width<=std::numeric_limits<int>::max()
            && height<=std::numeric_limits<int>::max()
            && width<=std::numeric_limits<int>::max()/height;
}

template<typename Source>
bool copyIntegralPixels(const void *pixels,int count,QVector<qint32> &target)
{
    if(!pixels || count<=0) return false;
    const Source *values=static_cast<const Source*>(pixels);
    QVector<qint32> candidate(count);
    for(int index=0;index<count;++index)
    {
        const auto value=values[index];
        if constexpr(sizeof(Source)>sizeof(qint32))
        {
            if(value<std::numeric_limits<qint32>::min()
                    || value>std::numeric_limits<qint32>::max())
            {
                return false;
            }
        }
        candidate[index]=qint32(value);
    }
    target=candidate;
    return true;
}
}

namespace XvHalconImageInterop
{

bool toHalcon(const QImage &source,HalconCpp::HImage &target,QString *error)
{
    setError(error,QString());
    if(source.isNull() || source.width()<=0 || source.height()<=0)
    {
        setError(error,"source QImage is empty");
        return false;
    }

    const int width=source.width();
    const int height=source.height();
    if(width>std::numeric_limits<int>::max()/height)
    {
        setError(error,"source QImage dimensions are too large");
        return false;
    }

    try
    {
        HalconCpp::HImage candidate;
        if(source.format()==QImage::Format_Grayscale8)
        {
            QByteArray pixels;
            pixels.resize(width*height);
            for(int row=0;row<height;++row)
            {
                std::memcpy(pixels.data()+row*width,source.constScanLine(row),
                            size_t(width));
            }
            candidate.GenImage1("byte",width,height,pixels.data());
        }
        else if(source.format()==QImage::Format_RGB888
                || source.format()==QImage::Format_RGB32
                || source.format()==QImage::Format_RGBA8888
                || source.format()==QImage::Format_ARGB32)
        {
            const QImage rgb=source.convertToFormat(QImage::Format_RGB888);
            if(rgb.isNull())
            {
                setError(error,"failed to normalize QImage to RGB888");
                return false;
            }
            const int pixelCount=width*height;
            QByteArray red;
            QByteArray green;
            QByteArray blue;
            red.resize(pixelCount);
            green.resize(pixelCount);
            blue.resize(pixelCount);
            for(int row=0;row<height;++row)
            {
                const uchar *line=rgb.constScanLine(row);
                const int rowOffset=row*width;
                for(int column=0;column<width;++column)
                {
                    const int sourceOffset=column*3;
                    const int targetOffset=rowOffset+column;
                    red[targetOffset]=char(line[sourceOffset]);
                    green[targetOffset]=char(line[sourceOffset+1]);
                    blue[targetOffset]=char(line[sourceOffset+2]);
                }
            }
            candidate.GenImage3("byte",width,height,red.data(),green.data(),blue.data());
        }
        else
        {
            setError(error,QString("unsupported QImage format %1").arg(source.format()));
            return false;
        }
        target=candidate;
        return true;
    }
    catch(const HalconCpp::HException &exception)
    {
        setError(error,halconError(exception));
    }
    catch(const std::exception &exception)
    {
        const QString message=QString::fromLocal8Bit(exception.what());
        setError(error,message.isEmpty()?"image conversion failed":message);
    }
    catch(...)
    {
        setError(error,"unknown error while converting QImage to Halcon image");
    }
    return false;
}

bool toQImage(const HalconCpp::HImage &source,QImage &target,QString *error)
{
    setError(error,QString());
    try
    {
        const Hlong channelCount=source.CountChannels().L();
        if(channelCount==1)
        {
            HalconCpp::HString type;
            Hlong width=0;
            Hlong height=0;
            void *pixels=source.GetImagePointer1(&type,&width,&height);
            if(type!="byte" || !pixels || !validDimensions(width,height))
            {
                setError(error,"Halcon image must be a non-empty byte image");
                return false;
            }
            QImage candidate(int(width),int(height),QImage::Format_Grayscale8);
            if(candidate.isNull())
            {
                setError(error,"failed to allocate grayscale QImage");
                return false;
            }
            const uchar *sourcePixels=static_cast<const uchar*>(pixels);
            for(int row=0;row<candidate.height();++row)
            {
                std::memcpy(candidate.scanLine(row),
                            sourcePixels+row*candidate.width(),
                            size_t(candidate.width()));
            }
            target=candidate;
            return true;
        }
        if(channelCount==3)
        {
            void *red=nullptr;
            void *green=nullptr;
            void *blue=nullptr;
            HalconCpp::HString type;
            Hlong width=0;
            Hlong height=0;
            source.GetImagePointer3(&red,&green,&blue,&type,&width,&height);
            if(type!="byte" || !red || !green || !blue
                    || !validDimensions(width,height))
            {
                setError(error,"Halcon image must contain three byte channels");
                return false;
            }
            QImage candidate(int(width),int(height),QImage::Format_RGB888);
            if(candidate.isNull())
            {
                setError(error,"failed to allocate RGB QImage");
                return false;
            }
            const uchar *redPixels=static_cast<const uchar*>(red);
            const uchar *greenPixels=static_cast<const uchar*>(green);
            const uchar *bluePixels=static_cast<const uchar*>(blue);
            for(int row=0;row<candidate.height();++row)
            {
                uchar *line=candidate.scanLine(row);
                const int rowOffset=row*candidate.width();
                for(int column=0;column<candidate.width();++column)
                {
                    const int sourceOffset=rowOffset+column;
                    const int targetOffset=column*3;
                    line[targetOffset]=redPixels[sourceOffset];
                    line[targetOffset+1]=greenPixels[sourceOffset];
                    line[targetOffset+2]=bluePixels[sourceOffset];
                }
            }
            target=candidate;
            return true;
        }
        setError(error,QString("unsupported Halcon channel count %1").arg(channelCount));
    }
    catch(const HalconCpp::HException &exception)
    {
        setError(error,halconError(exception));
    }
    catch(const std::exception &exception)
    {
        const QString message=QString::fromLocal8Bit(exception.what());
        setError(error,message.isEmpty()?"image conversion failed":message);
    }
    catch(...)
    {
        setError(error,"unknown error while converting Halcon image to QImage");
    }
    return false;
}

bool toInt32Pixels(const HalconCpp::HImage &source,QVector<qint32> &target,
                   int &width,int &height,QString *error)
{
    setError(error,QString());
    try
    {
        if(source.CountChannels().L()!=1)
        {
            setError(error,"Halcon label image must have one channel");
            return false;
        }
        HalconCpp::HString type;
        Hlong sourceWidth=0;
        Hlong sourceHeight=0;
        void *pixels=source.GetImagePointer1(&type,&sourceWidth,&sourceHeight);
        if(!pixels || !validDimensions(sourceWidth,sourceHeight))
        {
            setError(error,"Halcon label image is empty or too large");
            return false;
        }
        const int count=int(sourceWidth*sourceHeight);
        QVector<qint32> candidate;
        bool copied=false;
        if(type=="byte") copied=copyIntegralPixels<quint8>(pixels,count,candidate);
        else if(type=="int1") copied=copyIntegralPixels<qint8>(pixels,count,candidate);
        else if(type=="int2") copied=copyIntegralPixels<qint16>(pixels,count,candidate);
        else if(type=="uint2") copied=copyIntegralPixels<quint16>(pixels,count,candidate);
        else if(type=="int4") copied=copyIntegralPixels<qint32>(pixels,count,candidate);
        else if(type=="int8") copied=copyIntegralPixels<qint64>(pixels,count,candidate);
        else
        {
            setError(error,QString("unsupported Halcon label image type '%1'")
                     .arg(QString::fromLatin1(type.TextA())));
            return false;
        }
        if(!copied)
        {
            setError(error,"Halcon label image contains values outside int32");
            return false;
        }
        target=candidate;
        width=int(sourceWidth);
        height=int(sourceHeight);
        return true;
    }
    catch(const HalconCpp::HException &exception)
    {
        setError(error,halconError(exception));
    }
    catch(const std::exception &exception)
    {
        const QString message=QString::fromLocal8Bit(exception.what());
        setError(error,message.isEmpty()?"label image conversion failed":message);
    }
    catch(...)
    {
        setError(error,"unknown error while converting Halcon label image");
    }
    return false;
}

bool toFloatPixels(const HalconCpp::HImage &source,QVector<float> &target,
                   int &width,int &height,QString *error)
{
    setError(error,QString());
    try
    {
        if(source.CountChannels().L()!=1)
        {
            setError(error,"Halcon confidence image must have one channel");
            return false;
        }
        HalconCpp::HString type;
        Hlong sourceWidth=0;
        Hlong sourceHeight=0;
        void *pixels=source.GetImagePointer1(&type,&sourceWidth,&sourceHeight);
        if(type!="real" || !pixels || !validDimensions(sourceWidth,sourceHeight))
        {
            setError(error,"Halcon confidence image must be a non-empty real image");
            return false;
        }
        const int count=int(sourceWidth*sourceHeight);
        const float *values=static_cast<const float*>(pixels);
        QVector<float> candidate(count);
        for(int index=0;index<count;++index)
        {
            if(!std::isfinite(double(values[index])))
            {
                setError(error,"Halcon confidence image contains a non-finite value");
                return false;
            }
            candidate[index]=values[index];
        }
        target=candidate;
        width=int(sourceWidth);
        height=int(sourceHeight);
        return true;
    }
    catch(const HalconCpp::HException &exception)
    {
        setError(error,halconError(exception));
    }
    catch(const std::exception &exception)
    {
        const QString message=QString::fromLocal8Bit(exception.what());
        setError(error,message.isEmpty()?"confidence image conversion failed":message);
    }
    catch(...)
    {
        setError(error,"unknown error while converting Halcon confidence image");
    }
    return false;
}

}
