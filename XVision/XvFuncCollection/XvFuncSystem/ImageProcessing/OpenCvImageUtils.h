#ifndef OPENCVIMAGEUTILS_H
#define OPENCVIMAGEUTILS_H

#include <QImage>
#include <QString>

namespace cv
{
class Mat;
}

namespace OpenCvImageUtils
{
bool fromQImage(const QImage &source,cv::Mat &target,QString &error);
bool toQImage(const cv::Mat &source,QImage &target,QString &error);
bool toGray(const cv::Mat &source,cv::Mat &target,QString &error);
bool toBgr(const cv::Mat &source,cv::Mat &target,QString &error);
bool toMask(const cv::Mat &source,cv::Mat &target,QString &error);
bool validateOddKernel(int width,int height,QString &error);
}

#endif // OPENCVIMAGEUTILS_H
