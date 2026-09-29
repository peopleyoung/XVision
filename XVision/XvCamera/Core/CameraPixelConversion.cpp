#include "VendorLibrary.h"

namespace XvCamera { namespace Hardware {
Result copyPackedImage(const void *data,int width,int height,int pixelType,quint64 length,QImage &image)
{
    image={};
    const bool rgb=pixelType==Abi::Rgb8 || pixelType==Abi::Bgr8;
    const bool bayer=pixelType>=0x01080008 && pixelType<=0x0108000b;
    if(!data || !validImageSize(width,height,rgb?3:1,length))
        return {EXvCameraError::IoError,QStringLiteral("相机图像尺寸或缓冲区长度无效")};
    if(pixelType==Abi::Mono8 || rgb)
    {
        image=QImage(static_cast<const uchar*>(data),width,height,width*(rgb?3:1),
                     rgb?QImage::Format_RGB888:QImage::Format_Grayscale8).copy();
        if(pixelType==Abi::Bgr8) image=image.rgbSwapped();
    }
    else if(bayer && width>=2 && height>=2)
    {
        // Bilinear reconstruction with in-bounds neighbours at sensor borders.
        // PFNC order: GR, RG, GB, BG; RGB channel indices are 0,1,2.
        const int patterns[4][4]={{1,0,2,1},{0,1,1,2},{1,2,0,1},{2,1,1,0}};
        const int *pattern=patterns[pixelType-0x01080008];
        const auto *source=static_cast<const uchar*>(data);
        image=QImage(width,height,QImage::Format_RGB888);
        for(int y=0;y<height && !image.isNull();++y)
        {
            auto *destination=image.scanLine(y);
            for(int x=0;x<width;++x)
            {
                const int own=pattern[(y%2)*2+x%2];
                if(x>0 && x+1<width && y>0 && y+1<height)
                {
                    const int position=y*width+x;
                    const int horizontal=(int(source[position-1])+source[position+1])/2;
                    const int vertical=(int(source[position-width])+source[position+width])/2;
                    destination[x*3+own]=source[position];
                    if(own==1)
                    {
                        const bool redHorizontal=pattern[(y%2)*2+(x+1)%2]==0;
                        destination[x*3]=uchar(redHorizontal?horizontal:vertical);
                        destination[x*3+2]=uchar(redHorizontal?vertical:horizontal);
                    }
                    else
                    {
                        destination[x*3+1]=uchar((int(source[position-1])+source[position+1]+
                            source[position-width]+source[position+width])/4);
                        destination[x*3+2-own]=uchar((int(source[position-width-1])+source[position-width+1]+
                            source[position+width-1]+source[position+width+1])/4);
                    }
                    continue;
                }
                int sum[3]={0,0,0},count[3]={0,0,0};
                for(int dy=-1;dy<=1;++dy) for(int dx=-1;dx<=1;++dx)
                {
                    const int xx=x+dx,yy=y+dy;
                    if(xx<0 || xx>=width || yy<0 || yy>=height) continue;
                    const int channel=pattern[(yy%2)*2+xx%2];
                    sum[channel]+=source[yy*width+xx]; ++count[channel];
                }
                for(int channel=0;channel<3;++channel)
                    destination[x*3+channel]=uchar(channel==own?source[y*width+x]:sum[channel]/std::max(1,count[channel]));
            }
        }
    }
    else return {EXvCameraError::Unsupported,QStringLiteral("暂不支持像素格式 0x%1，请在厂商软件中选择 Mono8、Bayer8 或 RGB8")
        .arg(quint32(pixelType),8,16,QLatin1Char('0'))};
    if(image.isNull()) return {EXvCameraError::IoError,QStringLiteral("无法分配相机图像内存")};
    return {};
}
} }
