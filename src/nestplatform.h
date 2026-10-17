#pragma once
#include <QString>
class QWidget;
namespace NestPlatform {
QString shortcutLabel(QString text);
QString revealLabel();
void editImage(QWidget *parent, const QString &path);
}
