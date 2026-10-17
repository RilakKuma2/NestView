#include <QString>
#import <Foundation/Foundation.h>
#import <AppKit/AppKit.h>
int nativeApplicationUpdateItemCount() {
    NSMenu *appMenu = NSApp.mainMenu.itemArray.firstObject.submenu;
    int count = 0;
    for (NSMenuItem *item in appMenu.itemArray)
        if (!item.hidden && [item.title hasPrefix:@"업데이트 확인"]) ++count;
    return count;
}
#import <ImageIO/ImageIO.h>
#import <CoreGraphics/CoreGraphics.h>
bool createMetadataFixture(const QString &path) {
    @autoreleasepool {
        auto color=CGColorSpaceCreateDeviceRGB();
        auto context=CGBitmapContextCreate(nullptr,64,48,8,64*4,color,kCGImageAlphaPremultipliedLast);
        CGColorSpaceRelease(color);if(!context)return false;
        CGContextSetRGBFillColor(context,0.2,0.5,0.8,1);CGContextFillRect(context,CGRectMake(0,0,64,48));
        auto image=CGBitmapContextCreateImage(context);CGContextRelease(context);
        NSURL *url=[NSURL fileURLWithPath:[NSString stringWithUTF8String:path.toUtf8().constData()]];
        auto destination=CGImageDestinationCreateWithURL((__bridge CFURLRef)url,CFSTR("public.jpeg"),1,nullptr);
        if(!destination){CGImageRelease(image);return false;}
        NSDictionary *properties=@{(__bridge NSString *)kCGImagePropertyTIFFDictionary:@{@"Make":@"Fixture Camera",@"Model":@"Test Model",@"Software":@"NestView tests"},(__bridge NSString *)kCGImagePropertyExifDictionary:@{@"FNumber":@4.0,@"ExposureTime":@0.008,@"ISOSpeedRatings":@[@200],@"LensModel":@"50 mm test lens",@"DateTimeOriginal":@"2026:09:07 12:00:00"},(__bridge NSString *)kCGImagePropertyGPSDictionary:@{@"Latitude":@37.55,@"LatitudeRef":@"N",@"Longitude":@126.966667,@"LongitudeRef":@"E"}};
        CGImageDestinationAddImage(destination,image,(__bridge CFDictionaryRef)properties);
        bool ok=CGImageDestinationFinalize(destination);CFRelease(destination);CGImageRelease(image);return ok;
    }
}
