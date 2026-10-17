#include "photoupdater.h"
#include "nestupdate.h"
#include <QApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QJsonArray>
#include <QJsonDocument>
#include <QMessageBox>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QPushButton>
#include <QSettings>
#include <QTimer>

namespace {
bool checking = false;
bool interactive = false;
const QString repository = QStringLiteral(NEST_UPDATE_REPOSITORY);
void checkRelease(bool userInitiated) {
    interactive = interactive || userInitiated;
    if (checking) return;
    if (repository.isEmpty()) {
        if (interactive) QMessageBox::information(nullptr, QStringLiteral("업데이트"), QStringLiteral("이 빌드에는 업데이트 저장소가 설정되지 않았습니다."));
        interactive = false;
        return;
    }
    checking = true;
    auto manager = new QNetworkAccessManager(qApp);
    QNetworkRequest request(QUrl("https://api.github.com/repos/" + repository + "/releases/latest"));
    request.setRawHeader("User-Agent", "NestView/" NEST_VERSION);
    request.setRawHeader("Accept", "application/vnd.github+json");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
    request.setTransferTimeout(15000);
    auto reply = manager->get(request);
    QTimer::singleShot(20000, reply, [reply] { if (!reply->isFinished()) reply->abort(); });
    QObject::connect(reply, &QNetworkReply::readyRead, reply, [reply] {
        if (reply->bytesAvailable() > 2 * 1024 * 1024) reply->abort();
    });
    QObject::connect(reply, &QNetworkReply::finished, qApp, [reply, manager] {
        const bool show = interactive;
        interactive = false;
        checking = false;
        const auto error = reply->error();
        const auto document = QJsonDocument::fromJson(reply->readAll());
        const auto release = document.object();
        manager->deleteLater();
        if (error != QNetworkReply::NoError || !document.isObject() || release.value("tag_name").toString().isEmpty()) {
            if (show) QMessageBox::warning(nullptr, QStringLiteral("업데이트 확인"), QStringLiteral("업데이트 정보를 가져올 수 없습니다. 인터넷 연결을 확인한 뒤 다시 시도해 주세요."));
            return;
        }
        QSettings().setValue("nest/lastUpdateCheck", QDateTime::currentSecsSinceEpoch());
        const auto tag = release.value("tag_name").toString();
        if (release.value("draft").toBool() || release.value("prerelease").toBool() || !NestUpdate::newerVersion(tag, QStringLiteral(NEST_VERSION))) {
            if (show) QMessageBox::information(nullptr, QStringLiteral("업데이트 확인"), QStringLiteral("최신 버전을 사용 중입니다.\n현재 버전: ") + QStringLiteral(NEST_VERSION));
            return;
        }
        QUrl download;
        for (const auto &entry : release.value("assets").toArray()) {
            const auto asset = entry.toObject();
            const QUrl url(asset.value("browser_download_url").toString());
            if (asset.value("name").toString() == NestUpdate::assetName() && NestUpdate::validAssetURL(url, repository)) {
                download = url;
                break;
            }
        }
        if (download.isEmpty()) {
            if (show) QMessageBox::information(nullptr, QStringLiteral("업데이트 확인"), QStringLiteral("새 버전 %1이 있지만 이 운영체제의 설치 파일은 아직 게시되지 않았습니다.").arg(tag));
            return;
        }
        QMessageBox dialog(QMessageBox::Information, QStringLiteral("새 업데이트가 있습니다"),
            QStringLiteral("현재 버전: %1 → 새 버전: %2\n\n설치 파일을 다운로드한 뒤 NestView를 닫고 새 버전을 설치해 주세요.").arg(QStringLiteral(NEST_VERSION), tag), QMessageBox::Cancel);
        auto button = dialog.addButton(QStringLiteral("설치 파일 다운로드"), QMessageBox::AcceptRole);
        dialog.exec();
        if (dialog.clickedButton() == button && !QDesktopServices::openUrl(download))
            QMessageBox::warning(nullptr, QStringLiteral("업데이트"), QStringLiteral("기본 브라우저를 열 수 없습니다."));
    });
}
}

namespace PhotoUpdater {
bool configured() { return !repository.isEmpty(); }
bool automaticallyChecks() { return QSettings().value("nest/automaticallyChecksForUpdates", true).toBool(); }
void setAutomaticallyChecks(bool enabled) { QSettings().setValue("nest/automaticallyChecksForUpdates", enabled); }
void start() {
    if (automaticallyChecks() && QDateTime::currentSecsSinceEpoch() - QSettings().value("nest/lastUpdateCheck", 0).toLongLong() >= 86400)
        checkRelease(false);
}
void check() { checkRelease(true); }
}
