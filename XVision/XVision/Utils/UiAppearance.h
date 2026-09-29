#ifndef UIAPPEARANCE_H
#define UIAPPEARANCE_H
#include <QString>
#include <QVector>
#include <QIcon>
class QApplication;
class QWidget;
struct UiTheme { QString id; QString name; QString description; };
QVector<UiTheme> uiThemes();
QString currentUiThemeId();
QIcon uiThemeIcon(const QIcon &icon);
QString uiThemeStyle(const QString &resource);
void applyUiTheme(QApplication &application, const QString &id, bool persist=true);
void setThemedStyle(QWidget *widget, const QString &resource);
void showUiSettings(QWidget *parent);
void applyUiAppearance(QApplication &application);
#endif
