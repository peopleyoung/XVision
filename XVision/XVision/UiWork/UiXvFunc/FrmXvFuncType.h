#ifndef FRMXVFUNCTYPE_H
#define FRMXVFUNCTYPE_H
#include "BaseWidget.h"
#include "XvCoreDef.h"
class QLabel;
class QListWidget;
class FrmXvFuncType:public BaseWidget
{
    Q_OBJECT
public:
    explicit FrmXvFuncType(const XvCore::XvFuncTypeInfo &info,
                           const QList<XvCore::XvFuncInfo> &operators,QWidget *parent=nullptr);
    void setOperators(const XvCore::XvFuncTypeInfo &info,const QList<XvCore::XvFuncInfo> &operators);
    void setFilterText(const QString &text);
signals:
    void closeDrawer();
protected:
    void initFrm() override;
private:
    XvCore::XvFuncTypeInfo m_typeInfo;
    QList<XvCore::XvFuncInfo> m_operators;
    QListWidget *m_list=nullptr;
    QLabel *m_title=nullptr;
    QLabel *m_count=nullptr;
    QLabel *m_empty=nullptr;
};
#endif
