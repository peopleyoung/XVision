#include "GeometryOperatorBase.h"
#include "GeometryOperatorWdg.h"

using namespace XvCore;

GeometryOperatorBase::GeometryOperatorBase(QObject *parent)
    :XvFunc(parent)
{
    _funcType=EXvFuncType::Measurement;
}

GeometryOperatorBase::~GeometryOperatorBase()
{
    delete m_widget;
}

QPixmap GeometryOperatorBase::funcIcon()
{
    return QPixmap(":/images/BaseDataRealCalc.svg");
}

void GeometryOperatorBase::onShowFunc()
{
    if(!m_widget) m_widget=new GeometryOperatorWdg(this);
    m_widget->show();
    m_widget->raise();
}
