#include "EditorWindow.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QListWidget>
#include <QLineEdit>
#include <QLayout>
#include <QMessageBox>
#include <QPixmap>
#include <QTemporaryDir>
#include <QTimer>

#include <cstdlib>
#include <iostream>

namespace {
    void require(bool condition, const char* message) {
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(EXIT_FAILURE);
        }
    }

    QByteArray document(const QString& original) {
        return QJsonDocument(QJsonArray{QJsonObject{
            {QStringLiteral("OriginalModel"), original},
            {QStringLiteral("Variants"), QJsonArray{QStringLiteral("Replacement.nif")}},
            {QStringLiteral("LocationID"), QStringLiteral("Example.esp|123")},
            {QStringLiteral("RegionID"), QJsonArray{4660, QStringLiteral("Example.esp|456")}},
            {QStringLiteral("KeywordID"), QStringLiteral("01001234")},
            {QStringLiteral("CellID"), QJsonArray{}},
            {QStringLiteral("WorldspaceID"), 4660},
            {QStringLiteral("ExcludeLocationID"), QJsonArray{}},
            {QStringLiteral("ExcludeRegionID"), QStringLiteral("Example.esp|789")},
            {QStringLiteral("ExcludeKeywordID"), QJsonArray{QStringLiteral("Example.esp|ABC")}},
            {QStringLiteral("ExcludeCellID"), QStringLiteral("0x01001234")},
            {QStringLiteral("ExcludeWorldspaceID"), QJsonArray{4660}},
            {QStringLiteral("Extension"), QJsonObject{{QStringLiteral("unchanged"), true}}}
        }}).toJson();
    }

    void writeFile(const QString& path, const QByteArray& contents) {
        QFile file(path);
        require(file.open(QIODevice::WriteOnly | QIODevice::Truncate), "open fixture for writing");
        require(file.write(contents) == contents.size(), "write complete fixture");
    }

    QByteArray readFile(const QString& path) {
        QFile file(path);
        require(file.open(QIODevice::ReadOnly), "open fixture for reading");
        return file.readAll();
    }

    void answerNextDialog(QMessageBox::StandardButton response) {
        QTimer::singleShot(0, [response] {
            auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            require(dialog != nullptr, "expected confirmation or error dialog");
            auto* button = dialog->button(response);
            require(button != nullptr, "expected dialog response is available");
            button->click();
        });
    }

    void editCondition(EditorWindow& window, const QString& fieldName, const QString& id) {
        for (auto* included : window.findChildren<QCheckBox*>()) {
            if (included->text() != fieldName || !included->isChecked()) continue;
            for (auto* text : included->parentWidget()->findChildren<QLineEdit*>()) {
                if (text->accessibleName() == QStringLiteral("Form ID")) {
                    text->setText(id);
                    return;
                }
            }
        }
        require(false, "find dedicated condition input in the active entry form");
    }

    void renderPreview(const QString& screenshotPath);
}

int main(int argc, char* argv[]) {
    QApplication application(argc, argv);
    QTimer::singleShot(30000, [] {
        std::cerr << "FAIL: window interaction tests timed out\n";
        std::exit(EXIT_FAILURE);
    });

    const auto screenshotPath = qEnvironmentVariable("MS_EDITOR_TEST_SCREENSHOT");
    if (!screenshotPath.isEmpty()) {
        renderPreview(screenshotPath);
        std::cout << "Editor preview saved.\n";
        return EXIT_SUCCESS;
    }

    QTemporaryDir temporary;
    require(temporary.isValid(), "create temporary Data folder");
    const auto firstPath = QDir(temporary.path()).filePath(QStringLiteral("A_MS.json"));
    const auto secondPath = QDir(temporary.path()).filePath(QStringLiteral("B_MS.json"));
    writeFile(firstPath, document(QStringLiteral("First.nif")));
    writeFile(secondPath, document(QStringLiteral("Second.nif")));

    EditorWindow window(temporary.path());
    auto* files = window.findChild<QListWidget*>(QStringLiteral("configurationFiles"));
    auto* entries = window.findChild<QListWidget*>(QStringLiteral("swapEntries"));
    auto* duplicate = window.findChild<QAction*>(QStringLiteral("duplicateEntry"));
    auto* save = window.findChild<QAction*>(QStringLiteral("saveConfiguration"));
    require(files && entries && duplicate && save, "find window controls");
    require(files->count() == 2 && files->currentRow() == 0, "discover and select first configuration");
    require(entries->count() == 1 && !save->isEnabled(), "load original entry without dirtying document");

    duplicate->trigger();
    require(entries->count() == 2 && save->isEnabled(), "duplicate entry marks document dirty");
    answerNextDialog(QMessageBox::Cancel);
    files->setCurrentRow(1);
    require(files->currentRow() == 0, "cancelled file switch restores file selection");
    require(entries->count() == 2 && save->isEnabled(), "cancelled file switch preserves edited entries");

    answerNextDialog(QMessageBox::Discard);
    files->setCurrentRow(1);
    require(files->currentRow() == 1 && entries->count() == 1 && !save->isEnabled(),
            "discard switches to next configuration");
    require(QJsonDocument::fromJson(readFile(firstPath)).array().size() == 1,
            "discard leaves original file unchanged");

    files->setCurrentRow(0);
    editCondition(window, QStringLiteral("LocationID"), QStringLiteral("234"));
    require(save->isEnabled(), "editing a condition marks the document dirty");
    duplicate->trigger();
    answerNextDialog(QMessageBox::Save);
    files->setCurrentRow(1);
    require(files->currentRow() == 1 && !save->isEnabled(), "save switches to requested configuration");
    const auto savedEntries = QJsonDocument::fromJson(readFile(firstPath)).array();
    auto expectedEntry = QJsonDocument::fromJson(document(QStringLiteral("First.nif"))).array().first().toObject();
    expectedEntry.insert(QStringLiteral("LocationID"), QStringLiteral("Example.esp|234"));
    require(savedEntries == QJsonArray{expectedEntry, expectedEntry},
            "save and duplicate preserve edited conditions, all ID forms and unknown properties");

    files->setCurrentRow(0);
    duplicate->trigger();
    answerNextDialog(QMessageBox::Cancel);
    require(!window.close(), "cancel rejects window close");
    require(entries->count() == 3 && save->isEnabled(), "cancelled close preserves unsaved entries");

    const auto externallyChanged = document(QStringLiteral("ChangedElsewhere.nif"));
    writeFile(firstPath, externallyChanged);
    answerNextDialog(QMessageBox::Ok);
    save->trigger();
    require(save->isEnabled() && entries->count() == 3, "save conflict retains edited document");
    require(readFile(firstPath) == externallyChanged, "save conflict preserves external file changes");

    answerNextDialog(QMessageBox::Discard);
    require(window.close(), "discard permits window close");
    std::cout << "Editor window interaction tests passed.\n";
    return EXIT_SUCCESS;
}

namespace {
    void renderPreview(const QString& screenshotPath) {
        QTemporaryDir previewDirectory;
        require(previewDirectory.isValid(), "create screenshot fixture directory");
        for (const auto& name : {QStringLiteral("Skyrim.esm"), QStringLiteral("Example.esp"), QStringLiteral("Example.esl")}) {
            writeFile(QDir(previewDirectory.path()).filePath(name), QByteArray());
        }
        const QJsonObject previewEntry{
            {QStringLiteral("OriginalModel"), QJsonArray{QStringLiteral("Architecture\\Farmhouse.nif"),
                                                       QStringLiteral("Architecture\\FarmhouseSnow.nif")}},
            {QStringLiteral("Variants"), QJsonArray{QStringLiteral("ModelSwapper\\FarmhouseSummer.nif"),
                                                  QStringLiteral("ModelSwapper\\FarmhouseAutumn.nif")}},
            {QStringLiteral("BaseFormID"), QJsonArray{QStringLiteral("Example.esp|001234"), QStringLiteral("Skyrim.esm|001235")}},
            {QStringLiteral("ReferenceID"), QStringLiteral("Example.esp|005678")},
            {QStringLiteral("ExcludeBaseFormID"), QStringLiteral("Example.esp|001235")},
            {QStringLiteral("ExcludeReferenceID"), QJsonArray{QStringLiteral("Example.esp|005679")}},
            {QStringLiteral("LocationID"), QStringLiteral("Skyrim.esm|00018A45")},
            {QStringLiteral("ExcludeCellID"), QJsonArray{QStringLiteral("Example.esp|001ABC")}},
            {QStringLiteral("ActiveTimeSpan"), QStringLiteral("01-06~30-09")}
        };
        writeFile(QDir(previewDirectory.path()).filePath(QStringLiteral("SeasonalSwaps_MS.json")),
                  QJsonDocument(QJsonArray{previewEntry}).toJson());
        EditorWindow preview(previewDirectory.path());
        preview.resize(1500, 1100);
        preview.setAttribute(Qt::WA_DontShowOnScreen);
        preview.show();
        preview.ensurePolished();
        QApplication::processEvents();
        if (preview.layout()) {
            preview.layout()->activate();
        }
        if (preview.centralWidget() && preview.centralWidget()->layout()) {
            preview.centralWidget()->layout()->activate();
        }
        QPixmap screenshot(preview.size());
        screenshot.fill(Qt::white);
        preview.render(&screenshot);
        require(screenshot.save(screenshotPath), "save editor preview image");
    }
}
