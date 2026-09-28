#include <QtTest>
#include <memory>
#include <QApplication>
#include <QDialogButtonBox>
#include <QLineEdit>
#include <QFileDialog>
#include <QMenu>
#include <QPushButton>
#include <QRegularExpression>
#include "UiAppearance.h"
#include "XLanguage.h"
#include "XvFuncAssembly.h"

class UiAppearanceTest : public QObject
{
    Q_OBJECT
private slots:
    void initTestCase()
    {
        XLang->init();
        applyUiAppearance(*qApp);
    }
    void standardActionsAreChinese()
    {
        const QRegularExpression chinese(QStringLiteral("[\\x{4e00}-\\x{9fff}]"));
        QDialogButtonBox box(QDialogButtonBox::Ok|QDialogButtonBox::Cancel|
                            QDialogButtonBox::Yes|QDialogButtonBox::No);
        for(auto button:box.buttons())
            QVERIFY2(chinese.match(button->text()).hasMatch(),qPrintable(button->text()));
        QLineEdit editor;
        std::unique_ptr<QMenu> menu(editor.createStandardContextMenu());
        for(auto action:menu->actions())
            if(!action->isSeparator())
                QVERIFY2(chinese.match(action->text()).hasMatch(),qPrintable(action->text()));
        QFileDialog fileDialog;
        const QString fileName=fileDialog.labelText(QFileDialog::FileName);
        QVERIFY2(chinese.match(fileName).hasMatch(),qPrintable(fileName));
        QCOMPARE(QCoreApplication::translate("ads::CDockWidgetTab","Close"),QStringLiteral("关闭"));
        QVERIFY(QApplication::testAttribute(Qt::AA_DontUseNativeDialogs));
    }
    void darkPaletteKeepsTextReadable()
    {
        const QPalette palette=qApp->palette();
        QVERIFY(palette.color(QPalette::Window).lightness()<60);
        QVERIFY(palette.color(QPalette::Base).lightness()<60);
        QVERIFY(palette.color(QPalette::Text).lightness()>210);
        QVERIFY(palette.color(QPalette::WindowText).lightness()>210);
        QVERIFY(palette.color(QPalette::Highlight).blue()>palette.color(QPalette::Highlight).red());
        QVERIFY(qApp->styleSheet().contains(QStringLiteral("XFlowGraphicsView")));
    }
    void allCategoryIconsAndNamesArePresent()
    {
        const auto categories=XvFuncAsm->getXvFuncTypeInfos();
        QCOMPARE(categories.size(),16);
        const QRegularExpression chinese(QStringLiteral("[\\x{4e00}-\\x{9fff}]"));
        for(const auto &category:categories)
        {
            QVERIFY2(chinese.match(category.name).hasMatch(),qPrintable(category.name));
            QVERIFY2(!category.icon.isNull(),qPrintable(category.name));
        }
    }
    void displayTranslationPreservesUnknownUserData()
    {
        QCOMPARE(getUiText(QStringLiteral("GaussianBlur")),QStringLiteral("高斯滤波"));
        QCOMPARE(getUiText(QStringLiteral("Fixed length")),QStringLiteral("固定长度"));
        QCOMPARE(getUiText(QStringLiteral("Kernel width")),QStringLiteral("卷积核宽度"));
        QCOMPARE(getUiText(QStringLiteral("自定义检测 123")),QStringLiteral("自定义检测 123"));
        QCOMPARE(getUiText(QStringLiteral("customer-role-123")),QStringLiteral("customer-role-123"));
        QCOMPARE(getUiText(QStringLiteral("NCHW")),QStringLiteral("NCHW"));
    }
};
QTEST_MAIN(UiAppearanceTest)
#include "tst_uiappearance.moc"
