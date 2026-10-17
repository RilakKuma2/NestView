#pragma once
#include <QIcon>
#include <QPainter>
#include <QPixmap>
#include <QPainterPath>
// Small original line icons rendered at 2x for Retina displays.
inline QIcon nestIcon(const QString &name,const QColor &color=QColor("#f1f3f4")) {
    QPixmap pix(40,40);pix.fill(Qt::transparent);pix.setDevicePixelRatio(2);
    QPainter p(&pix);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(color,1.25,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
    if(name=="folder"){p.drawPolyline(QPolygonF{{2,15},{2,5},{8,5},{10,7},{17,7},{17,9}});p.drawPolygon(QPolygonF{{2,16},{5,9},{19,9},{16,16}});}
    else if(name=="list"){for(int y:{5,10,15}){p.drawLine(3,y,4,y);p.drawLine(7,y,17,y);}}
    else if(name=="grid"){for(int x:{3,11})for(int y:{3,11})p.drawRoundedRect(QRectF(x,y,6,6),1,1);}
    else if(name=="minus")p.drawLine(4,10,16,10);
    else if(name=="plus"){p.drawLine(4,10,16,10);p.drawLine(10,4,10,16);}
    else if(name=="zoom"){p.drawEllipse(QRectF(3,3,10,10));p.drawLine(12,12,17,17);p.drawLine(5,8,11,8);}
    else if(name=="previous")p.drawPolyline(QPolygonF{{12,3},{5,10},{12,17}});
    else if(name=="next")p.drawPolyline(QPolygonF{{7,3},{14,10},{7,17}});
    else if(name=="rotate"){p.drawArc(QRectF(4,3,12,12),0,245*16);p.drawPolyline(QPolygonF{{13,2},{17,5},{13,7}});p.drawRect(QRectF(6,10,6,7));}
    else if(name=="pin"){p.save();p.translate(10,9);p.rotate(35);p.drawPolygon(QPolygonF{{-4,-6},{4,-6},{3,-4},{3,0},{5,3},{-5,3},{-3,0},{-3,-4}});p.drawLine(QPointF(0,3),QPointF(0,9));p.restore();}
    else if(name=="close"){p.drawLine(5,5,15,15);p.drawLine(15,5,5,15);}
    else if(name=="copy"){p.drawRoundedRect(QRectF(4,3,9,12),1,1);p.drawPolyline(QPolygonF{{14,6},{17,6},{17,18},{8,18},{8,16}});}
    else if(name=="info"){p.drawEllipse(QRectF(2,2,16,16));p.drawLine(10,9,10,14);p.drawPoint(10,6);}
    else if(name=="image"){p.drawRoundedRect(QRectF(2,4,16,12),1,1);p.drawPolyline(QPolygonF{{5,13},{8,10},{11,13},{14,8},{17,12}});}
    if(name=="trash"){p.drawLine(3,5,17,5);p.drawPolyline(QPolygonF{{6,5},{6,3},{14,3},{14,5}});p.drawRect(QRectF(5,6,10,11));p.drawLine(8,8,8,14);p.drawLine(12,8,12,14);}
    else if(name=="rename"){p.drawRect(QRectF(2,6,16,9));p.drawLine(10,3,10,18);p.drawLine(8,3,12,3);p.drawLine(8,18,12,18);}
    else if(name=="print"){p.drawRect(QRectF(5,2,10,5));p.drawRoundedRect(QRectF(2,7,16,8),2,2);p.drawRect(QRectF(5,12,10,6));}
    else if(name=="settings"){p.drawEllipse(QRectF(5,5,10,10));p.drawEllipse(QRectF(8,8,4,4));for(int a=0;a<8;++a){p.save();p.translate(10,10);p.rotate(a*45);p.drawLine(0,5,0,8);p.restore();}}
    return QIcon(pix);
}
