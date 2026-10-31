#include "EditorWindow.h"

#include <QApplication>
#include <QCursor>
#include <QDir>
#include <QFile>
#include <QImage>
#include <QImageReader>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QPixmap>
#include <QSplitter>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdio>
#include <cstdlib>
#include <iostream>

#ifdef Q_OS_WIN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#ifdef _MSC_VER
#include <crtdbg.h>
#endif
#endif

namespace {
    void require(bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    void reportQtMessage(QtMsgType type, const QMessageLogContext&, const QString& message) {
        const auto text = message.toLocal8Bit();
        std::fprintf(stderr, "Qt[%d]: %s\n", static_cast<int>(type), text.constData());
        std::fflush(stderr);
        // A native-platform assertion must fail the test, never open the CRT's
        // interactive Abort/Retry/Ignore dialog and stall unattended tests.
        if (type == QtFatalMsg) std::_Exit(EXIT_FAILURE);
    }

    void configureNoninteractiveFailures() {
#ifdef Q_OS_WIN
        SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
#ifdef _MSC_VER
        _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#ifdef _DEBUG
        for (int kind : {_CRT_WARN, _CRT_ERROR, _CRT_ASSERT}) {
            _CrtSetReportMode(kind, _CRTDBG_MODE_FILE);
            _CrtSetReportFile(kind, _CRTDBG_FILE_STDERR);
        }
#endif
#endif
#endif
        qInstallMessageHandler(reportQtMessage);
    }

    void verifyNativeCursorImages() {
        require(QImageReader::supportedImageFormats().contains("png"),
                "Qt must include its PNG decoder; the Windows splitter cursors require it");
        for (const auto* shape : {"splithcursor", "splitvcursor"}) {
            for (int size : {32, 48, 64}) {
                const auto resource = QStringLiteral(":/qt-project.org/windows/cursors/images/%1_%2.png").arg(shape).arg(size);
                require(QFile::exists(resource), "The native Windows cursor PNG resource must be linked into the executable");
                const QImage image(resource);
                if (image.isNull()) std::cerr << "Cannot decode cursor resource: " << resource.toStdString() << '\n';
                require(!image.isNull() && image.size() == QSize(size, size),
                        "Every embedded horizontal and vertical splitter cursor must decode at its expected size");
            }
        }
    }

    void verifyWindowStartup() {
        QTemporaryDir data;
        require(data.isValid(), "Create the native startup test's isolated Data directory");
        QFile configuration(QDir(data.path()).filePath(QStringLiteral("NativeStartup_MS.json")));
        require(configuration.open(QIODevice::WriteOnly | QIODevice::NewOnly), "Create the native startup configuration fixture");
        const auto bytes = QJsonDocument(QJsonArray{QJsonObject{
            {QStringLiteral("OriginalModel"), QJsonArray{QStringLiteral("Original.nif"), QStringLiteral("Other.nif")}},
            {QStringLiteral("Variants"), QJsonArray{QStringLiteral("Replacement.nif")}},
            {QStringLiteral("LocationID"), QStringLiteral("Example.esp|123")},
            {QStringLiteral("ExcludeRegionID"), QJsonArray{QStringLiteral("Example.esp|456")}}
        }}).toJson();
        require(configuration.write(bytes) == bytes.size(), "Write the complete native startup configuration fixture");
        configuration.close();

        EditorWindow window(data.path());
        // This test deliberately creates an actual Windows platform window.
        // WA_DontShowOnScreen/minimal rendering does not exercise native cursors.
        window.show();
        window.ensurePolished();
        require(window.winId() != 0, "The editor must create its native window");
        QApplication::processEvents();
        require(QApplication::activeModalWidget() == nullptr, "Valid configuration startup must not open an error dialog");
        require(!window.findChildren<QSplitter*>().isEmpty(), "The production editor must create its splitter layout");
        for (const auto shape : {Qt::SplitHCursor, Qt::SplitVCursor}) {
            QApplication::setOverrideCursor(QCursor(shape));
            QApplication::processEvents();
            QApplication::restoreOverrideCursor();
        }
        QPixmap rendered(window.size());
        require(!rendered.isNull(), "Allocate the native editor render surface");
        rendered.fill(Qt::transparent);
        window.render(&rendered);
        require(!rendered.toImage().isNull(), "The native editor must render after both splitter cursors are exercised");
        window.close();
    }
}

int main(int argc, char** argv) {
    configureNoninteractiveFailures();
    QApplication application(argc, argv);
    QApplication::setStyle(QStringLiteral("Fusion"));
    require(QGuiApplication::platformName() == QStringLiteral("windows"), "Run native startup tests with -platform windows");
    std::cout << "Native startup: Qt " << qVersion() << ", Windows platform, "
#ifdef QT_DEBUG
              << "Debug Qt assertions enabled\n";
#else
              << "Release configuration\n";
#endif
    QTimer::singleShot(10000, [] {
        std::cerr << "FAIL: native startup test timed out\n";
        std::_Exit(EXIT_FAILURE);
    });
    verifyNativeCursorImages();
    verifyWindowStartup();
    std::cout << "Native Windows startup and splitter cursors passed\n";
    return EXIT_SUCCESS;
}
