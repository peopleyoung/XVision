#include "UiAppearance.h"
#include "XLanguage.h"
#include <QApplication>
#include <QFile>
#include <QPalette>
#include <QStyleFactory>
#include <QTranslator>

void applyUiAppearance(QApplication &application)
{
    // Use Qt dialogs so their language and palette do not depend on Windows locale.
    QApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    const QString language=XLang->getCurLangType();
    if(language.isEmpty() || language==QStringLiteral("cn_s"))
    {
        for(const QString &resource:{QStringLiteral(":/translations/qtbase_zh_CN.qm"),
                                     QStringLiteral(":/translations/xvision_zh_CN.qm")})
        {
            auto translator=new QTranslator(&application);
            if(translator->load(resource)) application.installTranslator(translator);
            else delete translator;
        }
    }
    QFile style(QStringLiteral(":/style/TechBlue.css"));
    style.open(QIODevice::ReadOnly);
    application.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    QPalette palette;
    palette.setColor(QPalette::Window, QColor("#101F33"));
    palette.setColor(QPalette::WindowText, QColor("#E3EDF9"));
    palette.setColor(QPalette::Base, QColor("#0B192B"));
    palette.setColor(QPalette::AlternateBase, QColor("#13263D"));
    palette.setColor(QPalette::Text, QColor("#E3EDF9"));
    palette.setColor(QPalette::Button, QColor("#152A43"));
    palette.setColor(QPalette::ButtonText, QColor("#D8E9FF"));
    palette.setColor(QPalette::BrightText, Qt::white);
    palette.setColor(QPalette::Highlight, QColor("#24568A"));
    palette.setColor(QPalette::HighlightedText, Qt::white);
    palette.setColor(QPalette::Link, QColor("#54BBFF"));
    palette.setColor(QPalette::ToolTipBase, QColor("#19324F"));
    palette.setColor(QPalette::ToolTipText, QColor("#E3EDF9"));
    palette.setColor(QPalette::Disabled, QPalette::Text, QColor("#627B98"));
    palette.setColor(QPalette::Disabled, QPalette::ButtonText, QColor("#627B98"));
    palette.setColor(QPalette::Disabled, QPalette::WindowText, QColor("#627B98"));
    application.setPalette(palette);
    application.setStyleSheet(QString::fromUtf8(style.readAll()));
}
