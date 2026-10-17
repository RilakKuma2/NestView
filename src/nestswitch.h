#pragma once
#include <QCheckBox>
#include <QPainter>
class NestSwitch : public QCheckBox {
public:
    explicit NestSwitch(QWidget *parent=nullptr):QCheckBox(parent){setFixedSize(34,22);setCursor(Qt::PointingHandCursor);}
protected:
    bool hitButton(const QPoint &p) const override{return rect().contains(p);}
    void paintEvent(QPaintEvent *) override{QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.setPen(QPen(isChecked()?QColor("#61afff"):QColor("#adb5bc"),1));p.setBrush(isChecked()?QColor("#61afff"):Qt::white);p.drawRoundedRect(QRectF(1,3,31,17),8.5,8.5);p.setPen(Qt::NoPen);p.setBrush(isChecked()?Qt::white:QColor("#c1c7cd"));p.drawEllipse(QRectF(isChecked()?17:3,5,13,13));}
};
