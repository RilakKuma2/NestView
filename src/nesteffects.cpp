#include "nestmedia.h"
#include <QSettings>
#include <cmath>
QString NestMedia::Effects::signature() const {return QString("%1:%2:%3:%4:%5").arg(invert).arg(grayscale).arg(sepia).arg(gamma,0,'f',2).arg(brightness);}
NestMedia::Effects NestMedia::effects(){QSettings s;return {s.value("nest/effects/invert",false).toBool(),s.value("nest/effects/grayscale",false).toBool(),s.value("nest/effects/sepia",false).toBool(),qBound(0.1,s.value("nest/effects/gamma",1.0).toDouble(),5.0),qBound(-100,s.value("nest/effects/brightness",0).toInt(),100)};}
QImage NestMedia::applyEffects(QImage image,const Effects &settings){
    if(image.isNull()||(!settings.invert&&!settings.grayscale&&!settings.sepia&&settings.gamma==1.0&&settings.brightness==0))return image;
    image=image.convertToFormat(QImage::Format_ARGB32);
    int lut[256];for(int i=0;i<256;++i){int n=settings.invert?255-i:i;lut[i]=qBound(0,int(std::round(255*std::pow(n/255.0,1.0/settings.gamma)))+settings.brightness*255/100,255);}
    for(int y=0;y<image.height();++y){auto line=reinterpret_cast<QRgb *>(image.scanLine(y));for(int x=0;x<image.width();++x){auto pixel=line[x];int r=lut[qRed(pixel)],g=lut[qGreen(pixel)],b=lut[qBlue(pixel)];if(settings.grayscale)r=g=b=qGray(r,g,b);if(settings.sepia){int sr=qMin(255,int(.393*r+.769*g+.189*b));int sg=qMin(255,int(.349*r+.686*g+.168*b));int sb=qMin(255,int(.272*r+.534*g+.131*b));r=sr;g=sg;b=sb;}line[x]=qRgba(r,g,b,qAlpha(pixel));}}
    return image;
}
