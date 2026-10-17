#include "nestshell.h"
#include "nestplatform.h"
#include "nesticons.h"
#include "mainwindow.h"
#include "qvapplication.h"
#include <QtWidgets>
#include <QPrintDialog>
#include <QPrinter>

void NestShell::updateMetadata() {
    const auto &file=w->getCurrentFileDetails().fileInfo;
    QString identity=file.absoluteFilePath()+QString::number(file.lastModified().toMSecsSinceEpoch());
    if(lastMetadata==identity)return;
    lastMetadata=identity;metaSnapshot=NestMedia::inspect(file.absoluteFilePath());
    if(exifText){QStringList lines;for(const auto &row:metaSnapshot.fields)if(row.first!="파일 경로")lines.append(row.first+": "+row.second);exifText->setText(lines.mid(0,22).join('\n'));}
    if(metaTable){metaTable->setRowCount(metaSnapshot.fields.size());for(int i=0;i<metaSnapshot.fields.size();++i){auto key=new QTableWidgetItem(metaSnapshot.fields[i].first);auto value=new QTableWidgetItem(metaSnapshot.fields[i].second);key->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);value->setFlags(Qt::ItemIsEnabled|Qt::ItemIsSelectable);metaTable->setItem(i,0,key);metaTable->setItem(i,1,value);}for(int i=0;i<metaTable->rowCount();++i)metaTable->setRowHeight(i,metaSnapshot.fields[i].first=="파일 경로"?64:38);}
    if(metadata)metadata->setPlainText(metaSnapshot.raw);
}
void NestShell::showMetadata(){updateMetadata();inspector->move(w->mapToGlobal(w->rect().center())-QPoint(inspector->width()/2,inspector->height()/2));inspector->show();inspector->raise();}
void NestShell::openGPS(){updateMetadata();if(metaSnapshot.mapURL.isValid()&&!metaSnapshot.mapURL.isEmpty())QDesktopServices::openUrl(metaSnapshot.mapURL);else QMessageBox::information(w,"GPS 지도","이 이미지에는 읽을 수 있는 GPS 좌표가 없습니다.");}
void NestShell::printImage(){
    if(!w->getIsPixmapLoaded())return;
    QPrinter printer(QPrinter::HighResolution);QPrintDialog dialog(&printer,w);dialog.setWindowTitle("인쇄");if(dialog.exec()!=QDialog::Accepted)return;
    auto image=NestMedia::read(w->getCurrentFileDetails().fileInfo.absoluteFilePath());if(image.isNull())return;
    QPainter painter(&printer);QRect rect=printer.pageLayout().paintRectPixels(printer.resolution());QSize size=image.size().scaled(rect.size(),Qt::KeepAspectRatio);QRect target(QPoint(rect.x()+(rect.width()-size.width())/2,rect.y()+(rect.height()-size.height())/2),size);painter.drawImage(target,image);
}
void NestShell::showContextMenu(const QPoint &position){
    auto menu=new QMenu(w);menu->setAttribute(Qt::WA_DeleteOnClose);menu->setMinimumWidth(310);
    menu->setStyleSheet("QMenu{background:#303030;color:#f6f6f6;border:1px solid #555;border-radius:7px;padding:7px 5px;font-size:12px;}QMenu::item{padding:4px 8px;}QMenu::item:selected{background:#56595b;border-radius:3px;}QMenu::separator{height:1px;background:#555;margin:5px 2px;}QMenu QToolButton{background:#4b4b4b;border-radius:6px;padding:8px;}QMenu QToolButton:hover{background:#696969;}");
    auto icons=new QWidget(menu);auto row=new QHBoxLayout(icons);row->setContentsMargins(4,2,4,6);row->setSpacing(6);
    auto quick=[&](const QString &icon,const QString &tip,std::function<void()> callback){auto b=new QToolButton(icons);b->setIcon(nestIcon(icon));b->setIconSize(QSize(20,20));b->setFixedSize(36,36);b->setToolTip(tip);b->setAccessibleName(tip);row->addWidget(b);connect(b,&QToolButton::clicked,this,[menu,callback]{menu->close();callback();});};
    quick("image","이미지 열기",[this]{QVApplication::pickFile(w);});quick("folder","폴더 열기",[this]{openFolder();});quick("trash","휴지통으로 이동",[this]{w->askDeleteFile(false);});quick("rename","이름 바꾸기",[this]{w->rename();});quick("list","이미지 선택",[this]{toggleNavigator();});quick("print","인쇄",[this]{printImage();});quick("settings","설정",[this]{qvApp->openOptionsDialog(w);});
    auto widgetAction=new QWidgetAction(menu);widgetAction->setDefaultWidget(icons);menu->addAction(widgetAction);
    auto item=[this,menu](const QString &name,const QString &shortcut,std::function<void()> callback,bool requireImage=false){auto a=menu->addAction(shortcut.isEmpty()?name:name+"\t"+NestPlatform::shortcutLabel(shortcut));a->setEnabled(!requireImage||w->getIsPixmapLoaded());connect(a,&QAction::triggered,this,[callback]{callback();});return a;};
    QStringList sorts={"파일명","수정 날짜","생성 날짜","파일 크기","파일 형식","무작위"};int sort=qvApp->getSettingsManager().getInt("sortmode");auto sorting=menu->addMenu("정렬 방식: "+sorts.value(sort));sorting->setObjectName("imageSortMenu");auto sortGroup=new QActionGroup(sorting);
    for(int i=0;i<sorts.size();++i){auto a=sorting->addAction(sorts[i]);a->setObjectName(QString("sortMode%1").arg(i));a->setCheckable(true);a->setChecked(sort==i);sortGroup->addAction(a);connect(a,&QAction::triggered,this,[this,i]{QSettings().setValue("options/sortmode",i);qvApp->getSettingsManager().loadSettings();w->graphicsView->refreshFileList();});}
    sorting->addSeparator();auto descending=sorting->addAction("내림차순");descending->setObjectName("sortDescending");descending->setCheckable(true);descending->setChecked(qvApp->getSettingsManager().getBool("sortdescending"));connect(descending,&QAction::toggled,this,[this](bool v){QSettings().setValue("options/sortdescending",v);qvApp->getSettingsManager().loadSettings();w->graphicsView->refreshFileList();});
    item("이미지 둘러보기","Enter",[this]{toggleNavigator();});
    item("이미지…","⌥I",[this]{showPanel("이미지");});item("HDR…","⌥H",[this]{showPanel("HDR");});item("확대/축소 보기…","⌥Z",[this]{showPanel("확대/축소");});item("슬라이드 쇼…","⌥W",[this]{showPanel("슬라이드 쇼");});item("북마크…","⌥B",[this]{showPanel("북마크");});item("파일 정보/EXIF 정보 보기","Tab",[this]{toggleInfo();},true);item("메타 정보 더 자세히 보기","",[this]{showMetadata();},true);
    menu->addSeparator();item("클립보드로 복사하기","⌘C",[this]{w->copy();},true);item("클립보드에서 붙여넣기","⌘V",[this]{w->paste();});item("이미지 경로 복사","⌘⌥⇧C",[this]{QApplication::clipboard()->setText(w->getCurrentFileDetails().fileInfo.absoluteFilePath());},true);item("EXIF 정보 복사","⌘⇧C",[this]{QApplication::clipboard()->setText(metaSnapshot.text());},true);item(NestPlatform::revealLabel(),"⌘Enter",[this]{w->openContainingFolder();},true);item("편집 프로그램 실행","⌘E",[this]{NestPlatform::editImage(w,w->getCurrentFileDetails().fileInfo.absoluteFilePath());},true);item("이미지 변환","⌘T",[this]{exportImages(false);},true);item("보관 폴더로 복사","Insert",[this]{transferToPhotoFolder(false);},true);item("GPS 지도에서 보기","⌘⇧G",[this]{openGPS();},true);item("바탕 화면 배경으로 지정…","",[this]{QString error;if(!NestMedia::wallpaper(w->getCurrentFileDetails().fileInfo.absoluteFilePath(),&error))QMessageBox::warning(w,"바탕 화면",error);},true);
    menu->addSeparator();item("설정","F5",[this]{qvApp->openOptionsDialog(w);});item("프로그램 정보","F1",[this]{for(auto a:w->actions())if(a->text()=="NestView 정보"){a->trigger();break;}});item("종료","X",[this]{w->close();});menu->popup(position);
}
