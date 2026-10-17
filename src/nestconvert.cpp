#include "nestmedia.h"
#include <QFile>
#include <QImageWriter>
#include <QPainter>
QString NestMedia::convert(const QString &source,const QString &destination,const QByteArray &format,int maximum,int quality) {
    auto image=read(source,maximum);
    if(image.isNull())return QStringLiteral("읽기 실패: ")+source;
    if(maximum>0 && qMax(image.width(),image.height())>maximum)image=image.scaled(maximum,maximum,Qt::KeepAspectRatio,Qt::SmoothTransformation);
    if((format=="jpeg"||format=="jpg")&&image.hasAlphaChannel()) {
        QImage opaque(image.size(),QImage::Format_RGB32);opaque.fill(Qt::white);opaque.setColorSpace(image.colorSpace());
        QPainter painter(&opaque);painter.drawImage(0,0,image);painter.end();image=opaque;
    }
    QFile file(destination);
    if(!file.open(QIODevice::WriteOnly|QIODevice::NewOnly))return QStringLiteral("기존 파일 또는 쓰기 실패: ")+destination;
    QImageWriter writer(&file,format);writer.setQuality(qBound(1,quality,100));
    if(!writer.write(image)){QString error=writer.errorString();file.close();file.remove();return QStringLiteral("저장 실패: ")+error;}
    if(!file.flush()){QString error=file.errorString();file.close();file.remove();return error;}
    return {};
}
