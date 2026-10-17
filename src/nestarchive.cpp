#include "nestmedia.h"
#include <archive.h>
#include <archive_entry.h>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QImageReader>
#include <QCollator>
#include <algorithm>
bool NestMedia::isArchive(const QString &path) {
    return QStringList{"zip","cbz","rar","cbr","7z","cb7","tar"}.contains(QFileInfo(path).suffix().toLower());
}
NestMedia::ArchiveResult NestMedia::extract(const QString &path, const std::shared_ptr<std::atomic_bool> &cancelled) {
    ArchiveResult result;
    const auto isCancelled=[&]{return cancelled && cancelled->load();};
    if(isCancelled()){result.error="압축파일 열기를 취소했습니다.";return result;}
    result.directory.reset(new QTemporaryDir(QDir::tempPath()+"/nestview-XXXXXX"));
    if (!result.directory->isValid()) { result.error = "임시 폴더를 만들 수 없습니다."; return result; }
    auto a = archive_read_new();
    archive_read_support_filter_all(a); archive_read_support_format_all(a);
    auto fail = [&](const QString &message) { result.error = message; result.files.clear(); };
#ifdef Q_OS_WIN
    const int openStatus = archive_read_open_filename_w(a, path.toStdWString().c_str(), 65536);
#else
    const int openStatus = archive_read_open_filename(a, QFile::encodeName(path).constData(), 65536);
#endif
    if (openStatus != ARCHIVE_OK) {
        fail(QString::fromUtf8(archive_error_string(a))); archive_read_free(a); return result;
    }
    struct Entry { QString logical; QString path; };
    QList<Entry> entries;
    archive_entry *entry;
    qint64 total = 0;
    int count = 0, status;
    while ((status = archive_read_next_header(a, &entry)) == ARCHIVE_OK) {
        if(isCancelled()){fail("압축파일 열기를 취소했습니다.");break;}
        if (++count > 20000) { fail("압축 항목 수 제한(20,000)을 초과했습니다."); break; }
        const char *name = archive_entry_pathname_utf8(entry);
        if (!name) name = archive_entry_pathname(entry);
        QString logical = QString::fromUtf8(name ? name : "");
        logical.replace('\\','/');
        if (logical.startsWith('/') || logical.contains(':') || logical.split('/').contains("..") || archive_entry_symlink(entry) || archive_entry_hardlink(entry)) {
            fail("안전하지 않은 압축파일 경로 또는 링크입니다."); break;
        }
        if (archive_entry_filetype(entry) != AE_IFREG) { archive_read_data_skip(a); continue; }
        const QString suffix = QFileInfo(logical).suffix().toLower();
        if ((!QImageReader::supportedImageFormats().contains(suffix.toUtf8()) && !nativeExtensions().contains("."+suffix)) || logical.contains("__MACOSX/") || QFileInfo(logical).fileName().startsWith("._")) { archive_read_data_skip(a); continue; }
        if (archive_entry_size(entry) > 512LL*1024*1024) { fail("압축 항목이 512 MiB를 초과했습니다."); break; }
        QString output = result.directory->filePath(QString::number(entries.size())+"."+suffix);
        QFile file(output);
        if (!file.open(QIODevice::WriteOnly)) { fail(file.errorString()); break; }
        char buffer[65536]; la_ssize_t n; qint64 item = 0;
        while ((n = archive_read_data(a, buffer, sizeof(buffer))) > 0) {
            if(isCancelled()){fail("압축파일 열기를 취소했습니다.");break;}
            total += n; item += n;
            if (total > 2LL*1024*1024*1024 || item > 512LL*1024*1024) { fail("압축 해제 용량 제한을 초과했습니다."); break; }
            if (file.write(buffer,n) != n) { fail(file.errorString()); break; }
        }
        file.close();
        if (n < 0) fail(QString::fromUtf8(archive_error_string(a)));
        if (!result.error.isEmpty()) break;
        entries.append({logical,output});
    }
    if (status != ARCHIVE_EOF && result.error.isEmpty()) fail(QString::fromUtf8(archive_error_string(a)));
    archive_read_free(a);
    if (!result.error.isEmpty()) return result;
    QCollator collator; collator.setNumericMode(true); collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(entries.begin(),entries.end(),[&](const Entry &a,const Entry &b){return collator.compare(a.logical,b.logical)<0;});
    for (int i=0; i<entries.size(); ++i) {
        if(isCancelled()){fail("압축파일 열기를 취소했습니다.");return result;}
        QString base = QFileInfo(entries[i].logical).fileName();
        QString output = result.directory->filePath(QString("%1_%2").arg(i+1,6,10,QChar('0')).arg(base));
        if (!QFile::rename(entries[i].path,output)) { fail("임시 이미지 이름을 정리할 수 없습니다."); return result; }
        result.files.append(output);
    }
    if (result.files.isEmpty()) fail("지원되는 이미지가 없는 압축파일입니다.");
    return result;
}
