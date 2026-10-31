#include "EditorWindow.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QCoreApplication>
#include <QDir>

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    // Fusion draws its own checkbox and dropdown indicators in static builds.
    QApplication::setStyle(QStringLiteral("Fusion"));
    QCoreApplication::setApplicationName(QStringLiteral("ModelSwapperEditor"));
    QCoreApplication::setApplicationVersion(QStringLiteral("0.3.0"));
    QCoreApplication::setOrganizationName(QStringLiteral("Model Swapper"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Edit Model Swapper *_MS.json configurations in Skyrim's Data folder."));
    parser.addHelpOption();
    parser.addVersionOption();
    QCommandLineOption dataOption(QStringLiteral("data-dir"), QStringLiteral("Open the specified Data folder."),
                                 QStringLiteral("directory"));
    parser.addOption(dataOption);
    parser.addPositionalArgument(QStringLiteral("directory"), QStringLiteral("Data folder (defaults to the executable's folder)."),
                                 QStringLiteral("[directory]"));
    parser.process(application);
    const auto positional = parser.positionalArguments();
    if (positional.size() > 1) {
        parser.showHelp(1);
    }
    QString dataDirectory = QCoreApplication::applicationDirPath();
    if (parser.isSet(dataOption)) {
        dataDirectory = parser.value(dataOption);
    } else if (!positional.isEmpty()) {
        dataDirectory = positional.first();
    }
    EditorWindow window(QDir(dataDirectory).absolutePath());
    window.show();
    return application.exec();
}
