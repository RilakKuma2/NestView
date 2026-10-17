#include <QFile>
#include <QImage>
#include <exiv2/exiv2.hpp>
int nativeApplicationUpdateItemCount() { return 0; }
bool createMetadataFixture(const QString &path) {
    QImage image(64, 48, QImage::Format_RGB32);
    image.fill(Qt::blue);
    if (!image.save(path, "JPEG")) return false;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return false;
    const auto bytes = file.readAll();
    file.close();
    try {
        auto metadata = Exiv2::ImageFactory::open(reinterpret_cast<const Exiv2::byte *>(bytes.constData()), bytes.size());
        metadata->readMetadata();
        auto &data = metadata->exifData();
        data["Exif.Image.Make"] = "Fixture Camera";
        data["Exif.Image.Model"] = "Test Model";
        data["Exif.Photo.LensModel"] = "50 mm test lens";
        data["Exif.Photo.FNumber"] = "4/1";
        data["Exif.Photo.ExposureTime"] = "1/125";
        data["Exif.Photo.ISOSpeedRatings"] = uint16_t(200);
        data["Exif.GPSInfo.GPSLatitude"] = "37/1 33/1 0/1";
        data["Exif.GPSInfo.GPSLatitudeRef"] = "N";
        data["Exif.GPSInfo.GPSLongitude"] = "126/1 58/1 1/500";
        data["Exif.GPSInfo.GPSLongitudeRef"] = "E";
        metadata->writeMetadata();
        const auto &io = metadata->io();
        // MemIo owns the modified JPEG and its metadata.
        auto &writable = metadata->io();
        auto contents = writable.mmap();
        if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        return file.write(reinterpret_cast<const char *>(contents), io.size()) == static_cast<qint64>(io.size());
    } catch (const Exiv2::Error &) { return false; }
}
