/*
    SPDX-FileCopyrightText: 2025 Krusader Krew <https://krusader.org>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef IMAGEBROWSER_H
#define IMAGEBROWSER_H

// QtCore
#include <QObject>
#include <QPointer>
#include <QStringList>
#include <QUrl>

#include <KParts/ReadOnlyPart>

class QWidget;

/**
 * Helper that allows browsing through all images of the current directory
 * directly inside KrViewer.
 *
 * The image KPart (e.g. Gwenview's gvpart) does not know about sibling
 * images, so the list of images has to be built by Krusader itself.
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
     */
    void setPart(KParts::ReadOnlyPart *part);

    /**
     * Returns true if the given part displays an image.
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
     * different file.
     */
    bool canStep(bool forward);

public slots:
    /**
     * Compute the next/previous image URL of the current directory.
     * @param forward true: next image, false: previous image
     * @return the target URL, or an invalid URL if there is nothing to step to
     */
    QUrl nextUrl(bool forward);

    /**
     * Forget the cached list of images - it is rebuilt on the next step.
     */
    void reset();

private:
    /**
     * Rebuild the list of images of the directory the current URL lives in.
     */
    void refreshImageList();

    /**
     * @return the index of the currently shown image, -1 if unknown
     */
    int currentIndex() const;

    /**
     * @return the directory the currently shown file lives in
     */
    QString dirPath() const;

    QPointer<KParts::ReadOnlyPart> m_part;
    QStringList m_images; // absolute local file paths, sorted by name
    QString m_currentDir;
};

#endif