#ifndef GLOBALSCRIPT_H
#define GLOBALSCRIPT_H
#include <QObject>
#include <QJsonObject>
#include <QProcess>
#include <QTimer>
class GlobalScriptRunner : public QObject {
    Q_OBJECT
public:
    explicit GlobalScriptRunner(QObject *parent=nullptr);
    ~GlobalScriptRunner() override;
    bool start(const QJsonObject &input,int timeoutMs=5000,const QString &program=QString());
    bool isRunning() const { return m_active; }
    void stop();
signals:
    void finished(bool success,const QJsonObject &result,const QString &error);
private:
    void finish(bool success,const QJsonObject &result,const QString &error);
    QProcess m_process;
    QTimer m_timer;
    QByteArray m_output;
    QString m_stopReason;
    bool m_active=false;
#ifdef Q_OS_WIN
    void *m_job=nullptr;
#endif
};
int runGlobalScriptWorker(int argc,char **argv);
#endif
