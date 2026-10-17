#include "mainwindow.h"
#include "qvapplication.h"
#include "qvwin32functions.h"

#include <QCommandLineParser>
#include <QSettings>
#include <QTimer>
#include <QBuffer>
#include <QImageReader>
#include "photoupdater.h"

int main(int argc, char *argv[])
{
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
#endif
    QCoreApplication::setOrganizationName("NestView");
    QCoreApplication::setApplicationName("NestView");
    QCoreApplication::setApplicationVersion(QString::number(VERSION));
    QVApplication app(argc, argv);
    QGuiApplication::setApplicationDisplayName("NestView");
    QGuiApplication::setWindowIcon(QIcon(QStringLiteral(":/images/nestview.svg")));

    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption smokeTest(QStringLiteral("smoke-test"));
    smokeTest.setFlags(QCommandLineOption::HiddenFromHelp);
    parser.addOption(smokeTest);
    parser.addPositionalArgument(QObject::tr("file"), QObject::tr("The file to open."));
#if defined Q_OS_WIN && WIN32_LOADED && QT_VERSION < QT_VERSION_CHECK(6, 7, 2)
    // Workaround for unicode characters getting mangled in certain cases. To support unicode
    // arguments on Windows, QCoreApplication normally ignores argv and gets them from the Windows
    // API instead. But this only happens if it thinks argv hasn't been modified prior to being
    // passed into QCoreApplication's constructor. Certain characters like U+2033 (double prime) get
    // converted differently in argv versus the value Qt is comparing with (__argv). This makes Qt
    // incorrectly think the data was changed, and it skips fetching unicode arguments from the API.
    // https://bugreports.qt.io/browse/QTBUG-125380
    parser.process(QVWin32Functions::getCommandLineArgs());
#else
    parser.process(app);
#endif

    if (parser.isSet(smokeTest)) {
        // Verify the deployed GUI and codecs without opening a command-line message box.
        QWidget probe;
        if (!probe.winId()) return 2;
        for (const auto &format : {"PNG", "JPEG", "WEBP"}) {
            QImage source(32, 24, QImage::Format_RGB32);
            source.fill(Qt::green);
            QBuffer buffer;
            if (!buffer.open(QIODevice::ReadWrite) || !source.save(&buffer, format)) return 3;
            buffer.seek(0);
            QImageReader reader(&buffer, format);
            if (reader.read().size() != source.size()) return 4;
        }
        return 0;
    }

    auto *window = QVApplication::newWindow();
    QTimer::singleShot(1000, &app, [] { PhotoUpdater::start(); });
    if (!parser.positionalArguments().isEmpty())
        QVApplication::openFile(window, parser.positionalArguments().constFirst(), true);

    return QApplication::exec();
}
