#pragma once
#include <QJsonObject>
#include <QUrl>
#include <QVersionNumber>
#include <QSysInfo>

namespace NestUpdate {
inline QString assetName() {
#ifdef Q_OS_WIN
    return QStringLiteral("NestView-Windows-x86_64.zip");
#elif defined(Q_OS_LINUX)
    return QStringLiteral("NestView-Linux-%1.deb").arg(QSysInfo::buildCpuArchitecture() == "arm64" ? "arm64" : "x86_64");
#else
    return QStringLiteral("NestView-macOS-AppleSilicon.dmg");
#endif
}

inline bool newerVersion(QString candidate, const QString &current) {
    if (candidate.startsWith('v')) candidate.remove(0, 1);
    int suffix = 0;
    const auto latest = QVersionNumber::fromString(candidate, &suffix);
    return !latest.isNull() && suffix == candidate.size() && latest > QVersionNumber::fromString(current);
}

inline bool validAssetURL(const QUrl &url, const QString &repository) {
    return url.scheme() == "https" && url.host() == "github.com" && url.userInfo().isEmpty()
        && url.port(-1) == -1 && url.path().startsWith("/" + repository + "/releases/download/")
        && !url.hasQuery() && !url.hasFragment();
}
}
