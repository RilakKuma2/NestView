#include "qvimagecore.h"
#include "nestmedia.h"
#include <QSvgRenderer>
#include <QPainter>
#include "qvapplication.h"
#include "qvwin32functions.h"
#include "qvcocoafunctions.h"
#include "qvlinuxx11functions.h"
#include <cstring>
#include <random>
#include <QMessageBox>
#include <QDir>
#include <QUrl>
#include <QSettings>
#include "nestcollation.h"
#include <QtConcurrent/QtConcurrentRun>
#include <QIcon>
#include <QGuiApplication>
#include <QScreen>

QCache<QString, QVImageCore::ReadData> QVImageCore::imageCache;

QVImageCore::QVImageCore(QObject *parent) : QObject(parent)
{
// Bound each decoder allocation on Qt6.
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    QImageReader::setAllocationLimit(512);
#endif

    currentRotation = 0;

    connect(&loadedMovie, &QMovie::updated, this, &QVImageCore::animatedFrameChanged);

    connect(&loadFutureWatcher, &QFutureWatcher<ReadData>::finished, this,
            [this]() { loadPixmap(loadFutureWatcher.result()); });

    largestDimension = 0;
    const auto screenList = QGuiApplication::screens();
    for (auto const &screen : screenList) {
        int largerDimension;
        if (screen->size().height() > screen->size().width()) {
            largerDimension = screen->size().height();
        } else {
            largerDimension = screen->size().width();
        }

        if (largerDimension > largestDimension) {
            largestDimension = largerDimension;
        }
    }

    waitingOnLoad = false;

    // Connect to settings signal
    connect(&qvApp->getSettingsManager(), &SettingsManager::settingsUpdated, this,
            &QVImageCore::settingsUpdated);
    settingsUpdated();
}

void QVImageCore::loadFile(const QString &fileName, bool isReloading)
{
    if (waitingOnLoad) {
        pendingFile = fileName;
        pendingReload = isReloading;
        return;
    }

    QString sanitaryFileName = fileName;

    // sanitize file name if necessary
    QUrl sanitaryUrl = QUrl(fileName);
    if (sanitaryUrl.isLocalFile())
        sanitaryFileName = sanitaryUrl.toLocalFile();

    QFileInfo fileInfo(sanitaryFileName);
    sanitaryFileName = fileInfo.absoluteFilePath();

    if (fileInfo.isDir()) {
        updateFolderInfo(sanitaryFileName);
        if (currentFileDetails.folderFileInfoList.isEmpty())
            closeImage();
        else
            loadFile(currentFileDetails.folderFileInfoList.at(0).absoluteFilePath);
        return;
    }

    // Pause playing movie because it feels better that way
    setPaused(true);

    currentFileDetails.isLoadRequested = true;
    waitingOnLoad = true;

    QColorSpace targetColorSpace = getTargetColorSpace();
    QString cacheKey = getPixmapCacheKey(sanitaryFileName, fileInfo.size(), targetColorSpace);

    // check if cached already before loading the long way
    auto *cachedData = isReloading ? nullptr : QVImageCore::imageCache.take(cacheKey);
    if (cachedData != nullptr) {
        ReadData readData = *cachedData;
        delete cachedData;
        loadPixmap(readData);
    }
    // or see if the preloader is already working on it
    else if (preloadFilesInProgress.contains(sanitaryFileName)) {
        waitingOnPreloadFile = sanitaryFileName;
    } else {
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
        loadFutureWatcher.setFuture(QtConcurrent::run(this, &QVImageCore::readFile,
                                                      sanitaryFileName, targetColorSpace));
#else
        loadFutureWatcher.setFuture(QtConcurrent::run(&QVImageCore::readFile, this,
                                                      sanitaryFileName, targetColorSpace));
#endif
    }
}

QVImageCore::ReadData QVImageCore::readFile(const QString &fileName, const QColorSpace &targetColorSpace)
{
    const auto effects = NestMedia::effects();
    QImageReader imageReader;
    imageReader.setAutoTransform(true);

    imageReader.setFileName(fileName);

    QSize sourceSize = imageReader.size();
    QImage readImage;
    if (imageReader.format() == "svg" || imageReader.format() == "svgz") {
        // Render vectors into a high resolution
        QSvgRenderer renderer(fileName);
        QSize size = renderer.defaultSize().scaled(largestDimension, largestDimension, Qt::KeepAspectRatio);
        readImage = QImage(size, QImage::Format_ARGB32_Premultiplied);
        readImage.fill(Qt::transparent);
        QPainter painter(&readImage);
        renderer.render(&painter);
        painter.end();
        // If this fails, try reading the normal way so that a proper error message is given
        if (readImage.isNull())
            readImage = imageReader.read();
    } else {
        readImage = imageReader.read();
    }

    if (readImage.isNull()) readImage = NestMedia::read(fileName);
    readImage = NestMedia::applyEffects(readImage,effects);

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    readImage.convertTo(QImage::Format::Format_ARGB32_Premultiplied);
#else
    readImage = readImage.convertToFormat(QImage::Format::Format_ARGB32_Premultiplied);
#endif
    // Handle color space information

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && QT_VERSION < QT_VERSION_CHECK(6, 7, 2)
    // Work around Qt ICC profile parsing bug
    if (!readImage.colorSpace().isValid() && !readImage.colorSpace().iccProfile().isEmpty()) {
        QByteArray profileData = readImage.colorSpace().iccProfile();
        if (removeTinyDataTagsFromIccProfile(profileData))
            readImage.setColorSpace(QColorSpace::fromIccProfile(profileData));
    }
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    // Assume image is sRGB if it doesn't specify
    if (!readImage.colorSpace().isValid())
        readImage.setColorSpace(QColorSpace::SRgb);

    // Convert image color space if we have a target that's different
    if (targetColorSpace.isValid() && readImage.colorSpace() != targetColorSpace)
        readImage.convertToColorSpace(targetColorSpace);
#endif

    QFileInfo fileInfo(fileName);

    ReadData readData = { readImage,        fileInfo.absoluteFilePath(),
                          fileInfo.size(),  sourceSize.isValid() ? sourceSize : readImage.size(),
                          targetColorSpace, {}, effects.signature() };

    if (readImage.isNull()) {
        readData.errorData = { true, imageReader.error(), imageReader.errorString() };
    }

    return readData;
}

void QVImageCore::loadPixmap(const ReadData &readData)
{
    if (readData.errorData.hasError) {
        currentFileDetails = getEmptyFileDetails();
        currentFileDetails.errorData = readData.errorData;
    } else {
        currentFileDetails.errorData = {};
    }

    // Do this first so we can keep folder info even when loading errored files
    currentFileDetails.fileInfo = QFileInfo(readData.absoluteFilePath);
    currentFileDetails.updateLoadedIndexInFolder();
    if (currentFileDetails.loadedIndexInFolder == -1)
        updateFolderInfo();

    // Reset mechanism to avoid stalling while loading
    waitingOnLoad = false;

    if (currentFileDetails.errorData.hasError) {
        loadEmptyPixmap();
        finishPendingLoad();
        return;
    }

    loadedPixmap = QPixmap::fromImage(matchCurrentRotation(readData.image));

    // Set file details
    currentFileDetails.isPixmapLoaded = true;
    currentFileDetails.baseImageSize = readData.imageSize;
    currentFileDetails.loadedPixmapSize = loadedPixmap.size();
    if (currentFileDetails.baseImageSize == QSize(-1, -1)) {
        qInfo() << "QImageReader::size gave an invalid size for "
                        + currentFileDetails.fileInfo.fileName()
                        + ", using size from loaded pixmap";
        currentFileDetails.baseImageSize = currentFileDetails.loadedPixmapSize;
    }

    addToCache(std::move(readData));

    // Animation detection
    loadedMovie.setFormat("");
    loadedMovie.stop();
    loadedMovie.setFileName(currentFileDetails.fileInfo.absoluteFilePath());

    // APNG workaround
    if (loadedMovie.format() == "png") {
        loadedMovie.setFormat("apng");
        loadedMovie.setFileName(currentFileDetails.fileInfo.absoluteFilePath());
    }

    if (loadedMovie.isValid() && loadedMovie.frameCount() != 1)
        loadedMovie.start();

    currentFileDetails.isMovieLoaded = loadedMovie.state() == QMovie::Running;

    if (!currentFileDetails.isMovieLoaded)
        if (auto device = loadedMovie.device())
            device->close();

    currentFileDetails.timeSinceLoaded.start();

    emit fileChanged();

    requestCaching();
    finishPendingLoad();
}

void QVImageCore::finishPendingLoad()
{
    if (pendingFile.isEmpty()) return;
    const QString next = pendingFile;
    const bool reload = pendingReload;
    pendingFile.clear();
    loadFile(next, reload);
}

void QVImageCore::closeImage()
{
    currentFileDetails = getEmptyFileDetails();
    loadEmptyPixmap();
}

void QVImageCore::loadEmptyPixmap()
{
    loadedPixmap = QPixmap();
    loadedMovie.stop();
    loadedMovie.setFileName("");

    emit fileChanged();
}

QVImageCore::FileDetails QVImageCore::getEmptyFileDetails()
{
    return { QFileInfo(),
             currentFileDetails.folderFileInfoList,
             currentFileDetails.loadedIndexInFolder,
             false,
             false,
             false,
             QSize(),
             QSize(),
             QElapsedTimer(),
             {} };
}

// All file logic, sorting, etc should be moved to a different class or file
QList<QVImageCore::CompatibleFile> QVImageCore::getCompatibleFiles(const QString &dirPath) const
{
    int sortMode = qvGetSettingInt(SortMode);
    QList<CompatibleFile> fileList;

    QMimeDatabase mimeDb;
    const auto &extensions = qvApp->getFileExtensionList();
    const auto &mimeTypes = qvApp->getMimeTypeNameList();

    QMimeDatabase::MatchMode mimeMatchMode = qvGetSettingBool(AllowMimeContentDetection)
            ? QMimeDatabase::MatchDefault
            : QMimeDatabase::MatchExtension;

    // skip hidden files if user wants to
    QDir::Filters filters = QDir::Files;

    auto &settingsManager = qvApp->getSettingsManager();
    if (!settingsManager.getBool("skiphidden"))
        filters |= QDir::Hidden;

    const QFileInfoList currentFolder = QDir(dirPath).entryInfoList(filters, QDir::Unsorted);
    for (const QFileInfo &fileInfo : currentFolder) {
        bool matched = false;
        const QString absoluteFilePath = fileInfo.absoluteFilePath();
        const QString fileName = fileInfo.fileName();
        for (const QString &extension : extensions) {
            if (fileName.endsWith(extension, Qt::CaseInsensitive)) {
                matched = true;
                break;
            }
        }
        QString mimeType;
        if (!matched || sortMode == 4) {
            mimeType = mimeDb.mimeTypeForFile(absoluteFilePath, mimeMatchMode).name();
            matched |= mimeTypes.contains(mimeType);
        }

        // ignore macOS ._ metadata files
        if (fileName.startsWith("._")) {
            matched = false;
        }

        if (matched) {
            fileList.append({ absoluteFilePath, fileName,
                              sortMode == 1 ? fileInfo.lastModified().toMSecsSinceEpoch() : 0,
#if QT_VERSION >= QT_VERSION_CHECK(5, 12, 0)
                              sortMode == 2 ? fileInfo.birthTime().toMSecsSinceEpoch() : 0,
#else
                              sortMode == 2 ? fileInfo.created().toMSecsSinceEpoch() : 0,
#endif
                              sortMode == 3 ? fileInfo.size() : 0,
                              sortMode == 4 ? mimeType : QString() });
        }
    }

    if (QSettings().value("nest/groupRawJpeg",false).toBool()) {
        QSet<QString> jpegNames;
        for(const auto &f:fileList){QFileInfo info(f.absoluteFilePath);if(QStringList{"jpg","jpeg"}.contains(info.suffix().toLower()))jpegNames.insert(info.completeBaseName().toLower());}
        fileList.erase(std::remove_if(fileList.begin(),fileList.end(),[&](const CompatibleFile &f){QFileInfo info(f.absoluteFilePath);return NestMedia::nativeExtensions().contains("."+info.suffix().toLower())&&QStringList{"dng","cr2","cr3","nef","arw","orf","raf","rw2","pef"}.contains(info.suffix().toLower())&&jpegNames.contains(info.completeBaseName().toLower());}),fileList.end());
    }
    return fileList;
}

void QVImageCore::updateFolderInfo(QString dirPath)
{
    if (dirPath.isEmpty()) {
        dirPath = currentFileDetails.fileInfo.path();

        // No directory specified and a file is not already loaded from which we can infer one
        if (dirPath.isEmpty())
            return;
    }

    const auto previousFiles = currentFileDetails.folderFileInfoList;
    currentFileDetails.folderFileInfoList = getCompatibleFiles(dirPath);

    DirInfo dirInfo = { dirPath, currentFileDetails.folderFileInfoList.count(),
                        qvGetSettingInt(SortMode), qvGetSettingBool(SortDescending) };
    // If the current folder changed since the last image, assign a new seed for random sorting
    const bool shouldSort = lastDirInfo != dirInfo;
    lastDirInfo = dirInfo;

    auto &files = currentFileDetails.folderFileInfoList;
    if (dirInfo.sortMode == 5) {
        if (shouldSort) {
            std::shuffle(files.begin(), files.end(), std::default_random_engine(
                std::chrono::system_clock::now().time_since_epoch().count()));
        } else {
            // A rescan must retain the random sequence across navigation/refresh.
            QHash<QString,int> rank;
            for (int i=0;i<previousFiles.size();++i)rank.insert(previousFiles[i].absoluteFilePath,i);
            std::stable_sort(files.begin(),files.end(),[&](const CompatibleFile &a,const CompatibleFile &b){
                return rank.value(a.absoluteFilePath,previousFiles.size())<rank.value(b.absoluteFilePath,previousFiles.size());
            });
        }
    } else {
        // Every scan produces an unsorted list, even if the selected mode is unchanged.
        NestNaturalCollator collator;
        auto compareNumber=[](qint64 a,qint64 b){return a<b?-1:(a>b?1:0);};
        std::sort(files.begin(),files.end(),[&](const CompatibleFile &a,const CompatibleFile &b){
            int comparison=0;
            switch(dirInfo.sortMode){
            case 1:comparison=compareNumber(a.lastModified,b.lastModified);break;
            case 2:comparison=compareNumber(a.lastCreated,b.lastCreated);break;
            case 3:comparison=compareNumber(a.size,b.size);break;
            case 4:comparison=collator.compare(a.mimeType,b.mimeType);break;
            default:break;
            }
            if(!comparison)comparison=collator.compare(a.fileName,b.fileName);
            if(!comparison)comparison=QString::compare(a.fileName,b.fileName,Qt::CaseSensitive);
            return dirInfo.sortDescending?comparison>0:comparison<0;
        });
    }

    // Set current file index variable
    currentFileDetails.updateLoadedIndexInFolder();
}

void QVImageCore::requestCaching()
{
    int preloadingMode = qvGetSettingInt(PreloadingMode);
    if (preloadingMode == 0) {
        QVImageCore::imageCache.clear();
        return;
    }

    QColorSpace targetColorSpace = getTargetColorSpace();

    int preloadingDistance = 1;

    if (preloadingMode > 1)
        preloadingDistance = 4;

    QStringList filesToPreload;
    for (int i = currentFileDetails.loadedIndexInFolder - preloadingDistance;
         i <= currentFileDetails.loadedIndexInFolder + preloadingDistance; i++) {
        int index = i;

        // Don't try to cache the currently loaded image
        if (index == currentFileDetails.loadedIndexInFolder)
            continue;

        // keep within index range
        if (qvGetSettingBool(LoopFoldersEnabled)) {
            if (index > currentFileDetails.folderFileInfoList.length() - 1)
                index = index - (currentFileDetails.folderFileInfoList.length());
            else if (index < 0)
                index = index + (currentFileDetails.folderFileInfoList.length());
        }

        // if still out of range after looping, just cancel the cache for this index
        if (index > currentFileDetails.folderFileInfoList.length() - 1 || index < 0
            || currentFileDetails.folderFileInfoList.isEmpty())
            continue;

        QString filePath = currentFileDetails.folderFileInfoList[index].absoluteFilePath;
        filesToPreload.append(filePath);

        requestCachingFile(filePath, targetColorSpace);
    }
    lastFilesPreloaded = filesToPreload;
}

void QVImageCore::requestCachingFile(const QString &filePath, const QColorSpace &targetColorSpace)
{
    QFile imgFile(filePath);
    QString cacheKey = getPixmapCacheKey(filePath, imgFile.size(), targetColorSpace);

    // check if image is already loaded or requested
    if (QVImageCore::imageCache.contains(cacheKey) || lastFilesPreloaded.contains(filePath)
        || preloadFilesInProgress.contains(filePath) || preloadFilesInProgress.size() >= 2)
        return;

    if (imgFile.size() / 1024 > QVImageCore::imageCache.maxCost() / 2)
        return;

    preloadFilesInProgress.append(filePath);

    auto *cacheFutureWatcher = new QFutureWatcher<ReadData>(this);
    connect(cacheFutureWatcher, &QFutureWatcher<ReadData>::finished, this,
            [cacheFutureWatcher, this]() {
                const ReadData readData = cacheFutureWatcher->result();
                if (waitingOnPreloadFile == readData.absoluteFilePath) {
                    loadPixmap(readData);
                    waitingOnPreloadFile = QString();
                } else {
                    addToCache(std::move(readData));
                }
                preloadFilesInProgress.removeAll(readData.absoluteFilePath);
                cacheFutureWatcher->deleteLater();
            });

#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    cacheFutureWatcher->setFuture(
            QtConcurrent::run(this, &QVImageCore::readFile, filePath, targetColorSpace));
#else
    cacheFutureWatcher->setFuture(
            QtConcurrent::run(&QVImageCore::readFile, this, filePath, targetColorSpace));
#endif
}

void QVImageCore::addToCache(const ReadData &&readData)
{
    if (readData.image.isNull())
        return;

    QString cacheKey = getPixmapCacheKey(readData.absoluteFilePath, readData.fileSize,
                                         readData.targetColorSpace, readData.effectsSignature);
    qint64 pixmapMemoryBytes = static_cast<qint64>(readData.image.width()) * readData.image.height()
            * readData.image.depth() / 8;

    qint64 pixmapMemoryKiB = qMax(pixmapMemoryBytes / 1024, 1LL);
    QVImageCore::imageCache.insert(cacheKey, new ReadData(std::move(readData)), pixmapMemoryKiB);
}

QVImageCore::~QVImageCore()
{
    loadFutureWatcher.waitForFinished();
    for (auto child : children())
        if (auto watcher = dynamic_cast<QFutureWatcher<ReadData> *>(child)) watcher->waitForFinished();
}

QString QVImageCore::getPixmapCacheKey(const QString &absoluteFilePath, const qint64 &fileSize, const QColorSpace &targetColorSpace, const QString &effectsSignature)
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QString targetColorSpaceHash =
            QCryptographicHash::hash(targetColorSpace.iccProfile(), QCryptographicHash::Md5)
                    .toHex();
#else
    QString targetColorSpaceHash = "";
#endif
    return absoluteFilePath + "\n" + QString::number(fileSize) + "\n" + QString::number(QFileInfo(absoluteFilePath).lastModified().toMSecsSinceEpoch()) + "\n" + targetColorSpaceHash + "\n" + effectsSignature;
}

QColorSpace QVImageCore::getTargetColorSpace() const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    return colorSpaceConversion == 1    ? detectDisplayColorSpace()
            : colorSpaceConversion == 2 ? QColorSpace::SRgb
            : colorSpaceConversion == 3 ? QColorSpace::DisplayP3
                                        : QColorSpace();
#else
    return {};
#endif
}

QColorSpace QVImageCore::detectDisplayColorSpace() const
{
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
    QWindow *window = static_cast<QWidget *>(parent())->window()->windowHandle();
    if (!window) return {};

    QByteArray profileData;
#  ifdef WIN32_LOADED
    profileData = QVWin32Functions::getIccProfileForWindow(window);
#  endif
#  ifdef COCOA_LOADED
    profileData = QVCocoaFunctions::getIccProfileForWindow(window);
#  endif
#  ifdef X11_LOADED
    profileData = QVLinuxX11Functions::getIccProfileForWindow(window);
#  endif

    if (!profileData.isEmpty()) {
        QColorSpace colorSpace = QColorSpace::fromIccProfile(profileData);
#  if QT_VERSION < QT_VERSION_CHECK(6, 7, 2)
        if (!colorSpace.isValid() && removeTinyDataTagsFromIccProfile(profileData))
            colorSpace = QColorSpace::fromIccProfile(profileData);
#  endif
        return colorSpace;
    }
#endif

    return {};
}

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0) && QT_VERSION < QT_VERSION_CHECK(6, 7, 2)
// Workaround for QTBUG-125241
bool QVImageCore::removeTinyDataTagsFromIccProfile(QByteArray &profile)
{
    const int offsetTagCount = 128;
    const qsizetype length = profile.length();
    qsizetype offset = offsetTagCount;
    char *data = profile.data();
    bool foundTinyData = false;
    // read tag count
    if (length - offset < 4)
        return false;
    quint32 tagCount = qFromBigEndian<quint32>(data + offset);
    offset += 4;
    // so we don't have to worry about overflows
    if (tagCount > 99999)
        return false;
    // loop through tags
    if (length - offset < qsizetype(tagCount * 12))
        return false;
    while (tagCount) {
        tagCount -= 1;
        const quint32 dataSize = qFromBigEndian<quint32>(data + offset + 8);
        if (dataSize >= 12) {
            // this tag is fine
            offset += 12;
            continue;
        }
        // qt will fail on this tag, remove it
        foundTinyData = true;
        if (tagCount) {
            // shift subsequent tags back
            std::memmove(data + offset, data + offset + 12, tagCount * 12);
        }
        // zero fill gap at end
        std::memset(data + offset + (tagCount * 12), 0, 12);
        // decrement tag count
        qToBigEndian(qFromBigEndian<quint32>(data + offsetTagCount) - 1, data + offsetTagCount);
    }
    return foundTinyData;
}
#endif

void QVImageCore::jumpToNextFrame()
{
    if (currentFileDetails.isMovieLoaded)
        loadedMovie.jumpToNextFrame();
}

void QVImageCore::setPaused(bool desiredState)
{
    if (currentFileDetails.isMovieLoaded)
        loadedMovie.setPaused(desiredState);
}

void QVImageCore::setSpeed(int desiredSpeed)
{
    if (desiredSpeed < 0)
        desiredSpeed = 0;

    if (desiredSpeed > 1000)
        desiredSpeed = 1000;

    if (currentFileDetails.isMovieLoaded)
        loadedMovie.setSpeed(desiredSpeed);
}

void QVImageCore::rotateImage(int rotation)
{
    currentRotation += rotation;

    // normalize between 360 and 0
    currentRotation = (currentRotation % 360 + 360) % 360;
    QTransform transform;

    QImage transformedImage;
    if (currentFileDetails.isMovieLoaded) {
        transform.rotate(currentRotation);
        transformedImage = loadedMovie.currentImage().transformed(transform);
    } else {
        transform.rotate(rotation);
        transformedImage = loadedPixmap.toImage().transformed(transform);
    }

    loadedPixmap.convertFromImage(transformedImage);

    currentFileDetails.loadedPixmapSize = QSize(loadedPixmap.width(), loadedPixmap.height());
    emit updateLoadedPixmapItem();
}

QImage QVImageCore::matchCurrentRotation(const QImage &imageToRotate)
{
    if (!currentRotation)
        return imageToRotate;

    QTransform transform;
    transform.rotate(currentRotation);
    return imageToRotate.transformed(transform);
}

// TODO: Remove this function---extremely inefficient
QPixmap QVImageCore::matchCurrentRotation(const QPixmap &pixmapToRotate)
{
    if (!currentRotation)
        return pixmapToRotate;

    return QPixmap::fromImage(matchCurrentRotation(pixmapToRotate.toImage()));
}

QPixmap QVImageCore::scaleExpensively(const int desiredWidth, const int desiredHeight)
{
    return scaleExpensively(QSizeF(desiredWidth, desiredHeight));
}

QPixmap QVImageCore::scaleExpensively(const QSizeF desiredSize)
{
    if (!currentFileDetails.isPixmapLoaded)
        return QPixmap();

    QSize size = QSize(loadedPixmap.width(), loadedPixmap.height());
    size.scale(desiredSize.toSize(), Qt::KeepAspectRatio);

    // Get the current frame of the animation if this is an animation
    QPixmap relevantPixmap;
    if (!currentFileDetails.isMovieLoaded) {
        relevantPixmap = loadedPixmap;
    } else {
        relevantPixmap = loadedMovie.currentPixmap();
        relevantPixmap = matchCurrentRotation(relevantPixmap);
    }

    // If we are really close to the original size, just return the original
    if (abs(desiredSize.width() - relevantPixmap.width()) < 1
        && abs(desiredSize.height() - relevantPixmap.height()) < 1) {
        return relevantPixmap;
    }

    return relevantPixmap.scaled(size, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    ;
}

void QVImageCore::settingsUpdated()
{
    auto &settingsManager = qvApp->getSettingsManager();

    // preloading mode
    // Cost is in KiB
    switch (qvGetSettingInt(PreloadingMode)) {
    case 0: {
        QVImageCore::imageCache.setMaxCost(0);
        break;
    }
    case 1: {
        QVImageCore::imageCache.setMaxCost(256000);
        break;
    }
    case 2: {
        QVImageCore::imageCache.setMaxCost(2048000);
        break;
    }
    default:
        Q_ASSERT(false);
    }

    // update folder info to reflect new settings (e.g. sort order)
    updateFolderInfo();

    const QString signature=NestMedia::effects().signature();
    bool changedImagePreprocessing = signature != lastEffectSignature;
    lastEffectSignature=signature;

    // colorspaceconversion
    if (colorSpaceConversion != qvGetSettingInt(ColorSpaceConversion)) {
        colorSpaceConversion = qvGetSettingInt(ColorSpaceConversion);
        changedImagePreprocessing = true;
    }

    if (changedImagePreprocessing && currentFileDetails.isPixmapLoaded)
        loadFile(currentFileDetails.fileInfo.absoluteFilePath());
}

void QVImageCore::FileDetails::updateLoadedIndexInFolder()
{
    const QString targetPath = fileInfo.absoluteFilePath().normalized(QString::NormalizationForm_D);
    for (int i = 0; i < folderFileInfoList.length(); i++) {
        // Compare absoluteFilePath first because it's way faster, but double-check with
        // QFileInfo::operator== because it respects file system case sensitivity rules
        QString candidatePath =
                folderFileInfoList[i].absoluteFilePath.normalized(QString::NormalizationForm_D);
        if (candidatePath.compare(targetPath, Qt::CaseInsensitive) == 0
            && QFileInfo(folderFileInfoList[i].absoluteFilePath) == fileInfo) {
            loadedIndexInFolder = i;
            return;
        }
    }
    loadedIndexInFolder = -1;
}
