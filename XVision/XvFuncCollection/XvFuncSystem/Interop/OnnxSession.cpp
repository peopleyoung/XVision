#include "OnnxSession.h"

#include <QFileInfo>
#include <QHash>
#include <QSet>

#include <algorithm>
#include <cstring>
#include <exception>
#include <limits>
#include <mutex>
#include <utility>
#include <vector>

#if defined(XVISION_ENABLE_ONNXRUNTIME)
#include <onnxruntime_cxx_api.h>

class OnnxSession::Impl
{
public:
    Ort::Env env{ORT_LOGGING_LEVEL_WARNING,"XVision"};
    std::unique_ptr<Ort::Session> session;
    QString path;
    QList<XOnnxTensorInfo> inputInfo;
    QList<XOnnxTensorInfo> outputInfo;
    mutable std::mutex mutex;
};
#else
class OnnxSession::Impl
{
public:
    QString path;
    mutable std::mutex mutex;
};
#endif

namespace
{
void setError(QString *error,const QString &message)
{
    if(error) *error=message;
}

#if defined(XVISION_ENABLE_ONNXRUNTIME)
int elementSize(const QString &elementType)
{
    if(elementType=="uint8" || elementType=="int8" || elementType=="bool") return 1;
    if(elementType=="uint16" || elementType=="int16"
            || elementType=="float16" || elementType=="bfloat16") return 2;
    if(elementType=="uint32" || elementType=="int32" || elementType=="float32") return 4;
    if(elementType=="uint64" || elementType=="int64"
            || elementType=="float64" || elementType=="complex64") return 8;
    if(elementType=="complex128") return 16;
    return 0;
}

ONNXTensorElementDataType elementTypeValue(const QString &elementType)
{
    if(elementType=="float32") return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
    if(elementType=="uint8") return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8;
    if(elementType=="int8") return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8;
    if(elementType=="uint16") return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16;
    if(elementType=="int16") return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16;
    if(elementType=="int32") return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32;
    if(elementType=="int64") return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64;
    if(elementType=="bool") return ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL;
    if(elementType=="float16") return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16;
    if(elementType=="float64") return ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE;
    if(elementType=="uint32") return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32;
    if(elementType=="uint64") return ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64;
    if(elementType=="complex64") return ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64;
    if(elementType=="complex128") return ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128;
    if(elementType=="bfloat16") return ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16;
    return ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED;
}

QString elementTypeName(ONNXTensorElementDataType type)
{
    switch(type)
    {
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: return "float32";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8: return "uint8";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT8: return "int8";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT16: return "uint16";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT16: return "int16";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: return "int32";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: return "int64";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_STRING: return "string";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: return "bool";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: return "float16";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_DOUBLE: return "float64";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT32: return "uint32";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT64: return "uint64";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX64: return "complex64";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_COMPLEX128: return "complex128";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_BFLOAT16: return "bfloat16";
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED:
    default:
        return QString("unsupported:%1").arg(static_cast<int>(type));
    }
}

bool tensorInfo(Ort::Session &session,bool input,size_t index,
                Ort::AllocatorWithDefaultOptions &allocator,
                XOnnxTensorInfo &info,QString &error)
{
    auto name=input?session.GetInputNameAllocated(index,allocator)
                   :session.GetOutputNameAllocated(index,allocator);
    if(!name || !name.get() || name.get()[0]=='\0')
    {
        error=QString("ONNX %1 %2 has no name")
                .arg(input?"input":"output").arg(static_cast<qulonglong>(index));
        return false;
    }

    const Ort::TypeInfo type=input?session.GetInputTypeInfo(index)
                                  :session.GetOutputTypeInfo(index);
    const auto tensorType=type.GetTensorTypeAndShapeInfo();
    info.name=QString::fromUtf8(name.get());
    info.elementType=elementTypeName(tensorType.GetElementType());
    const auto shape=tensorType.GetShape();
    info.dimensions.reserve(static_cast<qsizetype>(shape.size()));
    for(const auto dimension:shape)
        info.dimensions.append(static_cast<qint64>(dimension));
    return true;
}

bool validateDimensions(const QVector<qint64> &actual,
                        const QVector<qint64> &declared,
                        qint64 &elementCount,QString &error)
{
    if(actual.size()!=declared.size())
    {
        error=QString("ONNX tensor rank mismatch: expected %1, got %2")
                .arg(declared.size()).arg(actual.size());
        return false;
    }
    elementCount=1;
    for(int index=0;index<actual.size();++index)
    {
        const qint64 dimension=actual.at(index);
        if(dimension<=0)
        {
            error=QString("ONNX tensor dimension %1 must be positive").arg(index);
            return false;
        }
        if(declared.at(index)>0 && declared.at(index)!=dimension)
        {
            error=QString("ONNX tensor dimension %1 mismatch: expected %2, got %3")
                    .arg(index).arg(declared.at(index)).arg(dimension);
            return false;
        }
        if(elementCount>std::numeric_limits<qint64>::max()/dimension)
        {
            error="ONNX tensor element count overflows";
            return false;
        }
        elementCount*=dimension;
    }
    return true;
}
#endif
}

OnnxSession::OnnxSession():m_impl(std::make_unique<Impl>())
{
}

OnnxSession::~OnnxSession()=default;

OnnxSession &OnnxSession::operator=(OnnxSession &&other) noexcept
{
    if(this!=&other)
    {
        std::scoped_lock locker(m_impl->mutex,other.m_impl->mutex);
        m_impl.swap(other.m_impl);
    }
    return *this;
}

bool OnnxSession::load(const QString &modelPath,QString *error)
{
    const QFileInfo file(modelPath);
    if(modelPath.trimmed().isEmpty() || !file.isAbsolute() || !file.isFile() || !file.isReadable())
    {
        setError(error,"ONNX model path must be an absolute readable file");
        return false;
    }
#if defined(XVISION_ENABLE_ONNXRUNTIME)
    try
    {
        Ort::SessionOptions options;
        options.SetIntraOpNumThreads(1);
        options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_EXTENDED);
#ifdef Q_OS_WIN
        const std::wstring nativePath=modelPath.toStdWString();
        auto candidate=std::make_unique<Ort::Session>(m_impl->env,nativePath.c_str(),options);
#else
        const QByteArray nativePath=modelPath.toUtf8();
        auto candidate=std::make_unique<Ort::Session>(m_impl->env,nativePath.constData(),options);
#endif
        Ort::AllocatorWithDefaultOptions allocator;
        QList<XOnnxTensorInfo> inputInfo;
        QList<XOnnxTensorInfo> outputInfo;
        const size_t inputCount=candidate->GetInputCount();
        for(size_t index=0;index<inputCount;++index)
        {
            XOnnxTensorInfo info;
            QString metadataError;
            if(!tensorInfo(*candidate,true,index,allocator,info,metadataError))
            {
                setError(error,metadataError);
                return false;
            }
            inputInfo.append(info);
        }
        const size_t outputCount=candidate->GetOutputCount();
        for(size_t index=0;index<outputCount;++index)
        {
            XOnnxTensorInfo info;
            QString metadataError;
            if(!tensorInfo(*candidate,false,index,allocator,info,metadataError))
            {
                setError(error,metadataError);
                return false;
            }
            outputInfo.append(info);
        }
        QSet<QString> inputNames;
        for(const XOnnxTensorInfo &info:inputInfo)
        {
            if(inputNames.contains(info.name))
            {
                setError(error,QString("ONNX model has duplicate input name '%1'")
                                 .arg(info.name));
                return false;
            }
            inputNames.insert(info.name);
        }
        QSet<QString> outputNames;
        for(const XOnnxTensorInfo &info:outputInfo)
        {
            if(outputNames.contains(info.name))
            {
                setError(error,QString("ONNX model has duplicate output name '%1'")
                                 .arg(info.name));
                return false;
            }
            outputNames.insert(info.name);
        }
        {
            std::lock_guard<std::mutex> locker(m_impl->mutex);
            m_impl->session=std::move(candidate);
            m_impl->path=file.absoluteFilePath();
            m_impl->inputInfo=inputInfo;
            m_impl->outputInfo=outputInfo;
        }
        setError(error,QString());
        return true;
    }
    catch(const Ort::Exception &exception)
    {
        setError(error,QString("ONNX Runtime failed to load model: %1")
                         .arg(QString::fromUtf8(exception.what())));
        return false;
    }
    catch(const std::exception &exception)
    {
        setError(error,QString("ONNX model loading failed: %1")
                         .arg(QString::fromUtf8(exception.what())));
        return false;
    }
#else
    Q_UNUSED(file);
    setError(error,"ONNX Runtime backend is disabled; configure with XVISION_ENABLE_ONNXRUNTIME=ON");
    return false;
#endif
}

void OnnxSession::clear()
{
    std::lock_guard<std::mutex> locker(m_impl->mutex);
#if defined(XVISION_ENABLE_ONNXRUNTIME)
    m_impl->session.reset();
    m_impl->inputInfo.clear();
    m_impl->outputInfo.clear();
#endif
    m_impl->path.clear();
}

bool OnnxSession::isLoaded() const
{
    std::lock_guard<std::mutex> locker(m_impl->mutex);
#if defined(XVISION_ENABLE_ONNXRUNTIME)
    return m_impl->session!=nullptr;
#else
    return false;
#endif
}

QString OnnxSession::modelPath() const
{
    std::lock_guard<std::mutex> locker(m_impl->mutex);
    return m_impl->path;
}

QList<XOnnxTensorInfo> OnnxSession::inputs() const
{
    std::lock_guard<std::mutex> locker(m_impl->mutex);
#if defined(XVISION_ENABLE_ONNXRUNTIME)
    return m_impl->inputInfo;
#else
    return {};
#endif
}

QList<XOnnxTensorInfo> OnnxSession::outputs() const
{
    std::lock_guard<std::mutex> locker(m_impl->mutex);
#if defined(XVISION_ENABLE_ONNXRUNTIME)
    return m_impl->outputInfo;
#else
    return {};
#endif
}

bool OnnxSession::run(const QList<XOnnxTensorData> &inputs,
                      QList<XOnnxTensorData> &outputs,QString *error)
{
#if !defined(XVISION_ENABLE_ONNXRUNTIME)
    Q_UNUSED(inputs);
    Q_UNUSED(outputs);
    setError(error,"ONNX Runtime backend is disabled; configure with XVISION_ENABLE_ONNXRUNTIME=ON");
    return false;
#else
    std::lock_guard<std::mutex> locker(m_impl->mutex);
    if(!m_impl->session)
    {
        setError(error,"ONNX model is not loaded");
        return false;
    }
    if(inputs.size()!=m_impl->inputInfo.size())
    {
        setError(error,QString("ONNX input count mismatch: expected %1, got %2")
                         .arg(m_impl->inputInfo.size()).arg(inputs.size()));
        return false;
    }

    try
    {
        QHash<QString,int> inputIndex;
        for(int index=0;index<m_impl->inputInfo.size();++index)
            inputIndex.insert(m_impl->inputInfo.at(index).name,index);

        struct PreparedInput
        {
            int index=0;
            Ort::Value value;
        };
        std::vector<PreparedInput> prepared;
        prepared.reserve(static_cast<size_t>(inputs.size()));
        QSet<int> usedIndexes;
        Ort::AllocatorWithDefaultOptions allocator;
        for(int candidateIndex=0;candidateIndex<inputs.size();++candidateIndex)
        {
            const XOnnxTensorData &candidate=inputs.at(candidateIndex);
            const int modelIndex=candidate.name.trimmed().isEmpty()
                    ?candidateIndex:inputIndex.value(candidate.name,-1);
            if(modelIndex<0 || usedIndexes.contains(modelIndex))
            {
                setError(error,QString("ONNX input name is unknown or duplicated: '%1'")
                                 .arg(candidate.name));
                return false;
            }
            usedIndexes.insert(modelIndex);
            const XOnnxTensorInfo &expected=m_impl->inputInfo.at(modelIndex);
            if(candidate.elementType!=expected.elementType)
            {
                setError(error,QString("ONNX input '%1' type mismatch: expected %2, got %3")
                                 .arg(expected.name,expected.elementType,candidate.elementType));
                return false;
            }
            const int bytesPerElement=elementSize(candidate.elementType);
            const ONNXTensorElementDataType type=elementTypeValue(candidate.elementType);
            if(bytesPerElement<=0 || type==ONNX_TENSOR_ELEMENT_DATA_TYPE_UNDEFINED)
            {
                setError(error,QString("ONNX input '%1' has unsupported type %2")
                                 .arg(expected.name,candidate.elementType));
                return false;
            }
            qint64 elementCount=0;
            QString validationError;
            if(!validateDimensions(candidate.dimensions,expected.dimensions,
                                   elementCount,validationError)
                    || elementCount>std::numeric_limits<qint64>::max()/bytesPerElement
                    || elementCount*bytesPerElement!=candidate.bytes.size())
            {
                if(validationError.isEmpty()) validationError="ONNX input byte count mismatch";
                setError(error,QString("ONNX input '%1': %2")
                                 .arg(expected.name,validationError));
                return false;
            }
            std::vector<int64_t> shape;
            shape.reserve(static_cast<size_t>(candidate.dimensions.size()));
            for(qint64 dimension:candidate.dimensions)
                shape.push_back(static_cast<int64_t>(dimension));
            Ort::Value value=Ort::Value::CreateTensor(
                        allocator,shape.data(),shape.size(),type);
            void *destination=value.GetTensorMutableData<void>();
            if(!destination)
            {
                setError(error,QString("ONNX input '%1' has no tensor storage")
                                 .arg(expected.name));
                return false;
            }
            std::memcpy(destination,candidate.bytes.constData(),
                        static_cast<size_t>(candidate.bytes.size()));
            prepared.push_back(PreparedInput{modelIndex,std::move(value)});
        }
        std::sort(prepared.begin(),prepared.end(),[](const PreparedInput &left,
                                                     const PreparedInput &right)
        {
            return left.index<right.index;
        });

        std::vector<QByteArray> inputNameStorage;
        std::vector<const char*> inputNames;
        std::vector<Ort::Value> inputValues;
        inputNameStorage.reserve(prepared.size());
        inputNames.reserve(prepared.size());
        inputValues.reserve(prepared.size());
        for(const PreparedInput &item:prepared)
            inputNameStorage.push_back(m_impl->inputInfo.at(item.index).name.toUtf8());
        for(const QByteArray &name:inputNameStorage) inputNames.push_back(name.constData());
        for(PreparedInput &item:prepared) inputValues.push_back(std::move(item.value));

        std::vector<QByteArray> outputNameStorage;
        std::vector<const char*> outputNames;
        outputNameStorage.reserve(static_cast<size_t>(m_impl->outputInfo.size()));
        outputNames.reserve(static_cast<size_t>(m_impl->outputInfo.size()));
        for(const XOnnxTensorInfo &info:m_impl->outputInfo)
            outputNameStorage.push_back(info.name.toUtf8());
        for(const QByteArray &name:outputNameStorage) outputNames.push_back(name.constData());

        std::vector<Ort::Value> runtimeOutputs=m_impl->session->Run(
                    Ort::RunOptions{nullptr},inputNames.data(),inputValues.data(),
                    inputValues.size(),outputNames.data(),outputNames.size());
        if(runtimeOutputs.size()!=outputNames.size())
        {
            setError(error,"ONNX Runtime returned an unexpected output count");
            return false;
        }

        QList<XOnnxTensorData> candidateOutputs;
        candidateOutputs.reserve(static_cast<qsizetype>(runtimeOutputs.size()));
        for(size_t index=0;index<runtimeOutputs.size();++index)
        {
            const Ort::Value &value=runtimeOutputs.at(index);
            if(!value.IsTensor())
            {
                setError(error,QString("ONNX output '%1' is not a tensor")
                                 .arg(m_impl->outputInfo.at(static_cast<int>(index)).name));
                return false;
            }
            const auto info=value.GetTensorTypeAndShapeInfo();
            XOnnxTensorData candidate;
            const XOnnxTensorInfo &declared=
                    m_impl->outputInfo.at(static_cast<int>(index));
            candidate.name=declared.name;
            candidate.elementType=elementTypeName(info.GetElementType());
            if(candidate.elementType!=declared.elementType)
            {
                setError(error,QString("ONNX output '%1' type mismatch: expected %2, got %3")
                                 .arg(candidate.name,declared.elementType,
                                      candidate.elementType));
                return false;
            }
            const int bytesPerElement=elementSize(candidate.elementType);
            if(bytesPerElement<=0)
            {
                setError(error,QString("ONNX output '%1' has unsupported type %2")
                                 .arg(candidate.name,candidate.elementType));
                return false;
            }
            const auto shape=info.GetShape();
            for(int64_t dimension:shape)
                candidate.dimensions.append(static_cast<qint64>(dimension));
            qint64 elementCount=0;
            QString validationError;
            if(!validateDimensions(candidate.dimensions,declared.dimensions,
                                   elementCount,validationError))
            {
                setError(error,QString("ONNX output '%1': %2")
                                 .arg(candidate.name,validationError));
                return false;
            }
            if(elementCount>std::numeric_limits<int>::max()/bytesPerElement)
            {
                setError(error,QString("ONNX output '%1' is too large")
                                 .arg(candidate.name));
                return false;
            }
            const void *source=value.GetTensorData<void>();
            if(!source)
            {
                setError(error,QString("ONNX output '%1' has no tensor storage")
                                 .arg(candidate.name));
                return false;
            }
            candidate.bytes=QByteArray(static_cast<const char*>(source),
                                       static_cast<int>(elementCount*bytesPerElement));
            candidateOutputs.append(candidate);
        }
        outputs=candidateOutputs;
        setError(error,QString());
        return true;
    }
    catch(const Ort::Exception &exception)
    {
        setError(error,QString("ONNX Runtime execution failed: %1")
                         .arg(QString::fromUtf8(exception.what())));
        return false;
    }
    catch(const std::exception &exception)
    {
        setError(error,QString("ONNX execution failed: %1")
                         .arg(QString::fromUtf8(exception.what())));
        return false;
    }
#endif
}
