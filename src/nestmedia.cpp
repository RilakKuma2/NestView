#include "nestmedia.h"
#include "qvapplication.h"
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QImageReader>
#include <QImageIOHandler>
#include <QMessageBox>
#include <QWidget>
#include <exiv2/exiv2.hpp>
#include <libraw/libraw.h>
#ifdef WIN32_LOADED
#include "qvwin32functions.h"
#include <windows.h>
#endif
#ifdef X11_LOADED
#include "qvlinuxx11functions.h"
#endif
#ifdef Q_OS_LINUX
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusPendingCallWatcher>
#include <QDBusPendingReply>
#endif

namespace {
constexpr qint64 maximumPixels = 100000000;
constexpr qint64 maximumSourceBytes = 512 * 1024 * 1024;
bool reasonable(const QSize &size) {
    return size.isValid() && qint64(size.width()) * size.height() <= maximumPixels;
}
QString exifString(const Exiv2::ExifData &data, const char *key) {
    auto it = data.findKey(Exiv2::ExifKey(key));
    return it == data.end() ? QString() : QString::fromStdString(it->toString());
}
double exifNumber(const Exiv2::ExifData &data, const char *key, int index = 0) {
    auto it = data.findKey(Exiv2::ExifKey(key));
    return it == data.end() || index >= it->count() ? 0 : it->toFloat(index);
}
}

QStringList NestMedia::nativeExtensions() {
    // Other formats (including HEIF/PSD) are advertised only when a Qt plugin can read them.
    return {".dng", ".cr2", ".cr3", ".nef", ".arw", ".orf", ".raf", ".rw2", ".pef"};
}

QSize NestMedia::size(const QString &path) {
    QImageReader reader(path);
    auto dimensions = reader.size();
    if (dimensions.isValid()) {
        if (reader.transformation() & QImageIOHandler::TransformationRotate90) dimensions.transpose();
        return dimensions;
    }
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > maximumSourceBytes) return {};
    auto bytes = file.map(0, file.size());
    if (!bytes) return {};
    try {
        auto image = Exiv2::ImageFactory::open(bytes, static_cast<size_t>(file.size()));
        if (!image.get()) return {};
        image->readMetadata();
        dimensions = QSize(image->pixelWidth(), image->pixelHeight());
        if (exifNumber(image->exifData(), "Exif.Image.Orientation") >= 5) dimensions.transpose();
    } catch (const Exiv2::Error &) { return {}; }
    return dimensions;
}

QImage NestMedia::read(const QString &path, int maximum) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    const auto dimensions = reader.size();
    if (maximum > 0 && dimensions.isValid() && qMax(dimensions.width(), dimensions.height()) > maximum)
        reader.setScaledSize(dimensions.scaled(maximum, maximum, Qt::KeepAspectRatio));
    if ((maximum > 0 || !dimensions.isValid() || reasonable(dimensions))) {
        auto image = reader.read();
        if (!image.isNull()) return image;
    }
    const auto oriented = size(path);
    if (!reasonable(oriented)) return {};
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() <= 0 || file.size() > maximumSourceBytes) return {};
    auto bytes = file.map(0, file.size());
    if (!bytes) return {};
    // LibRaw contains large decode tables; worker threads have small stacks.
    auto decoder = std::make_unique<LibRaw>();
    auto &raw = *decoder;
    raw.imgdata.params.output_bps = 8;
    raw.imgdata.params.output_color = 1;
    raw.imgdata.params.use_camera_wb = 1;
    raw.imgdata.params.half_size = maximum > 0 && qMax(oriented.width(), oriented.height()) > qint64(maximum) * 2;
    if (raw.open_buffer(bytes, static_cast<size_t>(file.size())) != LIBRAW_SUCCESS) return {};
    if (!reasonable(QSize(raw.imgdata.sizes.raw_width, raw.imgdata.sizes.raw_height))) return {};
    if (raw.unpack() != LIBRAW_SUCCESS || raw.dcraw_process() != LIBRAW_SUCCESS) return {};
    int error = 0;
    auto bitmap = raw.dcraw_make_mem_image(&error);
    if (!bitmap) return {};
    QImage result;
    if (error == LIBRAW_SUCCESS && bitmap->type == LIBRAW_IMAGE_BITMAP && bitmap->colors == 3 && bitmap->bits == 8)
        result = QImage(bitmap->data, bitmap->width, bitmap->height, bitmap->width * 3, QImage::Format_RGB888).copy();
    LibRaw::dcraw_clear_mem(bitmap);
    if (maximum > 0 && qMax(result.width(), result.height()) > maximum) result = result.scaled(maximum, maximum, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    result.setColorSpace(QColorSpace::SRgb);
    return result;
}

QColorSpace NestMedia::displaySpace(const QWidget *widget) {
    const int mode = qvApp->getSettingsManager().getInt(SettingsManager::Setting::ColorSpaceConversion);
    if (mode == 2) return QColorSpace::SRgb;
    if (mode == 3) return QColorSpace::DisplayP3;
    if (mode != 1 || !widget || !widget->window()->windowHandle()) return {};
    QByteArray profile;
#ifdef WIN32_LOADED
    profile = QVWin32Functions::getIccProfileForWindow(widget->window()->windowHandle());
#elif defined(X11_LOADED)
    profile = QVLinuxX11Functions::getIccProfileForWindow(widget->window()->windowHandle());
#endif
    return profile.isEmpty() ? QColorSpace() : QColorSpace::fromIccProfile(profile);
}

NestMedia::MetadataSnapshot NestMedia::inspect(const QString &path) {
    MetadataSnapshot result;
    QFileInfo info(path);
    if (!info.isFile()) return result;
    auto add = [&](const QString &label, const QString &value) { if (!value.isEmpty()) result.fields.append({label, value}); };
    add("파일 이름", info.fileName());
    add("파일 크기", QString::number(info.size() / 1048576.0, 'f', 2) + " MB (" + QString::number(info.size()) + " bytes)");
    add("수정한 날짜", info.lastModified().toString("yyyy/MM/dd HH:mm:ss"));
    auto dimensions = size(path);
    if (dimensions.isValid()) add("이미지 정보", QString("%1 × %2 (%3)").arg(dimensions.width()).arg(dimensions.height()).arg(info.suffix().toUpper()));
    QFile file(path);
    if (file.open(QIODevice::ReadOnly) && file.size() > 0 && file.size() <= maximumSourceBytes) {
        auto bytes = file.map(0, file.size());
        if (!bytes) {
            add("파일 경로", info.absoluteFilePath());
            result.raw = result.text();
            return result;
        }
        try {
            auto image = Exiv2::ImageFactory::open(bytes, static_cast<size_t>(file.size()));
            if (image.get()) {
                image->readMetadata();
                const auto &data = image->exifData();
                for (const auto &field : QList<QPair<QString, const char *>>{
                    {"카메라 제조사", "Exif.Image.Make"}, {"카메라 모델명", "Exif.Image.Model"},
                    {"렌즈 모델명", "Exif.Photo.LensModel"}, {"촬영 시간", "Exif.Photo.DateTimeOriginal"},
                    {"ISO 감도", "Exif.Photo.ISOSpeedRatings"}, {"소프트웨어", "Exif.Image.Software"},
                    {"제작", "Exif.Image.Artist"}, {"저작권", "Exif.Image.Copyright"}, {"설명", "Exif.Image.ImageDescription"}})
                    add(field.first, exifString(data, field.second));
                const double exposure = exifNumber(data, "Exif.Photo.ExposureTime");
                if (exposure > 0) add("노출 시간", exposure < 1 ? QString("1/%1 sec.").arg(qRound(1 / exposure)) : QString::number(exposure) + " sec.");
                const double aperture = exifNumber(data, "Exif.Photo.FNumber");
                if (aperture > 0) add("조리개 값", "f/" + QString::number(aperture));
                const double focal = exifNumber(data, "Exif.Photo.FocalLength");
                if (focal > 0) add("초점 거리", QString::number(focal) + " mm");
                const auto latRef = exifString(data, "Exif.GPSInfo.GPSLatitudeRef");
                const auto lonRef = exifString(data, "Exif.GPSInfo.GPSLongitudeRef");
                if (!latRef.isEmpty() && !lonRef.isEmpty()) {
                    auto coordinate = [&](const char *key) { return exifNumber(data, key) + exifNumber(data, key, 1) / 60 + exifNumber(data, key, 2) / 3600; };
                    double lat = coordinate("Exif.GPSInfo.GPSLatitude"), lon = coordinate("Exif.GPSInfo.GPSLongitude");
                    if (latRef == "S") lat = -lat;
                    if (lonRef == "W") lon = -lon;
                    if (qAbs(lat) <= 90 && qAbs(lon) <= 180) {
                        add("GPS 위도", QString::number(lat, 'f', 6));
                        add("GPS 경도", QString::number(lon, 'f', 6));
                        result.mapURL = QUrl(QString("https://www.openstreetmap.org/?mlat=%1&mlon=%2#map=15/%1/%2").arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6));
                    }
                }
                for (const auto &entry : data) result.raw += QString::fromStdString(entry.key() + ": " + entry.toString()) + '\n';
            }
        } catch (const Exiv2::Error &) { /* File details remain available for unsupported metadata. */ }
    }
    add("파일 경로", info.absoluteFilePath());
    if (result.raw.isEmpty()) result.raw = result.text();
    return result;
}

QString NestMedia::metadata(const QString &path) { return inspect(path).raw; }

bool NestMedia::wallpaper(const QString &path, QString *error) {
    if (!QFileInfo::exists(path)) { if (error) *error = QStringLiteral("파일을 찾을 수 없습니다."); return false; }
#ifdef Q_OS_WIN
    // Windows accepts BMP reliably; keep a persistent copy for the desktop.
    const auto output = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation) + "/wallpaper.bmp";
    QDir().mkpath(QFileInfo(output).absolutePath());
    if (!read(path).save(output, "BMP")) { if (error) *error = QStringLiteral("배경 이미지를 저장할 수 없습니다."); return false; }
    const auto native = QDir::toNativeSeparators(output).toStdWString();
    if (SystemParametersInfoW(SPI_SETDESKWALLPAPER, 0, const_cast<wchar_t *>(native.c_str()), SPIF_UPDATEINIFILE | SPIF_SENDCHANGE)) return true;
    if (error) *error = QStringLiteral("Windows에서 배경 화면을 변경할 수 없습니다.");
#elif defined(Q_OS_LINUX)
    QDBusInterface portal("org.freedesktop.portal.Desktop", "/org/freedesktop/portal/desktop", "org.freedesktop.portal.Wallpaper", QDBusConnection::sessionBus());
    if (portal.isValid()) {
        QVariantMap options{{"show-preview", true}, {"set-on", "background"}};
        auto watcher = new QDBusPendingCallWatcher(portal.asyncCall("SetWallpaperURI", "", QUrl::fromLocalFile(path).toString(), options), qApp);
        QObject::connect(watcher, &QDBusPendingCallWatcher::finished, qApp, [watcher] {
            QDBusPendingReply<QDBusObjectPath> reply = *watcher;
            if (reply.isError()) QMessageBox::warning(nullptr, QStringLiteral("바탕 화면"), reply.error().message());
            watcher->deleteLater();
        });
        return true;
    }
    if (error) *error = QStringLiteral("이 데스크톱에서는 배경 화면 설정 포털을 사용할 수 없습니다.");
#else
    if (error) *error = QStringLiteral("이 환경에서는 배경 화면 변경을 지원하지 않습니다.");
#endif
    return false;
}
