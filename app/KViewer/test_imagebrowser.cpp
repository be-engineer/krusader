/*
    SPDX-License-Identifier: GPL-2.0-or-later

    Standalone test for the ImageBrowser helper of KrViewer.

    It verifies that the list of images of the current directory is built
    correctly and that stepping forward/backward selects the expected file.
*/

#include <QApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QUrl>
#include <QWidget>

#include <KParts/ReadOnlyPart>

#include "imagebrowser.h"

static int failures = 0;

static void check(bool condition, const QString &what)
{
    QTextStream out(stdout);
    out << (condition ? "PASS  " : "FAIL  ") << what << Qt::endl;
    if (!condition)
        failures++;
}

/**
 * Minimal part stub - only url() / setUrl() are used by ImageBrowser.
 */
class StubPart : public KParts::ReadOnlyPart
{
public:
    using KParts::ReadOnlyPart::ReadOnlyPart;
    void setUrlPublic(const QUrl &url) { setUrl(url); }
};

static QString nativeFile(const QDir &dir, const QString &name)
{
    return QDir::toNativeSeparators(dir.absoluteFilePath(name));
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);

    const QString testDir = QString::fromLocal8Bit(argv[1] ? argv[1] : "/tmp/opencode/testimgs");
    QDir dir(testDir);
    if (!dir.exists()) {
        QTextStream out(stdout);
        out << "FAIL  test directory does not exist: " << testDir << Qt::endl;
        return 1;
    }

    // images are named img1.png .. img5.png; sorted by name:
    // img1.png, img2.png, img3.png, img4.png, img5.png
    const QStringList imageNames = {
        QStringLiteral("img1.png"),
        QStringLiteral("img2.png"),
        QStringLiteral("img3.png"),
        QStringLiteral("img4.png"),
        QStringLiteral("img5.png"),
    };

    // --- isImagePart ---------------------------------------------------
    StubPart imagePart(nullptr);
    imagePart.setUrlPublic(QUrl::fromLocalFile(dir.absoluteFilePath(imageNames.first())));
    check(ImageBrowser::isImagePart(&imagePart), "isImagePart() detects an image");

    // create a text file to test rejection
    const QString txtFile = dir.absoluteFilePath(QStringLiteral("notanimage.txt"));
    {
        QFile file(txtFile);
        file.open(QIODevice::WriteOnly);
        file.close();
    }
    StubPart textPart(nullptr);
    textPart.setUrlPublic(QUrl::fromLocalFile(txtFile));
    check(!ImageBrowser::isImagePart(&textPart), "isImagePart() rejects a text file");

    check(!ImageBrowser::isImagePart(nullptr), "isImagePart() handles nullptr");

    // --- wheel direction mapping --------------------------------------
    check(!ImageBrowser::isForwardWheelDelta(120), "wheel up (positive delta) selects the previous image");
    check(ImageBrowser::isForwardWheelDelta(-120), "wheel down (negative delta) selects the next image");

    QWidget partWidget;
    QWidget childWidget(&partWidget);
    QWidget unrelatedWidget;
    check(ImageBrowser::isPartWidgetTarget(&partWidget, &partWidget), "wheel on part widget is accepted");
    check(ImageBrowser::isPartWidgetTarget(&childWidget, &partWidget), "wheel on part child widget is accepted");
    check(!ImageBrowser::isPartWidgetTarget(&unrelatedWidget, &partWidget), "wheel on unrelated widget is ignored");
    check(!ImageBrowser::isPartWidgetTarget(nullptr, &partWidget), "wheel with no widget is ignored");

    // --- stepping within one directory --------------------------------
    ImageBrowser browser;
    browser.setPart(&imagePart);

    // current file: img1.png -> forward steps to img2.png
    QUrl next = browser.nextUrl(true);
    check(next.isValid() && next.toLocalFile() == nativeFile(dir, imageNames.at(1)),
          QString("step forward selects %1, got %2").arg(imageNames.at(1)).arg(next.toLocalFile()));
    imagePart.setUrlPublic(next);

    // forward again -> img3.png
    next = browser.nextUrl(true);
    check(next.isValid() && next.toLocalFile() == nativeFile(dir, imageNames.at(2)),
          QString("step forward selects %1, got %2").arg(imageNames.at(2)).arg(next.toLocalFile()));
    imagePart.setUrlPublic(next);

    // forward repeatedly until the last one (img5.png), then wrap to img1.png
    next = browser.nextUrl(true); // img4.png
    imagePart.setUrlPublic(next);
    next = browser.nextUrl(true); // img5.png
    imagePart.setUrlPublic(next);
    next = browser.nextUrl(true); // wrap to img1.png
    check(next.isValid() && next.toLocalFile() == nativeFile(dir, imageNames.first()),
          QString("step forward wraps to %1, got %2").arg(imageNames.first()).arg(next.toLocalFile()));
    imagePart.setUrlPublic(next);

    // backward from img1.png wraps to img5.png
    QUrl prev = browser.nextUrl(false);
    check(prev.isValid() && prev.toLocalFile() == nativeFile(dir, imageNames.last()),
          QString("step backward wraps to %1, got %2").arg(imageNames.last()).arg(prev.toLocalFile()));
    imagePart.setUrlPublic(prev);

    // backward from img5.png -> img4.png
    prev = browser.nextUrl(false);
    check(prev.isValid() && prev.toLocalFile() == nativeFile(dir, imageNames.at(3)),
          QString("step backward selects %1, got %2").arg(imageNames.at(3)).arg(prev.toLocalFile()));
    imagePart.setUrlPublic(prev);

    // backward from img4.png -> img3.png
    prev = browser.nextUrl(false);
    check(prev.isValid() && prev.toLocalFile() == nativeFile(dir, imageNames.at(2)),
          QString("step backward selects %1, got %2").arg(imageNames.at(2)).arg(prev.toLocalFile()));
    imagePart.setUrlPublic(prev);

    // backward from img3.png -> img2.png
    prev = browser.nextUrl(false);
    check(prev.isValid() && prev.toLocalFile() == nativeFile(dir, imageNames.at(1)),
          QString("step backward selects %1, got %2").arg(imageNames.at(1)).arg(prev.toLocalFile()));
    imagePart.setUrlPublic(prev);

    // backward from img2.png -> img1.png
    prev = browser.nextUrl(false);
    check(prev.isValid() && prev.toLocalFile() == nativeFile(dir, imageNames.first()),
          QString("step backward selects %1, got %2").arg(imageNames.first()).arg(prev.toLocalFile()));

    // the text file must not be part of the list: stepping from img1.png forward -> img2.png
    imagePart.setUrlPublic(QUrl::fromLocalFile(dir.absoluteFilePath(imageNames.first())));
    browser.setPart(&imagePart); // refresh list
    next = browser.nextUrl(true);
    check(next.isValid() && next.toLocalFile() == nativeFile(dir, imageNames.at(1)),
          QString("non-image files are skipped, got %1").arg(next.toLocalFile()));

    // --- canStep -------------------------------------------------------
    imagePart.setUrlPublic(QUrl::fromLocalFile(dir.absoluteFilePath(imageNames.first())));
    browser.setPart(&imagePart);
    check(browser.canStep(true), "canStep(true) true when more images follow");
    check(browser.canStep(false), "canStep(false) true when previous images exist");

    imagePart.setUrlPublic(QUrl::fromLocalFile(dir.absoluteFilePath(imageNames.last())));
    browser.setPart(&imagePart);
    check(browser.canStep(true), "canStep(true) true at the last image (wraps)");
    check(browser.canStep(false), "canStep(false) true at the last image");

    // --- a directory with a single image must not be stepped -----------
    const QString singleDir = dir.absoluteFilePath(QStringLiteral("single"));
    QDir(singleDir).mkpath(".");
    {
        QFile file(QDir(singleDir).absoluteFilePath(QStringLiteral("only.png")));
        file.open(QIODevice::WriteOnly);
        file.close();
    }
    StubPart singlePart(nullptr);
    singlePart.setUrlPublic(QUrl::fromLocalFile(QDir(singleDir).absoluteFilePath(QStringLiteral("only.png"))));
    ImageBrowser singleBrowser;
    singleBrowser.setPart(&singlePart);
    check(!singleBrowser.canStep(true), "single image dir: canStep(true) is false");
    check(!singleBrowser.canStep(false), "single image dir: canStep(false) is false");
    QUrl singleNext = singleBrowser.nextUrl(true);
    check(!singleNext.isValid(), "single image dir: nextUrl() returns invalid URL");

    QDir(singleDir).removeRecursively();

    QTextStream out(stdout);
    out << (failures == 0 ? "RESULT: PASS" : "RESULT: FAIL") << Qt::endl;
    return failures == 0 ? 0 : 1;
}
