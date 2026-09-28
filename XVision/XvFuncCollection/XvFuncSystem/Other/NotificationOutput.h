#ifndef NOTIFICATIONOUTPUT_H
#define NOTIFICATIONOUTPUT_H

#include "XVFuncSystemGlobal.h"
#include "XvFunc.h"

class NotificationOutputWdg;

namespace XvCore
{
class NotificationOutputParam:public XvBaseParam
{
public:
    NotificationOutputParam()
    {
        inputImage=new XImage("inputImage",QImage(),this,"输入图像");
        message=new XString("message","",this,"消息内容");
    }

    XImage *inputImage=nullptr;
    XString *message=nullptr;
};

class NotificationOutputResult:public XvBaseResult
{
public:
    NotificationOutputResult()
    {
        outputImage=new XImage("outputImage",QImage(),this,"输出图像");
        published=new XBool("published",false,this,"已发布");
        accepted=new XBool("accepted",false,this,"已接收");
        notificationKind=new XInt("notificationKind",0,this,"通知类型");
        publishedMessage=new XString("publishedMessage","",this,"已发布消息");
    }

    XImage *outputImage=nullptr;
    XBool *published=nullptr;
    XBool *accepted=nullptr;
    XInt *notificationKind=nullptr;
    XString *publishedMessage=nullptr;
};

class XVFUNCSYSTEM_EXPORT NotificationOutput:public XvFunc
{
    Q_OBJECT
    Q_PROPERTY(Mode mode READ mode WRITE setMode)
    friend class ::NotificationOutputWdg;
public:
    enum Mode
    {
        Ok=0,
        Ng=1,
        Info=2,
        Success=3,
        Warning=4,
        Error=5,
        Fatal=6,
        Dialog=7
    };
    Q_ENUM(Mode)

    Q_INVOKABLE explicit NotificationOutput(QObject *parent=nullptr);
    ~NotificationOutput() override;

    Mode mode() const { return m_mode; }
    void setMode(Mode mode) { m_mode=mode; }
    QStringList persistentPropertyNames() const override { return {"mode"}; }

public slots:
    void onShowFunc() override;

protected:
    QPixmap funcIcon() override { return QPixmap(":/images/LogOutput.svg"); }
    EXvFuncRunStatus run() override;
    XvBaseParam *getParam() const override { return m_param; }
    XvBaseResult *getResult() const override { return m_result; }
    bool readPersistentData(const QDomElement &dataElement,QString &error) override;

private:
    QString defaultMessage() const;

    NotificationOutputParam *m_param=nullptr;
    NotificationOutputResult *m_result=nullptr;
    NotificationOutputWdg *m_widget=nullptr;
    Mode m_mode=Ok;
};
}

#endif // NOTIFICATIONOUTPUT_H
