#pragma once
#include "nestmedia.h"
#include "nestthumbnailstore.h"
#include <QStyledItemDelegate>
#include <QCache>
#include <QSet>
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QPainter>
#include <QPainterPath>
#include <QFileInfo>
#include <QTimer>
// Only visible rows request thumbnails. Images stay off the GUI thread until paint.
class NestThumbnails : public QStyledItemDelegate {
public:
    explicit NestThumbnails(QObject *parent,bool strip=false,bool transparentCards=false):QStyledItemDelegate(parent),strip(strip),transparentCards(transparentCards) {}

    void paint(QPainter *p,const QStyleOptionViewItem &option,const QModelIndex &index) const override {
        auto self=const_cast<NestThumbnails *>(this);
        const QString path=index.data(Qt::UserRole).toString();
        const auto target=NestMedia::displaySpace(option.widget);
        auto &store=NestThumbnailStore::instance();
        int requested=qMax(option.rect.width(),option.rect.height())*2;int edge=requested>1024?2048:(requested>480?1024:480);
        const QString key=NestThumbnailStore::key(path,target,edge);
        p->save();p->setRenderHint(QPainter::Antialiasing);
        QRect area=option.rect.adjusted(6,4,-option.rect.width()+70,-4);
        if(strip){
            area=option.rect.adjusted(2,2,-2,-2);
            p->setPen(Qt::NoPen);p->setBrush(transparentCards?QColor(20,20,24,option.state&QStyle::State_MouseOver?35:0):(option.state&QStyle::State_MouseOver?QColor("#37373d"):QColor("#242428")));p->drawRoundedRect(area,4,4);
            if(option.state&QStyle::State_Selected){p->setPen(QPen(QColor("#a8d7c5"),2));p->setBrush(Qt::NoBrush);p->drawRoundedRect(QRectF(area).adjusted(-2,-2,2,2),9,9);}
            QPainterPath clip;clip.addRoundedRect(area,4,4);p->setClipPath(clip);
        }else{
            QStyleOptionViewItem textOption(option);textOption.rect.adjust(76,0,0,0);
            QStyledItemDelegate::paint(p,textOption,index);
        }
        auto image=store.image(key);if(!image&&edge>480)image=store.image(NestThumbnailStore::key(path,target));
        if(image) {
            QSize fit=image->size().scaled(area.size(),strip?Qt::KeepAspectRatioByExpanding:Qt::KeepAspectRatio);
            QRect target(QPoint(area.center().x()-fit.width()/2,area.center().y()-fit.height()/2),fit);
            p->setRenderHint(QPainter::SmoothPixmapTransform);p->drawImage(target,*image);
        }
        if(!store.image(key)&&!pending.contains(key)) {
            if(auto watcher=store.request(path,target,edge)){
                self->pending.insert(key);
                connect(watcher,&QFutureWatcher<QImage>::finished,self,[self,key]{
                    self->pending.remove(key);
                    // Repaint using each item's current path, even if the model was replaced.
                    if(auto view=qobject_cast<QAbstractItemView *>(self->parent()))view->viewport()->update();
                });
            }else if(store.pendingCount()>=NestThumbnailStore::maximumPendingRequests&&!retryScheduled){
                // Another view may own every queued request. Retry visible rows
                // after capacity is released, without retaining stale row indexes.
                self->retryScheduled=true;
                QTimer::singleShot(100,self,[self]{
                    self->retryScheduled=false;
                    if(auto view=qobject_cast<QAbstractItemView *>(self->parent()))
                        if(view->isVisible())view->viewport()->update();
                });
            }
        }
        p->restore();
    }
    QSize sizeHint(const QStyleOptionViewItem &,const QModelIndex &index) const override {
        if(strip){auto dimensions=index.data(Qt::UserRole+1).toSize();int height=index.data(Qt::UserRole+2).toInt();if(height<=0)height=80;
            double ratio=dimensions.isValid()?double(dimensions.width())/dimensions.height():1;
            ratio=qBound(0.18,ratio,4.0);
            int width=qMax(24,qRound((height-4)*ratio)+4);
            if(auto view=qobject_cast<QAbstractItemView *>(parent())){int available=qMax(24,view->viewport()->width()-8);if(width>available){width=available;height=qRound((width-4)/ratio)+4;}}
            return QSize(width,height);
        }
        return QSize(220,58);
    }
private:
    bool strip=false,transparentCards=false;
    bool retryScheduled=false;
    QSet<QString> pending;
};
