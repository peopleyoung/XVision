#include "FrmXvFuncType.h"
#include <QDrag>
#include <QLabel>
#include <QListWidget>
#include <QMimeData>
#include <QPointer>
#include <QToolButton>
#include <QVBoxLayout>
#include <algorithm>

namespace
{
class OperatorList:public QListWidget
{
public:
    explicit OperatorList(QWidget *parent):QListWidget(parent) {}
protected:
    void startDrag(Qt::DropActions) override
    {
        auto item=currentItem();
        if(!item) return;
        const QString role=item->data(Qt::UserRole).toString();
        QPointer<QDrag> drag=new QDrag(this);
        auto mime=new QMimeData;
        mime->setText(role);
        drag->setMimeData(mime);
        drag->setPixmap(item->icon().pixmap(28,28));
        drag->exec(Qt::CopyAction);
        // A completed drag must not accumulate a QObject for every operation.
        if(drag) drag->deleteLater();
    }
};
}

FrmXvFuncType::FrmXvFuncType(const XvCore::XvFuncTypeInfo &info,
                            const QList<XvCore::XvFuncInfo> &operators,QWidget *parent)
    :BaseWidget(parent)
{
    initFrm();
    setOperators(info,operators);
}

void FrmXvFuncType::initFrm()
{
    setObjectName("operatorCatalogPanel");
    setMinimumSize(240,180);
    auto layout=new QVBoxLayout(this);
    layout->setContentsMargins(12,14,12,12);
    layout->setSpacing(10);
    auto header=new QHBoxLayout;
    m_title=new QLabel(this);m_title->setObjectName("operatorPanelTitle");
    header->addWidget(m_title,1);
    m_count=new QLabel(this);m_count->setObjectName("operatorPanelCount");
    header->addWidget(m_count);
    auto close=new QToolButton(this);
    close->setIcon(QIcon(":/images/Ui/FrmXvFuncTypeClose.svg"));
    close->setIconSize(QSize(18,18));
    close->setToolTip(getLang("OperatorPanel_Close","关闭算子列表"));
    close->setAccessibleName(close->toolTip());
    connect(close,&QToolButton::clicked,this,&FrmXvFuncType::closeDrawer);
    header->addWidget(close);layout->addLayout(header);
    auto hint=new QLabel(getLang("OperatorPanel_Hint","拖动算子到流程画布"),this);
    hint->setObjectName("operatorPanelHint");layout->addWidget(hint);
    m_list=new OperatorList(this);
    m_list->setObjectName("operatorList");
    m_list->setUniformItemSizes(true);
    m_list->setIconSize(QSize(22,22));
    m_list->setSpacing(2);
    m_list->setWordWrap(false);
    m_list->setTextElideMode(Qt::ElideRight);
    m_list->setDragEnabled(true);
    m_list->setDragDropMode(QAbstractItemView::DragOnly);
    m_list->setDefaultDropAction(Qt::CopyAction);
    m_list->setSelectionMode(QAbstractItemView::SingleSelection);
    m_list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_list->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    layout->addWidget(m_list,1);
    m_empty=new QLabel(getLang("OperatorPanel_Empty","没有匹配的算子，请调整关键词"),this);
    m_empty->setWordWrap(true);m_empty->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_empty);m_empty->hide();
}

void FrmXvFuncType::setOperators(const XvCore::XvFuncTypeInfo &info,
                                const QList<XvCore::XvFuncInfo> &operators)
{
    m_typeInfo=info;m_operators=operators;
    std::stable_sort(m_operators.begin(),m_operators.end(),[](const auto &a,const auto &b) {
        if(a.canonicalRole!=b.canonicalRole) return a.canonicalRole<b.canonicalRole;
        if(a.preset!=b.preset) return !a.preset;
        return a.role<b.role;
    });
    m_title->setText(info.name);
    m_list->setUpdatesEnabled(false);
    m_list->clear();
    for(const auto &op:m_operators)
    {
        auto item=new QListWidgetItem(QIcon(op.icon),getUiText(op.name),m_list);
        item->setData(Qt::UserRole,op.role);
        item->setData(Qt::UserRole+1,getUiText(op.name)+" "+op.role+" "+op.canonicalRole);
        item->setToolTip(getUiText(op.name));
        item->setSizeHint(QSize(0,38));
        item->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable|Qt::ItemIsDragEnabled);
    }
    m_list->setUpdatesEnabled(true);
    setFilterText(QString());
}

void FrmXvFuncType::setFilterText(const QString &text)
{
    const QString query=text.trimmed();
    int visible=0;
    m_list->setUpdatesEnabled(false);
    for(int index=0;index<m_list->count();++index)
    {
        auto item=m_list->item(index);
        const bool match=query.isEmpty() || item->data(Qt::UserRole+1).toString().contains(query,Qt::CaseInsensitive);
        item->setHidden(!match);
        if(match) ++visible;
    }
    m_list->setUpdatesEnabled(true);
    m_count->setText(getLang("OperatorPanel_Count","%1 项").arg(visible));
    m_empty->setVisible(visible==0);
    m_list->setVisible(visible!=0);
}
