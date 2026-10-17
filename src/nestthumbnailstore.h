#pragma once
#include "nestmedia.h"
#include <QApplication>
#include <QCache>
#include <QSet>
#include <QBuffer>
#include <memory>
#include <QCryptographicHash>
#include <QDataStream>
#include <QFileInfo>
#include <QDir>
#include <QStandardPaths>
#include <QSaveFile>
#include <QThreadPool>
#include <QFutureWatcher>
#include <QtConcurrent>
// Accessed only by the single disk worker. Evict before writing, not on a timer.
class NestThumbnailDiskCache {
public:
    NestThumbnailDiskCache(const QString &directory,qint64 limit):directory(directory),limit(limit){
        QDir().mkpath(directory);
        for(const auto &f:QDir(directory).entryInfoList({"*.png"},QDir::Files,QDir::Time|QDir::Reversed)){order.append(f.fileName());sizes.insert(f.fileName(),f.size());used+=f.size();}
        trim(0);
    }
    void touch(const QString &name){if(sizes.contains(name)){order.removeAll(name);order.append(name);}}
    bool put(const QString &name,const QByteArray &bytes){
        if(bytes.size()>limit)return false;
        if(sizes.contains(name))return true;
        if(!trim(bytes.size()))return false;
        QSaveFile file(directory+"/"+name);if(!file.open(QIODevice::WriteOnly)||file.write(bytes)!=bytes.size()||!file.commit())return false;
        used+=bytes.size();sizes.insert(name,bytes.size());order.append(name);return true;
    }
    qint64 bytesUsed() const{return used;}
private:
    bool trim(qint64 incoming){while(used+incoming>limit&&!order.isEmpty()){auto name=order.first();if(!QFile::remove(directory+"/"+name)&&QFile::exists(directory+"/"+name))return false;order.removeFirst();used-=sizes.take(name);}return used+incoming<=limit;}
    QString directory;QStringList order;QHash<QString,qint64> sizes;qint64 limit,used=0;
};
// GUI-thread-owned shared cache. Workers only read files and return immutable images.
class NestThumbnailStore : public QObject {
public:
    enum { maximumPendingRequests = 32 };
    static NestThumbnailStore &instance(){static auto store=new NestThumbnailStore(qApp);return *store;}
    static QString sourceKey(const QString &path){QFileInfo f(path);QByteArray bytes;QDataStream out(&bytes,QIODevice::WriteOnly);out<<f.absoluteFilePath()<<f.size()<<f.lastModified().toMSecsSinceEpoch()<<f.metadataChangeTime().toMSecsSinceEpoch();return QString::fromLatin1(QCryptographicHash::hash(bytes,QCryptographicHash::Sha256).toHex());}
    static QString key(const QString &path,const QColorSpace &space,int edge=480){return (edge==480?QString():QString::number(edge)+"-")+sourceKey(path)+QString::fromLatin1(QCryptographicHash::hash(space.iccProfile(),QCryptographicHash::Sha256).toHex());}
    const QImage *image(const QString &key){return cache.object(key);}
    QSize dimensions(const QString &path) const {auto size=sizes.object(sourceKey(path));return size?*size:QSize();}
    void rememberDimensions(const QString &key,const QSize &size){if(size.isValid())sizes.insert(key,new QSize(size));}
    QFutureWatcher<QImage> *request(const QString &path,const QColorSpace &space,int edge=480){
        QString id=key(path,space,edge);if(pending.contains(id))return pending.value(id);
        if(cache.contains(id)||failed.contains(id))return nullptr;
        if(pending.size()>=maximumPendingRequests)return nullptr;
        auto watcher=new QFutureWatcher<QImage>(this);pending.insert(id,watcher);++requests;
        QString file=directory+"/"+id+".png";
        connect(watcher,&QFutureWatcher<QImage>::finished,this,[this,watcher,id,path]{
            auto result=watcher->result();pending.remove(id);
            if(!result.isNull()){
                cache.insert(id,new QImage(result),qMax(1,int(result.sizeInBytes()/1024)));
                // Disk compression never delays display. Bound outstanding save buffers too.
                if(pendingWrites<2){++pendingWrites;diskPool.start([this,id,result]{
                    if(QFile::exists(directory+"/"+id+".png"))disk->touch(id+".png");
                    else {QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);if(result.save(&buffer,"PNG"))disk->put(id+".png",bytes);}
                    QMetaObject::invokeMethod(this,[this]{--pendingWrites;},Qt::QueuedConnection);
                });}
            }else {if(failed.size()>=4096)failed.clear();failed.insert(id);}
            watcher->deleteLater();
        });
        watcher->setFuture(QtConcurrent::run(&pool,[path,space,file,edge]{
            QImage image(file);if(!image.isNull())return image;
            image=NestMedia::read(path,edge);if(image.isNull())return image;
            if(!image.colorSpace().isValid())image.setColorSpace(QColorSpace::SRgb);if(space.isValid())image.convertToColorSpace(space);
            return image;
        }));return watcher;
    }
    int requestCount() const{return requests;}
    int pendingCount() const{return pending.size();}
    ~NestThumbnailStore() override {pool.waitForDone();diskPool.waitForDone();}
private:
    explicit NestThumbnailStore(QObject *parent):QObject(parent){
        pool.setMaxThreadCount(2);diskPool.setMaxThreadCount(1);
        directory=QStandardPaths::writableLocation(QStandardPaths::CacheLocation)+"/thumbnails-v2";
        diskPool.start([this]{disk.reset(new NestThumbnailDiskCache(directory,256LL*1024*1024));});
    }
    QCache<QString,QImage> cache{128*1024};
    mutable QCache<QString,QSize> sizes{32768};
    QThreadPool diskPool;
    std::unique_ptr<NestThumbnailDiskCache> disk;
    int pendingWrites=0;
    QHash<QString,QFutureWatcher<QImage> *> pending;
    QSet<QString> failed;
    QThreadPool pool;
    QString directory;
    int requests=0;
};
