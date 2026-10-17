#pragma once
#include <QListWidget>
#include <QScrollBar>
#include <QPainter>
#include <QMouseEvent>
#include <QKeyEvent>
#include <algorithm>
// Symmetric end padding lets even the first/last image occupy the exact center.
class NestFilmstrip : public QListWidget {
public:
    explicit NestFilmstrip(QWidget *parent):QListWidget(parent){setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);setMouseTracking(true);
        connect(model(),&QAbstractItemModel::dataChanged,this,[this]{updateGeometries();viewport()->update();});
    }
    enum { Step=84 };
    QRect visualRect(const QModelIndex &index) const override {
        if(!index.isValid()||index.row()>=rects.size())return {};
        return rects[index.row()].translated(viewport()->width()/2-horizontalScrollBar()->value(),2);
    }
    QModelIndex indexAt(const QPoint &point) const override {
        int x=point.x()+horizontalScrollBar()->value()-viewport()->width()/2;
        auto it=std::lower_bound(rects.cbegin(),rects.cend(),x,[](const QRect &r,int value){return r.right()<value;});
        if(it!=rects.cend()&&it->contains(QPoint(x,point.y()-2)))return model()->index(int(it-rects.cbegin()),0);
        return {};
    }
    void scrollTo(const QModelIndex &index,ScrollHint =EnsureVisible) override {if(index.isValid()&&index.row()<rects.size())horizontalScrollBar()->setValue(rects[index.row()].center().x());}
    int centeredRow() const {
        int x=horizontalScrollBar()->value();auto it=std::lower_bound(rects.cbegin(),rects.cend(),x,[](const QRect &r,int value){return r.center().x()<value;});
        int i=int(it-rects.cbegin());if(i>=rects.size())return qMax(0,int(rects.size())-1);
        if(i>0&&x-rects[i-1].center().x()<rects[i].center().x()-x)--i;return i;
    }
    void stepBy(int steps){int row=qBound(0,centeredRow()+steps,qMax(0,count()-1));scrollTo(model()->index(row,0));}
protected:
    void updateGeometries() override {
        if(layingOut)return;layingOut=true;int selected=centeredRow();rects.clear();int x=0,height=qMax(20,qMin(80,viewport()->height()-4));
        for(int i=0;i<count();++i){auto size=item(i)->data(Qt::UserRole+1).toSize();double ratio=size.isValid()?double(size.width())/size.height():1;
            int width=qMax(20,qRound((height-4)*qBound(0.18,ratio,4.0))+4);rects.append(QRect(x,0,width,height));x+=width+2;}
        if(!rects.isEmpty()){int shift=rects.first().center().x();for(auto &r:rects)r.translate(-shift,0);}
        horizontalScrollBar()->setRange(0,rects.isEmpty()?0:rects.last().center().x());horizontalScrollBar()->setPageStep(viewport()->width());horizontalScrollBar()->setSingleStep(Step);
        if(selected<rects.size())horizontalScrollBar()->setValue(rects[selected].center().x());layingOut=false;
    }
    int horizontalOffset() const override {return horizontalScrollBar()->value();}
    void scrollContentsBy(int,int) override {viewport()->update();}
    void paintEvent(QPaintEvent *) override {
        QPainter painter(viewport());
        int center=centeredRow(), radius=viewport()->width()/20+2;
        for(int i=qMax(0,center-radius);i<qMin(count(),center+radius+1);++i){
            auto index=model()->index(i,0);QStyleOptionViewItem option;option.initFrom(viewport());option.widget=this;option.rect=visualRect(index);
            if(i==currentRow())option.state|=QStyle::State_Selected;
            if(option.rect.contains(viewport()->mapFromGlobal(QCursor::pos())))option.state|=QStyle::State_MouseOver;
            itemDelegate()->paint(&painter,option,index);
        }
    }
    void mousePressEvent(QMouseEvent *event) override {
        if(event->button()!=Qt::LeftButton){event->ignore();return;}
        auto index=indexAt(event->position().toPoint());if(index.isValid()){setCurrentRow(index.row());scrollTo(index);emit itemClicked(item(index.row()));}event->accept();
    }
    void mouseReleaseEvent(QMouseEvent *event) override {event->accept();}
    void keyPressEvent(QKeyEvent *event) override {
        int row=currentRow();if(event->key()==Qt::Key_Left)--row;else if(event->key()==Qt::Key_Right)++row;else {QListWidget::keyPressEvent(event);return;}
        row=qBound(0,row,qMax(0,count()-1));setCurrentRow(row);scrollTo(model()->index(row,0));if(item(row))emit itemClicked(item(row));event->accept();
    }
private:
    QVector<QRect> rects;
    bool layingOut=false;
};
