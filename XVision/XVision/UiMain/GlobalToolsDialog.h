#ifndef GLOBALTOOLSDIALOG_H
#define GLOBALTOOLSDIALOG_H
class QWidget;
namespace XvCore { class XvProject; }
void showGlobalManager(XvCore::XvProject *project,QWidget *parent);
void showVariableMonitor(XvCore::XvProject *project,QWidget *parent);
void showGlobalScript(XvCore::XvProject *project,QWidget *parent);
#endif
