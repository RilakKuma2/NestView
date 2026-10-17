#include "nestreadingview.h"
#include "nestmedia.h"
#include <QPainter>
#include <QScrollBar>
#include <QFutureWatcher>
#include <QtConcurrent>
#include <QFileInfo>
NestReadingView::NestReadingView(QWidget *parent) : QAbstractScrollArea(parent) {
    pool.setMaxThreadCount(2);
    setFrameShape(QFrame::NoFrame);
    setFocusPolicy(Qt::StrongFocus);
    verticalScrollBar()->setSingleStep(100);
    connect(verticalScrollBar(),&QScrollBar::valueChanged,this,[this] {
        if (mode != 5) return;
        double y = verticalScrollBar()->value()+viewport()->height()*0.25;
        for (int i=0;i<rects.size();++i) if (rects[i].bottom() >= y) {
            if (current!=i) {current=i; emit currentChanged(i);} break;
        }
    });
}
NestReadingView::~NestReadingView() { *cancelled=true; pool.waitForDone(); }
void NestReadingView::cancelPreviousWork() {
    *cancelled=true;
    cancelled=std::make_shared<std::atomic_bool>(false);
    ++generation;
}
void NestReadingView::invalidateImages() {
    cache.clear();pending.clear();failed.clear();
    if(sizes.isEmpty()){auto original=files;files.clear();setFiles(original,current,mode);}
    else{cancelPreviousWork();viewport()->update();}
}
void NestReadingView::setFiles(const QStringList &paths, int index, int value) {
    mode=value; current=qBound(0,index,qMax(0,int(paths.size())-1)); zoom=1;
    if (files==paths && !sizes.isEmpty()) { layoutPages(); jump(current); return; }
    files=paths; sizes.clear(); rects.clear(); cache.clear(); pending.clear(); failed.clear(); cancelPreviousWork();
    const int token=generation;
    auto watcher=new QFutureWatcher<QVector<QSize>>(this);
    connect(watcher,&QFutureWatcher<QVector<QSize>>::finished,this,[this,watcher,token] {
        auto values=watcher->result(); watcher->deleteLater();
        if (token!=generation) return;
        sizes=values; layoutPages(); jump(current);
    });
    watcher->setFuture(QtConcurrent::run(&pool,[paths,cancelled=cancelled] {
        QVector<QSize> values; values.reserve(paths.size());
        for (const auto &path:paths) {if(*cancelled)return QVector<QSize>();auto s=NestMedia::size(path); values.append(s.isValid()?s:QSize(1000,1400));}
        return values;
    }));
    viewport()->update();
}
void NestReadingView::layoutPages() {
    const double width=qMax(100,viewport()->width()-32);
    rects.fill(QRectF(),files.size());
    double y=16;
    if (mode==5) {
        for (int i=0;i<sizes.size();++i) {
            double w=qMin(width,width*zoom),h=w*sizes[i].height()/qMax(1,sizes[i].width());
            rects[i]=QRectF((viewport()->width()-w)/2,y,w,h); y+=h+12;
        }
    } else if (current<sizes.size()) {
        // A landscape cover occupies its own spread.
        bool pair=sizes[current].width()<sizes[current].height() && current+1<sizes.size() && sizes[current+1].width()<sizes[current+1].height();
        double h=qMax(100,viewport()->height()-32), left=0;
        int n=pair?2:1;
        double ratio=double(sizes[current].width())/sizes[current].height();
        if(pair) ratio+=double(sizes[current+1].width())/sizes[current+1].height();
        h=qMin(h,(width-12*(n-1))/ratio)*zoom;
        left=(viewport()->width()-h*ratio-12*(n-1))/2;
        for(int j=0;j<n;++j) {
            int i=current+(mode==6?n-1-j:j);
            double w=h*sizes[i].width()/sizes[i].height();
            rects[i]=QRectF(left,16,w,h);left+=w+12;
        }
        y=h+32;
    }
    verticalScrollBar()->setPageStep(viewport()->height());
    verticalScrollBar()->setRange(0,qMax(0,int(qMin(y,2000000000.0))-viewport()->height()));
    viewport()->update();
}
void NestReadingView::jump(int index) {
    current=qBound(0,index,qMax(0,int(files.size())-1));
    if(mode==5) { if(current<rects.size()) verticalScrollBar()->setValue(qRound(rects[current].top()-16)); }
    else { layoutPages(); verticalScrollBar()->setValue(0); }
    viewport()->update();
}
void NestReadingView::zoomBy(double factor) {zoom=qBound(0.25,zoom*factor,3.0);layoutPages();}
void NestReadingView::resizeEvent(QResizeEvent *e) {QAbstractScrollArea::resizeEvent(e);layoutPages();}
void NestReadingView::scrollContentsBy(int,int) {viewport()->update();}
void NestReadingView::request(int i) {
    if(pending.contains(i)||cache.contains(i)||failed.contains(i)||pending.size()>=4) return;
    pending.insert(i); const int token=generation;
    QString path=files[i];
    // A decoded page must fit the cache; otherwise each repaint decodes it again.
    int pixels=qMin(4096,qMax(1600,int(viewport()->width()*devicePixelRatioF())));
    const auto target=NestMedia::displaySpace(this);
    const auto effects=NestMedia::effects();
    auto watcher=new QFutureWatcher<QImage>(this);
    connect(watcher,&QFutureWatcher<QImage>::finished,this,[this,watcher,i,token] {
        auto image=watcher->result();watcher->deleteLater();
        if(token!=generation)return;
        pending.remove(i);
        if(!image.isNull()) {int cost=qMax(1,int(image.sizeInBytes()/1024));cache.insert(i,new QImage(image),cost);} else failed.insert(i);
        viewport()->update();
    });
    watcher->setFuture(QtConcurrent::run(&pool,[path,pixels,target,effects,cancelled=cancelled]{if(*cancelled)return QImage();auto image=NestMedia::read(path,pixels);if(*cancelled)return QImage();image=NestMedia::applyEffects(image,effects);if(!image.colorSpace().isValid())image.setColorSpace(QColorSpace::SRgb);if(target.isValid())image.convertToColorSpace(target);return image;}));
}
void NestReadingView::paintEvent(QPaintEvent *) {
    QPainter p(viewport());p.fillRect(viewport()->rect(),QColor("#202020"));p.setRenderHint(QPainter::SmoothPixmapTransform);
    if(sizes.isEmpty()) {p.setPen(QColor("#a4adbb"));p.drawText(viewport()->rect(),Qt::AlignCenter,QStringLiteral("페이지 정보를 읽는 중…"));return;}
    const QRectF visible(0,verticalScrollBar()->value(),viewport()->width(),viewport()->height());
    p.translate(0,-verticalScrollBar()->value());
    for(int i=0;i<rects.size();++i) {
        if(rects[i].isEmpty()||!rects[i].intersects(visible.adjusted(0,-400,0,400)))continue;
        if(auto image=cache.object(i))p.drawImage(rects[i],*image);
        else {p.fillRect(rects[i],QColor("#202631"));p.setPen(QColor("#a4adbb"));p.drawText(rects[i],Qt::AlignCenter,failed.contains(i)?QStringLiteral("이미지를 읽을 수 없습니다"):QFileInfo(files[i]).fileName());request(i);}
    }
}
