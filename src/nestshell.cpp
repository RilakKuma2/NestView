#include "nestshell.h"
#include "nestplatform.h"
#include "nestthumbnails.h"
#include "nestfilmstrip.h"
#include "nesticons.h"
#include "nestreadingview.h"
#include "nestbackgroundactivity.h"
#include <QPointer>
#include "mainwindow.h"
#include "qvapplication.h"
#include <QMenuBar>
#include <QDialogButtonBox>
#include <QHeaderView>
#include <QPushButton>
#include <QToolButton>
#include <QSignalBlocker>
#include <QTabWidget>
#include <QProcess>
#include <QPainter>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QStatusBar>
#include <QVBoxLayout>
#include <QFileDialog>
#include <QMessageBox>
#include <QInputDialog>
#include <QSettings>
#include <QLineEdit>
#include <QKeyEvent>
#include <QNativeGestureEvent>
#include <QWheelEvent>
#include <QToolTip>
#include <QMouseEvent>
#include <QtMath>
#include <QScrollBar>
#include <QClipboard>
#include <QSaveFile>
#include <QImageWriter>
#include <QProgressDialog>
#include <QDesktopServices>
#include <QMimeData>
#include <QtConcurrent>
#include <atomic>

QAction *NestShell::action(QMenu *menu,const QString &text,const QString &key,std::function<void()> fn) {
    auto a=new QAction(text,w);a->setData(QStringList{"nest"});
    if(!key.isEmpty()) {a->setProperty("nestShortcutId",text);a->setShortcut(QKeySequence(QSettings().value("nest/shortcuts/"+text,key).toString()));}
    a->setShortcutContext(Qt::WindowShortcut);w->addAction(a);
    if(menu)menu->addAction(a);
    connect(a,&QAction::triggered,this,[fn]{fn();});return a;
}
NestShell::NestShell(MainWindow *window):QObject(window),w(window) {
    w->setMinimumSize(640,420);if(!QSettings().contains("geometry"))w->resize(1180,820);
    w->setStyleSheet("QToolBar { background:#20252e; border:0; spacing:8px; padding:8px; } QToolButton { color:#e9edf3; padding:6px 10px; border-radius:5px; } QToolButton:hover { background:#354052; } QStatusBar { color:#aeb8c7; background:#20252e; } QDockWidget { color:#c5cdd9; } QListWidget,QPlainTextEdit,QLineEdit { background:#191e27; color:#dde4ee; border:0; selection-background-color:#395d87; } QComboBox { min-height:27px; } ");
    top=new QToolBar("메뉴",w);top->setMovable(false);w->addToolBar(Qt::TopToolBarArea,top);
    bottom=new QToolBar("조작바",w);bottom->setMovable(false);w->addToolBar(Qt::BottomToolBarArea,bottom);
    w->menuBar()->setNativeMenuBar(true);
    auto existingMenu=[this](const QString &key,const QString &title){
        for(auto entry:w->menuBar()->actions()) {
            if(entry->data().toString()==key && entry->menu()) {
                entry->menu()->addSeparator();
                return entry->menu();
            }
        }
        return w->menuBar()->addMenu(title);
    };
    auto viewMenu=existingMenu("view","보기");
    auto toolsMenu=existingMenu("tools","도구");
    bookmarks=w->menuBar()->addMenu("북마크");
    auto helpMenu=existingMenu("help","도움말");
    auto open=action(nullptr,"열기", "",[this]{QVApplication::pickFile(w);});top->addAction(open);
    top->addAction(action(nullptr,"폴더", "F",[this]{openFolder();}));
    top->addSeparator();
    const QList<QPair<int,QString>> modeList={{1,"창에 맞춤"},{0,"원본 크기"},{8,"너비에 맞춤"},{7,"두 장 · 왼쪽 → 오른쪽"},{6,"두 장 · 오른쪽 → 왼쪽"},{5,"웹툰 · 연속 보기"}};
    modes=new QComboBox(top);modes->setMinimumWidth(205);
    for(const auto &item:modeList) {
        modes->addItem(item.second,item.first);
        if(item.first==1||item.first==0) continue; // qView actions handle 1/9 and 0.
        action(viewMenu,item.second,QString::number(item.first),[this,item]{setMode(item.first);});
    }
    connect(modes,qOverload<int>(&QComboBox::activated),this,[this](int i){setMode(modes->itemData(i).toInt());});top->addWidget(modes);
    top->addSeparator();
    top->addAction(action(viewMenu,"파일 탐색", "Return",[this]{toggleNavigator();}));
    top->addAction(action(viewMenu,"정보", "Tab",[this]{toggleInfo();}));
    top->addAction(action(nullptr,"북마크 +", "B",[this]{addBookmark();}));
    top->addAction(action(nullptr,"전체 화면", "F11",[this]{w->toggleFullScreen();}));
    action(viewMenu,"전체 화면 전환", "Alt+Return",[this]{w->toggleFullScreen();});
    action(viewMenu,"읽기 방향 바꾸기","2",[this]{setMode(mode==6?7:6);});
    action(viewMenu,"조작바 표시/숨김","`",[this]{bool visible=top->isVisible();top->setVisible(!visible);});
    auto pin=action(viewMenu,"항상 위에 표시","Ctrl+A",[this]{bool on=w->windowFlags().testFlag(Qt::WindowStaysOnTopHint);w->setWindowFlag(Qt::WindowStaysOnTopHint,!on);w->show();});pin->setCheckable(true);
    action(viewMenu,"다음 폴더 / 압축파일","]",[this]{adjacentFolder(1);});
    action(viewMenu,"이전 폴더 / 압축파일","[",[this]{adjacentFolder(-1);});
    action(viewMenu,"10장 앞으로","Ctrl+PgDown",[this]{navigate(10);});
    action(viewMenu,"10장 뒤로","Ctrl+PgUp",[this]{navigate(-10);});
    action(viewMenu,"한 장 앞으로","Shift+PgDown",[this]{navigate(1);});
    action(viewMenu,"한 장 뒤로","Shift+PgUp",[this]{navigate(-1);});
    action(toolsMenu,"현재 이미지 변환…","Ctrl+Shift+S",[this]{exportImages(false);});
    action(toolsMenu,"폴더 일괄 변환…","",[this]{exportImages(true);});
    action(toolsMenu,"이미지 경로 복사","Ctrl+Alt+Shift+C",[this]{QApplication::clipboard()->setText(w->getCurrentFileDetails().fileInfo.absoluteFilePath());});
    action(toolsMenu,"EXIF 복사","Ctrl+Shift+C",[this]{QApplication::clipboard()->setText(metaSnapshot.text());});
    action(toolsMenu,"편집 프로그램으로 열기…","Ctrl+E",[this]{NestPlatform::editImage(w,w->getCurrentFileDetails().fileInfo.absoluteFilePath());});
    action(helpMenu,"단축키 안내","?",[this]{QMessageBox::information(w,"NestView 단축키",QStringLiteral("← / → · PgUp / PgDn   이전 / 다음\nHome / End   처음 / 마지막\n⌘ + PgUp / PgDn   10장 이동\nF   폴더 열기     [ / ]   이전 / 다음 폴더\nEnter   파일 탐색     Tab   메타데이터\n0   원본     1 / 9   창 맞춤     8   너비 맞춤\n7 / 6   두 장 좌→우 / 우→좌     5   웹툰\n2   읽기 방향 전환\n+ / −   확대 / 축소     F11 / ⌥Enter   전체 화면\nB   북마크     F2   이름 바꾸기\n⌘1…9   1…9초 슬라이드쇼     ⌘0   중지\n⌘⇧S   이미지 변환\n기존 뷰어 단축키는 Settings → Shortcuts에서 변경할 수 있습니다."));});
    action(helpMenu,"NestView 정보","",[this]{QMessageBox::about(w,"NestView " + QCoreApplication::applicationVersion(), "멀티플랫폼 이미지 뷰어 · qView 7 기반\nGPL-3.0 · qView 기여자 및 NestView 수정본\n사진 탐색과 감상을 위한 독립 이미지 뷰어입니다.\nHDR 출력·CLIP 전용 디코더는 아직 지원하지 않습니다.");});
    bottom->addAction(action(nullptr,"|‹","",[this]{w->firstFile();}));
    bottom->addAction(action(nullptr,"‹ 이전","",[this]{w->previousFile();}));
    bottom->addAction(action(nullptr,"다음 ›","",[this]{w->nextFile();}));
    bottom->addAction(action(nullptr,"›|","",[this]{w->lastFile();}));
    bottom->addSeparator();
    bottom->addAction(action(nullptr,"−","",[this]{w->zoomOut();}));
    bottom->addAction(action(nullptr,"맞춤","",[this]{setMode(1);}));
    bottom->addAction(action(nullptr,"100%","",[this]{setMode(0);}));
    bottom->addAction(action(nullptr,"+","",[this]{w->zoomIn();}));
    bottom->addSeparator();
    bottom->addAction(action(nullptr,"회전 ↻","",[this]{setMode(1);w->rotateRight();}));
    bottom->addAction(action(nullptr,"재생 / 정지","Space",[this]{w->toggleSlideshow();}));
    for(int i=0;i<=9;++i)action(viewMenu,i?QString("슬라이드쇼 · %1초").arg(i):QStringLiteral("슬라이드쇼 중지"),"Ctrl+"+QString::number(i),[this,i]{w->cancelSlideshow();if(i){w->slideshowTimer->setInterval(i*1000);w->toggleSlideshow();}});
    status=new QLabel("NestView · 이미지를 열어 시작하세요",w);w->statusBar()->addWidget(status,1);
    navigator=new QDialog(w);navigator->setWindowTitle("이미지 선택");navigator->resize(580,510);navigator->setStyleSheet("QDialog{background:#f2f2f2;color:#172d3c;} QTreeWidget{background:white;color:#162939;border:1px solid #82898e;selection-background-color:#d3eafa;selection-color:#14283c;} QHeaderView::section{background:white;color:#16354c;border:0;padding:4px;text-align:left;} QPushButton{background:white;color:#17384f;border:1px solid #cccccc;border-radius:4px;padding:5px 12px;}");
    auto selectionLayout=new QVBoxLayout(navigator);selectionLayout->setContentsMargins(12,12,12,12);
    list=new QListWidget(navigator);list->setViewMode(QListView::IconMode);list->setFlow(QListView::LeftToRight);list->setWrapping(false);list->setFixedHeight(88);list->setIconSize(QSize(74,74));list->setSpacing(2);
    list->setStyleSheet("QListWidget{background:#151515;border:0;} QListWidget::item{color:transparent;border:2px solid transparent;} QListWidget::item:selected{border:2px solid #ff9c3b;}");
    selectionLayout->addWidget(list);
    fileTable=new QTreeWidget(navigator);fileTable->setHeaderLabels({"사진 목록","수정 날짜","크기"});fileTable->setRootIsDecorated(false);fileTable->setAlternatingRowColors(false);fileTable->setSelectionMode(QAbstractItemView::SingleSelection);fileTable->setColumnWidth(0,295);fileTable->setColumnWidth(1,160);selectionLayout->addWidget(fileTable,1);
    auto selectionButtons=new QHBoxLayout;auto sort=new QPushButton("정렬 방식: 파일명",navigator);selectionButtons->addWidget(sort);selectionButtons->addStretch();auto confirm=new QPushButton("확인",navigator);confirm->setDefault(true);confirm->setMinimumWidth(90);selectionButtons->addWidget(confirm);selectionLayout->addLayout(selectionButtons);
    connect(sort,&QPushButton::clicked,this,[this]{qvApp->openOptionsDialog(w);});
    auto selectFile=[this]{int row=fileTable->indexOfTopLevelItem(fileTable->currentItem());if(row>=0&&row<paths.size())w->openFile(paths[row]);navigator->hide();};
    connect(confirm,&QPushButton::clicked,this,selectFile);
    connect(fileTable,&QTreeWidget::itemDoubleClicked,this,[selectFile](QTreeWidgetItem*,int){selectFile();});
    connect(fileTable,&QTreeWidget::currentItemChanged,this,[this](QTreeWidgetItem *item){int row=fileTable->indexOfTopLevelItem(item);if(row>=0){QSignalBlocker block(list);list->setCurrentRow(row);list->scrollToItem(list->item(row));}});
    connect(list,&QListWidget::currentRowChanged,this,[this](int row){if(row>=0&&row<fileTable->topLevelItemCount())fileTable->setCurrentItem(fileTable->topLevelItem(row));});
    inspector=new QDialog(w);inspector->setWindowTitle("메타 정보");inspector->resize(580,560);inspector->setStyleSheet("QDialog{background:#f2f2f2;}QTabBar::tab{color:#1b3a51;background:#e5e9ed;padding:6px 16px;border:1px solid #c9d0d6;}QTabBar::tab:selected{background:white;}QTableWidget{font-size:13px;font-weight:400;background:white;color:#15354c;gridline-color:#d0d0d0;border:1px solid #c0c0c0;}QHeaderView::section{background:#f2f2f2;color:#15354c;border:0;padding:5px;}QPushButton{background:white;color:#15354c;border:1px solid #c6c6c6;border-radius:3px;padding:6px 18px;}");auto detailsLayout=new QVBoxLayout(inspector);auto tabs=new QTabWidget(inspector);metaTable=new QTableWidget(inspector);metaTable->setColumnCount(2);metaTable->setHorizontalHeaderLabels({"항목","값"});metaTable->horizontalHeader()->setSectionResizeMode(0,QHeaderView::Fixed);metaTable->setColumnWidth(0,155);metaTable->horizontalHeader()->setSectionResizeMode(1,QHeaderView::Stretch);metaTable->verticalHeader()->hide();metaTable->setWordWrap(true);tabs->addTab(metaTable,"메타 정보");metadata=new QPlainTextEdit(inspector);metadata->setReadOnly(true);tabs->addTab(metadata,"원시 데이터");detailsLayout->addWidget(tabs);auto detailButtons=new QHBoxLayout;auto copyMetadata=new QPushButton("복사",inspector);auto map=new QPushButton("GPS 지도",inspector);auto detailClose=new QPushButton("닫기",inspector);detailButtons->addWidget(copyMetadata);detailButtons->addWidget(map);detailButtons->addStretch();detailButtons->addWidget(detailClose);detailsLayout->addLayout(detailButtons);connect(copyMetadata,&QPushButton::clicked,this,[this]{QApplication::clipboard()->setText(metaSnapshot.text());});connect(map,&QPushButton::clicked,this,&NestShell::openGPS);connect(detailClose,&QPushButton::clicked,inspector,&QWidget::hide);

    reading=new NestReadingView(w);w->centralWidget()->layout()->addWidget(reading);reading->hide();
    connect(reading,&NestReadingView::currentChanged,this,[this](int index){QTimer::singleShot(180,this,[this,index]{if(isReading()&&reading->currentIndex()==index&&index<paths.size()&&w->getCurrentFileDetails().loadedIndexInFolder!=index)w->graphicsView->loadFile(paths[index]);});});
    welcome=new QLabel(QStringLiteral("NestView\n\n이미지를 크게, 탐색은 가볍게\n\n이미지 또는 압축파일을 끌어 놓으세요\n⌘O  파일 열기     F  폴더 열기\n\n1  창 맞춤   ·   5  웹툰   ·   7  두 장 보기"),w);welcome->setAlignment(Qt::AlignCenter);welcome->setStyleSheet("color:#aebdd0; font-size:18px; padding:50px;");w->centralWidget()->layout()->addWidget(welcome);w->graphicsView->hide();
    w->graphicsView->viewport()->installEventFilter(this);w->graphicsView->installEventFilter(this);
    connect(w->graphicsView,&QVGraphicsView::fileChanged,this,&NestShell::refresh);
    connect(&qvApp->getSettingsManager(),&SettingsManager::settingsUpdated,this,[this]{reading->invalidateImages();});
    rebuildBookmarks();
    buildChrome();
}
NestShell::~NestShell(){
    if(archiveCancelled)*archiveCancelled=true;
    auto retired=std::move(archiveStorage);archive={};
    if(!retired.isEmpty())QThreadPool::globalInstance()->start([retired]{});
}
void NestShell::openFolder(){auto p=QFileDialog::getExistingDirectory(w,"이미지 폴더",folder);if(!p.isEmpty())w->openFile(p);}
bool NestShell::openSpecial(const QString &p){
    if(archiveCancelled)*archiveCancelled=true;
    const auto generation=++archiveGeneration;
    busy=false;
    pendingArchiveImage.clear();
    if(!NestMedia::isArchive(p))return false;
    busy=true;status->setText("압축파일을 여는 중…");
    archiveCancelled=std::make_shared<std::atomic_bool>(false);
    auto watcher=new QFutureWatcher<NestMedia::ArchiveResult>(this);
    connect(watcher,&QFutureWatcher<NestMedia::ArchiveResult>::finished,this,[this,watcher,generation,p]{
        const auto result=watcher->result();watcher->deleteLater();
        if(generation!=archiveGeneration)return;
        busy=false;
        if(!result.error.isEmpty()){QMessageBox::warning(w,"압축파일 열기",result.error);status->setText(result.error);return;}
        archive=result;archivePath=QFileInfo(p).absoluteFilePath();archiveStorage.append(result.directory);
        pendingArchiveImage=result.files.first();
        w->graphicsView->loadFile(result.files.first());w->cancelSlideshow();
    });
    watcher->setFuture(QtConcurrent::run([p,cancelled=archiveCancelled,activity=NestBackgroundActivity::begin()]{return NestMedia::extract(p,cancelled);}));return true;
}
void NestShell::refresh(){
    const auto &d=w->getCurrentFileDetails();bool loaded=d.isPixmapLoaded;
    welcome->setVisible(!loaded&&!d.errorData.hasError);w->graphicsView->setVisible(!isReading()&&(loaded||d.errorData.hasError));
    reading->setVisible(isReading()&&loaded);
    QStringList fresh;for(const auto &f:d.folderFileInfoList)fresh.append(f.absoluteFilePath);
    bool changed=fresh!=paths;paths=fresh;folder=d.fileInfo.absolutePath();
    // A previous decode may finish before the first extracted image. Keep its
    // directory alive until the requested archive image actually reaches the UI.
    if(d.fileInfo.absoluteFilePath()==pendingArchiveImage)pendingArchiveImage.clear();
    if(loaded&&!busy&&pendingArchiveImage.isEmpty()){
        QList<QSharedPointer<QTemporaryDir>> retained,retired;
        for(const auto &directory:archiveStorage){
            if(folder==directory->path())retained.append(directory);else retired.append(directory);
        }
        if(archive.directory&&folder!=archive.directory->path()){archive={};archivePath.clear();}
        archiveStorage=retained;
        // Removing thousands of extracted pages should not block the GUI.
        if(!retired.isEmpty())QThreadPool::globalInstance()->start([retired]{});
    }
    if(changed){
        QSignalBlocker blockList(list),blockTable(fileTable);list->clear();fileTable->clear();
        for(const auto &p:paths){QFileInfo f(p);auto item=new QListWidgetItem("",list);item->setData(Qt::UserRole,p);item->setToolTip(f.fileName());item->setSizeHint(QSize(76,76));
            new QTreeWidgetItem(fileTable,{f.fileName(),f.lastModified().toString("yyyy-MM-dd HH:mm"),QLocale().toString(f.size())});}
    }
    if(hoverThumbnails){
        QSignalBlocker block(hoverThumbnails);
        if(changed){scrubbingThumbnails=false;thumbnailSelectTimer->stop();hoverThumbnails->clear();for(const auto &path:paths){auto item=new QListWidgetItem("",hoverThumbnails);item->setData(Qt::UserRole,path);item->setData(Qt::AccessibleTextRole,QFileInfo(path).fileName());}}
        if(!scrubbingThumbnails)hoverThumbnails->setCurrentRow(d.loadedIndexInFolder);
        if(!scrubbingThumbnails&&hoverThumbnails->currentItem())hoverThumbnails->scrollToItem(hoverThumbnails->currentItem(),QAbstractItemView::PositionAtCenter);
        if(paths.isEmpty())thumbnailTray->hide();
        thumbnailCount->setText(QString("%1 / %2").arg(d.loadedIndexInFolder+1).arg(paths.size()));
        if(folderGrid){
            if(changed){folderGrid->clear();for(const auto &path:paths){auto item=new QListWidgetItem("",folderGrid);item->setData(Qt::UserRole+2,QSettings().value("nest/gridSize",152).toInt());item->setData(Qt::UserRole,path);item->setData(Qt::AccessibleTextRole,QFileInfo(path).fileName());}}
            folderGrid->setCurrentRow(d.loadedIndexInFolder);gridCount->setText(QString("현재 폴더 · %1장").arg(paths.size()));
        }
    }
    if(changed&&hoverThumbnails)prepareThumbnailLayout();
    if(d.loadedIndexInFolder>=0&&d.loadedIndexInFolder<fileTable->topLevelItemCount())fileTable->setCurrentItem(fileTable->topLevelItem(d.loadedIndexInFolder));
    if(pageSlider){QSignalBlocker b(pageSlider);pageSlider->setRange(0,qMax(0,int(paths.size())-1));pageSlider->setValue(d.loadedIndexInFolder);pageSlider->setEnabled(paths.size()>1);}
    if(loaded)updateMetadata();
    if(loaded)w->setWindowTitle(QString("%1 [%2/%3] - NestView").arg(d.fileInfo.fileName()).arg(d.loadedIndexInFolder+1).arg(paths.size()));
    layoutChrome();
    if(loaded){
        status->setText(QString(" %1 / %2   ·   %3   ·   %4 × %5   ·   %6").arg(d.loadedIndexInFolder+1).arg(paths.size()).arg(d.fileInfo.fileName()).arg(d.baseImageSize.width()).arg(d.baseImageSize.height()).arg(QVInfoDialog::formatBytes(d.fileInfo.size())));
        if(isReading()) {if(changed)reading->setFiles(paths,d.loadedIndexInFolder,mode);else if(reading->currentIndex()!=d.loadedIndexInFolder)reading->jump(d.loadedIndexInFolder);}
        if(mode==8)QTimer::singleShot(0,this,&NestShell::fitWidth);
        if(mode==0)QTimer::singleShot(0,this,[this]{w->graphicsView->originalSize();});

    } else if(d.errorData.hasError)status->setText("열기 실패: "+d.errorData.errorString);
}
void NestShell::setMode(int value){mode=value;if(modes)modes->setCurrentIndex(modes->findData(mode));if(isReading()){w->graphicsView->setPaused(true);reading->setFiles(paths,w->getCurrentFileDetails().loadedIndexInFolder,mode);}else{reading->hide();w->graphicsView->show();if(mode==0){w->graphicsView->originalSize();}else if(mode==8)fitWidth();else w->graphicsView->resetScale();}refresh();}
void NestShell::fitWidth(){if(!w->getIsPixmapLoaded())return;auto g=w->graphicsView;g->originalSize();double factor=double(g->viewport()->width()-4)/qMax(1,g->getLoadedPixmap().width());g->zoom(factor);}
bool NestShell::navigate(int delta){
    if(paths.isEmpty())return false;
    int i=isReading()?reading->currentIndex():w->getCurrentFileDetails().loadedIndexInFolder;
    if ((mode==6||mode==7) && qAbs(delta)==2) {
        int check=delta>0?i:i-1;
        if(check>=0&&check<paths.size()){auto s=NestMedia::size(paths[check]);if(s.width()>=s.height())delta=delta>0?1:-1;}
        if(delta==2&&i+1<paths.size()){auto s=NestMedia::size(paths[i+1]);if(s.width()>=s.height())delta=1;}
    }
    int next=i+delta;
    if(qvApp->getSettingsManager().getBool("loopfoldersenabled")){if(next>=paths.size())next=0;if(next<0)next=paths.size()-1;}
    else {next=qBound(0,next,int(paths.size())-1);if(next==i)w->cancelSlideshow();}
    w->graphicsView->goToFile(QVGraphicsView::GoToFileMode::constant,next);return true;
}
void NestShell::zoom(double f){if(isReading())reading->zoomBy(f);}
void NestShell::toggleNavigator(){
    if(navigator->isVisible()){navigator->hide();return;}
    navigator->move(w->mapToGlobal(w->rect().center())-QPoint(navigator->width()/2,navigator->height()/2));
    navigator->show();navigator->raise();fileTable->setFocus();
    const QStringList files=paths;
    const int start=qMax(0,w->getCurrentFileDetails().loadedIndexInFolder-4);
    for(int i=start;i<qMin(start+12,int(files.size()));++i){
        auto watcher=new QFutureWatcher<QImage>(navigator);QString path=files[i];auto target=NestMedia::displaySpace(w);
        connect(watcher,&QFutureWatcher<QImage>::finished,this,[this,watcher,path,i]{auto image=watcher->result();watcher->deleteLater();if(i<paths.size()&&paths[i]==path)list->item(i)->setIcon(QPixmap::fromImage(image));});
        watcher->setFuture(QtConcurrent::run([path,target]{auto image=NestMedia::read(path,160);if(!image.colorSpace().isValid())image.setColorSpace(QColorSpace::SRgb);if(target.isValid())image.convertToColorSpace(target);return image;}));
    }
}
void NestShell::selectCenteredThumbnail(){
    int row=static_cast<NestFilmstrip *>(hoverThumbnails)->centeredRow();
    if(row>=0&&row<paths.size()){
        hoverThumbnails->setCurrentRow(row);hoverThumbnails->scrollToItem(hoverThumbnails->item(row),QAbstractItemView::PositionAtCenter);
        scrubbingThumbnails=false;
        if(w->getCurrentFileDetails().fileInfo.absoluteFilePath()!=paths[row])w->openFile(paths[row]);
    }else scrubbingThumbnails=false;
}
void NestShell::applyThumbnailDimensions(const QVector<int> &rows){
    auto &store=NestThumbnailStore::instance();
    for(auto view:{hoverThumbnails,folderGrid})if(view){
        QListWidgetItem *anchor=nullptr;int offset=0;
        if(view==folderGrid){for(int i=0;i<view->count();++i){auto r=view->visualItemRect(view->item(i));if(r.bottom()>=0){anchor=view->item(i);offset=r.top();break;}}}
        else anchor=view->currentItem();
        bool changed=false;
        {QSignalBlocker block(view->model());for(int j=0;j<(rows.isEmpty()?view->count():rows.size());++j){int i=rows.isEmpty()?j:rows[j];if(i<0||i>=view->count())continue;auto item=view->item(i);auto size=store.dimensions(item->data(Qt::UserRole).toString());if(size.isValid()&&item->data(Qt::UserRole+1).toSize()!=size){item->setData(Qt::UserRole+1,size);changed=true;}}}
        if(changed){view->doItemsLayout();if(anchor){if(view==folderGrid){view->scrollToItem(anchor,QAbstractItemView::PositionAtTop);view->verticalScrollBar()->setValue(view->verticalScrollBar()->value()-offset);}else if(!scrubbingThumbnails)view->scrollToItem(anchor,QAbstractItemView::PositionAtCenter);}view->viewport()->update();}
    }
}
void NestShell::prepareThumbnailLayout(){
    ++thumbnailLayoutGeneration;thumbnailDimensionsPending.clear();
    for(int i=0;i<paths.size();++i)if(!NestThumbnailStore::instance().dimensions(paths[i]).isValid())thumbnailDimensionsPending.append(i);
    applyThumbnailDimensions();thumbnailLayoutReady=true;
    // Never gate the grid on folder-wide metadata. Prioritize the current neighborhood.
    loadThumbnailDimensions(thumbnailLayoutGeneration);
}
void NestShell::loadThumbnailDimensions(int generation){
    if(generation!=thumbnailLayoutGeneration||thumbnailDimensionsPending.isEmpty())return;
    int center=w->getCurrentFileDetails().loadedIndexInFolder;
    if(folderGrid&&folderGrid->isVisible()){auto index=folderGrid->indexAt(folderGrid->viewport()->rect().center());if(index.isValid())center=index.row();}
    std::stable_sort(thumbnailDimensionsPending.begin(),thumbnailDimensionsPending.end(),[center](int a,int b){return qAbs(a-center)<qAbs(b-center);});
    QVector<QPair<QString,QString>> batch;
    while(!thumbnailDimensionsPending.isEmpty()&&batch.size()<24){int row=thumbnailDimensionsPending.takeFirst();batch.append({paths[row],NestThumbnailStore::sourceKey(paths[row])});}
    auto watcher=new QFutureWatcher<QVector<QPair<QString,QSize>>>(this);
    connect(watcher,&QFutureWatcher<QVector<QPair<QString,QSize>>>::finished,this,[this,watcher,generation,batch]{auto result=watcher->result();watcher->deleteLater();for(const auto &entry:result)NestThumbnailStore::instance().rememberDimensions(entry.first,entry.second);if(generation!=thumbnailLayoutGeneration)return;QVector<int> rows;for(const auto &entry:batch)rows.append(paths.indexOf(entry.first));applyThumbnailDimensions(rows);QTimer::singleShot(25,this,[this,generation]{loadThumbnailDimensions(generation);});});
    watcher->setFuture(QtConcurrent::run([batch]{QVector<QPair<QString,QSize>> result;for(const auto &entry:batch){QSize size=NestMedia::size(entry.first);result.append({entry.second,size.isValid()?size:QSize(1,1)});}return result;}));
}
void NestShell::showFolderGrid(){
    if(!gridDialog){
        gridDialog=new QDialog(w);gridDialog->setWindowFlags(Qt::Widget);gridDialog->setObjectName("folderGridDialog");gridDialog->setWindowTitle("폴더 갤러리");gridDialog->resize(880,660);gridDialog->setMinimumSize(440,360);
        gridDialog->setStyleSheet("QDialog{background:#1d2123;}QLabel{color:#dfe7e2;background:transparent;font-size:13px;}QListWidget{background:#1d2123;border:0;outline:0;padding:0;}QPushButton{color:#dedee6;background:#353f3c;border:0;border-radius:7px;padding:7px 14px;}QPushButton:hover{background:#465750;}QSlider::groove:horizontal{height:3px;background:#4a4a54;border-radius:1px;}QSlider::handle:horizontal{width:12px;margin:-5px 0;background:#b9e3d5;border-radius:6px;}");
        auto layout=new QVBoxLayout(gridDialog);layout->setContentsMargins(20,16,20,16);layout->setSpacing(14);auto header=new QHBoxLayout;
        gridCount=new QLabel(gridDialog);header->addWidget(gridCount);header->addStretch();auto sizeLabel=new QLabel("작게 / 크게",gridDialog);header->addWidget(sizeLabel);
        auto size=new QSlider(Qt::Horizontal,gridDialog);size->setObjectName("gridThumbnailSize");size->setAccessibleName("썸네일 크기");size->setRange(64,8192);size->setValue(QSettings().value("nest/gridSize",152).toInt());size->setFixedWidth(130);header->addWidget(size);
        auto close=new QPushButton("사진으로 돌아가기  Esc",gridDialog);header->addWidget(close);connect(close,&QPushButton::clicked,gridDialog,&QDialog::hide);layout->addLayout(header);
        folderGrid=new QListWidget(gridDialog);folderGrid->setObjectName("folderGrid");folderGrid->viewport()->installEventFilter(this);folderGrid->installEventFilter(this);folderGrid->setAccessibleName("현재 폴더 전체 이미지");folderGrid->setViewMode(QListView::IconMode);folderGrid->setFlow(QListView::LeftToRight);folderGrid->setWrapping(true);folderGrid->setResizeMode(QListView::Adjust);folderGrid->setMovement(QListView::Static);folderGrid->setSpacing(1);folderGrid->setUniformItemSizes(false);folderGrid->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);folderGrid->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);folderGrid->setItemDelegate(new NestThumbnails(folderGrid,true));layout->addWidget(folderGrid,1);
        auto resizeCells=[this](int value){{QSignalBlocker block(folderGrid->model());for(int i=0;i<folderGrid->count();++i)folderGrid->item(i)->setData(Qt::UserRole+2,value);}folderGrid->doItemsLayout();QSettings().setValue("nest/gridSize",value);};
        for(const auto &path:paths){auto item=new QListWidgetItem("",folderGrid);item->setData(Qt::UserRole+2,QSettings().value("nest/gridSize",152).toInt());item->setData(Qt::UserRole,path);item->setData(Qt::AccessibleTextRole,QFileInfo(path).fileName());}resizeCells(size->value());connect(size,&QSlider::valueChanged,this,resizeCells);
        applyThumbnailDimensions();
        gridDialog->installEventFilter(this);for(auto child:gridDialog->findChildren<QWidget *>())child->installEventFilter(this);
        auto open=[this](QListWidgetItem *item){if(item){w->openFile(item->data(Qt::UserRole).toString());gridDialog->hide();}};
        connect(folderGrid,&QListWidget::itemClicked,this,open);connect(folderGrid,&QListWidget::itemActivated,this,open);
    }
    thumbnailTray->hide();thumbnailSelectTimer->stop();scrubbingThumbnails=false;
    gridCount->setText(QString("현재 폴더 · %1장").arg(paths.size()));folderGrid->show();folderGrid->setCurrentRow(w->getCurrentFileDetails().loadedIndexInFolder);
    gridDialog->setGeometry(w->centralWidget()->geometry().adjusted(8,8,-8,-8));
    gridDialog->show();gridDialog->raise();floating->hide();folderGrid->setFocus();
    if(auto item=folderGrid->currentItem())folderGrid->scrollToItem(item,QAbstractItemView::PositionAtCenter);
}
bool NestShell::zoomGallery(double factor){
    if(!gridDialog||!gridDialog->isVisible())return false;
    QPoint point=folderGrid->viewport()->mapFromGlobal(QCursor::pos());
    if(!folderGrid->viewport()->rect().contains(point))point=folderGrid->viewport()->rect().center();
    zoomFolderGrid(factor,point);return true;
}
void NestShell::zoomFolderGrid(double factor,const QPoint &position){
    if(!folderGrid||!gridDialog)return;
    auto slider=gridDialog->findChild<QSlider *>("gridThumbnailSize");if(!slider)return;
    auto index=folderGrid->indexAt(position);if(!index.isValid())index=folderGrid->currentIndex();
    const QRect before=folderGrid->visualRect(index);double fraction=before.height()>0?double(position.y()-before.top())/before.height():0.5;
    slider->setValue(qBound(slider->minimum(),qRound(slider->value()*factor),slider->maximum()));
    if(index.isValid()){folderGrid->scrollTo(index,QAbstractItemView::PositionAtTop);auto rect=folderGrid->visualRect(index);folderGrid->verticalScrollBar()->setValue(folderGrid->verticalScrollBar()->value()+qRound(rect.top()+fraction*rect.height()-position.y()));}
}
void NestShell::toggleInfo(){if(exifOverlay){exifOverlay->setVisible(!exifOverlay->isVisible());refresh();}}
void NestShell::adjacentFolder(int delta){QString current=folder;if(archive.directory&&folder==archive.directory->path())current=archivePath;QDir parent=QFileInfo(current).dir();QStringList items;for(const auto &f:parent.entryInfoList(QDir::Dirs|QDir::Files|QDir::NoDotAndDotDot,QDir::Name))if(f.isDir()||NestMedia::isArchive(f.filePath()))items.append(f.absoluteFilePath());int i=items.indexOf(current)+delta;if(i>=0&&i<items.size())w->openFile(items[i]);}
void NestShell::addBookmark(){auto p=w->getCurrentFileDetails().fileInfo.absoluteFilePath();if(p.isEmpty())return;if(archive.directory&&folder==archive.directory->path())p=archivePath;QSettings s;auto values=s.value("nest/bookmarks").toStringList();values.removeAll(p);values.prepend(p);while(values.size()>100)values.removeLast();s.setValue("nest/bookmarks",values);rebuildBookmarks();status->setText("북마크에 저장했습니다: "+QFileInfo(p).fileName());}
void NestShell::rebuildBookmarks(){bookmarks->clear();for(const auto &p:QSettings().value("nest/bookmarks").toStringList()){auto a=bookmarks->addAction(QFileInfo(p).fileName());a->setData(QStringList{"nest"});a->setToolTip(p);connect(a,&QAction::triggered,this,[this,p]{w->openFile(p);});}bookmarks->addSeparator();auto clear=bookmarks->addAction("북마크 비우기");clear->setData(QStringList{"nest"});connect(clear,&QAction::triggered,this,[this]{QSettings().remove("nest/bookmarks");rebuildBookmarks();});}
bool NestShell::eventFilter(QObject *obj,QEvent *event){
    if(gridDialog&&gridDialog->isVisible()&&(event->type()==QEvent::ShortcutOverride||event->type()==QEvent::KeyPress)){
        auto widget=qobject_cast<QWidget *>(obj);auto key=static_cast<QKeyEvent *>(event);
        if(widget&&(widget==gridDialog||gridDialog->isAncestorOf(widget))&&key->key()==Qt::Key_Escape){
            if(event->type()==QEvent::KeyPress){gridDialog->hide();floating->setVisible(controlsPinned);w->graphicsView->setFocus();}
            event->accept();return true;
        }
    }

    const bool imageSurface=obj==w->graphicsView||obj==w->graphicsView->viewport();
    const bool readingSurface=obj==reading||obj==reading->viewport();
    bool overGallery=false;
    if(gridDialog&&gridDialog->isVisible()){
        QPoint global;
        if(event->type()==QEvent::Wheel)global=static_cast<QWheelEvent *>(event)->globalPosition().toPoint();
        else if(event->type()==QEvent::NativeGesture)global=static_cast<QNativeGestureEvent *>(event)->globalPosition().toPoint();
        else global=QCursor::pos();
        overGallery=gridDialog->rect().contains(gridDialog->mapFromGlobal(global));
    }
    const bool gridSurface=overGallery||(folderGrid&&(obj==folderGrid||obj==folderGrid->viewport()));
    if(event->type()==QEvent::NativeGesture&&(imageSurface||readingSurface||gridSurface)){
        auto e=static_cast<QNativeGestureEvent *>(event);
        if(e->gestureType()==Qt::ZoomNativeGesture){
            double factor=qBound(0.1,1.0+e->value(),10.0);
            if(gridSurface)zoomFolderGrid(factor,folderGrid->viewport()->mapFromGlobal(e->globalPosition().toPoint()));
            else if(readingSurface)reading->zoomBy(factor);
            else w->graphicsView->zoom(factor,w->graphicsView->viewport()->mapFromGlobal(e->globalPosition().toPoint()));
            e->accept();return true;
        }
        if(e->gestureType()==Qt::SmartZoomNativeGesture){
            if(gridSurface){auto slider=gridDialog->findChild<QSlider *>("gridThumbnailSize");zoomFolderGrid(slider->value()>300?180.0/slider->value():6.0,folderGrid->viewport()->mapFromGlobal(e->globalPosition().toPoint()));}
            else if(imageSurface){if(qAbs(w->graphicsView->displayedImageRect().width()-w->getCurrentFileDetails().loadedPixmapSize.width())>2)w->graphicsView->originalSize();else w->graphicsView->resetScale();}
            e->accept();return true;
        }
    }

    if(gridSurface&&event->type()==QEvent::Wheel){
        auto e=static_cast<QWheelEvent *>(event);
        if(e->modifiers()&(Qt::MetaModifier|Qt::ControlModifier)){
            double delta=e->pixelDelta().isNull()?e->angleDelta().y()/120.0:e->pixelDelta().y()/80.0;
            zoomFolderGrid(qPow(1.18,delta),folderGrid->viewport()->mapFromGlobal(e->globalPosition().toPoint()));e->accept();return true;
        }
    }

    // A misrouted plain wheel must scroll the gallery, never navigate the covered image.
    if(overGallery&&event->type()==QEvent::Wheel&&(imageSurface||readingSurface)){
        auto e=static_cast<QWheelEvent *>(event);int delta=e->pixelDelta().isNull()?e->angleDelta().y()/2:e->pixelDelta().y();auto bar=folderGrid->verticalScrollBar();bar->setValue(bar->value()-delta);e->accept();return true;
    }
    if(event->type()==QEvent::Leave&&(obj==pageSlider||obj==hoverThumbnails->viewport()))QToolTip::hideText();
    if(event->type()==QEvent::MouseMove){auto e=static_cast<QMouseEvent *>(event);updateHover(w->mapFromGlobal(e->globalPosition().toPoint()));
        int row=-1;
        if(obj==pageSlider&&!paths.isEmpty())row=QStyle::sliderValueFromPosition(0,paths.size()-1,qBound(0,int(e->position().x())-13,qMax(1,pageSlider->width()-26)),qMax(1,pageSlider->width()-26));
        else if(obj==hoverThumbnails->viewport()){auto index=hoverThumbnails->indexAt(e->position().toPoint());if(index.isValid())row=index.row();}
        if(row>=0)QToolTip::showText(e->globalPosition().toPoint()-QPoint(0,30),QString("%1/%2").arg(row+1).arg(paths.size()),qobject_cast<QWidget *>(obj));
        else if(obj==hoverThumbnails->viewport())QToolTip::hideText();
    }
    if(event->type()==QEvent::Wheel && (obj==w->graphicsView->viewport()||obj==reading->viewport()||obj==hoverThumbnails->viewport())){
        auto e=static_cast<QWheelEvent *>(event);
        const bool command=e->modifiers().testFlag(Qt::ControlModifier); // Qt maps Command to Control on macOS.
        const int deviceMode=QSettings().value("nest/scrollDevice",0).toInt();
        // Pixel deltas alone do not identify a trackpad: smooth mouse wheels also send them.
        const bool trackpad=deviceMode==2||(deviceMode==0&&(
            e->pointingDevice()->type()==QInputDevice::DeviceType::TouchPad||
            (!e->pixelDelta().isNull()&&e->phase()!=Qt::NoScrollPhase)));

        if(obj==hoverThumbnails->viewport()&&!command){
            QPoint delta=e->pixelDelta().isNull()?e->angleDelta():e->pixelDelta();
            int movement=qAbs(delta.x())>qAbs(delta.y())?delta.x():delta.y();

            scrubbingThumbnails=true;thumbnailHideTimer->stop();
            auto bar=hoverThumbnails->horizontalScrollBar();
            if(!trackpad){if(movement)static_cast<NestFilmstrip *>(hoverThumbnails)->stepBy((movement>0?-1:1)*qMax(1,qAbs(e->angleDelta().y())/120));}else bar->setValue(bar->value()-movement);
            hoverThumbnails->setCurrentRow(static_cast<NestFilmstrip *>(hoverThumbnails)->centeredRow());
            if(trackpad)thumbnailSelectTimer->start();else{thumbnailSelectTimer->stop();selectCenteredThumbnail();}e->accept();return true;
        }
        // Native pixel scrolling pans zoomed content; at fit it navigates responsively.
        if(!command&&trackpad){
            if(mode==5)return false;
            if(isReading())return false;
            auto view=w->graphicsView;auto h=view->horizontalScrollBar();auto v=view->verticalScrollBar();
            const int padAction=QSettings().value("nest/trackpadAction",0).toInt();
            auto displayed=view->displayedImageRect();
            if(padAction==2||(padAction==0&&(displayed.width()>view->viewport()->width()+8||displayed.height()>view->viewport()->height()+8))){
                trackpadNavigation.reset();
                h->setValue(h->value()-e->pixelDelta().x());v->setValue(v->value()-e->pixelDelta().y());e->accept();return true;
            }
            if(!trackpadClock.isValid())trackpadClock.start();
            const quint64 timestamp=e->timestamp()?e->timestamp():quint64(trackpadClock.elapsed());
            const auto delta=e->pixelDelta().isNull()?e->angleDelta():e->pixelDelta();
            const int step=trackpadNavigation.step(delta,e->phase(),timestamp);
            if(step)navigate(step);
            e->accept();return true;
        }
        const int behavior=QSettings().value("nest/wheelAction",0).toInt();
        if(command||behavior==2){
            int delta=e->angleDelta().y();if(!delta)delta=e->pixelDelta().y()*3;
            if(delta){double factor=qPow(1.0+qvApp->getSettingsManager().getInt("scalefactor")/100.0,delta/120.0);
                if(isReading())reading->zoomBy(factor);else w->graphicsView->zoom(factor,w->graphicsView->viewport()->mapFromGlobal(e->globalPosition().toPoint()));}
            e->accept();return true;
        }
        if(behavior==1||mode==5){
            if(isReading())return false;
            auto bar=w->graphicsView->verticalScrollBar();int delta=e->pixelDelta().isNull()?e->angleDelta().y()/2:e->pixelDelta().y();bar->setValue(bar->value()-delta);e->accept();return true;
        }
        // Mouse wheels are discrete navigation commands, without travel thresholds or cooldowns.
        if(e->phase()==Qt::ScrollMomentum){e->accept();return true;}
        QPoint delta=e->angleDelta().isNull()?e->pixelDelta():e->angleDelta();
        int movement=qAbs(delta.x())>qAbs(delta.y())?delta.x():delta.y();
        if(movement)navigate((movement>0?-1:1)*qMax(1,qAbs(movement)/120));
        e->accept();return true;
    }
    if(event->type()==QEvent::DragEnter){auto e=static_cast<QDragEnterEvent *>(event);if(e->mimeData()->hasUrls()){e->acceptProposedAction();return true;}}
    if(event->type()==QEvent::Drop){auto e=static_cast<QDropEvent *>(event);if(e->mimeData()->hasUrls()){auto urls=e->mimeData()->urls();if(!urls.isEmpty()&&urls.first().isLocalFile()){w->openFile(urls.first().toLocalFile());e->acceptProposedAction();return true;}}}
    if(event->type()==QEvent::Resize)QTimer::singleShot(0,this,[this]{layoutChrome();if(gridDialog&&gridDialog->isVisible())gridDialog->setGeometry(w->centralWidget()->geometry().adjusted(8,8,-8,-8));});
    if(event->type()==QEvent::Resize&&mode==8)QTimer::singleShot(0,this,&NestShell::fitWidth);
    return QObject::eventFilter(obj,event);
}
void NestShell::exportImages(bool batch){
    if(paths.isEmpty())return;
    bool ok=false;QString format=QInputDialog::getItem(w,"이미지 변환","출력 형식",{"PNG","JPEG","WebP"},0,false,&ok);if(!ok)return;
    int max=QInputDialog::getInt(w,"이미지 크기","긴 변 최대 픽셀 (0 = 원본)",0,0,30000,100,&ok);if(!ok)return;
    int quality=90;if(format!="PNG"){quality=QInputDialog::getInt(w,"품질","출력 품질",90,1,100,1,&ok);if(!ok)return;}
    QString dir=QFileDialog::getExistingDirectory(w,"변환 결과 저장 폴더",folder);if(dir.isEmpty())return;
    QStringList inputs=batch?paths:QStringList{w->getCurrentFileDetails().fileInfo.absoluteFilePath()};
    auto progress=new QProgressDialog("이미지 변환 중…","취소",0,0,w);progress->setWindowModality(Qt::WindowModal);progress->setAttribute(Qt::WA_DeleteOnClose);progress->show();
    auto cancelled=std::make_shared<std::atomic_bool>(false);connect(progress,&QProgressDialog::canceled,this,[cancelled]{*cancelled=true;});
    connect(progress,&QObject::destroyed,[cancelled]{*cancelled=true;});
    auto watcher=new QFutureWatcher<QStringList>(this);
    QPointer<QProgressDialog> progressGuard(progress);
    connect(watcher,&QFutureWatcher<QStringList>::finished,this,[this,watcher,progressGuard]{auto errors=watcher->result();if(progressGuard)progressGuard->close();watcher->deleteLater();QMessageBox::information(w,"변환 결과",errors.isEmpty()?QStringLiteral("변환을 완료했습니다. 원본 파일은 유지됩니다."):errors.join('\n'));});
    connect(watcher,&QFutureWatcher<QStringList>::progressValueChanged,progress,&QProgressDialog::setValue);
    watcher->setFuture(QtConcurrent::run([inputs,dir,format,max,quality,cancelled,storage=archiveStorage,activity=NestBackgroundActivity::begin()]{
        QStringList errors;QString ext=format=="JPEG"?"jpg":format.toLower();
        for(const auto &path:inputs){if(*cancelled){errors.append("사용자가 변환을 취소했습니다.");break;}
            QString output=QDir(dir).filePath(QFileInfo(path).completeBaseName()+"-converted."+ext);
            const auto error=NestMedia::convert(path,output,format.toLower().toUtf8(),max,quality);
            if(!error.isEmpty())errors.append(error);
        }return errors;
    }));
}
