#include "GlobalScript.h"
#include <QCoreApplication>
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJSEngine>
#include <QJSValue>
#include <cstdio>
#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif
#ifdef Q_OS_UNIX
#include <sys/resource.h>
#endif
namespace {
constexpr int MaximumBytes=4*1024*1024;
QJsonObject evaluate(const QJsonObject &input) {
    QJSEngine engine;
    // Closures keep the original snapshot separate from user changes. No host objects,
    // filesystem, network, GUI or project pointers are exposed to the JS engine.
    auto factory=engine.evaluate(QStringLiteral(R"JS((function(input) {
        'use strict';
        var own=Function.call.bind(Object.prototype.hasOwnProperty);
        var logs=[], pending=Object.create(null), values=Object.create(null);
        Object.keys(input.globals).forEach(function(k){values[k]=input.globals[k];});
        function read(object,key) {if(!own(object,key))throw new Error('变量不存在: '+key);return object[key];}
        function scalar(value) {if(typeof value!=='string' && typeof value!=='number' && typeof value!=='boolean')throw new Error('只支持布尔、整数、实数和字符串');if(typeof value==='number' && !isFinite(value))throw new Error('数值必须有限');if(typeof value==='string' && value.length>65536)throw new Error('字符串过长');return value;}
        function log() {if(logs.length>=100)return;var text=Array.prototype.map.call(arguments,String).join(' ');logs.push(text.slice(0,1024));}
        return {
            globals:Object.freeze({get:function(k){return read(values,k);},set:function(k,v){read(values,k);values[k]=scalar(v);}}),
            parameters:Object.freeze({get:function(k){return own(pending,k)?pending[k]:read(input.parameters,k);},set:function(k,v){read(input.parameters,k);pending[k]=scalar(v);}}),
            results:Object.freeze({get:function(k){return read(input.results,k);}}),
            console:Object.freeze({log:log,warn:log,error:log}),
            exportResult:function(){return {globals:values,parameters:pending,logs:logs};}
        };
    }))JS"));
    auto api=factory.call({engine.toScriptValue(input.toVariantMap())});
    if(api.isError()) return {{"ok",false},{"error",api.toString()}};
    for(const auto name:{"globals","parameters","results","console"}) engine.globalObject().setProperty(name,api.property(name));
    auto value=engine.evaluate(input.value("script").toString(),QStringLiteral("全局脚本.js"));
    if(value.isError()) return {{"ok",false},{"error",QStringLiteral("第 %1 行：%2").arg(value.property("lineNumber").toInt()).arg(value.toString())}};
    auto exporter=api.property("exportResult");auto output=exporter.call();
    if(output.isError()) return {{"ok",false},{"error",output.toString()}};
    auto result=QJsonObject::fromVariantMap(output.toVariant().toMap());result.insert("ok",true);return result;
}
}
int runGlobalScriptWorker(int argc,char **argv) {
    QCoreApplication app(argc,argv);
#ifdef Q_OS_UNIX
    // A runaway allocation stays within the disposable worker.
    struct rlimit limit;limit.rlim_cur=limit.rlim_max=1024ULL*1024*1024;
    setrlimit(RLIMIT_AS,&limit);
#endif
    QFile input;input.open(stdin,QIODevice::ReadOnly);const auto bytes=input.read(MaximumBytes+1);
    QJsonParseError error;const auto document=QJsonDocument::fromJson(bytes,&error);
    QJsonObject result;
    if(bytes.size()>MaximumBytes || error.error!=QJsonParseError::NoError || !document.isObject()) result={{"ok",false},{"error",QStringLiteral("脚本输入无效或过大")}};
    else result=evaluate(document.object());
    auto output=QJsonDocument(result).toJson(QJsonDocument::Compact);
    if(output.size()>MaximumBytes) output=QJsonDocument(QJsonObject{{"ok",false},{"error",QStringLiteral("脚本输出过大")}}).toJson(QJsonDocument::Compact);
    QFile out;out.open(stdout,QIODevice::WriteOnly);out.write(output);out.flush();return 0;
}
GlobalScriptRunner::GlobalScriptRunner(QObject *parent):QObject(parent) {
#ifdef Q_OS_WIN
    m_job=CreateJobObjectW(nullptr,nullptr);
    if(m_job) {
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits={};
        limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE|JOB_OBJECT_LIMIT_PROCESS_MEMORY;
        limits.ProcessMemoryLimit=512ULL*1024*1024;
        if(!SetInformationJobObject(m_job,JobObjectExtendedLimitInformation,&limits,sizeof(limits))) {CloseHandle(m_job);m_job=nullptr;}
    }
    connect(&m_process,&QProcess::started,this,[this]{
        HANDLE worker=OpenProcess(PROCESS_SET_QUOTA|PROCESS_TERMINATE,FALSE,DWORD(m_process.processId()));
        const bool bounded=m_job && worker && AssignProcessToJobObject(m_job,worker);
        if(worker)CloseHandle(worker);
        if(!bounded) {m_stopReason=QStringLiteral("无法设置脚本进程资源限制");m_process.kill();}
    });
#endif
    m_timer.setSingleShot(true);
    connect(&m_timer,&QTimer::timeout,this,[this]{m_stopReason=QStringLiteral("脚本运行超时，已停止；数据未修改");m_process.kill();});
    connect(&m_process,&QProcess::readyReadStandardOutput,this,[this]{
        m_output+=m_process.readAllStandardOutput();
        if(m_output.size()>MaximumBytes) {m_stopReason=QStringLiteral("脚本输出超出大小限制");m_process.kill();}
    });
    connect(&m_process,&QProcess::readyReadStandardError,this,[this]{m_process.readAllStandardError();});
    connect(&m_process,&QProcess::errorOccurred,this,[this](QProcess::ProcessError error){
        if(error==QProcess::FailedToStart) finish(false,{},QStringLiteral("无法启动脚本引擎: ")+m_process.errorString());
    });
    connect(&m_process,QOverload<int,QProcess::ExitStatus>::of(&QProcess::finished),this,[this](int code,QProcess::ExitStatus status){
        m_output+=m_process.readAllStandardOutput();
        if(!m_stopReason.isEmpty()) {finish(false,{},m_stopReason);return;}
        if(code!=0 || status!=QProcess::NormalExit) {finish(false,{},QStringLiteral("脚本引擎异常退出；数据未修改"));return;}
        QJsonParseError error;const auto doc=QJsonDocument::fromJson(m_output,&error);
        if(error.error!=QJsonParseError::NoError || !doc.isObject() || !doc.object().value("ok").isBool()) {finish(false,{},QStringLiteral("脚本引擎返回了无效数据"));return;}
        const auto result=doc.object();finish(result.value("ok").toBool(),result,result.value("error").toString());
    });
}
GlobalScriptRunner::~GlobalScriptRunner() {
    m_timer.stop();if(m_process.state()!=QProcess::NotRunning) {m_process.kill();m_process.waitForFinished(1000);}
#ifdef Q_OS_WIN
    if(m_job)CloseHandle(m_job);
#endif
}
bool GlobalScriptRunner::start(const QJsonObject &input,int timeoutMs,const QString &program) {
    if(m_active || timeoutMs<100 || timeoutMs>30000) return false;
    const auto bytes=QJsonDocument(input).toJson(QJsonDocument::Compact);if(bytes.size()>MaximumBytes) return false;
    m_active=true;m_output.clear();m_stopReason.clear();
    m_process.setProgram(program.isEmpty()?QCoreApplication::applicationFilePath():program);
    m_process.setArguments({QStringLiteral("--script-worker")});m_process.start();
    m_process.write(bytes);m_process.closeWriteChannel();m_timer.start(timeoutMs);return true;
}
void GlobalScriptRunner::stop() {if(m_active){m_stopReason=QStringLiteral("脚本已停止；数据未修改");m_process.kill();}}
void GlobalScriptRunner::finish(bool success,const QJsonObject &result,const QString &error) {
    if(!m_active)return;m_timer.stop();m_active=false;emit finished(success,result,error);
}
