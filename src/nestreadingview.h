#pragma once
#include <QAbstractScrollArea>
#include <QCache>
#include <QSet>
#include <QThreadPool>
#include <atomic>
#include <memory>
class NestReadingView : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit NestReadingView(QWidget *parent = nullptr);
    ~NestReadingView() override;
    void setFiles(const QStringList &files, int index, int mode);
    void jump(int index);
    int currentIndex() const { return current; }
    int readingMode() const { return mode; }
    void zoomBy(double factor);
    void invalidateImages();
signals:
    void currentChanged(int index);
protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void scrollContentsBy(int, int) override;
private:
    void layoutPages();
    void request(int index);
    void cancelPreviousWork();
    std::shared_ptr<std::atomic_bool> cancelled=std::make_shared<std::atomic_bool>(false);
    QStringList files;
    QVector<QSize> sizes;
    QVector<QRectF> rects;
    QCache<int,QImage> cache{128*1024};
    QSet<int> pending;
    QSet<int> failed;
    QThreadPool pool;
    int current = 0, mode = 5, generation = 0;
    double zoom = 1;
};
