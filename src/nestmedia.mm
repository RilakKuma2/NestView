#include "nestmedia.h"
#include "qvcocoafunctions.h"
#include "qvapplication.h"
#include <QWidget>
#include <QImageReader>
#include <QColorSpace>
#include <QFileInfo>
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
#include <QDateTime>
#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>
namespace {
CGImageSourceRef source(const QString &path) {
    NSURL *url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path.toUtf8().constData()]];
    return CGImageSourceCreateWithURL((__bridge CFURLRef)url, nullptr);
}
}
QStringList NestMedia::nativeExtensions() {
    return {".heic", ".heif", ".avif", ".psd", ".dng", ".cr2", ".cr3", ".nef", ".arw", ".orf", ".raf", ".rw2", ".pef"};
}
QSize NestMedia::size(const QString &path) {
    @autoreleasepool {
        auto src = source(path);
        if (!src) return QImageReader(path).size();
        NSDictionary *p = CFBridgingRelease(CGImageSourceCopyPropertiesAtIndex(src, 0, nullptr));
        CFRelease(src);
        int w = [p[(__bridge NSString *)kCGImagePropertyPixelWidth] intValue];
        int h = [p[(__bridge NSString *)kCGImagePropertyPixelHeight] intValue];
        int o = [p[(__bridge NSString *)kCGImagePropertyOrientation] intValue];
        return o >= 5 && o <= 8 ? QSize(h,w) : QSize(w,h);
    }
}
QImage NestMedia::read(const QString &path, int maximum) {
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (maximum > 0 && qMax(reader.size().width(),reader.size().height()) > maximum)
        reader.setScaledSize(reader.size().scaled(maximum, maximum, Qt::KeepAspectRatio));
    auto image = reader.read();
    if (!image.isNull()) return image;
    @autoreleasepool {
        auto src = source(path);
        if (!src) return {};
        QSize dimensions = size(path);
        if (maximum <= 0 && qint64(dimensions.width()) * dimensions.height() > 134217728) { CFRelease(src); return {}; }
        const int limit = maximum > 0 ? maximum : qMax(dimensions.width(), dimensions.height());
        NSDictionary *opts = @{(__bridge NSString *)kCGImageSourceCreateThumbnailFromImageAlways: @YES,
            (__bridge NSString *)kCGImageSourceCreateThumbnailWithTransform: @YES,
            (__bridge NSString *)kCGImageSourceThumbnailMaxPixelSize: @(qMax(1,limit))};
        CGImageRef cg = CGImageSourceCreateThumbnailAtIndex(src, 0, (__bridge CFDictionaryRef)opts);
        CFRelease(src);
        if (!cg) return {};
        QImage result(int(CGImageGetWidth(cg)), int(CGImageGetHeight(cg)), QImage::Format_RGBA8888_Premultiplied);
        result.fill(Qt::transparent);
        CGColorSpaceRef color = CGColorSpaceCreateWithName(kCGColorSpaceSRGB);
        CGContextRef context = CGBitmapContextCreate(result.bits(), result.width(), result.height(), 8, result.bytesPerLine(), color, kCGImageAlphaPremultipliedLast | kCGBitmapByteOrder32Big);
        if (context) { CGContextDrawImage(context, CGRectMake(0,0,result.width(),result.height()), cg); CGContextRelease(context); }
        CGColorSpaceRelease(color); CGImageRelease(cg);
        result.setColorSpace(QColorSpace::SRgb);
        return context ? result : QImage();
    }
}
QString NestMedia::metadata(const QString &path) {
    @autoreleasepool {
        auto src = source(path);
        if (!src) return QStringLiteral("메타데이터를 읽을 수 없습니다.");
        NSDictionary *p = CFBridgingRelease(CGImageSourceCopyPropertiesAtIndex(src, 0, nullptr));
        CFRelease(src);
        return QString::fromUtf8([[p description] UTF8String]);
    }
}

QColorSpace NestMedia::displaySpace(const QWidget *widget) {
    int mode=qvApp->getSettingsManager().getInt(SettingsManager::Setting::ColorSpaceConversion);
    if(mode==2)return QColorSpace::SRgb;
    if(mode==3)return QColorSpace::DisplayP3;
    if(mode!=1||!widget||!widget->window()->windowHandle())return {};
    return QColorSpace::fromIccProfile(QVCocoaFunctions::getIccProfileForWindow(widget->window()->windowHandle()));
}

NestMedia::MetadataSnapshot NestMedia::inspect(const QString &path) {
    MetadataSnapshot result;
    QFileInfo file(path);
    if(!file.exists())return result;
    auto add=[&](const QString &label,const QString &value){if(!value.isEmpty())result.fields.append({label,value});};
    add("파일 이름",file.fileName());
    add("파일 크기",QString::number(file.size()/1048576.0,'f',2)+" MB ("+QString::number(file.size())+" bytes)");
    add("수정한 날짜",file.lastModified().toString("yyyy/MM/dd HH:mm:ss"));
    @autoreleasepool {
        auto src=source(path);if(!src){result.raw=metadata(path);return result;}
        NSDictionary *p=CFBridgingRelease(CGImageSourceCopyPropertiesAtIndex(src,0,nullptr));
        auto frameCount=CGImageSourceGetCount(src);CFRelease(src);
        auto string=[](id value)->QString {if(!value||value==[NSNull null])return {};return QString::fromUtf8([[value description] UTF8String]);};
        auto field=[&](NSDictionary *dict,NSString *key,const QString &label){add(label,string(dict[key]));};
        auto sizeValue=size(path);add("이미지 정보",QString("%1 × %2 (%3)").arg(sizeValue.width()).arg(sizeValue.height()).arg(file.suffix().toUpper()));
        field(p,(__bridge NSString*)kCGImagePropertyDepth,"비트 깊이");field(p,(__bridge NSString*)kCGImagePropertyColorModel,"색상 모델");field(p,(__bridge NSString*)kCGImagePropertyProfileName,"색상 프로파일");
        if(frameCount>1)add("프레임 / 페이지 수",QString::number(frameCount));
        NSDictionary *tiff=p[(__bridge NSString*)kCGImagePropertyTIFFDictionary];NSDictionary *exif=p[(__bridge NSString*)kCGImagePropertyExifDictionary];NSDictionary *gps=p[(__bridge NSString*)kCGImagePropertyGPSDictionary];NSDictionary *iptc=p[(__bridge NSString*)kCGImagePropertyIPTCDictionary];
        field(tiff,@"Make","카메라 제조사");field(tiff,@"Model","카메라 모델명");field(exif,@"LensModel","렌즈 모델명");field(tiff,@"Software","소프트웨어");field(tiff,@"Artist","제작");field(tiff,@"Copyright","저작권");field(exif,@"DateTimeOriginal","촬영 시간");
        if(exif[@"FocalLength"])add("초점 거리",string(exif[@"FocalLength"])+" mm");
        if(exif[@"ExposureTime"]){double value=[exif[@"ExposureTime"] doubleValue];add("노출 시간",value>0&&value<1?QString("1/%1 sec.").arg(qRound(1/value)):QString::number(value)+" sec.");}
        if(exif[@"FNumber"])add("조리개 값","f/"+string(exif[@"FNumber"]));
        if(exif[@"ExposureBiasValue"])add("노출 보정",string(exif[@"ExposureBiasValue"])+" EV");
        id iso=exif[@"ISOSpeedRatings"];if([iso isKindOfClass:[NSArray class]])iso=[iso firstObject];add("ISO 감도",string(iso));
        if(exif[@"Flash"])add("플래시",([exif[@"Flash"] intValue]&1)?"발광":"발광 안함");
        if(exif[@"ExposureProgram"]){QStringList modes={"지정 안함","수동","일반 프로그램","조리개 우선","셔터 우선","크리에이티브","액션","인물","풍경"};add("노출 방식",modes.value([exif[@"ExposureProgram"] intValue],string(exif[@"ExposureProgram"])));}
        if(exif[@"MeteringMode"]){QStringList modes={"알 수 없음","평균","중앙 중점","스팟","다중 스팟","패턴","부분"};add("측광 방식",modes.value([exif[@"MeteringMode"] intValue],"기타"));}
        if(exif[@"WhiteBalance"])add("화이트 밸런스",[exif[@"WhiteBalance"] intValue]==0?"자동":"수동");
        field(tiff,@"ImageDescription","설명");field(exif,@"UserComment","사용자 메모");field(iptc,@"Caption/Abstract","캡션");field(iptc,@"Keywords","키워드");field(iptc,@"ObjectName","제목");
        NSDictionary *png=p[(__bridge NSString*)kCGImagePropertyPNGDictionary];field(png,@"Description","PNG 설명");field(png,@"Comment","PNG 메모");field(png,@"Software","PNG 소프트웨어");
        if(gps[@"Latitude"]&&gps[@"Longitude"]){double lat=[gps[@"Latitude"] doubleValue],lon=[gps[@"Longitude"] doubleValue];if([gps[@"LatitudeRef"] isEqual:@"S"])lat=-lat;if([gps[@"LongitudeRef"] isEqual:@"W"])lon=-lon;
            if(qAbs(lat)<=90&&qAbs(lon)<=180){add("GPS 위도",QString::number(lat,'f',6));add("GPS 경도",QString::number(lon,'f',6));result.mapURL=QUrl(QString("https://maps.apple.com/?ll=%1,%2").arg(lat,0,'f',6).arg(lon,0,'f',6));}
        }
        if(gps[@"Altitude"])add("GPS 고도",string(gps[@"Altitude"])+" m");
        result.raw=QString::fromUtf8([[p description] UTF8String]);
    }
    add("파일 경로",file.absoluteFilePath());
    return result;
}
bool NestMedia::wallpaper(const QString &path,QString *error) {
    @autoreleasepool {
        NSURL *url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:path.toUtf8().constData()]];
        NSError *failure=nil;BOOL ok=[[NSWorkspace sharedWorkspace] setDesktopImageURL:url forScreen:[NSScreen mainScreen] options:@{} error:&failure];
        if(!ok&&error)*error=QString::fromUtf8([[failure localizedDescription] UTF8String]);return ok;
    }
}
