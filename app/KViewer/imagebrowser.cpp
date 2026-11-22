/*
    SPDX-FileCopyrightText: 2025 Krusader Krew <https://krusader.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "imagebrowser.h"

#include <algorithm>

// QtCore
#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>
// QtWidgets
#include <QWidget>

// KF
#include <KIO/Job>
#include <KIO/ListJob>
#include <KIO/UDSEntry>
#include <KJob>
#include <KParts/ReadOnlyPart>

ImageBrowser::ImageBrowser(QObject *parent)
    : QObject(parent)
{
}

ImageBrowser::~ImageBrowser()
{
    cancelPendingList();
}

bool ImageBrowser::isForwardWheelDelta(int deltaY)
{
    // In Qt a positive vertical delta means the wheel was scrolled up.
    // Scrolling down shows the next image, scrolling up the previous one.
    return deltaY < 0;
}

bool ImageBrowser::isPartWidgetTarget(QObject *watched, QWidget *partWidget)
{
    auto *eventWidget = qobject_cast<QWidget *>(watched);
    return partWidget && eventWidget && (eventWidget == partWidget || partWidget->isAncestorOf(eventWidget));
}

bool ImageBrowser::isImagePart(KParts::ReadOnlyPart *part)
{
    if (!part)
        return false;

    const QUrl url = part->url();
    if (!url.isValid() || url.isEmpty())
        return false;

    // mimeTypeForUrl() uses the file name for non-local URLs as well, so this
    // works for file://, sftp:// and other KIO URLs without touching the file.
    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForUrl(url);
    // QMimeType::inherits() walks the subtype hierarchy and would not match a
    // generic "image/" type, so the top-level type name is checked directly.
    return mime.name().startsWith(QLatin1String("image/"), Qt::CaseInsensitive);
}

void ImageBrowser::setPart(KParts::ReadOnlyPart *part)
{
    const QUrl oldUrl = m_part ? m_part->url() : QUrl();
    if (m_part == part && (!part || part->url() == oldUrl) && (m_listReady || m_listJob))
        return;

    cancelPendingList();
    m_part = part;

    if (part)
        ensureList();
}

bool ImageBrowser::canStep(bool /*forward*/)
{
    ensureList();
    return m_listReady && m_items.size() >= 2;
}

bool ImageBrowser::hasPendingStep() const
{
    return m_hasPendingStep;
}

void ImageBrowser::reset()
{
    cancelPendingList();
}

QUrl ImageBrowser::nextUrl(bool forward)
{
    return nextItem(forward).url();
}

KFileItem ImageBrowser::nextItem(bool forward)
{
    if (!m_part)
        return KFileItem();

    const QUrl url = m_part->url();
    if (!url.isValid() || url.isEmpty())
        return KFileItem();

    ensureList();

    if (!m_listReady) {
        // A remote listing is still running. Remember the direction and let
        // nextItemReady() deliver the target once the list is available.
        m_pendingForward = forward;
        m_hasPendingStep = true;
        return KFileItem();
    }

    if (m_items.size() < 2)
        return KFileItem();

    int index = currentIndex();
    if (index < 0) {
        // The current file is not part of the list (e.g. it was deleted):
        // start at the beginning/end of the list.
        index = forward ? -1 : 0;
    }

    index += forward ? 1 : -1;
    const int count = m_items.size();
    if (index < 0)
        index = count - 1;
    else if (index >= count)
        index = 0;

    return m_items.at(index);
}

void ImageBrowser::ensureList()
{
    if (!m_part)
        return;

    const QUrl url = m_part->url();
    if (!url.isValid() || url.isEmpty())
        return;

    const QUrl dir = directoryUrl(url);
    if ((m_listReady || m_listJob) && dir == m_currentDirUrl)
        return;

    cancelPendingList();
    m_currentDirUrl = dir;

    if (url.isLocalFile()) {
        const QDir directory(QDir::toNativeSeparators(dir.toLocalFile()));
        if (!directory.exists())
            return;

        QMimeDatabase db;
        const QStringList entries = directory.entryList(QDir::Files | QDir::Readable | QDir::Hidden, QDir::Name);
        for (const QString &entry : entries) {
            const QString path = QDir::toNativeSeparators(directory.absoluteFilePath(entry));
            const QMimeType mime = db.mimeTypeForFile(path, QMimeDatabase::MatchExtension);
            if (mime.name().startsWith(QLatin1String("image/"), Qt::CaseInsensitive)) {
                const QUrl itemUrl = QUrl::fromLocalFile(path);
                m_items.append(KFileItem(itemUrl, mime.name()));
            }
        }
        m_listReady = true;
        return;
    }

    // Remote URL (sftp://, fish://, ...): list it asynchronously through KIO.
    startRemoteList(dir);
}

void ImageBrowser::startRemoteList(const QUrl &dirUrl)
{
    KIO::ListJob *job = KIO::listDir(dirUrl, KIO::HideProgressInfo);
    if (!job)
        return;

    job->setParent(this);
    // Keep the default KIO UI delegate: it is needed to ask for credentials
    // on WebDAV/SFTP servers. KIO::HideProgressInfo above only suppresses the
    // progress dialog; list failures still report an error through KIO.
    m_listJob = job;

    connect(job, &KIO::ListJob::entries, this, [this](KIO::Job *, const KIO::UDSEntryList &list) {
        for (const KIO::UDSEntry &entry : list) {
            const QString name = entry.stringValue(KIO::UDSEntry::UDS_NAME);
            if (name.isEmpty() || name == QLatin1String(".") || name == QLatin1String(".."))
                continue;

            // urlIsDirectory=true lets KFileItem resolve relative UDS_URLs
            // against the directory that was listed.
            KFileItem item(entry, m_currentDirUrl, false, true);
            if (item.isNull() || !item.url().isValid())
                continue;

            QString mimeName = entry.stringValue(KIO::UDSEntry::UDS_MIME_TYPE);
            if (mimeName.isEmpty())
                mimeName = item.mimetype();
            if (mimeName.startsWith(QLatin1String("image/"), Qt::CaseInsensitive))
                m_items.append(item);
        }
    });

    connect(job, &KJob::result, this, [this](KJob *finishedJob) {
        if (m_listJob != finishedJob)
            return;

        m_listJob = nullptr;
        std::sort(m_items.begin(), m_items.end(), [](const KFileItem &a, const KFileItem &b) {
            return a.url().fileName().compare(b.url().fileName(), Qt::CaseInsensitive) < 0;
        });

        m_listReady = (finishedJob->error() == 0);
        if (!m_listReady)
            m_items.clear();

        if (m_hasPendingStep) {
            m_hasPendingStep = false;
            emit nextItemReady(m_listReady ? nextItem(m_pendingForward) : KFileItem());
        }
    });
}

void ImageBrowser::cancelPendingList()
{
    if (m_listJob) {
        disconnect(m_listJob, nullptr, this, nullptr);
        m_listJob->kill();
        m_listJob = nullptr;
    }

    m_items.clear();
    m_currentDirUrl = QUrl();
    m_listReady = false;
    m_hasPendingStep = false;
}

int ImageBrowser::currentIndex() const
{
    if (!m_part)
        return -1;

    const QUrl current = m_part->url();
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items.at(i).url() == current)
            return i;
    }
    return -1;
}

QUrl ImageBrowser::directoryUrl(const QUrl &url)
{
    return url.adjusted(QUrl::RemoveFilename | QUrl::StripTrailingSlash);
}
