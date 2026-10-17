#pragma once
#include <QImage>
#include <QColorSpace>
#include <QUrl>
class QWidget;
#include <QSize>
#include <QStringList>
#include <QSharedPointer>
#include <QTemporaryDir>
#include <atomic>
#include <memory>
namespace NestMedia {
QImage read(const QString &path, int maximum = 0);
QSize size(const QString &path);
QColorSpace displaySpace(const QWidget *widget);
QString metadata(const QString &path);
QStringList nativeExtensions();
bool isArchive(const QString &path);
struct ArchiveResult {
    QSharedPointer<QTemporaryDir> directory;
    QStringList files;
    QString error;
};
ArchiveResult extract(const QString &path, const std::shared_ptr<std::atomic_bool> &cancelled = {});
}
namespace NestMedia {
// Empty string on success. Never overwrites an existing destination.
QString convert(const QString &source, const QString &destination, const QByteArray &format, int maximum, int quality);
}

namespace NestMedia {
struct MetadataSnapshot {
    QList<QPair<QString,QString>> fields;
    QString raw;
    QUrl mapURL;
    QString text() const {QStringList lines;for(const auto &row:fields)lines.append(row.first+": "+row.second);return lines.join('\n');}
};
MetadataSnapshot inspect(const QString &path);
bool wallpaper(const QString &path, QString *error);
}
namespace NestMedia {
struct Effects {
    bool invert=false, grayscale=false, sepia=false;
    double gamma=1.0;
    int brightness=0;
    QString signature() const;
};
Effects effects();
QImage applyEffects(QImage image,const Effects &settings);
}
