#include <QtTest>
#include <QtWidgets>
#include <QRadioButton>
#include <QToolButton>
#include "nestshell.h"
#include "nestsettings.h"
#include "nestreadingview.h"
#include "nestmedia.h"
#include "nestupdate.h"
#include "nestthumbnails.h"
#include "nestbackgroundactivity.h"
#include <archive.h>
#include <archive_entry.h>

#include "qvapplication.h"
#include "photoupdater.h"

bool createMetadataFixture(const QString &path);
int nativeApplicationUpdateItemCount();

class ActionManagerTests : public QObject
{
    Q_OBJECT

public:
    ActionManagerTests();
    ~ActionManagerTests();

private slots:
    void testUpdaterIsOfflineOutsideAppBundle();
    void testClonedActionsUntracked();
    void testDefaultShortcutProfile();
    void testPortableReleasePolicy();
    void testNaturalNavigationAndReading();
    void testNativeDecoder();
    void testArchiveAndTraversal();
    void testLatestArchiveRequest();
    void testCloseDuringDecode();
    void testConversionPreservesOriginal();
    void testReferenceChrome();
    void testLatestOpenRequest();
    void testSettingsAndGrouping();
    void testStructuredMetadata();
    void testEffectsAndCacheIsolation();
    void testWheelAndHoverThumbnails();
    void testFolderGridAndScrubbing();
    void testContextSortingPersists();
    void testThumbnailCacheAndIdentity();
    void testThumbnailDiskBudget();
    void testGridWheelZoom();
    void testMacTrackpadGestures();
    void testTrackpadInputTiming();
    void testBackgroundActivityLifetime();
    void testPreloadQueueBound();
    void testUndoDeleteConflictCanRetry();
    void testReadingRequestReplacement();
    void testThumbnailQueueBound();
    void testGalleryZoomRouting();
    void testScrollDeviceSeparation();
};

ActionManagerTests::ActionManagerTests() { }

ActionManagerTests::~ActionManagerTests() { }

void ActionManagerTests::testUpdaterIsOfflineOutsideAppBundle()
{
#ifndef NEST_PORTABLE_UPDATER
    QVERIFY(!PhotoUpdater::configured());
    PhotoUpdater::start();
    QVERIFY(!PhotoUpdater::automaticallyChecks());
    PhotoUpdater::setAutomaticallyChecks(true);
    QVERIFY(!PhotoUpdater::automaticallyChecks());
#else
    QVERIFY(PhotoUpdater::configured());
    const bool previous = PhotoUpdater::automaticallyChecks();
    PhotoUpdater::setAutomaticallyChecks(!previous);
    QCOMPARE(PhotoUpdater::automaticallyChecks(), !previous);
    PhotoUpdater::setAutomaticallyChecks(previous);
#endif
}

void ActionManagerTests::testClonedActionsUntracked()
{
    // Get initial counts of certain actions
    int fullscreenCount = qvApp->getActionManager().getAllInstancesOfAction("fullscreen").length();
    int openCount = qvApp->getActionManager().getAllInstancesOfAction("open").length();
    qDebug() << fullscreenCount;

    // Have window clone actions
    MainWindow window;
    window.setAttribute(Qt::WA_DeleteOnClose, false);
    window.show();
    // Make sure they were cloned
    QVERIFY(qvApp->getActionManager().getAllInstancesOfAction("fullscreen").length()
            != fullscreenCount);
    QVERIFY(qvApp->getActionManager().getAllInstancesOfAction("open").length() != openCount);
    // Untrack them
    window.close();

    // Make sure the count has not changed from the initial
    QCOMPARE(qvApp->getActionManager().getAllInstancesOfAction("fullscreen").length(),
             fullscreenCount);
    QCOMPARE(qvApp->getActionManager().getAllInstancesOfAction("open").length(), openCount);
}

void ActionManagerTests::testDefaultShortcutProfile() {
    auto list = qvApp->getShortcutManager().getShortcutsList();
    auto keys=[list](const QString &name){for(auto s:list)if(s.name==name)return s.defaultShortcuts;return QStringList();};
    QVERIFY(keys("nextfile").contains("PgDown"));
    QVERIFY(keys("previousfile").contains("Backspace"));
    QVERIFY(keys("resetzoom").contains("9"));
    QVERIFY(keys("originalsize").contains("0"));
    QVERIFY(!keys("rename").contains("Ctrl+Return"));
}
void ActionManagerTests::testNaturalNavigationAndReading() {
    QTemporaryDir dir;
    QImage image(240,400,QImage::Format_RGB32); image.fill(Qt::red);
    for(auto name:{"page10.png","page2.png","page1.png"}) QVERIFY(image.save(dir.filePath(name)));
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.show();
    window.openFile(dir.path());
    QTRY_VERIFY_WITH_TIMEOUT(window.getIsPixmapLoaded(),5000);
    QCOMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("page1.png"));
    window.nextFile();
    QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("page2.png"));
    window.lastFile();
    QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("page10.png"));
    auto shell=window.findChild<NestShell *>();QVERIFY(shell);
    shell->setMode(5);
    auto reader=window.findChild<NestReadingView *>();QVERIFY(reader);QVERIFY(reader->isVisible());
    QCOMPARE(reader->readingMode(),5);
    shell->setMode(7);QCOMPARE(reader->readingMode(),7);
    shell->setMode(1);QVERIFY(!reader->isVisible());
    window.close();
}
void ActionManagerTests::testPortableReleasePolicy() {
    QVERIFY(NestUpdate::newerVersion("v0.10", "0.9"));
    QVERIFY(!NestUpdate::newerVersion("v0.1", "0.1"));
    QVERIFY(!NestUpdate::newerVersion("v0.2-beta", "0.1"));
    QVERIFY(!NestUpdate::newerVersion("invalid", "0.1"));
    const QString repository = "RilakKuma2/NestView";
    QVERIFY(NestUpdate::validAssetURL(QUrl("https://github.com/RilakKuma2/NestView/releases/download/v0.2/NestView.zip"), repository));
    QVERIFY(!NestUpdate::validAssetURL(QUrl("http://github.com/RilakKuma2/NestView/releases/download/v0.2/app.zip"), repository));
    QVERIFY(!NestUpdate::validAssetURL(QUrl("https://github.com/other/NestView/releases/download/v0.2/app.zip"), repository));
    QVERIFY(!NestUpdate::validAssetURL(QUrl("https://github.com.example.com/RilakKuma2/NestView/releases/download/v0.2/app.zip"), repository));
}
void ActionManagerTests::testNativeDecoder() {
    QTemporaryDir dir;QImage image(320,200,QImage::Format_RGB32);image.fill(Qt::green);
    QString path=dir.filePath("한글 이미지.png");QVERIFY(image.save(path));
    QCOMPARE(NestMedia::size(path),QSize(320,200));
    auto thumb=NestMedia::read(path,80);QVERIFY(!thumb.isNull());QVERIFY(thumb.width()<=80);
    QCOMPARE(NestMedia::read(path,1600).size(),QSize(320,200)); // Maximum is a cap, not an upscale target.
    QVERIFY(NestMedia::metadata(path).contains("320"));
    QVERIFY(NestMedia::read(dir.filePath("missing.heic")).isNull());
}
static bool makeArchive(const QString &path, const QStringList &names, const QByteArray &data) {
    auto a=archive_write_new();archive_write_set_format_zip(a);
    if(archive_write_open_filename(a,QFile::encodeName(path))!=ARCHIVE_OK){archive_write_free(a);return false;}
    for(auto name:names){auto e=archive_entry_new();archive_entry_set_pathname(e,name.toUtf8());archive_entry_set_filetype(e,AE_IFREG);archive_entry_set_perm(e,0644);archive_entry_set_size(e,data.size());archive_write_header(a,e);archive_write_data(a,data.constData(),data.size());archive_entry_free(e);}
    archive_write_close(a);archive_write_free(a);return true;
}
void ActionManagerTests::testArchiveAndTraversal() {
    QTemporaryDir dir;QImage image(40,60,QImage::Format_RGB32);image.fill(Qt::blue);QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);QVERIFY(image.save(&buffer,"PNG"));
    auto zip=dir.filePath("comic.cbz");QVERIFY(makeArchive(zip,{"chapter/page10.png","chapter/page2.png"},bytes));
    auto result=NestMedia::extract(zip);QVERIFY2(result.error.isEmpty(),qPrintable(result.error));QCOMPARE(result.files.size(),2);
    QVERIFY(result.files.first().endsWith("page2.png"));QVERIFY(!NestMedia::read(result.files.first()).isNull());
    auto evil=dir.filePath("bad.zip");QVERIFY(makeArchive(evil,{"../escaped.png"},bytes));auto rejected=NestMedia::extract(evil);QVERIFY(!rejected.error.isEmpty());QVERIFY(rejected.files.isEmpty());QVERIFY(!QFile::exists(dir.filePath("escaped.png")));
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.show();window.openFile(zip);QTRY_VERIFY_WITH_TIMEOUT(window.getIsPixmapLoaded(),5000);QCOMPARE(window.getCurrentFileDetails().folderFileInfoList.size(),2);window.close();
}
void ActionManagerTests::testCloseDuringDecode() {
    QTemporaryDir dir;QImage image(3000,2000,QImage::Format_RGB32);image.fill(Qt::cyan);QVERIFY(image.save(dir.filePath("large.png")));
    for(int i=0;i<4;++i){auto window=new MainWindow;window->openFile(dir.filePath("large.png"));delete window;}
    QVERIFY(true);
}
void ActionManagerTests::testLatestArchiveRequest() {
    QTemporaryDir dir;QImage image(80,60,QImage::Format_RGB32);image.fill(Qt::red);
    QByteArray bytes;QBuffer buffer(&bytes);buffer.open(QIODevice::WriteOnly);QVERIFY(image.save(&buffer,"PNG"));
    const auto first=dir.filePath("first.cbz"),second=dir.filePath("second.cbz"),normal=dir.filePath("normal.png");
    QVERIFY(makeArchive(first,{"first.png"},bytes));QVERIFY(makeArchive(second,{"second.png"},bytes));QVERIFY(image.save(normal));
    auto cancelled=std::make_shared<std::atomic_bool>(true);
    QVERIFY(!NestMedia::extract(first,cancelled).error.isEmpty());
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.show();
    window.openFile(first);window.openFile(second);
    QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("000001_second.png"));
    const auto temporary=window.getCurrentFileDetails().fileInfo.absolutePath();
    window.openFile(first);window.openFile(normal);
    QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("normal.png"));
    QTRY_VERIFY(!NestBackgroundActivity::active());QTest::qWait(50);
    QCOMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("normal.png"));
    QTRY_VERIFY(!QDir(temporary).exists());
    QImage slow(3000,2000,QImage::Format_RGB32);slow.fill(Qt::blue);
    const auto slowPath=dir.filePath("slow.png");QVERIFY(slow.save(slowPath));
    window.openFile(slowPath);window.openFile(second);
    QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("000001_second.png"));
    QTRY_VERIFY(window.getIsPixmapLoaded());
    QVERIFY(QFile::exists(window.getCurrentFileDetails().fileInfo.absoluteFilePath()));
    window.close();
    auto closing=new MainWindow;closing->openFile(first);delete closing;
    QTRY_VERIFY(!NestBackgroundActivity::active());
}

void ActionManagerTests::testConversionPreservesOriginal() {
    QTemporaryDir dir;
    QImage image(400,200,QImage::Format_ARGB32);image.fill(Qt::transparent);
    auto source=dir.filePath("source.png");QVERIFY(image.save(source));
    QFile original(source);QVERIFY(original.open(QIODevice::ReadOnly));auto bytes=original.readAll();original.close();
    for(const auto &format:QList<QByteArray>{"png","jpeg","webp"}) {
        auto dest=dir.filePath("output."+QString::fromUtf8(format));
        auto error=NestMedia::convert(source,dest,format,100,90);QVERIFY2(error.isEmpty(),qPrintable(error));
        auto output=NestMedia::read(dest);QCOMPARE(output.size(),QSize(100,50));
        if(format=="jpeg")QVERIFY(output.pixelColor(10,10).red()>245);
        QVERIFY(!NestMedia::convert(source,dest,format,100,90).isEmpty());
    }
    QVERIFY(!NestMedia::convert(source,source,"png",100,90).isEmpty());
    QVERIFY(original.open(QIODevice::ReadOnly));QCOMPARE(original.readAll(),bytes);
}

void ActionManagerTests::testReferenceChrome() {
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.resize(960,630);window.show();
    auto toolbar=window.findChild<QToolBar *>("workspaceToolbar");QVERIFY(toolbar);QCOMPARE(toolbar->height(),36);
    auto updateButton=toolbar->findChild<QToolButton *>("checkForUpdatesButton");
    QVERIFY(!updateButton);
#ifdef Q_OS_MACOS
    QVERIFY(window.menuBar()->isNativeMenuBar());
#else
    QVERIFY(!window.menuBar()->isNativeMenuBar());
#endif
    QVERIFY(window.menuBar()->actions().size() >= 7);
    QVERIFY(!window.menuBar()->findChild<QAction *>("checkForUpdatesAction"));
    auto &manager=qvApp->getActionManager();
    auto updateAction=manager.findChild<QAction *>("checkForUpdatesAction");
    QVERIFY(updateAction);QVERIFY(updateAction->isVisible());QVERIFY(updateAction->isEnabled());
    QCOMPARE(updateAction->menuRole(),QAction::ApplicationSpecificRole);
#ifdef Q_OS_MACOS
    QTRY_COMPARE(nativeApplicationUpdateItemCount(),1);
#endif
    {
        MainWindow secondWindow;secondWindow.setAttribute(Qt::WA_DeleteOnClose,false);secondWindow.show();
#ifdef Q_OS_MACOS
        QTRY_COMPARE(nativeApplicationUpdateItemCount(),1);
#endif
        QCOMPARE(manager.findChildren<QAction *>("checkForUpdatesAction").size(),1);
        QVERIFY(!secondWindow.menuBar()->findChild<QAction *>("checkForUpdatesAction"));
        secondWindow.close();
    }
    window.activateWindow();
#ifdef Q_OS_MACOS
    QTRY_COMPARE(nativeApplicationUpdateItemCount(),1);
#endif
    QCOMPARE(manager.findChildren<QAction *>("checkForUpdatesAction").size(),1);
    auto floating=window.findChild<QFrame *>("floatingControls");QVERIFY(floating);QCOMPARE(floating->height(),46);
    QTRY_VERIFY(floating->geometry().bottom()<window.height());
    const QString capture=qEnvironmentVariable("NEST_CAPTURE_DIR");
    if(!capture.isEmpty()){window.openFile(qEnvironmentVariable("NEST_SAMPLE_PATH"));QTRY_VERIFY_WITH_TIMEOUT(window.getIsPixmapLoaded(),5000);QTest::qWait(150);window.grab().save(capture+"/viewer.png");}
    auto panel=window.findChild<QFrame *>("inspectorPanel");QVERIFY(panel);QVERIFY(!panel->isVisible());
    auto before=window.centralWidget()->geometry();
    for(auto button:toolbar->findChildren<QToolButton *>())if(button->objectName()=="viewOptionsButton"){button->click();break;}
    QVERIFY(panel->isVisible());if(!capture.isEmpty()){QTest::qWait(100);window.grab().save(capture+"/image-panel.png");}
    QCOMPARE(panel->width(),386);QCOMPARE(window.centralWidget()->geometry(),before);
    for(auto radio:panel->findChildren<QRadioButton *>())if(radio->text().contains("웹툰")){radio->click();break;}
    QCOMPARE(window.findChild<NestShell *>()->viewMode(),5);
    window.close();
}
void ActionManagerTests::testWheelAndHoverThumbnails() {
    QSettings().setValue("nest/wheelAction",0);QSettings().setValue("nest/hoverThumbnails",true);
    QTemporaryDir dir;QImage image(1200,900,QImage::Format_RGB32);
    for(int i=1;i<=3;++i){image.fill(QColor::fromHsv(i*70,150,220));QVERIFY(image.save(dir.filePath(QString("page%1.png").arg(i))));}
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.show();window.openFile(dir.filePath("page2.png"));
    QTRY_VERIFY(window.getIsPixmapLoaded());
    auto view=window.findChild<QVGraphicsView *>();QVERIFY(view);
    auto wheel=[&](int delta,Qt::KeyboardModifiers modifiers=Qt::NoModifier,Qt::ScrollPhase phase=Qt::NoScrollPhase){
        auto target=view->viewport();QPoint local=target->rect().center();
        QWheelEvent event(local,target->mapToGlobal(local),QPoint(),QPoint(0,delta),Qt::NoButton,modifiers,phase,false);
        QApplication::sendEvent(target,&event);
    };
    wheel(-120);QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("page3.png"));
    wheel(120);QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("page2.png"));
    wheel(-1);QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("page3.png"));
    wheel(120,Qt::NoModifier,Qt::ScrollMomentum);QTest::qWait(30);QCOMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("page3.png"));
    auto scale=view->transform().m11();wheel(120,Qt::ControlModifier);QTRY_VERIFY(view->transform().m11()>scale);
    QCOMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("page3.png"));
    auto strip=window.findChild<QListWidget *>("hoverThumbnails");QVERIFY(strip);QCOMPARE(strip->count(),3);
    auto area=window.centralWidget()->geometry();
    auto move=[&](QPoint point){QCursor::setPos(window.mapToGlobal(point));QMouseEvent event(QEvent::MouseMove,view->viewport()->mapFrom(&window,point),window.mapToGlobal(point),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(view->viewport(),&event);};
    move(QPoint(window.width()/2,area.bottom()-4));QVERIFY(strip->isVisible());QCOMPARE(strip->currentRow(),2);
    QCOMPARE(window.centralWidget()->geometry(),area);
    QTest::mouseClick(strip->viewport(),Qt::LeftButton,Qt::NoModifier,strip->visualItemRect(strip->item(0)).center());
    QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("page1.png"));
    QCOMPARE(strip->currentRow(),0);
    const auto capture=qEnvironmentVariable("NEST_CAPTURE_DIR");if(!capture.isEmpty()){move(QPoint(window.width()/2,area.bottom()-4));QTest::qWait(120);window.grab().save(capture+"/hover-thumbnails.png");}
    move(QPoint(window.width()/2,area.top()+30));QTRY_VERIFY_WITH_TIMEOUT(!strip->isVisible(),1000);
    window.close();
}
void ActionManagerTests::testFolderGridAndScrubbing(){
    QSettings().setValue("nest/hoverThumbnails",true);
    QTemporaryDir dir;
    for(int i=0;i<18;++i){QImage image(i%2?640:420,i%2?420:640,QImage::Format_RGB32);image.fill(QColor::fromHsv(i*17,110,170));QPainter painter(&image);painter.setPen(Qt::NoPen);painter.setBrush(QColor::fromHsv(i*17+20,80,240));painter.drawEllipse(QPoint(image.width()*2/3,image.height()/3),65,65);painter.setBrush(QColor::fromHsv(i*17,140,100));painter.drawPolygon(QPolygon()<<QPoint(0,image.height())<<QPoint(image.width()/3,image.height()/2)<<QPoint(image.width(),image.height()));painter.end();QVERIFY(image.save(dir.filePath(QString("image%1.png").arg(i,2,10,QChar('0')))));}
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.show();window.openFile(dir.filePath("image00.png"));QTRY_VERIFY(window.getIsPixmapLoaded());
    auto strip=window.findChild<QListWidget *>("hoverThumbnails");auto tray=window.findChild<QFrame *>("thumbnailTray");QVERIFY(strip);QVERIFY(tray);tray->show();QTest::qWait(30);
    QCOMPARE(strip->count(),18);QVERIFY(strip->item(0)->text().isEmpty());
    auto floating=window.findChild<QFrame *>("floatingControls");QVERIFY(floating->y()-tray->geometry().bottom()<=2);
    auto slider=window.findChild<QSlider *>("pagePosition");QPoint hover(slider->width()-1,slider->height()/2);QMouseEvent event(QEvent::MouseMove,hover,slider->mapToGlobal(hover),Qt::NoButton,Qt::NoButton,Qt::NoModifier);QApplication::sendEvent(slider,&event);QCOMPARE(QToolTip::text(),QString("18/18"));QToolTip::hideText();
    QCOMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("image00.png"));
    QVERIFY(qAbs(strip->visualRect(strip->model()->index(0,0)).center().x()-strip->viewport()->rect().center().x())<=1);
    auto sendWheel=[&](QPoint pixel,QPoint angle){QPoint point=strip->viewport()->rect().center();QWheelEvent e(point,strip->viewport()->mapToGlobal(point),pixel,angle,Qt::NoButton,Qt::NoModifier,pixel.isNull()?Qt::NoScrollPhase:Qt::ScrollUpdate,false);QApplication::sendEvent(strip->viewport(),&e);};
    sendWheel({},QPoint(0,-120));QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("image01.png"));
    sendWheel(QPoint(-168,0),{});QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("image03.png"));
    QVERIFY(qAbs(strip->visualRect(strip->model()->index(3,0)).center().x()-strip->viewport()->rect().center().x())<=1);
    sendWheel({},QPoint(0,-12000));QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("image17.png"));
    QVERIFY(qAbs(strip->visualRect(strip->model()->index(17,0)).center().x()-strip->viewport()->rect().center().x())<=1);
    auto button=window.findChild<QToolButton *>("folderGridButton");QVERIFY(button);button->click();
    auto dialog=window.findChild<QDialog *>("folderGridDialog");auto grid=window.findChild<QListWidget *>("folderGrid");QVERIFY(dialog->isVisible());QCOMPARE(grid->count(),18);QVERIFY(grid->isWrapping());QCOMPARE(grid->currentRow(),17);QVERIFY(grid->item(1)->text().isEmpty());
    auto size=dialog->findChild<QSlider *>("gridThumbnailSize");size->setValue(180);QCOMPARE(grid->item(0)->data(Qt::UserRole+2).toInt(),180);
    grid->scrollToItem(grid->item(2));QTRY_VERIFY(grid->item(2)->data(Qt::UserRole+1).toSize().isValid());QTRY_VERIFY(grid->item(1)->data(Qt::UserRole+1).toSize().isValid());
    QVERIFY(grid->visualItemRect(grid->item(2)).width()<grid->visualItemRect(grid->item(2)).height());
    QVERIFY(grid->visualItemRect(grid->item(1)).width()>grid->visualItemRect(grid->item(1)).height());
    auto capture=qEnvironmentVariable("NEST_CAPTURE_DIR");if(!capture.isEmpty()){QTest::qWait(250);dialog->grab().save(capture+"/folder-grid.png");}
    QTest::mouseClick(grid->viewport(),Qt::LeftButton,Qt::NoModifier,grid->visualItemRect(grid->item(2)).center());QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("image02.png"));QVERIFY(!dialog->isVisible());
    if(!capture.isEmpty()){tray->show();QTest::qWait(100);tray->show();window.grab().save(capture+"/filmstrip.png");}
    window.close();
}
void ActionManagerTests::testScrollDeviceSeparation(){
    QSettings().setValue("nest/scrollDevice",0);QSettings().setValue("nest/trackpadAction",0);QSettings().setValue("nest/wheelAction",0);
    QPointingDevice mouse("test mouse",101,QInputDevice::DeviceType::Mouse,QPointingDevice::PointerType::Generic,QInputDevice::Capability::Position,1,3);
    QPointingDevice pad("test trackpad",102,QInputDevice::DeviceType::TouchPad,QPointingDevice::PointerType::Finger,QInputDevice::Capability::Position,5,0);
    QTemporaryDir dir;QImage image(1800,1200,QImage::Format_RGB32);image.fill(Qt::blue);for(int i=1;i<=4;++i)QVERIFY(image.save(dir.filePath(QString("%1.png").arg(i))));
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.show();window.openFile(dir.filePath("2.png"));QTRY_VERIFY(window.getIsPixmapLoaded());auto view=window.findChild<QVGraphicsView *>();view->originalSize();
    auto send=[&](const QPointingDevice *device,QPoint pixel,QPoint angle){auto target=view->viewport();QPoint p=target->rect().center();QWheelEvent e(p,target->mapToGlobal(p),pixel,angle,Qt::NoButton,Qt::NoModifier,Qt::NoScrollPhase,false,Qt::MouseEventNotSynthesized,device);QApplication::sendEvent(target,&e);};
    send(&mouse,QPoint(0,-1),QPoint(0,-1));QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("3.png"));
    send(&mouse,QPoint(0,1),QPoint(0,1));QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("2.png"));
    view->originalSize();int before=view->horizontalScrollBar()->value();send(&pad,QPoint(-30,0),{});QVERIFY(view->horizontalScrollBar()->value()>before);QCOMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("2.png"));
    QSettings().setValue("nest/wheelAction",2);before=view->horizontalScrollBar()->value();send(&pad,QPoint(-20,0),{});QVERIFY(view->horizontalScrollBar()->value()>before);
    QSettings().setValue("nest/wheelAction",0);QSettings().setValue("nest/scrollDevice",1);send(&pad,QPoint(0,-1),{});QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("3.png"));
    window.close();QSettings().setValue("nest/scrollDevice",0);
}
void ActionManagerTests::testGalleryZoomRouting(){
    QTemporaryDir dir;QImage image(1600,1200,QImage::Format_RGB32);image.fill(Qt::cyan);QVERIFY(image.save(dir.filePath("one.png")));
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.show();window.openFile(dir.filePath("one.png"));QTRY_VERIFY(window.getIsPixmapLoaded());window.findChild<QToolButton *>("folderGridButton")->click();
    auto grid=window.findChild<QListWidget *>("folderGrid");auto size=window.findChild<QSlider *>("gridThumbnailSize");auto view=window.findChild<QVGraphicsView *>();size->setValue(180);QTest::qWait(100);
    const auto before=view->displayedImageRect();QPoint global=grid->viewport()->mapToGlobal(grid->viewport()->rect().center());QPoint local=view->viewport()->mapFromGlobal(global);
    // Qt may target the underlying viewport; position must decide the zoom recipient.
    QWheelEvent wheel(local,global,{},QPoint(0,120),Qt::NoButton,Qt::ControlModifier,Qt::NoScrollPhase,false);QApplication::sendEvent(view->viewport(),&wheel);QVERIFY(size->value()>180);QCOMPARE(view->displayedImageRect(),before);
    int previous=size->value();QNativeGestureEvent pinch(Qt::ZoomNativeGesture,QPointingDevice::primaryPointingDevice(),2,local,local,global,0.25,{});QApplication::sendEvent(view->viewport(),&pinch);QVERIFY(size->value()>previous);QCOMPARE(view->displayedImageRect(),before);
    previous=size->value();window.zoomIn();QVERIFY(size->value()>previous);window.zoomOut();QCOMPARE(view->displayedImageRect(),before);
    window.close();QSettings().setValue("nest/gridSize",180);
}
void ActionManagerTests::testMacTrackpadGestures(){
    QTemporaryDir dir;QImage image(1800,1200,QImage::Format_RGB32);image.fill(Qt::darkCyan);for(int i=1;i<=4;++i)QVERIFY(image.save(dir.filePath(QString("%1.png").arg(i))));
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.show();window.openFile(dir.filePath("2.png"));QTRY_VERIFY(window.getIsPixmapLoaded());auto view=window.findChild<QVGraphicsView *>();view->resetScale();
    quint64 inputTime=1000;
    auto wheel=[&](QPoint delta,Qt::ScrollPhase phase){auto target=view->viewport();QPoint p=target->rect().center();QWheelEvent e(p,target->mapToGlobal(p),delta,{},Qt::NoButton,Qt::NoModifier,phase,false);e.setTimestamp(inputTime++);QApplication::sendEvent(target,&e);};
    wheel(QPoint(-12,0),Qt::ScrollBegin);QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("3.png"));
    wheel(QPoint(-100,0),Qt::ScrollUpdate);wheel(QPoint(-200,0),Qt::ScrollMomentum);QTest::qWait(30);QCOMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("3.png"));
    wheel(QPoint(12,0),Qt::ScrollUpdate);QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("2.png"));
    wheel(QPoint(-12,0),Qt::ScrollUpdate);QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("3.png"));
    inputTime+=110;wheel(QPoint(-12,0),Qt::ScrollUpdate);QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("4.png"));
    wheel(QPoint(12,0),Qt::ScrollBegin);QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("3.png"));
    wheel(QPoint(12,0),Qt::ScrollBegin);QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("2.png"));
    auto gesture=[&](QWidget *target,Qt::NativeGestureType type,double value){QPoint p=target->rect().center();QNativeGestureEvent e(type,QPointingDevice::primaryPointingDevice(),2,p,p,target->mapToGlobal(p),value,{});QApplication::sendEvent(target,&e);};
    double scale=view->transform().m11();gesture(view->viewport(),Qt::ZoomNativeGesture,0.3);QVERIFY(view->transform().m11()>scale);
    view->originalSize();auto h=view->horizontalScrollBar();int before=h->value();wheel(QPoint(-35,-20),Qt::ScrollBegin);QVERIFY(h->value()>before);QCOMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("2.png"));
    gesture(view->viewport(),Qt::SmartZoomNativeGesture,1);QVERIFY(view->displayedImageRect().width()<=view->viewport()->width()+8);QVERIFY(view->displayedImageRect().height()<=view->viewport()->height()+8);
    window.findChild<QToolButton *>("folderGridButton")->click();auto grid=window.findChild<QListWidget *>("folderGrid");auto size=window.findChild<QSlider *>("gridThumbnailSize");size->setValue(180);gesture(grid->viewport(),Qt::ZoomNativeGesture,0.25);QVERIFY(size->value()>180);
    auto dialog=window.findChild<QDialog *>("folderGridDialog");QVERIFY(!dialog->isWindow());QVERIFY(dialog->width()>=window.centralWidget()->width()-20);
    QTest::keyClick(grid,Qt::Key_Escape);QVERIFY(!dialog->isVisible());QVERIFY(window.isVisible());window.close();QSettings().setValue("nest/gridSize",180);
}
void ActionManagerTests::testTrackpadInputTiming() {
    NestTrackpadNavigation gate;
    QCOMPARE(gate.step({0,-6},Qt::ScrollBegin,1000),0);
    QCOMPARE(gate.step({0,-6},Qt::ScrollUpdate,1001),1);
    QCOMPARE(gate.step({0,-120},Qt::ScrollUpdate,1002),0);
    QCOMPARE(gate.step({0,-120},Qt::ScrollMomentum,1200),0);
    QCOMPARE(gate.step({0,-12},Qt::ScrollUpdate,1100),0);
    QCOMPARE(gate.step({0,-12},Qt::ScrollUpdate,1101),1);
    QCOMPARE(gate.step({0,12},Qt::ScrollUpdate,1102),-1);
    QCOMPARE(gate.step({0,-120},Qt::ScrollEnd,1103),0);
    QCOMPARE(gate.step({0,-12},Qt::ScrollBegin,1104),1);
    QCOMPARE(gate.step({0,-12},Qt::ScrollUpdate,2000),1);
    QCOMPARE(gate.step({0,-12},Qt::ScrollUpdate,10),1); // New timestamp origin.
}
void ActionManagerTests::testBackgroundActivityLifetime() {
    QVERIFY(!NestBackgroundActivity::active());
    auto activity=NestBackgroundActivity::begin();
    auto worker=activity;
    activity.reset(); // The dialog/owner has gone, but its worker has not.
    QVERIFY(NestBackgroundActivity::active());
    worker.reset();
    QVERIFY(!NestBackgroundActivity::active());
}
void ActionManagerTests::testPreloadQueueBound() {
    QTemporaryDir dir;
    QVImageCore core;
    for(int i=0;i<1000;++i)core.requestCachingFile(dir.filePath(QString::number(i)+".png"),QColorSpace());
    QCOMPARE(core.pendingPreloadCount(),2);
    core.requestCachingFile(dir.filePath("0.png"),QColorSpace());
    QCOMPARE(core.pendingPreloadCount(),2);
    QTRY_COMPARE(core.pendingPreloadCount(),0);
}
void ActionManagerTests::testUndoDeleteConflictCanRetry() {
    QTemporaryDir dir;QImage image(80,60,QImage::Format_RGB32);image.fill(Qt::red);
    const auto path=dir.filePath("undo-test.png");QVERIFY(image.save(path));
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.show();window.openFile(path);
    QTRY_VERIFY(window.getIsPixmapLoaded());window.deleteFile(false);QVERIFY(!QFile::exists(path));
    QFile conflict(path);QVERIFY(conflict.open(QIODevice::WriteOnly));conflict.write("occupied");conflict.close();
    QTimer::singleShot(0,&window,[]{
        auto dialog=qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        QVERIFY(dialog);dialog->accept();
    });
    window.undoDelete();
    QVERIFY(conflict.open(QIODevice::ReadOnly));QCOMPARE(conflict.readAll(),QByteArray("occupied"));conflict.close();
    QVERIFY(QFile::remove(path));window.undoDelete();
    QTRY_VERIFY(window.getIsPixmapLoaded());QCOMPARE(NestMedia::read(path).pixelColor(0,0),QColor(Qt::red));
    window.close();
}
void ActionManagerTests::testReadingRequestReplacement() {
    QTemporaryDir dir;QImage image(80,120,QImage::Format_RGB32);image.fill(Qt::green);
    const auto path=dir.filePath("page.png");QVERIFY(image.save(path));
    NestReadingView reader;reader.resize(600,500);reader.show();
    QStringList stale;for(int i=0;i<2000;++i)stale.append(dir.filePath(QString::number(i)+".png"));
    reader.setFiles(stale,0,5);reader.setFiles({path},0,7);reader.invalidateImages();
    QTRY_VERIFY(reader.viewport()->grab().toImage().pixelColor(300,200).green()>200);
    QCOMPARE(reader.currentIndex(),0);QCOMPARE(reader.readingMode(),7);
    auto closing=new NestReadingView;closing->setFiles(stale,0,5);delete closing;
}
void ActionManagerTests::testThumbnailQueueBound() {
    auto &store=NestThumbnailStore::instance();
    QTRY_COMPARE(store.pendingCount(),0);
    QTemporaryDir dir;
    for(int i=0;i<1000;++i)store.request(dir.filePath(QString::number(i)+".png"),QColorSpace());
    QCOMPARE(store.pendingCount(),NestThumbnailStore::maximumPendingRequests);
    QTRY_COMPARE_WITH_TIMEOUT(store.pendingCount(),0,5000);
    QVERIFY(store.request(dir.filePath("retry.png"),QColorSpace()));
    QTRY_COMPARE(store.pendingCount(),0);
}
void ActionManagerTests::testGridWheelZoom(){
    QTemporaryDir dir;QImage image(600,400,QImage::Format_RGB32);image.fill(Qt::cyan);QVERIFY(image.save(dir.filePath("one.png")));
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.show();window.openFile(dir.filePath("one.png"));QTRY_VERIFY(window.getIsPixmapLoaded());window.findChild<QToolButton *>("folderGridButton")->click();
    auto grid=window.findChild<QListWidget *>("folderGrid");auto size=window.findChild<QSlider *>("gridThumbnailSize");QTRY_VERIFY(grid->item(0)->data(Qt::UserRole+1).toSize().isValid());size->setValue(180);
    auto wheel=[&](int delta,Qt::KeyboardModifiers modifiers){QPoint p=grid->viewport()->rect().center();QWheelEvent e(p,grid->viewport()->mapToGlobal(p),{},QPoint(0,delta),Qt::NoButton,modifiers,Qt::NoScrollPhase,false);QApplication::sendEvent(grid->viewport(),&e);};
    wheel(120,Qt::MetaModifier);QVERIFY(size->value()>180);int value=size->value();wheel(-120,Qt::MetaModifier);QVERIFY(size->value()<value);
    value=size->value();wheel(-120,Qt::NoModifier);QCOMPARE(size->value(),value);
    size->setValue(size->maximum());QVERIFY(grid->visualItemRect(grid->item(0)).width()>=grid->viewport()->width()-12);QVERIFY(grid->visualItemRect(grid->item(0)).width()<=grid->viewport()->width());
    window.close();QSettings().setValue("nest/gridSize",180);
}
void ActionManagerTests::testThumbnailDiskBudget(){
    QTemporaryDir dir;
    NestThumbnailDiskCache cache(dir.path(),100);
    QVERIFY(cache.put("a.png",QByteArray(40,'a')));QVERIFY(cache.put("b.png",QByteArray(40,'b')));
    cache.touch("a.png");QVERIFY(cache.put("c.png",QByteArray(40,'c')));
    QCOMPARE(cache.bytesUsed(),qint64(80));QVERIFY(QFile::exists(dir.filePath("a.png")));QVERIFY(!QFile::exists(dir.filePath("b.png")));
    QVERIFY(!cache.put("oversized.png",QByteArray(101,'x')));QCOMPARE(cache.bytesUsed(),qint64(80));
    NestThumbnailDiskCache reopened(dir.path(),50);QVERIFY(reopened.bytesUsed()<=50);
    qint64 total=0;for(const auto &f:QDir(dir.path()).entryInfoList(QDir::Files))total+=f.size();QCOMPARE(total,reopened.bytesUsed());
}
void ActionManagerTests::testThumbnailCacheAndIdentity(){
    QTemporaryDir dir;
    for(int i=0;i<8;++i){QImage image(i%2?160:90,120,QImage::Format_RGB32);image.fill(i%2?Qt::blue:Qt::red);QVERIFY(image.save(dir.filePath(QString("%1.png").arg(i))));}
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.show();window.openFile(dir.filePath("0.png"));QTRY_VERIFY(window.getIsPixmapLoaded());
    auto button=window.findChild<QToolButton *>("folderGridButton");button->click();
    auto grid=window.findChild<QListWidget *>("folderGrid");auto dialog=window.findChild<QDialog *>("folderGridDialog");QVERIFY(grid->isVisible()); // Opening never waits for metadata workers.
    QTRY_VERIFY(grid->item(0)->data(Qt::UserRole+1).toSize().isValid());
    grid->scrollToTop();QTest::qWait(50);
    QVector<QRect> rects;for(int i=0;i<grid->count();++i)rects<<grid->visualItemRect(grid->item(i));
    auto &store=NestThumbnailStore::instance();QTRY_COMPARE(store.pendingCount(),0);
    for(int i=0;i<grid->count();++i)QCOMPARE(grid->visualItemRect(grid->item(i)),rects[i]);
    int requests=store.requestCount();dialog->hide();button->click();grid->scrollToTop();QTest::qWait(100);QCOMPARE(store.requestCount(),requests);
    for(int i=0;i<grid->count();++i)QCOMPARE(grid->visualItemRect(grid->item(i)),rects[i]);
    // Replace rows while requests are in flight; rendering must follow the current path.
    QListWidget list;list.resize(200,200);auto delegate=new NestThumbnails(&list,true);list.setItemDelegate(delegate);
    auto item=new QListWidgetItem(&list);item->setData(Qt::UserRole,dir.filePath("0.png"));
    QImage canvas(100,100,QImage::Format_RGB32);QStyleOptionViewItem option;option.rect=canvas.rect();option.widget=&list;
    auto render=[&]{canvas.fill(Qt::black);QPainter painter(&canvas);delegate->paint(&painter,option,list.model()->index(0,0));};
    render();list.clear();item=new QListWidgetItem(&list);item->setData(Qt::UserRole,dir.filePath("1.png"));render();QTRY_COMPARE(store.pendingCount(),0);render();QVERIFY(canvas.pixelColor(50,50).blue()>240);QVERIFY(canvas.pixelColor(50,50).red()<10);
    const auto oldKey=NestThumbnailStore::key(dir.filePath("1.png"),NestMedia::displaySpace(&list));
    QImage replacement(180,100,QImage::Format_RGB32);replacement.fill(Qt::green);QVERIFY(replacement.save(dir.filePath("1.png")));
    QVERIFY(oldKey!=NestThumbnailStore::key(dir.filePath("1.png"),NestMedia::displaySpace(&list)));render();QTRY_COMPARE(store.pendingCount(),0);render();QVERIFY(canvas.pixelColor(50,50).green()>240);
    window.close();
}
void ActionManagerTests::testContextSortingPersists(){
    QTemporaryDir dir;
    QStringList names={"page10.png","page2.bmp","page1.png"};
    for(int i=0;i<3;++i){int edge=i==0?20:(i==1?60:40);QImage image(edge,edge,QImage::Format_RGB32);image.fill(Qt::cyan);QImageWriter writer(dir.filePath(names[i]));writer.setCompression(0);QVERIFY(writer.write(image));QFile file(dir.filePath(names[i]));QVERIFY(file.open(QIODevice::ReadWrite));QVERIFY(file.setFileTime(QDateTime::fromSecsSinceEpoch(1700000000+(i==0?20:(i==1?10:30))),QFileDevice::FileModificationTime));}
    QSettings().setValue("options/sortmode",0);QSettings().setValue("options/sortdescending",false);qvApp->getSettingsManager().loadSettings();
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.show();window.openFile(dir.filePath("page1.png"));QTRY_VERIFY(window.getIsPixmapLoaded());
    auto shell=window.findChild<NestShell *>();auto view=window.findChild<QVGraphicsView *>();
    window.findChild<QToolButton *>("folderGridButton")->click();window.findChild<QDialog *>("folderGridDialog")->hide();
    auto order=[&]{QStringList result;for(const auto &f:window.getCurrentFileDetails().folderFileInfoList)result<<f.fileName;return result;};
    auto choose=[&](const QString &id){shell->showContextMenu(window.mapToGlobal(QPoint(30,70)));auto menu=window.findChild<QMenu *>("imageSortMenu");if(!menu)return false;auto action=menu->findChild<QAction *>(id);if(!action)return false;action->trigger();auto root=qobject_cast<QMenu *>(menu->parentWidget());if(root){root->close();delete root;}return true;};
    auto synchronized=[&](const QStringList &expected){for(auto name:{"hoverThumbnails","folderGrid"}){auto list=window.findChild<QListWidget *>(name);if(!list||list->count()!=expected.size())return false;for(int i=0;i<list->count();++i)if(QFileInfo(list->item(i)->data(Qt::UserRole).toString()).fileName()!=expected[i])return false;}return true;};
    QCOMPARE(order(),QStringList({"page1.png","page2.bmp","page10.png"}));
    QVERIFY(choose("sortMode3"));QCOMPARE(order(),QStringList({"page10.png","page1.png","page2.bmp"}));QVERIFY(synchronized(order()));
    QVERIFY(choose("sortDescending"));QCOMPARE(order(),QStringList({"page2.bmp","page1.png","page10.png"}));
    QCOMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("page1.png"));
    auto expected=order();shell->navigate(1);QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),QString("page10.png"));QCOMPARE(order(),expected);view->refreshFileList();QCOMPARE(order(),expected);QVERIFY(synchronized(expected));
    QVERIFY(choose("sortDescending"));QVERIFY(choose("sortMode1"));QCOMPARE(order(),QStringList({"page2.bmp","page10.png","page1.png"}));
    QVERIFY(choose("sortDescending"));QCOMPARE(order(),QStringList({"page1.png","page10.png","page2.bmp"}));
    QVERIFY(choose("sortDescending"));QVERIFY(choose("sortMode4"));QCOMPARE(order(),QStringList({"page2.bmp","page1.png","page10.png"}));
    QVERIFY(choose("sortMode2"));expected=order();view->refreshFileList();QCOMPARE(order(),expected);
    QVERIFY(choose("sortMode5"));expected=order();auto members=expected;members.sort();auto all=names;all.sort();QCOMPARE(members,all);
    for(int i=0;i<3;++i){view->refreshFileList();QCOMPARE(order(),expected);}
    int next=(window.getCurrentFileDetails().loadedIndexInFolder+1)%3;shell->navigate(1);QTRY_COMPARE(window.getCurrentFileDetails().fileInfo.fileName(),expected[next]);QCOMPARE(order(),expected);QVERIFY(synchronized(expected));
    QVERIFY(choose("sortMode0"));QCOMPARE(order(),QStringList({"page1.png","page2.bmp","page10.png"}));
    QVERIFY(choose("sortDescending"));QCOMPARE(order(),QStringList({"page10.png","page2.bmp","page1.png"}));
    window.close();QSettings().setValue("options/sortmode",0);QSettings().setValue("options/sortdescending",false);qvApp->getSettingsManager().loadSettings();
}
void ActionManagerTests::testLatestOpenRequest() {
    QTemporaryDir dir;QImage image(2000,1000,QImage::Format_RGB32);image.fill(Qt::yellow);
    for(auto name:{"first.png","second.png","last.png"})QVERIFY(image.save(dir.filePath(name)));
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.show();
    window.openFile(dir.filePath("first.png"));window.openFile(dir.filePath("second.png"));window.openFile(dir.filePath("last.png"));
    QTRY_COMPARE_WITH_TIMEOUT(window.getCurrentFileDetails().fileInfo.fileName(),QString("last.png"),5000);
    window.close();
}

void ActionManagerTests::testSettingsAndGrouping() {
    QTemporaryDir dir;QImage image(120,80,QImage::Format_RGB32);image.fill(Qt::red);QVERIFY(image.save(dir.filePath("photo.jpg")));
    QFile raw(dir.filePath("photo.nef"));QVERIFY(raw.open(QIODevice::WriteOnly));raw.write("fixture");raw.close();
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.show();window.openFile(dir.filePath("photo.jpg"));QTRY_VERIFY(window.getIsPixmapLoaded());
    QCOMPARE(window.getCurrentFileDetails().folderFileInfoList.size(),2);
    QSettings().setValue("nest/groupRawJpeg",true);window.findChild<QVGraphicsView *>()->refreshFileList();QCOMPARE(window.getCurrentFileDetails().folderFileInfoList.size(),1);
    QSettings().remove("nest/groupRawJpeg");
    showNestSettings(&window);QDialog *dialog=nullptr;for(auto widget:QApplication::topLevelWidgets())if(widget->objectName()=="nestPreferences")dialog=qobject_cast<QDialog *>(widget);
    QVERIFY(dialog);QVERIFY(dialog->isVisible());QCOMPARE(dialog->findChild<QStackedWidget *>()->count(),10);
    auto capture=qEnvironmentVariable("NEST_CAPTURE_DIR");if(!capture.isEmpty()){QTest::qWait(100);dialog->grab().save(capture+"/settings.png");}
    for(auto check:dialog->findChildren<QCheckBox *>())if(check->accessibleName().contains("ESC키"))check->setChecked(true);
    auto save=dialog->findChild<QPushButton *>("confirm");QVERIFY(save);save->click();QVERIFY(QSettings().value("nest/escapeCloses").toBool());QSettings().remove("nest/escapeCloses");
    window.close();
}

void ActionManagerTests::testStructuredMetadata() {
    QTemporaryDir dir;auto path=dir.filePath("camera.jpg");QVERIFY(createMetadataFixture(path));
    auto data=NestMedia::inspect(path);QVERIFY(data.text().contains("Fixture Camera"));QVERIFY(data.text().contains("50 mm test lens"));QVERIFY(data.text().contains("1/125"));QVERIFY(data.text().contains("f/4"));QVERIFY(data.text().contains("ISO 감도: 200"));QVERIFY(data.mapURL.toString().contains("37.550000"));QVERIFY(data.mapURL.toString().contains("126.966667"));
    MainWindow window;window.setAttribute(Qt::WA_DeleteOnClose,false);window.show();window.openFile(path);QTRY_VERIFY(window.getIsPixmapLoaded());
    auto shell=window.findChild<NestShell *>();shell->showContextMenu(window.mapToGlobal(QPoint(10,40)));QTest::qWait(50);
    QPointer<QMenu> context;for(auto menu:window.findChildren<QMenu *>())if(menu->isVisible()&&menu->minimumWidth()==310)context=menu;QVERIFY(context);
    auto capture=qEnvironmentVariable("NEST_CAPTURE_DIR");if(!capture.isEmpty())context->grab().save(capture+"/context-menu.png");
    for(auto action:context->actions())if(action->text().contains("메타 정보 더 자세히")){action->trigger();break;}
    QDialog *metadata=nullptr;for(auto dialog:window.findChildren<QDialog *>())if(dialog->windowTitle()=="메타 정보")metadata=dialog;QVERIFY(metadata);QVERIFY(metadata->isVisible());
    if(!capture.isEmpty()){QTest::qWait(50);metadata->grab().save(capture+"/metadata.png");}
    if(context)context->close();window.close();
}
void ActionManagerTests::testEffectsAndCacheIsolation() {
    QImage image(1,1,QImage::Format_ARGB32);image.setPixel(0,0,qRgba(20,40,80,128));
    NestMedia::Effects effect;effect.invert=true;auto inverted=NestMedia::applyEffects(image,effect);QCOMPARE(inverted.pixel(0,0),qRgba(235,215,175,128));
    effect.invert=false;effect.grayscale=true;auto gray=NestMedia::applyEffects(image,effect).pixelColor(0,0);QCOMPARE(gray.red(),gray.green());QCOMPARE(gray.green(),gray.blue());QCOMPARE(gray.alpha(),128);
    auto key1=QVImageCore::getPixmapCacheKey("test",100,QColorSpace::SRgb,effect.signature());effect.gamma=2;auto key2=QVImageCore::getPixmapCacheKey("test",100,QColorSpace::SRgb,effect.signature());QVERIFY(key1!=key2);
}

int main(int argc, char *argv[])
{
    QCoreApplication::setOrganizationName("NestViewTests");
    QCoreApplication::setApplicationName("NestViewTests");
    QSettings().clear();
    QVApplication app(argc, argv);
    ActionManagerTests actionManagerTests;
    return QTest::qExec(&actionManagerTests, argc, argv);
}

#include "tst_actionmanagertests.moc"
