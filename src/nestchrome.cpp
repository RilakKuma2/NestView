#include "nestshell.h"
#include "nestplatform.h"
#include "nesticons.h"
#include "nestthumbnails.h"
#include "nestfilmstrip.h"
#include "nestreadingview.h"
#include "mainwindow.h"
#include "qvapplication.h"
#include "photoupdater.h"
#include <QPushButton>
#include <QToolButton>
#include <QRadioButton>
#include <QButtonGroup>
#include <QCheckBox>
#include <QScrollArea>
#include <QMenuBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QDesktopServices>
#include <QClipboard>
#include <QSettings>
#include <QMessageBox>
#include <QGraphicsDropShadowEffect>
#include <QTimer>
#include <QInputDialog>
#include <QStandardPaths>

void NestShell::buildChrome() {
    // A compact workspace toolbar and a quiet navigation shelf.
    if(QSettings().value("nest/referenceUIVersion",0).toInt()<1){w->resize(960,630);QSettings().setValue("nest/referenceUIVersion",1);}
    w->removeToolBar(top);top->hide();w->removeToolBar(bottom);bottom->hide();modes->hide();
    w->menuBar()->setNativeMenuBar(true);
    w->menuBar()->show();
    w->statusBar()->hide();
    w->setStyleSheet(w->styleSheet()+QStringLiteral(
        "QToolBar#workspaceToolbar{background:#24272b;border:0;border-bottom:1px solid #34383c;padding:0px 10px;spacing:4px;}"
        "QToolBar#workspaceToolbar QToolButton{color:#e0e4e4;background:transparent;border:0;border-radius:8px;padding:3px 7px;margin:0px;font-size:11px;}"
        "QToolBar#workspaceToolbar QToolButton:hover{background:#363b40;}"
        "QToolBar#workspaceToolbar QToolButton:checked{background:#3d514d;color:#b9e3d5;}"
        "QToolBar#workspaceToolbar QToolButton::menu-indicator{width:0px;}"
        "QFrame#floatingControls{background:rgba(31,35,38,90);border:1px solid rgba(220,240,234,30);border-radius:12px;}"
        "QFrame#floatingControls QToolButton{background:transparent;border:0;border-radius:7px;padding:0px;}"
        "QFrame#floatingControls QToolButton:hover{background:rgba(165,194,182,95);}"
        "QFrame#floatingControls QToolButton:checked{background:rgba(122,174,155,65);}"
        "QSlider#pagePosition::groove:horizontal{height:3px;background:rgba(209,226,219,70);border-radius:1px;}"
        "QSlider#pagePosition::handle:horizontal{background:#b9e3d5;width:10px;margin:-5px 0;border-radius:5px;}"
        "QFrame#inspectorPanel{background:rgba(35,39,43,250);border:1px solid #41484c;border-radius:12px;color:#f1f1f1;}"
        "QFrame#inspectorPanel QLabel{color:#f1f1f1;background:transparent;border:0;}"
        "QFrame#inspectorPanel QScrollArea,QFrame#inspectorPanel QScrollArea>QWidget>QWidget{background:transparent;border:0;}"
        "QFrame#inspectorPanel QRadioButton,QFrame#inspectorPanel QCheckBox{color:#f1f1f1;background:transparent;spacing:7px;min-height:27px;font-size:13px;}"
        "QFrame#inspectorPanel QPushButton{background:#353c40;color:#f4f4f4;border:1px solid #485156;border-radius:4px;padding:6px 10px;text-align:left;}"
        "QFrame#inspectorPanel QPushButton:hover{background:#767b7e;}"
        "QFrame#inspectorPanel QGroupBox{background:rgba(87,108,99,35);border:0;border-radius:5px;margin-top:0;padding:12px;}"
        "QFrame#inspectorPanel QScrollBar:vertical{width:7px;background:transparent;}"
        "QFrame#inspectorPanel QScrollBar::handle:vertical{background:#919496;border-radius:3px;min-height:25px;}"
        "QFrame#inspectorPanel QScrollBar::add-line:vertical,QFrame#inspectorPanel QScrollBar::sub-line:vertical{height:0;}"
    ));
    top=new QToolBar("NestView 도구막대",w);top->setObjectName("workspaceToolbar");top->setFixedHeight(36);top->setContentsMargins(0,0,0,0);top->layout()->setContentsMargins(0,0,0,0);top->setMovable(false);w->addToolBar(Qt::TopToolBarArea,top);
    auto brand=new QLabel("NestView",top);brand->setStyleSheet("color:#c1e5d9;font-size:13px;font-weight:600;padding-right:8px;");top->addWidget(brand);
    auto header=[this](const QString &text,std::function<void()> callback){auto button=new QToolButton(top);button->setFixedHeight(26);button->setIconSize(QSize(16,16));button->setText(text);button->setAccessibleName(text);connect(button,&QToolButton::clicked,this,[callback]{callback();});top->addWidget(button);return button;};
    auto file=header("",[]{});file->setIcon(nestIcon("folder"));file->setToolTip("파일과 폴더 열기");file->setAccessibleName("파일과 폴더 열기");file->setPopupMode(QToolButton::InstantPopup);
    auto fileMenu=new QMenu(file);fileMenu->setStyleSheet("QMenu{background:#282d30;color:#e0e6e3;border:1px solid #454d50;padding:6px;}QMenu::item{padding:7px 20px;}QMenu::item:selected{background:#435750;border-radius:4px;}");
    auto menuEntry=[this,fileMenu](const QString &text,std::function<void()> callback){auto a=fileMenu->addAction(NestPlatform::shortcutLabel(text));connect(a,&QAction::triggered,this,[callback]{callback();});};
    menuEntry("파일 열기…                         ⌘O",[this]{QVApplication::pickFile(w);});
    menuEntry("폴더 열기…                            F",[this]{openFolder();});
    menuEntry("최근 파일",[this]{auto menu=qvApp->getActionManager().buildRecentsMenu(true,w);menu->setAttribute(Qt::WA_DeleteOnClose);connect(menu,&QMenu::triggered,this,[this](QAction*a){ActionManager::actionTriggered(a,w);});menu->popup(QCursor::pos());});
    fileMenu->addSeparator();
    menuEntry(NestPlatform::revealLabel()+"                    ⌘↵",[this]{w->openContainingFolder();});
    menuEntry("이미지 변환…                       ⌘⇧S",[this]{exportImages(false);});
    menuEntry("폴더 일괄 변환…",[this]{exportImages(true);});
    fileMenu->addSeparator();
    menuEntry("업데이트 확인…",[]{ PhotoUpdater::check(); });
    auto updates = fileMenu->addAction("자동으로 업데이트 확인");
    updates->setCheckable(true);
    connect(fileMenu, &QMenu::aboutToShow, this, [updates]{
        updates->setEnabled(PhotoUpdater::configured());
        updates->setChecked(PhotoUpdater::automaticallyChecks());
    });
    connect(updates, &QAction::toggled, this, [](bool enabled){ PhotoUpdater::setAutomaticallyChecks(enabled); });
    menuEntry("이름 바꾸기…                         F2",[this]{w->rename();});
    menuEntry("휴지통으로 이동",[this]{w->askDeleteFile(false);});
    fileMenu->addSeparator();
    menuEntry("설정…                            F5",[this]{qvApp->openOptionsDialog(w);});
    menuEntry("단축키 안내",[this]{for(auto a:w->actions())if(a->text()=="단축키 안내"){a->trigger();break;}});
    menuEntry("끝내기                                  ⌘Q",[]{qApp->quit();});file->setMenu(fileMenu);
    auto gallery=header("갤러리",[this]{showFolderGrid();});gallery->setIcon(nestIcon("grid"));gallery->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);gallery->setObjectName("folderGridButton");gallery->setToolTip("폴더 갤러리 (G)");
    auto spacer=new QWidget(top);spacer->setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Preferred);top->addWidget(spacer);
    auto fit=header("창에 맞춤",[this]{setMode(1);});fit->setToolTip("창에 맞춤 (9)");
    header("100%",[this]{setMode(0);});
    auto viewButton=header("보기",[this]{showPanel("이미지");});viewButton->setIcon(nestIcon("image"));viewButton->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);viewButton->setObjectName("viewOptionsButton");
    auto info=header("",[this]{toggleInfo();});info->setIcon(nestIcon("info"));info->setAccessibleName("사진 정보");info->setToolTip("사진 정보 (Tab)");
    auto more=header("•••",[]{});more->setAccessibleName("추가 도구");more->setPopupMode(QToolButton::InstantPopup);auto menu=new QMenu(more);more->setMenu(menu);
    for(auto name:QStringList{"북마크","그룹","확대/축소","슬라이드 쇼","보관 폴더","HDR"}){auto a=menu->addAction(name);connect(a,&QAction::triggered,this,[this,name]{showPanel(name);});}
    menu->addSeparator();connect(menu->addAction("설정…"),&QAction::triggered,this,[this]{qvApp->openOptionsDialog(w);});

    floating=new QFrame(w);floating->setObjectName("floatingControls");floating->setFixedHeight(46);
    auto row=new QHBoxLayout(floating);row->setContentsMargins(8,8,8,8);row->setSpacing(6);
    auto iconButton=[this,row](const QString &icon,const QString &tip,std::function<void()> callback){auto button=new QToolButton(floating);button->setIcon(nestIcon(icon));button->setIconSize(QSize(20,20));button->setFixedSize(30,30);button->setToolTip(tip);button->setAccessibleName(tip);connect(button,&QToolButton::clicked,this,[callback]{callback();});row->addWidget(button);return button;};
    iconButton("previous","이전 이미지 (PgUp)",[this]{w->previousFile();});
    iconButton("next","다음 이미지 (PgDn)",[this]{w->nextFile();});
    pageSlider=new QSlider(Qt::Horizontal,floating);pageSlider->setObjectName("pagePosition");pageSlider->setMouseTracking(true);pageSlider->installEventFilter(this);pageSlider->setAccessibleName("이미지 위치");pageSlider->setTracking(false);pageSlider->setFocusPolicy(Qt::NoFocus);row->addWidget(pageSlider,1);
    connect(pageSlider,&QSlider::valueChanged,this,[this](int index){if(index>=0&&index<paths.size())w->graphicsView->goToFile(QVGraphicsView::GoToFileMode::constant,index);});
    auto pin=iconButton("pin","컨트롤 바 고정",[this]{controlsPinned=!controlsPinned;});pin->setCheckable(true);pin->setChecked(true);

    thumbnailTray=new QFrame(w);thumbnailTray->setObjectName("thumbnailTray");
    thumbnailTray->setStyleSheet("QFrame#thumbnailTray{background:transparent;border:0;} QLabel{color:#eeeeF5;background:rgba(20,20,24,80);border:0;border-radius:5px;padding:3px 6px;font-size:11px;}QToolButton{color:#eeeeF5;background:rgba(20,20,24,80);border:0;border-radius:6px;padding:4px 8px;}QToolButton:hover{background:rgba(45,45,52,145);}");
    auto trayLayout=new QVBoxLayout(thumbnailTray);trayLayout->setContentsMargins(4,0,4,0);trayLayout->setSpacing(0);
    auto trayHeader=new QHBoxLayout;thumbnailCount=new QLabel(thumbnailTray);trayHeader->addWidget(thumbnailCount);trayHeader->addStretch();
    trayLayout->addLayout(trayHeader);
    hoverThumbnails=new NestFilmstrip(thumbnailTray);hoverThumbnails->setObjectName("hoverThumbnails");hoverThumbnails->setAccessibleName("현재 폴더 이미지 썸네일");
    hoverThumbnails->setStyleSheet("QListWidget{background:transparent;border:0;padding:0;outline:0;}");
    hoverThumbnails->setItemDelegate(new NestThumbnails(hoverThumbnails,true,true));trayLayout->addWidget(hoverThumbnails,1);thumbnailTray->hide();
    thumbnailHideTimer=new QTimer(this);thumbnailHideTimer->setSingleShot(true);thumbnailHideTimer->setInterval(400);
    connect(thumbnailHideTimer,&QTimer::timeout,thumbnailTray,&QWidget::hide);
    thumbnailSelectTimer=new QTimer(this);thumbnailSelectTimer->setSingleShot(true);thumbnailSelectTimer->setInterval(90);
    connect(thumbnailSelectTimer,&QTimer::timeout,this,&NestShell::selectCenteredThumbnail);
    connect(hoverThumbnails,&QListWidget::itemClicked,this,[this](QListWidgetItem *item){scrubbingThumbnails=false;thumbnailSelectTimer->stop();w->openFile(item->data(Qt::UserRole).toString());});
    action(nullptr,"현재 폴더 그리드","G",[this]{showFolderGrid();});

    for(auto surface:{w->graphicsView->viewport(),reading->viewport(),hoverThumbnails->viewport(),static_cast<QWidget *>(floating)}){surface->setMouseTracking(true);surface->installEventFilter(this);}
    for(auto child:thumbnailTray->findChildren<QWidget *>()){child->setMouseTracking(true);child->installEventFilter(this);}
    for(auto child:floating->findChildren<QWidget *>()){child->setMouseTracking(true);child->installEventFilter(this);}

    panel=new QFrame(w);panel->setObjectName("inspectorPanel");panel->hide();auto pl=new QVBoxLayout(panel);pl->setContentsMargins(14,14,14,14);pl->setSpacing(10);
    auto ph=new QHBoxLayout;panelTitle=new QLabel(panel);panelTitle->setStyleSheet("font-size:16px;font-weight:600;");ph->addWidget(panelTitle);ph->addStretch();auto closePanel=new QToolButton(panel);closePanel->setIcon(nestIcon("close",QColor("#bdc0c1")));closePanel->setToolTip("패널 닫기");closePanel->setStyleSheet("background:transparent;border:0;padding:0;");ph->addWidget(closePanel);connect(closePanel,&QToolButton::clicked,panel,&QWidget::hide);pl->addLayout(ph);
    auto scroll=new QScrollArea(panel);scroll->setWidgetResizable(true);panelBody=new QWidget(scroll);panelLayout=new QVBoxLayout(panelBody);panelLayout->setContentsMargins(0,0,0,0);panelLayout->setSpacing(10);scroll->setWidget(panelBody);pl->addWidget(scroll,1);
    exifOverlay=new QFrame(w);exifOverlay->setStyleSheet("QFrame{background:rgba(20,20,20,95);border:0;border-radius:3px;} QLabel{color:white;background:transparent;font-size:13px;}");auto el=new QVBoxLayout(exifOverlay);el->setContentsMargins(8,7,8,8);auto er=new QHBoxLayout;
    for(auto name:QStringList{"close","copy","info"}){auto b=new QToolButton(exifOverlay);b->setIcon(nestIcon(name));b->setFixedSize(28,28);b->setStyleSheet("background:rgba(110,113,116,150);border-radius:14px;padding:3px;");er->addWidget(b);connect(b,&QToolButton::clicked,this,[this,name]{if(name=="close")exifOverlay->hide();else if(name=="copy")QApplication::clipboard()->setText(exifText->text());else{showMetadata();}});}er->addStretch();el->addLayout(er);exifText=new QLabel(exifOverlay);exifText->setTextInteractionFlags(Qt::TextSelectableByMouse);el->addWidget(exifText);exifOverlay->hide();
    // Original NestView aperture mark and a useful empty state.
    welcome->setText("<span style='color:#b9e3d5;font-size:36px;font-weight:600'>NestView</span><br><br><span style='color:#dbe1de;font-size:17px'>사진에 집중하는 공간</span><br><br><span style='color:#939e99;font-size:13px'>이미지나 폴더를 끌어 놓으세요 · ⌘O로 열기</span>");welcome->setTextFormat(Qt::RichText);welcome->setStyleSheet("background:#1d2123;padding:0;");

    w->graphicsView->installEventFilter(this);w->centralWidget()->installEventFilter(this);w->installEventFilter(this);reading->installEventFilter(this);
    action(nullptr,"컨트롤 바 표시/숨김","~",[this]{floating->setVisible(!floating->isVisible());});
    action(nullptr,"확대/축소 패널","/",[this]{showPanel("확대/축소");});
    action(nullptr,"NestView 설정","F5",[this]{qvApp->openOptionsDialog(w);});
    action(nullptr,"이미지 패널","Alt+I",[this]{showPanel("이미지");});
    action(nullptr,"HDR 패널","Alt+H",[this]{showPanel("HDR");});
    action(nullptr,"줌 패널","Alt+Z",[this]{showPanel("확대/축소");});
    action(nullptr,"슬라이드 쇼 패널","Alt+W",[this]{showPanel("슬라이드 쇼");});
    action(nullptr,"북마크 패널","Alt+B",[this]{showPanel("북마크");});
    action(nullptr,"이미지 변환","Ctrl+T",[this]{exportImages(false);});
    action(nullptr,"GPS 지도","Ctrl+Shift+G",[this]{openGPS();});
    action(nullptr,"인쇄","Ctrl+P",[this]{printImage();});
    action(nullptr,"보관 폴더 복사","Insert",[this]{transferToPhotoFolder(false);});
    action(nullptr,"보관 폴더 이동","Ctrl+Insert",[this]{transferToPhotoFolder(true);});
    action(nullptr,"프로그램 정보","F1",[this]{for(auto a:w->actions())if(a->text()=="NestView 정보"){a->trigger();break;}});
    action(nullptr,"창 종료","X",[this]{w->close();});
    for(auto spec:QList<QPair<QString,QString>>{{"invert","Ctrl+I"},{"grayscale","Ctrl+Y"}})action(nullptr,spec.first=="invert"?"색상 반전":"흑백",spec.second,[spec]{QSettings s;QString key="nest/effects/"+spec.first;s.setValue(key,!s.value(key,false).toBool());qvApp->getSettingsManager().settingsUpdated();});
    auto hideTimer=new QTimer(this);hideTimer->setInterval(100);connect(hideTimer,&QTimer::timeout,this,[this]{updateHover(w->isActiveWindow()?w->mapFromGlobal(QCursor::pos()):QPoint(-1,-1));});hideTimer->start();
    action(nullptr,"NestView 설정 열기","Ctrl+,",[this]{qvApp->openOptionsDialog(w);});
    refresh();
    layoutChrome();
}
void NestShell::layoutChrome(){
    if(!floating)return;
    QRect area=w->centralWidget()->geometry();int fw=qMin(420,w->width()-32);
    floating->setGeometry((w->width()-fw)/2,area.bottom()-56,fw,46);floating->raise();
    if(thumbnailTray){int width=qMin(820,w->width()-48);thumbnailTray->setGeometry((w->width()-width)/2,floating->y()-104,width,104);thumbnailTray->raise();}
    if(panel){int pw=qMin(386,w->width()-40);int ph=qMin(486,qMax(240,area.height()-85));panel->setGeometry(w->width()-pw-26,area.top()+6,pw,ph);panel->raise();}
    if(exifOverlay){exifOverlay->setGeometry(area.left()+8,area.top()+8,qMin(650,area.width()-24),qMin(area.height()-80,qMax(145,exifText->sizeHint().height()+55)));exifOverlay->raise();}
}
void NestShell::updateHover(const QPoint &pos){
    if(!hoverThumbnails)return;
    if(gridDialog&&gridDialog->isVisible()){floating->hide();thumbnailTray->hide();return;}
    if(controlsPinned)floating->show();
    bool near=w->centralWidget()->geometry().contains(pos)&&pos.y()>=floating->y()-12;
    bool overStrip=thumbnailTray->isVisible()&&QRect(thumbnailTray->x(),thumbnailTray->y(),thumbnailTray->width(),w->height()-thumbnailTray->y()).contains(pos);
    if(!controlsPinned)floating->setVisible(near||overStrip);
    bool enabled=QSettings().value("nest/hoverThumbnails",true).toBool()&&!paths.isEmpty()&&!QApplication::activeModalWidget()&&!QApplication::activePopupWidget();
    if(enabled&&(near||overStrip)){
        thumbnailHideTimer->stop();
        if(!thumbnailTray->isVisible()){thumbnailTray->show();if(auto item=hoverThumbnails->currentItem())hoverThumbnails->scrollToItem(item,QAbstractItemView::PositionAtCenter);}
    }else if(!thumbnailHideTimer->isActive())thumbnailHideTimer->start();
}
void NestShell::showPanel(const QString &name){
    if(activePanel==name&&panel->isVisible()){panel->hide();return;}
    activePanel=name;panelTitle->setText(name);
    while(auto item=panelLayout->takeAt(0)){if(item->widget())item->widget()->deleteLater();delete item;}
    auto section=[this](const QString &title){auto frame=new QFrame(panelBody);frame->setStyleSheet("QFrame{background:rgba(87,108,99,35);border:0;border-radius:5px;}");auto layout=new QVBoxLayout(frame);layout->setContentsMargins(12,10,12,10);layout->setSpacing(2);if(!title.isEmpty()){auto label=new QLabel(title,frame);label->setStyleSheet("font-size:13px;font-weight:600;padding-bottom:5px;background:transparent;");layout->addWidget(label);}panelLayout->addWidget(frame);return layout;};
    auto button=[this](QVBoxLayout *layout,const QString &text,std::function<void()> callback){auto b=new QPushButton(text,panelBody);layout->addWidget(b);connect(b,&QPushButton::clicked,this,[callback]{callback();});return b;};
    if(name=="이미지"){
        auto layout=section("보기 모드");auto group=new QButtonGroup(panelBody);
        for(auto spec:QList<QPair<int,QString>>{{0,"실제 크기 · 100% (0)"},{1,"창에 맞춤 (9)"},{8,"너비에 맞춤 (8)"},{7,"두 페이지 · 왼쪽부터 (7)"},{6,"두 페이지 · 오른쪽부터 (6)"},{5,"웹툰 · 연속 스크롤 (5)"}}){auto r=new QRadioButton(spec.second,panelBody);r->setChecked(mode==spec.first);group->addButton(r);layout->addWidget(r);connect(r,&QRadioButton::clicked,this,[this,spec]{setMode(spec.first);});}
        layout->addSpacing(8);auto small=new QCheckBox("작은 이미지도 창에 맞춰 확대 (Z)",panelBody);small->setChecked(qvApp->getSettingsManager().getBool("pastactualsizeenabled"));layout->addWidget(small);connect(small,&QCheckBox::toggled,this,[this](bool value){QSettings s;s.setValue("options/pastactualsizeenabled",value);qvApp->getSettingsManager().loadSettings();w->resetZoom();});
        auto filtering=section("기본 필터");auto filterGroup=new QButtonGroup(panelBody);for(auto spec:QList<QPair<bool,QString>>{{false,"필터 적용 안함"},{true,"부드럽게"}}){auto radio=new QRadioButton(spec.second,panelBody);radio->setChecked(qvApp->getSettingsManager().getBool("filteringenabled")==spec.first);filterGroup->addButton(radio);filtering->addWidget(radio);connect(radio,&QRadioButton::clicked,this,[spec]{QSettings().setValue("options/filteringenabled",spec.first);qvApp->getSettingsManager().loadSettings();});}
        auto processing=section("영상 처리 · 정지 이미지");
        for(auto spec:QList<QPair<QString,QString>>{{"invert","색상 반전 (⌘I)"},{"grayscale","흑백 (⌘Y)"},{"sepia","세피아"}}){auto check=new QCheckBox(spec.second,panelBody);check->setChecked(QSettings().value("nest/effects/"+spec.first,false).toBool());check->setEnabled(!w->getCurrentFileDetails().isMovieLoaded);processing->addWidget(check);connect(check,&QCheckBox::toggled,this,[spec](bool value){QSettings().setValue("nest/effects/"+spec.first,value);qvApp->getSettingsManager().settingsUpdated();});}
        for(auto spec:QList<QPair<QString,QString>>{{"gamma","감마 보정"},{"brightness","밝기 조절"}}){processing->addWidget(new QLabel(spec.second,panelBody));auto slider=new QSlider(Qt::Horizontal,panelBody);slider->setRange(spec.first=="gamma"?20:-100,spec.first=="gamma"?300:100);slider->setValue(spec.first=="gamma"?qRound(QSettings().value("nest/effects/gamma",1.0).toDouble()*100):QSettings().value("nest/effects/brightness",0).toInt());slider->setTracking(false);processing->addWidget(slider);connect(slider,&QSlider::valueChanged,this,[spec](int value){QSettings().setValue("nest/effects/"+spec.first,spec.first=="gamma"?value/100.0:value);qvApp->getSettingsManager().settingsUpdated();});}
        button(processing,"영상 처리 초기화",[this]{QSettings().remove("nest/effects");qvApp->getSettingsManager().settingsUpdated();activePanel.clear();showPanel("이미지");});
        auto edits=section("회전 / 변환");button(edits,"왼쪽으로 회전",[this]{setMode(1);w->rotateLeft();});button(edits,"오른쪽으로 회전",[this]{setMode(1);w->rotateRight();});button(edits,"좌우 뒤집기",[this]{setMode(1);w->mirror();});button(edits,"이미지 변환…",[this]{exportImages(false);});
    }else if(name=="확대/축소"){
        auto zoom=section("");button(zoom,"−  축소",[this]{w->zoomOut();});button(zoom,"+  확대",[this]{w->zoomIn();});button(zoom,"100%",[this]{setMode(0);});auto info=section("확대/축소 정보");auto s=w->getCurrentFileDetails().baseImageSize;info->addWidget(new QLabel(QString("페이지 크기: %1 × %2").arg(s.width()).arg(s.height()),panelBody));button(info,"초기화 · 창에 맞춤",[this]{setMode(1);});button(info,"폭에 맞춤",[this]{setMode(8);});
    }else if(name=="슬라이드 쇼"){
        auto layout=section("");auto group=new QButtonGroup(panelBody);for(int i=0;i<=9;++i){auto radio=new QRadioButton(i?QString("%1 초                                ⌘%1").arg(i):QStringLiteral("슬라이드 쇼 끄기                   ⌘0"),panelBody);radio->setChecked(i==0?!w->slideshowTimer->isActive():(w->slideshowTimer->isActive()&&w->slideshowTimer->interval()==i*1000));group->addButton(radio);layout->addWidget(radio);connect(radio,&QRadioButton::clicked,this,[this,i]{w->cancelSlideshow();if(i){w->slideshowTimer->setInterval(i*1000);w->toggleSlideshow();}});}auto loop=new QCheckBox("슬라이드 쇼 반복하기",panelBody);loop->setChecked(qvApp->getSettingsManager().getBool("loopfoldersenabled"));layout->addWidget(loop);connect(loop,&QCheckBox::toggled,this,[](bool value){QSettings().setValue("options/loopfoldersenabled",value);qvApp->getSettingsManager().loadSettings();});
    }else if(name=="북마크"){
        auto layout=section("");auto saved=QSettings().value("nest/bookmarks").toStringList();if(saved.isEmpty())layout->addWidget(new QLabel("저장된 북마크가 없습니다.",panelBody));for(auto path:saved)button(layout,QFileInfo(path).fileName(),[this,path]{w->openFile(path);});button(layout,"추가 (B)",[this]{addBookmark();activePanel.clear();showPanel("북마크");});button(layout,"모두 삭제",[this]{QSettings().remove("nest/bookmarks");rebuildBookmarks();activePanel.clear();showPanel("북마크");});
    }else if(name=="보관 폴더"){
        auto layout=section("");button(layout,"기본 보관 폴더로 복사",[this]{transferToPhotoFolder(false);});button(layout,"기본 보관 폴더로 이동",[this]{transferToPhotoFolder(true);});auto location=QSettings().value("nest/photoFolder").toString();auto label=new QLabel(location.isEmpty()?"보관 폴더를 설정하세요.":location,panelBody);label->setWordWrap(true);layout->addWidget(label);button(layout,"보관 폴더 열기",[]{auto path=QSettings().value("nest/photoFolder").toString();if(!path.isEmpty())QDesktopServices::openUrl(QUrl::fromLocalFile(path));});button(layout,"설정…",[this]{auto path=QFileDialog::getExistingDirectory(w,"보관 폴더");if(!path.isEmpty()){QSettings().setValue("nest/photoFolder",path);activePanel.clear();showPanel("보관 폴더");}});
    }else if(name=="그룹"){
        auto layout=section("RAW + JPG 그룹");auto check=new QCheckBox("같은 이름의 RAW + JPG 묶기 (JPG 우선)",panelBody);check->setChecked(QSettings().value("nest/groupRawJpeg",false).toBool());layout->addWidget(check);connect(check,&QCheckBox::toggled,this,[this](bool value){QSettings().setValue("nest/groupRawJpeg",value);w->graphicsView->refreshFileList();});
    }else if(name=="HDR"){
        auto layout=section("HDR 출력");auto label=new QLabel("현재 빌드: SDR 출력\n\n색상 프로파일 변환은 지원합니다.\nHDR / EDR 출력은 아직 구현되지 않았습니다.",panelBody);label->setWordWrap(true);layout->addWidget(label);
    }
    panelLayout->addStretch();panel->show();layoutChrome();
}
void NestShell::transferToPhotoFolder(bool move){
    QString source=w->getCurrentFileDetails().fileInfo.absoluteFilePath();if(source.isEmpty())return;
    QString directory=QSettings().value("nest/photoFolder").toString();if(directory.isEmpty()){directory=QFileDialog::getExistingDirectory(w,"보관 폴더");if(directory.isEmpty())return;QSettings().setValue("nest/photoFolder",directory);}
    QString dest=QDir(directory).filePath(QFileInfo(source).fileName());
    bool ok=move?QFile::rename(source,dest):QFile::copy(source,dest);
    if(!ok)QMessageBox::warning(w,"보관 폴더",move?"이동하지 못했습니다. 같은 이름의 파일이 있거나 다른 볼륨일 수 있습니다.":"복사하지 못했습니다. 같은 이름의 파일 또는 폴더 권한을 확인하세요.");
    else if(move)w->openFile(dest);
}
