#ifndef XMATSTYLEDEF_H
#define XMATSTYLEDEF_H
#include <QColor>
#include <QFont>

/*************颜色*************/
///主题颜色
const QColor C_XMatThemeColor=QColor(59,158,255,255);
///涟漪颜色
const QColor C_XMatRippleColor=QColor(84,187,255,110);

///使能颜色(前景) （浅蓝文字）
const QColor C_XMatForegroundColor=QColor(227,237,249,255);
///使能颜色(背景) （深蓝面板）
const QColor C_XMatBackgroundColor=QColor(16,31,51,255);

///失能颜色(前景) （弱化文字）
const QColor C_XMatDisableForegroundColor=QColor(98,123,152,255);
///失能颜色(背景) （深蓝禁用背景）
const QColor C_XMatDisableBackgroundColor=QColor(17,31,48,255);
///覆盖颜色（深蓝）
const QColor C_XMatOverlaydColor=QColor(21,42,67,255);
///字体颜色
const QColor C_XMatFontdColor=QColor(227,237,249,255);

/*************字体*************/
const QFont C_XMatFont=QFont("Microsoft YaHei UI", 9, QFont::Normal);

#endif // XMATSTYLEDEF_H
