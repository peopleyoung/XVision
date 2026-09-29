#include "UiAppearance.h"
#include "XLanguage.h"
#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>
#include <QTranslator>
#include <QSettings>
#include <QRegularExpression>
#include <QDialog>
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QLabel>
#include <QComboBox>
#include <QVariant>
#include <QStyle>
#include <QIconEngine>
#include <QPainter>
#include <QAbstractButton>
#include <QAction>
#include <QEvent>
#include <QListWidget>
#include <QGraphicsView>
#include <QGraphicsScene>
#include <QGraphicsItem>
#include <QPointer>
#include <QSet>
#include <QTimer>
#include <algorithm>

namespace {
class ThemeIconEngine : public QIconEngine {
public:
    explicit ThemeIconEngine(const QIcon &icon):m_source(icon) {}
    QIconEngine *clone() const override {return new ThemeIconEngine(m_source);}
    QString key() const override {return QStringLiteral("XVisionThemeIcon");}
    QPixmap pixmap(const QSize &size,QIcon::Mode mode,QIcon::State state) override {
        auto pixmap=m_source.pixmap(size,mode,state);
        if(currentUiThemeId()!="mist-white")return pixmap;
        auto pixels=pixmap.toImage().convertToFormat(QImage::Format_ARGB32);
        for(int y=0;y<pixels.height();++y) {
            auto row=reinterpret_cast<QRgb*>(pixels.scanLine(y));
            for(int x=0;x<pixels.width();++x) {
                QColor color=QColor::fromRgba(row[x]);if(!color.alpha())continue;
                qreal h,s,l,a;color.getHslF(&h,&s,&l,&a);
                // Preserve semantic red/green accents while darkening pale line art.
                if(l>0.55)color=QColor::fromHslF(h<0?0.59:h,s<0.1?0.35:s,mode==QIcon::Disabled?0.52:0.34,a);
                row[x]=color.rgba();
            }
        }
        auto result=QPixmap::fromImage(pixels);result.setDevicePixelRatio(pixmap.devicePixelRatio());return result;
    }
    void paint(QPainter *painter,const QRect &rect,QIcon::Mode mode,QIcon::State state) override {
        const qreal ratio=painter->device()->devicePixelRatioF();auto image=pixmap(rect.size()*ratio,mode,state);image.setDevicePixelRatio(ratio);painter->drawPixmap(rect,image);
    }
#if QT_VERSION >= QT_VERSION_CHECK(6,0,0)
    QPixmap scaledPixmap(const QSize &size,QIcon::Mode mode,QIcon::State state,qreal scale) override {
        auto result=pixmap(size*scale,mode,state);result.setDevicePixelRatio(scale);return result;
    }
#else
    void virtual_hook(int id,void *data) override {
        if(id==QIconEngine::ScaledPixmapHook) {auto args=static_cast<QIconEngine::ScaledPixmapArgument*>(data);args->pixmap=pixmap(args->size*args->scale,args->mode,args->state);args->pixmap.setDevicePixelRatio(args->scale);return;}
        QIconEngine::virtual_hook(id,data);
    }
#endif
private:QIcon m_source;
};
void themeIcons(QWidget *widget) {
    if(auto button=qobject_cast<QAbstractButton*>(widget)) {
        if(!button->icon().isNull() && button->property("themeIconKey").toLongLong()!=button->icon().cacheKey()) {
            button->setIcon(QIcon(new ThemeIconEngine(button->icon())));button->setProperty("themeIconKey",button->icon().cacheKey());
        }
    }
    for(auto action:widget->actions()) {
        if(!action->icon().isNull() && action->property("themeIconKey").toLongLong()!=action->icon().cacheKey()) {
            action->setIcon(QIcon(new ThemeIconEngine(action->icon())));action->setProperty("themeIconKey",action->icon().cacheKey());
        }
    }
    if(auto list=qobject_cast<QListWidget*>(widget))for(int i=0;i<list->count();++i) {
        auto item=list->item(i);constexpr int keyRole=Qt::UserRole+500;
        if(!item->icon().isNull() && item->data(keyRole).toLongLong()!=item->icon().cacheKey()) {item->setIcon(QIcon(new ThemeIconEngine(item->icon())));item->setData(keyRole,item->icon().cacheKey());}
    }
}
class ThemeWidgetFilter : public QObject {
public:using QObject::QObject;
protected:bool eventFilter(QObject *object,QEvent *event) override {
    if(event->type()==QEvent::Polish || event->type()==QEvent::Show)if(auto widget=qobject_cast<QWidget*>(object)) {
        QPointer<QWidget> guard(widget);
        QTimer::singleShot(0,this,[guard]{if(guard)themeIcons(guard);});
    }
    return false;
}
};
QSettings settings() { return QSettings(QSettings::IniFormat,QSettings::UserScope,"XVision","Appearance"); }
QString checkedId(const QString &id) {
    for(const auto &theme:uiThemes()) if(theme.id==id) return id;
    return QStringLiteral("tech-blue");
}
QColor themeColor(const QColor &color,const QString &id) {
    if(id=="tech-blue" || !color.isValid()) return color;
    qreal h,s,l,a; color.getHslF(&h,&s,&l,&a);
    const bool blue=h>=0.45 && h<=0.72;
    if(id=="mist-white") {
        if(l>0.77) return QColor::fromHslF(0.59,0.32,0.18,a);
        if(l<0.34) return QColor::fromHslF(blue?0.59:std::max(qreal(0),h),0.18,0.985-l*0.38,a);
        if(blue) return QColor::fromHslF(0.59,std::min(qreal(0.8),s),std::max(qreal(0.28),0.68-l*0.4),a);
        return QColor::fromHslF(std::max(qreal(0),h),s,std::min(l,qreal(0.48)),a);
    }
    if(blue) {
        if(id=="obsidian") { s*=0.18; l*=l<0.35?0.72:1.0; }
        else h=id=="emerald"?0.43:0.73;
    }
    return QColor::fromHslF(std::max(qreal(0),h),s,l,a);
}
QString transformStyle(QString source,const QString &id) {
    const QRegularExpression colors(QStringLiteral("#[0-9A-Fa-f]{6}\\b"));
    const auto original=source; auto it=colors.globalMatch(original); int offset=0;
    while(it.hasNext()) { auto match=it.next();
        auto replacement=themeColor(QColor(match.captured()),id).name();
        source.replace(match.capturedStart()+offset,match.capturedLength(),replacement);
        offset+=replacement.size()-match.capturedLength();
    }
    if(id=="mist-white") {
        source+=QStringLiteral("QAbstractItemView::item:selected { color:white; background:#2864AD; } "
            "QWidget { selection-color:white; selection-background-color:#2864AD; } "
            "QMenu::item:selected { color:white; background:#2864AD; } "
            "QToolTip { color:#18334D; background:#F5F9FF; } "
            "QPushButton:default { color:white; background:#2864AD; } "
            "QComboBox QAbstractItemView { selection-color:white; selection-background-color:#2864AD; }");
    }
    return source;
}
}
QIcon uiThemeIcon(const QIcon &icon) {return QIcon(new ThemeIconEngine(icon));}
QVector<UiTheme> uiThemes() {
    return {{"tech-blue",QStringLiteral("科技蓝"),QStringLiteral("深蓝工作台 · 清晰的科技感")},
            {"obsidian",QStringLiteral("曜石黑"),QStringLiteral("低饱和深色 · 专注检测画面")},
            {"mist-white",QStringLiteral("雾白"),QStringLiteral("明亮浅色 · 适合日间工作")},
            {"emerald",QStringLiteral("翡翠绿"),QStringLiteral("沉静墨绿 · 柔和的视觉层次")},
            {"nebula",QStringLiteral("星云紫"),QStringLiteral("深紫背景 · 鲜明的操作重点")}};
}
QString currentUiThemeId() { return checkedId(qApp?qApp->property("uiThemeId").toString():QString()); }
QString uiThemeStyle(const QString &resource) {
    QFile file(resource); if(!file.open(QIODevice::ReadOnly)) return {};
    return transformStyle(QString::fromUtf8(file.readAll()),currentUiThemeId());
}
void setThemedStyle(QWidget *widget,const QString &resource) {
    widget->setProperty("themeStyleResource",resource); widget->setStyleSheet(uiThemeStyle(resource));
}
void applyUiTheme(QApplication &application,const QString &requested,bool persist) {
    const QString id=checkedId(requested); application.setProperty("uiThemeId",id);
    QPalette palette;
    const auto color=[&](const char *hex){return themeColor(QColor(hex),id);};
    palette.setColor(QPalette::Window,color("#101F33"));
    palette.setColor(QPalette::WindowText,color("#E3EDF9"));
    palette.setColor(QPalette::Base,color("#0B192B"));
    palette.setColor(QPalette::AlternateBase,color("#13263D"));
    palette.setColor(QPalette::Text,color("#E3EDF9"));
    palette.setColor(QPalette::Button,color("#152A43"));
    palette.setColor(QPalette::ButtonText,color("#D8E9FF"));
    palette.setColor(QPalette::BrightText,Qt::white);
    palette.setColor(QPalette::Mid,color("#2B4565"));
    palette.setColor(QPalette::Highlight,id=="mist-white"?QColor("#2864AD"):color("#24568A"));
    palette.setColor(QPalette::HighlightedText,Qt::white);
    palette.setColor(QPalette::Link,color("#54BBFF"));
    palette.setColor(QPalette::ToolTipBase,color("#19324F"));
    palette.setColor(QPalette::ToolTipText,color("#E3EDF9"));
    for(auto role:{QPalette::Text,QPalette::ButtonText,QPalette::WindowText})
        palette.setColor(QPalette::Disabled,role,color("#627B98"));
    application.setPalette(palette);
    application.setStyleSheet(uiThemeStyle(":/style/TechBlue.css"));
    QSet<QGraphicsScene*> refreshedScenes;
    QList<QPointer<QWidget>> widgets;
    for(auto widget:application.allWidgets())widgets.append(widget);
    for(const auto &guard:widgets) {
        auto widget=guard.data();if(!widget)continue;
        const QString resource=widget->property("themeStyleResource").toString();
        if(!resource.isEmpty()) widget->setStyleSheet(uiThemeStyle(resource));
        if(auto view=qobject_cast<QGraphicsView*>(widget))if(view->scene() && !refreshedScenes.contains(view->scene())) {
            refreshedScenes.insert(view->scene());
            for(auto item:view->scene()->items())if(auto object=dynamic_cast<QObject*>(item)) {
                if(object->metaObject()->indexOfMethod("refreshThemePalette()")>=0)
                    QMetaObject::invokeMethod(object,"refreshThemePalette",Qt::DirectConnection);
            }
            view->scene()->update();
        }
        themeIcons(widget);
        widget->update();
    }
    if(persist) { auto config=settings(); config.setValue("theme",id); config.sync(); }
}
void showUiSettings(QWidget *parent) {
    QDialog dialog(parent); dialog.setObjectName("systemSettingsDialog");
    dialog.setWindowTitle(QStringLiteral("系统设置")); dialog.resize(460,245);
    auto layout=new QVBoxLayout(&dialog); layout->setContentsMargins(24,20,24,20); layout->setSpacing(16);
    auto title=new QLabel(QStringLiteral("外观主题"),&dialog); title->setStyleSheet("font-size:18px;font-weight:bold"); layout->addWidget(title);
    auto choices=new QComboBox(&dialog); choices->setObjectName("themeSelector");
    for(const auto &theme:uiThemes()) choices->addItem(theme.name,theme.id);
    choices->setCurrentIndex(choices->findData(currentUiThemeId())); layout->addWidget(choices);
    auto description=new QLabel(&dialog); description->setWordWrap(true); layout->addWidget(description);
    const auto describe=[=](){description->setText(uiThemes().at(choices->currentIndex()).description+QStringLiteral("\n立即生效，重启后保留当前选择。"));}; describe();
    QObject::connect(choices,QOverload<int>::of(&QComboBox::currentIndexChanged),&dialog,[=](int){
        applyUiTheme(*qApp,choices->currentData().toString()); describe();
    });
    layout->addStretch(); auto buttons=new QDialogButtonBox(QDialogButtonBox::Close,&dialog); layout->addWidget(buttons);
    QObject::connect(buttons,&QDialogButtonBox::rejected,&dialog,&QDialog::accept); dialog.exec();
}
void applyUiAppearance(QApplication &application) {
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    if(!application.property("themeFilterInstalled").toBool()) {
        application.installEventFilter(new ThemeWidgetFilter(&application));
        application.setProperty("themeFilterInstalled",true);
    }
    const QString language=XLang->getCurLangType();
    if(!application.property("uiTranslationsInstalled").toBool() && (language.isEmpty() || language=="cn_s")) {
        for(const QString &resource:{QStringLiteral(":/translations/qtbase_zh_CN.qm"),QStringLiteral(":/translations/xvision_zh_CN.qm")}) {
            auto translator=new QTranslator(&application);
            if(translator->load(resource)) application.installTranslator(translator); else delete translator;
        }
        application.setProperty("uiTranslationsInstalled",true);
    }
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    auto config=settings(); applyUiTheme(application,config.value("theme","tech-blue").toString(),false);
}
