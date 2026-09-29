#ifndef FRMXVFUNCASM_H
#define FRMXVFUNCASM_H
#include "BaseWidget.h"
#include "XvCoreDef.h"
#include <QPointer>
class QFrame;
class QLineEdit;
class QButtonGroup;
class QTimer;
class FrmXvFuncType;
class FrmXvFuncAsm:public BaseWidget
{
    Q_OBJECT
public:
    explicit FrmXvFuncAsm(QWidget *parent=nullptr);
    ~FrmXvFuncAsm();
    void setDrawerParWidget(QWidget *parent);
    bool eventFilter(QObject *watched,QEvent *event) override;
protected:
    void initFrm() override;
private:
    void showCategory(XvCore::EXvFuncType type,const QString &query=QString());
    void closePanel();
    void updatePanelGeometry();
    QPointer<QWidget> m_panelParent;
    QPointer<QFrame> m_panel;
    QPointer<FrmXvFuncType> m_contents;
    QLineEdit *m_search=nullptr;
    QButtonGroup *m_categories=nullptr;
    QTimer *m_searchTimer=nullptr;
    XvCore::EXvFuncType m_currentType=XvCore::EXvFuncType::Null;
};
#endif
