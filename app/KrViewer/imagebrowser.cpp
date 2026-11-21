/*
    SPDX-FileCopyrightText: 2025 Krusader Krew <https://krusader.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "imagebrowser.h"

// QtCore
#include <QDir>
#include <QFileInfo>
#include <QMimeDatabase>

// KF
#include <KParts/OpenUrlArguments>
#include <KParts/ReadOnlyPart>

ImageBrowser::ImageBrowser(QObject *parent)
    : QObject(parent)
{
}

ImageBrowser::~ImageBrowser() = default;

bool ImageBrowser::isImagePart(KParts::ReadOnlyPart *part)
{
    if (!part)
        return false;

    const QUrl url = part->url();
    if (!url.isValid() || !url.isLocalFile())
        return false;

    QMimeDatabase db;
    const QMimeType mime = db.mimeTypeForFile(url.toLocalFile(), QMimeDatabase::MatchExtension);
    // QMimeType::inherits() walks the subtype hierarchy (e.g. image/jpeg ->
    // application/octet-stream) and would not match a generic "image/" type,
    // so the top-level type name is checked directly.
    return mime.name().startsWith(QLatin1String("image/"), Qt::CaseInsensitive);
}

void ImageBrowser::setPart(KParts::ReadOnlyPart *part)
{
    m_part = part;
    m_images.clear();
    m_currentDir.clear();
}

bool ImageBrowser::canStep(bool /*forward*/)
{
    if (m_images.isEmpty())
        refreshImageList();
    if (m_images.size() < 2)
        return false;

    // nextUrl() wraps around the image list, so both directions lead
    // somewhere as long as there are at least 2 images.
    return true;
}

void ImageBrowser::reset()
{
    m_images.clear();
    m_currentDir.clear();
}

void ImageBrowser::refreshImageList()
{
    m_images.clear();
    m_currentDir.clear();

    if (!m_part)
        return;

    const QUrl url = m_part->url();
    if (!url.isValid() || !url.isLocalFile())
        return;

    const QString localPath = QDir::toNativeSeparators(url.path());
    const QFileInfo fileInfo(localPath);
    const QString dirPath = fileInfo.absolutePath();
    if (dirPath.isEmpty())
        return;

    QDir dir(dirPath);
    if (!dir.exists())
        return;

    QMimeDatabase db;
    const QStringList entries = dir.entryList(QDir::Files | QDir::Readable | QDir::Hidden, QDir::Name);

    for (const QString &entry : entries) {
        const QString path = dir.absoluteFilePath(entry);
        if (QFileInfo(path).isDir())
            continue;

        const QMimeType mime = db.mimeTypeForFile(path, QMimeDatabase::MatchExtension);
        if (mime.name().startsWith(QLatin1String("image/"), Qt::CaseInsensitive)) {
            m_images.append(path);
        }
    }

    m_currentDir = dirPath;
}

int ImageBrowser::currentIndex() const
{
    if (!m_part)
        return -1;

    const QUrl url = m_part->url();
    if (!url.isValid() || !url.isLocalFile())
        return -1;

    const QString path = QDir::toNativeSeparators(url.path());
    return static_cast<int>(m_images.indexOf(path));
}

QUrl ImageBrowser::nextUrl(bool forward)
{
    if (!m_part)
        return QUrl();

    const QUrl url = m_part->url();
    if (!url.isValid() || !url.isLocalFile())
        return QUrl();

    // Rebuild the list when the directory changed or the list is empty.
    if (m_images.isEmpty() || dirPath() != m_currentDir) {
        refreshImageList();
    }

    if (m_images.size() < 2 || !canStep(forward))
        return QUrl();

    int index = currentIndex();
    if (index < 0) {
        // The current file is not part of the list (e.g. it was deleted):
        // start at the beginning/end of the list.
        index = forward ? -1 : 0;
    }

    index += forward ? 1 : -1;
    const int count = static_cast<int>(m_images.size());
    if (index < 0)
        index = count - 1;
    else if (index >= count)
        index = 0;

    const QString newPath = m_images.at(index);
    return QUrl::fromLocalFile(newPath);
}

QString ImageBrowser::dirPath() const
{
    if (!m_part)
        return QString();

    const QUrl url = m_part->url();
    if (!url.isValid() || !url.isLocalFile())
        return QString();

    return QFileInfo(QDir::toNativeSeparators(url.path())).absolutePath();
}
