#include "HModelMatch.h"
#include "HModelMatchWdg.h"
#include "HalconDef.h"
#include "HalconImageInterop.h"
#include "XvXmlUtils.h"

#include <QtGlobal>

#include <exception>
#include <limits>

using namespace XvCore;

namespace
{
constexpr int CreateMode=0;
constexpr int FindMode=1;
constexpr int MaxMatches=1000;
constexpr int MaxModelAssetBytes=16*1024*1024;
constexpr int MaxModelAssetBase64Bytes=((MaxModelAssetBytes+2)/3)*4;
constexpr double FullTurn=6.28318530717958647692;

QString halconError(const HException &exception)
{
    const QString message=QString::fromLocal8Bit(exception.ErrorMessage().TextA());
    return message.isEmpty()
            ?QString("Halcon error %1").arg(exception.ErrorCode())
            :message;
}

bool hasNonWhitespaceText(const QDomElement &element)
{
    for(QDomNode node=element.firstChild();!node.isNull();node=node.nextSibling())
    {
        if((node.isText() || node.isCDATASection())
                && !node.nodeValue().trimmed().isEmpty())
        {
            return true;
        }
    }
    return false;
}

bool serializeModel(const HShapeModel &model,QByteArray &asset,QString &error)
{
    HSerializedItem item=model.SerializeShapeModel();
    Hlong size=0;
    const void *data=item.GetSerializedItemPtr(&size);
    if(!data || size<=0 || size>MaxModelAssetBytes
            || size>std::numeric_limits<int>::max())
    {
        error=size>MaxModelAssetBytes
                ?QString("shape model asset exceeds %1 bytes").arg(MaxModelAssetBytes)
                :QString("Halcon returned an empty shape model asset");
        return false;
    }
    asset=QByteArray(static_cast<const char*>(data),int(size));
    return true;
}

bool deserializeModel(const QByteArray &asset,HShapeModel &model,QString &error)
{
    if(asset.isEmpty() || asset.size()>MaxModelAssetBytes)
    {
        error=asset.isEmpty() ? QString("shape model asset is empty")
                              : QString("shape model asset is too large");
        return false;
    }
    try
    {
        HSerializedItem item(const_cast<char*>(asset.constData()),asset.size(),"true");
        HShapeModel candidate;
        candidate.DeserializeShapeModel(item);
        model=candidate;
        return true;
    }
    catch(const HException &exception)
    {
        error=halconError(exception);
    }
    catch(const std::exception &exception)
    {
        error=QString::fromLocal8Bit(exception.what());
    }
    catch(...)
    {
        error="unknown error while restoring Halcon shape model";
    }
    return false;
}

bool validateParameters(const HModelMatchParam *param,QString &error)
{
    if(!param || !param->mode || !param->minScore || !param->angleStart
            || !param->angleExtent || !param->numMatches || !param->maxOverlap
            || !param->greediness)
    {
        error="shape model parameters are incomplete";
        return false;
    }
    const int mode=param->mode->value();
    const double minScore=param->minScore->value();
    const double angleStart=param->angleStart->value();
    const double angleExtent=param->angleExtent->value();
    const int numMatches=param->numMatches->value();
    const double maxOverlap=param->maxOverlap->value();
    const double greediness=param->greediness->value();
    if((mode!=CreateMode && mode!=FindMode)
            || !qIsFinite(minScore) || minScore<0.0 || minScore>1.0
            || !qIsFinite(angleStart)
            || !qIsFinite(angleExtent) || angleExtent<=0.0 || angleExtent>FullTurn
            || numMatches<1 || numMatches>MaxMatches
            || !qIsFinite(maxOverlap) || maxOverlap<0.0 || maxOverlap>1.0
            || !qIsFinite(greediness) || greediness<0.0 || greediness>1.0)
    {
        error="shape model parameters are outside their supported ranges";
        return false;
    }
    return true;
}

HImage normalizedGrayImage(const QImage &image,QString &error)
{
    HImage halconImage;
    if(!XvHalconImageInterop::toHalcon(image,halconImage,&error)) return HImage();
    const Hlong channels=halconImage.CountChannels().L();
    if(channels==1) return halconImage;
    if(channels==3) return halconImage.Rgb1ToGray();
    error=QString("unsupported Halcon channel count %1").arg(channels);
    return HImage();
}
}

HModelMatch::HModelMatch(QObject *parent)
    :XvFunc(parent)
{
    _funcRole="HModelMatch";
    _funcName=getLang("XvFuncSystem_HModelMatch_Name","模板匹配(H)");
    _funcType=EXvFuncType::Location;

    param=new HModelMatchParam();
    result=new HModelMatchResult();
}

HModelMatch::~HModelMatch()
{
    if(m_frm)
    {
        delete m_frm;
        m_frm=nullptr;
    }
    delete param;
    param=nullptr;
    delete result;
    result=nullptr;
}

void HModelMatch::onShowFunc()
{
    if(!m_frm)
    {
        m_frm=new HModelMatchWdg(this);
    }
    m_frm->show();
    m_frm->raise();
}

EXvFuncRunStatus HModelMatch::run()
{
    QString error;
    if(!validateParameters(param,error))
    {
        setRunMsg(getLang("XvFuncSystem_HModelMatch_InvalidParameters",
                          "模板匹配参数超出有效范围")+QString(": %1").arg(error));
        return EXvFuncRunStatus::Error;
    }
    const QImage input=param->inputImage->value();
    if(input.isNull())
    {
        setRunMsg(getLang("XvFuncSystem_HModelMatch_EmptyImage","输入图像为空"));
        return EXvFuncRunStatus::Error;
    }

    try
    {
        HImage image=normalizedGrayImage(input,error);
        if(!error.isEmpty())
        {
            setRunMsg(getLang("XvFuncSystem_HModelMatch_ImageConversionFailed",
                              "图像转换失败")+QString(": %1").arg(error));
            return EXvFuncRunStatus::Error;
        }

        if(param->mode->value()==CreateMode)
        {
            HImage templateImage=image;
            if(param->useTemplateRoi->value())
            {
                const XRotateRectRoi *roi=param->templateRoi;
                HRegion region;
                region.GenRectangle2(roi->centerY(),roi->centerX(),roi->angle(),
                                     roi->length1(),roi->length2());
                templateImage=image.ReduceDomain(region);
            }
            HShapeModel model=templateImage.CreateShapeModel(
                        HTuple("auto"),param->angleStart->value(),
                        param->angleExtent->value(),HTuple("auto"),
                        HTuple("auto"),HString("use_polarity"),
                        HTuple("auto"),HTuple("auto"));
            QByteArray candidateAsset;
            if(!serializeModel(model,candidateAsset,error))
            {
                setRunMsg(getLang("XvFuncSystem_HModelMatch_SerializeFailed",
                                  "模板序列化失败")+QString(": %1").arg(error));
                return EXvFuncRunStatus::Error;
            }

            XObjectList emptyMatches("matches",XMatchResult::type());
            if(!result->matches->setData(&emptyMatches))
            {
                setRunMsg(getLang("XvFuncSystem_HModelMatch_ResultUpdateFailed",
                                  "模板匹配结果更新失败"));
                return EXvFuncRunStatus::Error;
            }
            if(!result->templateRoi->setData(param->templateRoi))
            {
                setRunMsg(getLang("XvFuncSystem_HModelMatch_ResultUpdateFailed",
                                  "模板匹配结果更新失败"));
                return EXvFuncRunStatus::Error;
            }
            m_modelAsset=candidateAsset;
            result->matchCount->setValue(0);
            result->outputImage->setValue(input);
            setRunMsg(getLang("XvFuncSystem_HModelMatch_CreateOk","模板创建成功"));
            return EXvFuncRunStatus::Ok;
        }

        if(m_modelAsset.isEmpty())
        {
            setRunMsg(getLang("XvFuncSystem_HModelMatch_ModelMissing","尚未创建模板"));
            return EXvFuncRunStatus::Error;
        }
        HShapeModel model;
        if(!deserializeModel(m_modelAsset,model,error))
        {
            setRunMsg(getLang("XvFuncSystem_HModelMatch_ModelInvalid",
                              "模板资产无效")+QString(": %1").arg(error));
            return EXvFuncRunStatus::Error;
        }

        HTuple rows;
        HTuple columns;
        HTuple angles;
        HTuple scores;
        image.FindShapeModel(model,param->angleStart->value(),
                             param->angleExtent->value(),param->minScore->value(),
                             param->numMatches->value(),param->maxOverlap->value(),
                             "least_squares",0,param->greediness->value(),
                             &rows,&columns,&angles,&scores);
        const Hlong count=rows.Length();
        if(columns.Length()!=count || angles.Length()!=count || scores.Length()!=count
                || count>MaxMatches)
        {
            setRunMsg(getLang("XvFuncSystem_HModelMatch_InvalidResult",
                              "Halcon 返回了无效匹配结果"));
            return EXvFuncRunStatus::Error;
        }

        XObjectList candidateMatches("matches",XMatchResult::type());
        for(Hlong index=0;index<count;++index)
        {
            auto match=new XMatchResult(QString("match%1").arg(index),
                                        columns[index].D(),rows[index].D(),
                                        angles[index].D(),scores[index].D());
            if(!match->setValue(columns[index].D(),rows[index].D(),
                                angles[index].D(),scores[index].D())
                    || !candidateMatches.addValue(match))
            {
                delete match;
                setRunMsg(getLang("XvFuncSystem_HModelMatch_InvalidResult",
                                  "Halcon 返回了无效匹配结果"));
                return EXvFuncRunStatus::Error;
            }
        }
        if(!result->matches->setData(&candidateMatches))
        {
            setRunMsg(getLang("XvFuncSystem_HModelMatch_ResultUpdateFailed",
                              "模板匹配结果更新失败"));
            return EXvFuncRunStatus::Error;
        }
        if(!result->templateRoi->setData(param->templateRoi))
        {
            setRunMsg(getLang("XvFuncSystem_HModelMatch_ResultUpdateFailed",
                              "模板匹配结果更新失败"));
            return EXvFuncRunStatus::Error;
        }
        result->matchCount->setValue(int(count));
        result->outputImage->setValue(input);
        setRunMsg(QString(getLang("XvFuncSystem_HModelMatch_FindOk",
                                  "模板匹配完成，共 %1 个结果")).arg(count));
        return EXvFuncRunStatus::Ok;
    }
    catch(const HException &exception)
    {
        setRunMsg(getLang("XvFuncSystem_HModelMatch_HalconFailed",
                          "Halcon 模板匹配失败")+QString(": %1").arg(halconError(exception)));
    }
    catch(const std::exception &exception)
    {
        setRunMsg(getLang("XvFuncSystem_HModelMatch_RunFailed","模板匹配失败")
                  +QString(": %1").arg(QString::fromLocal8Bit(exception.what())));
    }
    catch(...)
    {
        setRunMsg(getLang("XvFuncSystem_HModelMatch_RunFailed","模板匹配失败"));
    }
    return EXvFuncRunStatus::Error;
}

bool HModelMatch::appendPersistentData(QDomDocument &doc,
                                       QDomElement &functionElement,
                                       QString &error) const
{
    if(m_modelAsset.isEmpty()) return true;
    if(m_modelAsset.size()>MaxModelAssetBytes)
    {
        error="shape model asset exceeds the persistent size limit";
        return false;
    }
    QDomElement dataElement=doc.createElement("PersistentData");
    QDomElement modelElement=doc.createElement("ShapeModel");
    modelElement.setAttribute("format","halcon-shape-model");
    modelElement.setAttribute("version","1");
    modelElement.setAttribute("encoding","base64");
    modelElement.setAttribute("length",QString::number(m_modelAsset.size()));
    modelElement.appendChild(doc.createTextNode(QString::fromLatin1(m_modelAsset.toBase64())));
    dataElement.appendChild(modelElement);
    functionElement.appendChild(dataElement);
    return true;
}

bool HModelMatch::readPersistentData(const QDomElement &dataElement,QString &error)
{
    if(dataElement.isNull())
    {
        m_modelAsset.clear();
        return true;
    }
    if(!XvXml::validateAttributes(dataElement,{}, {},error)
            || !XvXml::validateChildren(dataElement,{"ShapeModel"},{"ShapeModel"},error)
            || hasNonWhitespaceText(dataElement))
    {
        if(error.isEmpty()) error="<PersistentData> contains unsupported text";
        return false;
    }
    const QDomElement modelElement=XvXml::singleChild(dataElement,"ShapeModel");
    if(!XvXml::validateAttributes(modelElement,
                                  {"format","version","encoding","length"},
                                  {"format","version","encoding","length"},error)
            || !XvXml::validateChildren(modelElement,{}, {},error))
    {
        return false;
    }
    if(modelElement.attribute("format")!="halcon-shape-model"
            || modelElement.attribute("version")!="1"
            || modelElement.attribute("encoding")!="base64")
    {
        error="unsupported Halcon shape model format, version, or encoding";
        return false;
    }
    unsigned int expectedLength=0;
    if(!XvXml::unsignedAttribute(modelElement,"length",expectedLength,error)
            || expectedLength==0 || expectedLength>MaxModelAssetBytes)
    {
        if(error.isEmpty()) error="shape model asset length is outside the supported range";
        return false;
    }
    const QString encodedText=modelElement.text();
    if(encodedText.isEmpty() || encodedText.size()>MaxModelAssetBase64Bytes)
    {
        error="shape model asset is not canonical Base64";
        return false;
    }
    const QByteArray encoded=encodedText.toLatin1();
    if(encoded.size()%4!=0
            || encoded.contains(' ')
            || encoded.contains('\n') || encoded.contains('\r') || encoded.contains('\t'))
    {
        error="shape model asset is not canonical Base64";
        return false;
    }
    const QByteArray decoded=QByteArray::fromBase64(encoded);
    if(decoded.size()!=int(expectedLength) || decoded.toBase64()!=encoded)
    {
        error="shape model asset Base64 or length is invalid";
        return false;
    }
    HShapeModel model;
    if(!deserializeModel(decoded,model,error)) return false;
    m_modelAsset=decoded;
    return true;
}
