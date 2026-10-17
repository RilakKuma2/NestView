#pragma once
#include "nestmedia.h"
#include "nesttrackpadnavigation.h"
#include <QObject>
#include <QElapsedTimer>
#include <QTimer>
#include <QToolBar>
#include <QDockWidget>
#include <QDialog>
#include <QFrame>
#include <QSlider>
#include <QTreeWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QListWidget>
#include <QLabel>
#include <QComboBox>
#include <QPlainTextEdit>
#include <QFutureWatcher>
#include <functional>
class MainWindow;
class NestReadingView;
class NestShell : public QObject {
    Q_OBJECT
public:
    explicit NestShell(MainWindow *window);
    ~NestShell() override;
    bool openSpecial(const QString &path);
    bool navigate(int delta);
    void setMode(int mode);
    int viewMode() const {return mode;}
    void refresh();
    void showContextMenu(const QPoint &globalPosition);
    void zoom(double factor);
    bool zoomGallery(double factor);
    bool isReading() const {return mode>=5&&mode<=7;}
protected:
    bool eventFilter(QObject *,QEvent *) override;
private:
    QAction *action(QMenu *,const QString &,const QString &,std::function<void()>);
    void openFolder();
    void adjacentFolder(int delta);
    void toggleNavigator();
    void toggleInfo();
    void addBookmark();
    void rebuildBookmarks();
    void exportImages(bool batch);
    void fitWidth();
    MainWindow *w;
    QToolBar *top,*bottom;
    QLabel *status,*welcome;
    QDialog *navigator,*inspector;
    QTreeWidget *fileTable;
    QFrame *floating=nullptr,*panel=nullptr,*exifOverlay=nullptr;
    QLabel *exifText=nullptr,*panelTitle=nullptr;
    QSlider *pageSlider=nullptr;
    QWidget *panelBody=nullptr;
    QVBoxLayout *panelLayout=nullptr;
    QString activePanel;
    bool controlsPinned=true;
    void buildChrome();
    void layoutChrome();
    void updateHover(const QPoint &position);
    QListWidget *hoverThumbnails=nullptr,*folderGrid=nullptr;
    QFrame *thumbnailTray=nullptr;
    QDialog *gridDialog=nullptr;
    QLabel *thumbnailCount=nullptr,*gridCount=nullptr;
    QTimer *thumbnailSelectTimer=nullptr;
    bool scrubbingThumbnails=false;
    void showFolderGrid();
    void zoomFolderGrid(double factor,const QPoint &anchor);
    void prepareThumbnailLayout();
    int thumbnailLayoutGeneration=0;
    bool thumbnailLayoutReady=false;
    void loadThumbnailDimensions(int generation);
    void applyThumbnailDimensions(const QVector<int> &rows = {});
    QVector<int> thumbnailDimensionsPending;
    void selectCenteredThumbnail();
    QTimer *thumbnailHideTimer=nullptr;
    NestTrackpadNavigation trackpadNavigation;
    QElapsedTimer trackpadClock;
    void showPanel(const QString &name);
    void transferToPhotoFolder(bool move);
    QListWidget *list;
    QPlainTextEdit *metadata;
    QTableWidget *metaTable=nullptr;
    NestMedia::MetadataSnapshot metaSnapshot;
    void updateMetadata();
    void showMetadata();
    void openGPS();
    void printImage();
    QComboBox *modes;
    QMenu *bookmarks;
    NestReadingView *reading;
    QStringList paths;
    QString folder,archivePath,lastMetadata,pendingArchiveImage;
    NestMedia::ArchiveResult archive;
    QList<QSharedPointer<QTemporaryDir>> archiveStorage;
    std::shared_ptr<std::atomic_bool> archiveCancelled;
    quint64 archiveGeneration=0;
    int mode=1;
    bool busy=false;
};
