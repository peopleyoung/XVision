#include "FrmXvFuncAsm.h"
#include "FrmXvFuncType.h"
#include "XvFuncAssembly.h"
#include <QApplication>
#include <QButtonGroup>
#include <QFrame>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QTimer>
#include <QToolButton>
#include <QVBoxLayout>

FrmXvFuncAsm::FrmXvFuncAsm(QWidget *parent):BaseWidget(parent)
{
    initFrm();
    qApp->installEventFilter(this);
}

FrmXvFuncAsm::~FrmXvFuncAsm()
{
    qApp->removeEventFilter(this);
    // The panel belongs visually to the canvas, but its lifetime follows the sidebar.
    delete m_panel.data();
}

void FrmXvFuncAsm::initFrm()
{
    setObjectName("operatorSidebar");
    setWindowTitle(getLang("OperatorSidebar_Title","算子工具箱"));
    setFixedWidth(176);
    auto root=new QVBoxLayout(this);
    root->setContentsMargins(10,14,10,10);root->setSpacing(10);
    auto title=new QLabel(getLang("OperatorSidebar_Library","算子库"),this);
    title->setObjectName("operatorSidebarTitle");root->addWidget(title);
    m_search=new QLineEdit(this);
    m_search->setObjectName("operatorSearch");
    m_search->setPlaceholderText(getLang("OperatorSidebar_Search","搜索算子…"));
    m_search->setClearButtonEnabled(true);
    m_search->addAction(QIcon(":/images/Ui/OperatorSearch.svg"),QLineEdit::LeadingPosition);
    root->addWidget(m_search);
    auto scroll=new QScrollArea(this);
    scroll->setWidgetResizable(true);scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto content=new QWidget(scroll);auto layout=new QVBoxLayout(content);
    layout->setContentsMargins(0,0,0,0);layout->setSpacing(4);
    m_categories=new QButtonGroup(this);m_categories->setExclusive(true);
    int count=0;
    for(const auto &category:XvFuncAsm->getXvFuncTypeInfos())
    {
        const auto operators=XvFuncAsm->getXvFuncInfos(category.type);
        if(operators.isEmpty()) continue;
        ++count;
        auto button=new QToolButton(content);
        button->setObjectName("operatorCategory");
        button->setProperty("Type",QVariant::fromValue(category.type));
        button->setText(category.name+QString("  %1").arg(operators.size()));
        button->setToolTip(category.name);
        button->setAccessibleName(category.name);
        button->setIcon(QIcon(category.icon));button->setIconSize(QSize(20,20));
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        button->setCheckable(true);button->setMinimumHeight(36);
        button->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Fixed);
        m_categories->addButton(button);layout->addWidget(button);
        connect(button,&QToolButton::clicked,this,[this,category]() {
            m_searchTimer->stop();
            const QSignalBlocker blocker(m_search);m_search->clear();
            showCategory(category.type);
        });
    }
    layout->addStretch(1);scroll->setWidget(content);root->addWidget(scroll,1);
    auto total=new QLabel(getLang("OperatorSidebar_Total","%1 项算子 · %2 个分类")
                         .arg(XvFuncAsm->getXvFuncInfos().size()).arg(count),this);
    total->setObjectName("operatorSidebarSummary");root->addWidget(total);
    m_searchTimer=new QTimer(this);m_searchTimer->setSingleShot(true);m_searchTimer->setInterval(80);
    connect(m_search,&QLineEdit::textChanged,this,[this]() { m_searchTimer->start(); });
    connect(m_searchTimer,&QTimer::timeout,this,[this]() {
        if(m_search->text().trimmed().isEmpty()) { closePanel();return; }
        showCategory(XvCore::EXvFuncType::Null,m_search->text());
    });
}

void FrmXvFuncAsm::setDrawerParWidget(QWidget *parent)
{
    if(m_panelParent) m_panelParent->removeEventFilter(this);
    m_panelParent=parent;
    if(m_panelParent) m_panelParent->installEventFilter(this);
    if(m_panel) { m_panel->setParent(parent);m_panel->hide();updatePanelGeometry(); }
}

void FrmXvFuncAsm::updatePanelGeometry()
{
    if(!m_panel || !m_panel->parentWidget()) return;
    const QRect available=m_panel->parentWidget()->rect();
    m_panel->setGeometry(0,0,qMin(360,available.width()),available.height());
}

void FrmXvFuncAsm::showCategory(XvCore::EXvFuncType type,const QString &query)
{
    if(m_panel && m_panel->isVisible() && type==m_currentType && query.isEmpty())
    { closePanel();return; }
    auto info=XvFuncAsm->getXvFuncTypeInfo(type);
    const auto operators=type==XvCore::EXvFuncType::Null?XvFuncAsm->getXvFuncInfos():XvFuncAsm->getXvFuncInfos(type);
    if(type==XvCore::EXvFuncType::Null) info.name=getLang("OperatorSidebar_Results","搜索结果");
    if(!m_panel)
    {
        m_panel=new QFrame(m_panelParent?m_panelParent.data():parentWidget());
        m_panel->setObjectName("operatorPopup");
        auto layout=new QVBoxLayout(m_panel);layout->setContentsMargins(0,0,0,0);
        m_contents=new FrmXvFuncType(info,operators,m_panel);layout->addWidget(m_contents);
        connect(m_contents,&FrmXvFuncType::closeDrawer,this,&FrmXvFuncAsm::closePanel);
    }
    else if(type!=m_currentType)
        m_contents->setOperators(info,operators);
    m_currentType=type;
    m_contents->setFilterText(query);
    updatePanelGeometry();m_panel->show();m_panel->raise();
}

void FrmXvFuncAsm::closePanel()
{
    if(m_panel) m_panel->hide();
    m_categories->setExclusive(false);
    for(auto button:m_categories->buttons()) button->setChecked(false);
    m_categories->setExclusive(true);
}

bool FrmXvFuncAsm::eventFilter(QObject *watched,QEvent *event)
{
    if(watched==m_panelParent && event->type()==QEvent::Resize) updatePanelGeometry();
    if(m_panel && m_panel->isVisible())
    {
        if(event->type()==QEvent::KeyPress && static_cast<QKeyEvent*>(event)->key()==Qt::Key_Escape)
        { closePanel();return true; }
        if(event->type()==QEvent::MouseButtonPress)
            if(auto widget=qobject_cast<QWidget*>(watched))
                if(widget!=this && !isAncestorOf(widget) && widget!=m_panel && !m_panel->isAncestorOf(widget))
                    closePanel();
    }
    return BaseWidget::eventFilter(watched,event);
}
