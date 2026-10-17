#include "nestplatform.h"
#include "openwith.h"
#include <QFileInfo>
#include <QMessageBox>
#include <QProcess>

QString NestPlatform::shortcutLabel(QString text) {
#ifndef Q_OS_MACOS
    text.replace(QStringLiteral("⌘"), "Ctrl+");
    text.replace(QStringLiteral("⌥"), "Alt+");
    text.replace(QStringLiteral("⇧"), "Shift+");
    text.replace(QStringLiteral("↵"), "Enter");
#endif
    return text;
}

QString NestPlatform::revealLabel() {
#ifdef Q_OS_MACOS
    return QStringLiteral("Finder에서 보기");
#elif defined(Q_OS_WIN)
    return QStringLiteral("탐색기에서 보기");
#else
    return QStringLiteral("파일 관리자에서 보기");
#endif
}

void NestPlatform::editImage(QWidget *parent, const QString &path) {
    if (!QFileInfo::exists(path)) return;
#ifdef Q_OS_MACOS
    if (!QProcess::startDetached("/usr/bin/open", {"-a", "Preview", path}))
        QMessageBox::warning(parent, QStringLiteral("편집 프로그램"), QStringLiteral("미리보기를 열 수 없습니다."));
#else
    // Choose another application instead of reopening NestView as the default viewer.
    OpenWith::showOpenWithDialog(parent);
#endif
}
