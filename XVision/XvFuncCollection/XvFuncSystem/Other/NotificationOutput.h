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
        inputImage=new XImage("inputImage",QImage(),this,"Input image");
        message=new XString("message","",this,"Message");
    }

    XImage *inputImage=nullptr;
    XString *message=nullptr;
};

class NotificationOutputResult:public XvBaseResult
{
public:
    NotificationOutputResult()
    {
        outputImage=new XImage("outputImage",QImage(),this,"Output image");
        published=new XBool("published",false,this,"Published");
        accepted=new XBool("accepted",false,this,"Accepted");
        notificationKind=new XInt("notificationKind",0,this,"Notification kind");
        publishedMessage=new XString("publishedMessage","",this,"Published message");
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
    Q_PROPERTY(NotificationOutput::Mode mode READ mode WRITE setMode)
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
