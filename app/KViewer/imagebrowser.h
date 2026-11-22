/*
    SPDX-FileCopyrightText: 2025 Krusader Krew <https://krusader.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef IMAGEBROWSER_H
#define IMAGEBROWSER_H

// QtCore
#include <QObject>
#include <QPointer>
#include <QUrl>
#include <QVector>

#include <KParts/ReadOnlyPart>

class QWidget;

namespace KIO
{
class ListJob;
}

/**
 * Helper that allows browsing through all images of the current directory
 * directly inside KrViewer.
 *
 * The image KPart (e.g. Gwenview's gvpart) does not know about sibling
 * images, so the list of images has to be built by Krusader itself.
 * Local directories are read with QDir; remote directories (sftp://, etc.)
 * are listed asynchronously with KIO::listDir().
 *
 * The navigation is triggered either by the mouse wheel or by the arrow keys,
 * and reuses the already opened part - no new tab is created.
 */
class ImageBrowser : public QObject
{
    Q_OBJECT

public:
    explicit ImageBrowser(QObject *parent = nullptr);
    ~ImageBrowser() override;

    /**
     * Attach the browser to the part that is currently displayed.
     * Passing nullptr detaches it.
     * If the part/directory did not change, an already running list job is
     * kept, so stepping through a remote directory does not relist it.
     */
    void setPart(KParts::ReadOnlyPart *part);

    /**
     * Returns true if the given part displays an image (local or remote).
     */
    static bool isImagePart(KParts::ReadOnlyPart *part);

    /**
     * Map a vertical wheel delta to a browsing direction.
     * Qt reports a positive delta for scrolling up; scrolling down advances
     * to the next image and scrolling up goes to the previous one.
     */
    static bool isForwardWheelDelta(int deltaY);

    /**
     * Returns true if the wheel event was sent to the image part widget or
     * one of its child widgets (for example the graphics view viewport).
     */
    static bool isPartWidgetTarget(QObject *watched, QWidget *partWidget);

    /**
     * Returns true if stepping to the given direction would lead to a
     * different file. For a remote directory that is still being listed,
     * this returns false until the listing finishes.
     */
    bool canStep(bool forward);

    /**
     * True while a remote listing is in progress and a step was requested.
     */
    bool hasPendingStep() const;

public slots:
    /**
     * Compute the next/previous image URL of the current directory.
     * For local directories the URL is returned immediately. For remote
     * directories an invalid URL is returned and nextUrlReady() is emitted
     * once the directory listing has finished.
     *
     * @param forward true: next image, false: previous image
     * @return the target URL, or an invalid URL if it is not available yet
     */
    QUrl nextUrl(bool forward);

    /**
     * Forget the cached list of images and cancel any pending list job.
     */
    void reset();

Q_SIGNALS:
    /**
     * Emitted after a pending remote listing finishes. The URL is invalid if
     * the listing failed or there is no other image to show.
     */
    void nextUrlReady(const QUrl &url);

private:
    /**
     * Build the image list for the current part, using QDir for local URLs
     * and KIO::listDir() for remote URLs.
     */
    void ensureList();

    /**
     * Start an asynchronous KIO listing of a remote directory.
     */
    void startRemoteList(const QUrl &dirUrl);

    /**
     * Cancel any running KIO job and clear the cached state.
     */
    void cancelPendingList();

    /**
     * @return the index of the currently shown image, -1 if unknown
     */
    int currentIndex() const;

    /**
     * @return the parent directory URL of a file URL
     */
    static QUrl directoryUrl(const QUrl &url);

    QPointer<KParts::ReadOnlyPart> m_part;
    QVector<QUrl> m_images; // image URLs, sorted by file name
    QUrl m_currentDirUrl;
    KIO::ListJob *m_listJob = nullptr;
    bool m_listReady = false;
    bool m_hasPendingStep = false;
    bool m_pendingForward = true;
};

#endif
