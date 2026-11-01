#include "EditorWindow.h"

#include <QAbstractButton>
#include <QAction>
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QListWidget>
#include <QLineEdit>
#include <QLayout>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QPixmap>
#include <QStatusBar>
#include <QProcess>
#include <QPushButton>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTimer>
#include <QToolBar>

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

    void writeFromExternalProcess(const QString& path, const QByteArray& contents) {
        QProcess writer;
        writer.start(QCoreApplication::applicationFilePath(),
                     {QStringLiteral("--write-external-fixture"), path, QString::fromLatin1(contents.toBase64())});
        if (!writer.waitForFinished(10000)) {
            writer.kill();
            writer.waitForFinished(1000);
            require(false, "external fixture writer must finish");
        }
        require(writer.exitStatus() == QProcess::NormalExit && writer.exitCode() == EXIT_SUCCESS,
                "a separate process must create or rewrite the rescan fixture");
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

    void saveWithoutDialog(QAction* save) {
        QTimer unexpectedDialog;
        QObject::connect(&unexpectedDialog, &QTimer::timeout, [] {
            require(QApplication::activeModalWidget() == nullptr, "saving an unchanged source must not show a dialog");
        });
        unexpectedDialog.start(1);
        save->trigger();
        QApplication::processEvents();
        require(QApplication::activeModalWidget() == nullptr, "successful save leaves no modal dialog");
    }

    QTabWidget* selectSettingsTab(EditorWindow& window, const QString& label) {
        auto* tabs = window.findChild<QTabWidget*>(QStringLiteral("entryTabs"));
        require(tabs != nullptr, "find entry settings tabs");
        for (int index = 0; index < tabs->count(); ++index) {
            if (tabs->tabText(index) == label) {
                tabs->setCurrentIndex(index);
                return tabs;
            }
        }
        require(false, "find requested entry settings tab");
        return nullptr;
    }

    QLineEdit* conditionInput(EditorWindow& window, const QString& fieldName) {
        selectSettingsTab(window, QStringLiteral("Inclusions"));
        for (auto* included : window.findChildren<QCheckBox*>()) {
            if (included->text() != fieldName || !included->isChecked()) continue;
            for (auto* text : included->parentWidget()->findChildren<QLineEdit*>()) {
                if (text->accessibleName() == QStringLiteral("Form ID")) {
                    return text;
                }
            }
        }
        require(false, "find dedicated condition input in the active entry form");
        return nullptr;
    }

    void editCondition(EditorWindow& window, const QString& fieldName, const QString& id) {
        conditionInput(window, fieldName)->setText(id);
    }

    void answerRenameDialog(const QString& currentName, const QString& replacement, bool accept = true,
                            QMessageBox::StandardButton nextResponse = QMessageBox::NoButton) {
        QTimer::singleShot(0, [currentName, replacement, accept, nextResponse] {
            auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
            require(dialog != nullptr, "expected rename filename dialog");
            require(dialog->textValue() == currentName, "rename starts with the current filename");
            if (nextResponse != QMessageBox::NoButton) answerNextDialog(nextResponse);
            dialog->setTextValue(replacement);
            if (accept) dialog->accept();
            else dialog->reject();
        });
    }

    void confirmThenRename(QMessageBox::StandardButton response, const QString& currentName,
                           const QString& replacement) {
        QTimer::singleShot(0, [response, currentName, replacement] {
            auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            require(dialog != nullptr, "unsaved changes are confirmed before asking for a rename");
            auto* button = dialog->button(response);
            require(button != nullptr, "expected unsaved-change response is available");
            answerRenameDialog(currentName, replacement);
            button->click();
        });
    }

    void answerNewFilenameDialog(const QString& name, bool accept = true,
                                 QMessageBox::StandardButton nextResponse = QMessageBox::NoButton) {
        QTimer::singleShot(0, [name, accept, nextResponse] {
            auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
            require(dialog != nullptr, "New asks for a filename without a folder picker");
            require(dialog->textValue() == QStringLiteral("New_MS.json"), "New proposes the default configuration filename");
            dialog->setTextValue(name);
            if (nextResponse != QMessageBox::NoButton) answerNextDialog(nextResponse);
            if (accept) dialog->accept();
            else dialog->reject();
        });
    }

    void answerDeleteDialog(const QString& name, bool dirty, QMessageBox::StandardButton response,
                            QMessageBox::StandardButton nextResponse = QMessageBox::NoButton) {
        QTimer::singleShot(0, [name, dirty, response, nextResponse] {
            auto* dialog = qobject_cast<QMessageBox*>(QApplication::activeModalWidget());
            require(dialog && dialog->windowTitle() == QStringLiteral("Delete configuration"),
                    "Delete asks for confirmation before removing the configuration");
            require(dialog->standardButtons() == (QMessageBox::Yes | QMessageBox::No) &&
                        dialog->defaultButton() == dialog->button(QMessageBox::No),
                    "permanent deletion requires an explicit Yes and defaults to No");
            const auto message = dialog->text() + dialog->informativeText();
            require(message.contains(name) && message.contains(QStringLiteral("permanent"), Qt::CaseInsensitive),
                    "delete confirmation names the selected file and explains permanent removal");
            if (dirty) {
                require(message.contains(QStringLiteral("unsaved"), Qt::CaseInsensitive) &&
                            message.contains(QStringLiteral("discard"), Qt::CaseInsensitive),
                        "dirty deletion explains that pending changes will be discarded");
            }
            if (nextResponse != QMessageBox::NoButton) answerNextDialog(nextResponse);
            dialog->button(response)->click();
        });
    }

    void renderPreview(const QString& screenshotPath);
    void testFileActions();
    void testNewConfigurations();
    void testDeleteConfigurations();
    void testEntryLabels();
    void testEntryTitles();
    void testRescan();
}

int main(int argc, char* argv[]) {
    if (argc == 4 && QByteArray(argv[1]) == "--write-external-fixture") {
        QCoreApplication writer(argc, argv);
        const auto arguments = writer.arguments();
        writeFile(arguments.at(2), QByteArray::fromBase64(arguments.at(3).toLatin1()));
        return EXIT_SUCCESS;
    }
    QCoreApplication::setAttribute(Qt::AA_DontUseNativeDialogs);
    QApplication application(argc, argv);
    application.setQuitOnLastWindowClosed(false);
    QApplication::setStyle(QStringLiteral("Fusion"));
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
    auto firstDocument = document(QStringLiteral("First.nif"));
    firstDocument.replace("\n", "\r\n");
    writeFile(firstPath, firstDocument);
    writeFile(secondPath, document(QStringLiteral("Second.nif")));

    EditorWindow window(temporary.path());
    window.setAttribute(Qt::WA_DontShowOnScreen);
    window.show();
    window.ensurePolished();
    QApplication::processEvents();
    auto* files = window.findChild<QListWidget*>(QStringLiteral("configurationFiles"));
    auto* entries = window.findChild<QListWidget*>(QStringLiteral("swapEntries"));
    auto* create = window.findChild<QAction*>(QStringLiteral("newConfiguration"));
    auto* rename = window.findChild<QAction*>(QStringLiteral("renameConfiguration"));
    auto* rescan = window.findChild<QAction*>(QStringLiteral("rescanConfigurations"));
    auto* deleteFile = window.findChild<QAction*>(QStringLiteral("deleteConfiguration"));
    auto* add = window.findChild<QAction*>(QStringLiteral("addEntry"));
    auto* duplicate = window.findChild<QAction*>(QStringLiteral("duplicateEntry"));
    auto* remove = window.findChild<QAction*>(QStringLiteral("removeEntry"));
    auto* save = window.findChild<QAction*>(QStringLiteral("saveConfiguration"));
    require(files && entries && create && rename && rescan && deleteFile && add && duplicate && remove && save,
            "find window controls");
    require(!window.findChild<QAction*>(QStringLiteral("chooseDataDirectory")),
            "the editor keeps its startup Data folder without a folder-switch action");
    auto* fileActions = window.findChild<QToolBar*>(QStringLiteral("fileActions"));
    auto* entryActions = window.findChild<QToolBar*>(QStringLiteral("entryActions"));
    auto* documentActions = window.findChild<QToolBar*>(QStringLiteral("documentActions"));
    require(fileActions && entryActions && documentActions, "find local action bars");
    require(fileActions->actions() == QList<QAction*>{create, rename, rescan, deleteFile},
            "configuration file actions stay together above their file list");
    require(entryActions->actions() == QList<QAction*>{add, duplicate, remove},
            "entry actions stay together above their entry list");
    require(documentActions->actions() == QList<QAction*>{save},
            "Save is presented with the current document settings");
    require(fileActions->parentWidget() == files->parentWidget() &&
                entryActions->parentWidget() == entries->parentWidget(),
            "file and entry action bars belong to their respective panels");
    require(window.findChildren<QToolBar*>().size() == 3 &&
                window.findChildren<QToolBar*>(QString(), Qt::FindDirectChildrenOnly).isEmpty(),
            "local action bars replace the mixed global toolbar");
    auto* tabs = selectSettingsTab(window, QStringLiteral("Basics"));
    require(documentActions->parentWidget()->isAncestorOf(tabs),
            "Save and entry settings belong to the same document panel");
    require(tabs->count() == 4 && tabs->tabText(0) == QStringLiteral("Basics") &&
                tabs->tabText(1) == QStringLiteral("Targets") && tabs->tabText(2) == QStringLiteral("Inclusions") &&
                tabs->tabText(3) == QStringLiteral("Exclusions"),
            "entry settings expose only the four supported-field tabs");
    for (auto* button : window.findChildren<QAbstractButton*>()) {
        require(button->text() != QStringLiteral("Add extra property"),
                "entry settings do not expose an extra-property creation control");
    }
    for (auto* included : window.findChildren<QCheckBox*>()) {
        require(included->text() != QStringLiteral("Extension"),
                "unknown entry properties are preserved without editing controls");
    }
    auto* addButton = entryActions->widgetForAction(add);
    require(addButton && addButton->mapTo(&window, QPoint(0, addButton->height())).y() <=
                entries->mapTo(&window, QPoint()).y(),
            "Add entry is positioned above the entries it creates");
    require(fileActions->mapTo(&window, QPoint(0, fileActions->height())).y() <=
                files->mapTo(&window, QPoint()).y(),
            "configuration actions are positioned above the file list");
    require(documentActions->mapTo(&window, QPoint(0, documentActions->height())).y() <=
                tabs->mapTo(&window, QPoint()).y(),
            "Save is positioned above the settings tabs");
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
    auto expectedEntry = QJsonDocument::fromJson(document(QStringLiteral("First.nif"))).array().first().toObject();
    editCondition(window, QStringLiteral("LocationID"), QStringLiteral("234"));
    require(save->isEnabled(), "editing a condition marks the document dirty");
    saveWithoutDialog(save);
    expectedEntry.insert(QStringLiteral("LocationID"), QStringLiteral("Example.esp|234"));
    require(!save->isEnabled() && files->currentRow() == 0 && entries->count() == 1,
            "first direct save clears dirty state without reloading the entry");
    require(QJsonDocument::fromJson(readFile(firstPath)).array() == QJsonArray{expectedEntry},
            "first direct save writes the edited condition from the CRLF source");

    editCondition(window, QStringLiteral("LocationID"), QStringLiteral("345"));
    require(save->isEnabled(), "editing again after saving re-enables Save");
    saveWithoutDialog(save);
    expectedEntry.insert(QStringLiteral("LocationID"), QStringLiteral("Example.esp|345"));
    require(!save->isEnabled() && files->currentRow() == 0 && entries->count() == 1,
            "second direct save clears dirty state without switching or reloading the file");
    require(QJsonDocument::fromJson(readFile(firstPath)).array() == QJsonArray{expectedEntry},
            "second direct save writes the latest edit to the same loaded file");

    editCondition(window, QStringLiteral("LocationID"), QStringLiteral("456"));
    require(save->isEnabled(), "editing after repeated saves marks the document dirty");
    duplicate->trigger();
    answerNextDialog(QMessageBox::Save);
    files->setCurrentRow(1);
    require(files->currentRow() == 1 && !save->isEnabled(), "save switches to requested configuration");
    const auto savedEntries = QJsonDocument::fromJson(readFile(firstPath)).array();
    expectedEntry.insert(QStringLiteral("LocationID"), QStringLiteral("Example.esp|456"));
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
    testFileActions();
    testNewConfigurations();
    testDeleteConfigurations();
    testEntryLabels();
    testEntryTitles();
    testRescan();
    std::cout << "Editor window interaction tests passed.\n";
    return EXIT_SUCCESS;
}

namespace {
    void testRescan() {
        QTemporaryDir temporary;
        require(temporary.isValid(), "create isolated rescan fixtures");
        const QDir data(temporary.path());
        const auto selectedPath = data.filePath(QStringLiteral("A_MS.json"));
        const auto removedPath = data.filePath(QStringLiteral("B_MS.json"));
        const auto addedPath = data.filePath(QStringLiteral("C_MS.json"));
        const auto laterPath = data.filePath(QStringLiteral("D_MS.json"));
        const auto removedPlugin = data.filePath(QStringLiteral("Initial.esp"));
        writeFile(selectedPath, document(QStringLiteral("InitialA.nif")));
        writeFile(removedPath, document(QStringLiteral("InitialB.nif")));
        writeFile(removedPlugin, QByteArray());
        EditorWindow window(temporary.path());
        window.setAttribute(Qt::WA_DontShowOnScreen);
        window.show();
        QApplication::processEvents();
        auto* files = window.findChild<QListWidget*>(QStringLiteral("configurationFiles"));
        auto* entries = window.findChild<QListWidget*>(QStringLiteral("swapEntries"));
        auto* rescan = window.findChild<QAction*>(QStringLiteral("rescanConfigurations"));
        auto* save = window.findChild<QAction*>(QStringLiteral("saveConfiguration"));
        auto* add = window.findChild<QAction*>(QStringLiteral("addEntry"));
        auto* rename = window.findChild<QAction*>(QStringLiteral("renameConfiguration"));
        auto* actions = window.findChild<QToolBar*>(QStringLiteral("fileActions"));
        require(files && entries && rescan && save && add && rename && actions, "find rescan controls");
        auto* rescanButton = qobject_cast<QAbstractButton*>(actions->widgetForAction(rescan));
        require(rescanButton && rescanButton->isEnabled(), "Rescan must have an enabled button in the file toolbar");
        const auto containsFile = [files](const QString& path) {
            for (int row = 0; row < files->count(); ++row) {
                if (files->item(row)->data(Qt::UserRole).toString() == path) return true;
            }
            return false;
        };
        const auto currentFile = [files] {
            return files->currentItem() ? files->currentItem()->data(Qt::UserRole).toString() : QString();
        };
        require(files->count() == 2 && currentFile() == selectedPath && !save->isEnabled(),
                "rescan fixture initially selects the first clean configuration");

        auto refreshed = QJsonDocument::fromJson(document(QStringLiteral("ReloadedA.nif"))).array();
        refreshed.append(QJsonDocument::fromJson(document(QStringLiteral("SecondReloaded.nif"))).array().first());
        writeFromExternalProcess(selectedPath, QJsonDocument(refreshed).toJson());
        rescanButton->click();
        QApplication::processEvents();
        require(currentFile() == selectedPath && entries->count() == 2 && entries->item(0)->text() == "1. ReloadedA.nif" &&
                    entries->item(1)->text() == "2. SecondReloaded.nif" && !save->isEnabled(),
                "clicking the toolbar Rescan button reloads externally edited entries without dirtying the document");
        selectSettingsTab(window, QStringLiteral("Targets"));
        bool refreshedForm = false;
        for (auto* included : window.findChildren<QCheckBox*>()) {
            if (included->text() == QStringLiteral("OriginalModel") && included->isChecked()) {
                auto* input = included->parentWidget()->findChild<QLineEdit*>();
                refreshedForm = input && input->text() == QStringLiteral("ReloadedA.nif");
            }
        }
        require(refreshedForm, "rescan rebuilds the current entry form from the externally changed JSON");
        entries->setCurrentRow(1);
        selectSettingsTab(window, QStringLiteral("Targets"));
        rescan->trigger();
        require(entries->currentRow() == 1 &&
                    window.findChild<QTabWidget*>(QStringLiteral("entryTabs"))->currentIndex() == 1,
                "rescan retains the selected entry and settings tab in the current configuration");
        entries->setCurrentRow(0);
        editCondition(window, QStringLiteral("LocationID"), QStringLiteral("777"));
        saveWithoutDialog(save);
        auto refreshedFirst = refreshed.first().toObject();
        refreshedFirst.insert(QStringLiteral("LocationID"), QStringLiteral("Example.esp|777"));
        refreshed[0] = refreshedFirst;
        require(QJsonDocument::fromJson(readFile(selectedPath)).array() == refreshed && !save->isEnabled(),
                "saving after rescan uses the refreshed source baseline and preserves all reloaded entries");

        writeFromExternalProcess(addedPath, document(QStringLiteral("AddedC.nif")));
        writeFromExternalProcess(data.filePath(QStringLiteral("FreshPlugin.esp")), QByteArray());
        require(QFile::remove(removedPath) && QFile::remove(removedPlugin), "remove external config and plugin fixtures before rescan");
        rescan->trigger();
        require(files->count() == 2 && currentFile() == selectedPath && containsFile(addedPath) && !containsFile(removedPath),
                "rescan updates added and removed configuration filenames while retaining the selected existing file");
        selectSettingsTab(window, QStringLiteral("Inclusions"));
        bool pluginDropdownRefreshed = false;
        for (auto* included : window.findChildren<QCheckBox*>()) {
            if (included->text() != QStringLiteral("LocationID")) continue;
            for (auto* combo : included->parentWidget()->findChildren<QComboBox*>()) {
                if (combo->accessibleName() == QStringLiteral("Plugin filename")) {
                    pluginDropdownRefreshed = combo->findText(QStringLiteral("FreshPlugin.esp")) >= 0 &&
                        combo->findText(QStringLiteral("Initial.esp")) < 0;
                }
            }
        }
        require(pluginDropdownRefreshed, "rescan refreshes installed-plugin dropdowns in the selected document");

        const auto validSource = readFile(selectedPath);
        writeFromExternalProcess(selectedPath, QByteArray("[invalid"));
        answerNextDialog(QMessageBox::Ok);
        rescan->trigger();
        require(currentFile() == selectedPath && entries->count() == 2 && !save->isEnabled() &&
                    window.statusBar()->currentMessage().contains(QStringLiteral("could not reload")),
                "a failed rescan reload preserves the current document and reports failure instead of success");
        writeFromExternalProcess(selectedPath, validSource);
        rescan->trigger();
        require(currentFile() == selectedPath && entries->count() == 2 && !save->isEnabled(),
                "rescan recovers after the external configuration is repaired");

        editCondition(window, QStringLiteral("LocationID"), QStringLiteral("888"));
        writeFromExternalProcess(laterPath, document(QStringLiteral("AddedD.nif")));
        answerNextDialog(QMessageBox::Cancel);
        rescan->trigger();
        require(save->isEnabled() && conditionInput(window, QStringLiteral("LocationID"))->text() == QStringLiteral("888") &&
                    files->count() == 2 && !containsFile(laterPath),
                "cancelling unsaved changes prevents rescan and retains pending edits and the current list");
        answerNextDialog(QMessageBox::Discard);
        rescan->trigger();
        require(!save->isEnabled() && conditionInput(window, QStringLiteral("LocationID"))->text() == QStringLiteral("777") &&
                    files->count() == 3 && containsFile(laterPath),
                "discarding pending edits allows rescan to reload disk contents and discover new files");

        require(QFile::remove(selectedPath), "delete selected configuration externally");
        rescan->trigger();
        require(files->count() == 2 && currentFile() == addedPath && entries->count() == 1 &&
                    entries->item(0)->text() == "1. AddedC.nif" && !save->isEnabled(),
                "rescan replaces a deleted selection with the first remaining configuration");
        require(QFile::remove(addedPath) && QFile::remove(laterPath), "delete all remaining configuration fixtures");
        rescan->trigger();
        require(files->count() == 0 && entries->count() == 0 && currentFile().isEmpty() &&
                    !save->isEnabled() && !add->isEnabled() && !rename->isEnabled(),
                "rescan clears the document and disables file actions when every configuration is removed");
        require(window.close(), "close the clean rescan fixture");
    }

    void testEntryTitles() {
        QTemporaryDir temporary;
        require(temporary.isValid(), "create title interaction fixtures");
        const auto path = temporary.filePath(QStringLiteral("Titles_MS.json"));
        const auto legacy = QJsonDocument::fromJson(document(QStringLiteral("Original.nif"))).array().first().toObject();
        writeFile(path, QJsonDocument(QJsonArray{legacy}).toJson());
        {
            EditorWindow window(temporary.path());
            selectSettingsTab(window, QStringLiteral("Basics"));
            auto* entries = window.findChild<QListWidget*>(QStringLiteral("swapEntries"));
            auto* save = window.findChild<QAction*>(QStringLiteral("saveConfiguration"));
            auto* duplicate = window.findChild<QAction*>(QStringLiteral("duplicateEntry"));
            require(entries && save && duplicate, "find title interaction controls");
            const auto titleEnabled = [&window]() -> QCheckBox* {
                for (auto* check : window.findChildren<QCheckBox*>())
                    if (check->text() == QStringLiteral("Title")) return check;
                require(false, "find optional title control");
                return nullptr;
            };
            const auto titleInput = [&window]() -> QLineEdit* {
                for (auto* input : window.findChildren<QLineEdit*>())
                    if (input->accessibleName() == QStringLiteral("Title")) return input;
                require(false, "find single-line title input");
                return nullptr;
            };
            require(!titleEnabled()->isChecked() && !save->isEnabled(), "legacy entries start without a title or unsaved changes");
            titleEnabled()->setChecked(true);
            titleInput()->setText(QStringLiteral("   "));
            require(entries->item(0)->text() == QStringLiteral("1. Original.nif"), "blank titles retain the target-based label");
            titleInput()->setText(QStringLiteral("Riverwood firewood"));
            require(entries->item(0)->text() == QStringLiteral("1. Riverwood firewood") && save->isEnabled(),
                    "typing a title updates its swap entry name immediately");
            saveWithoutDialog(save);
            auto titled = legacy;
            titled.insert(QStringLiteral("Title"), QStringLiteral("Riverwood firewood"));
            require(QJsonDocument::fromJson(readFile(path)).array() == QJsonArray{titled},
                    "saving the title adds only its metadata while preserving the existing swap settings");
            duplicate->trigger();
            require(entries->count() == 2 && entries->item(1)->text() == QStringLiteral("2. Riverwood firewood"),
                    "duplicating a swap retains its title");
            titleInput()->setText(QStringLiteral("Whiterun firewood"));
            saveWithoutDialog(save);
            entries->setCurrentRow(0);
            titleEnabled()->setChecked(false);
            require(entries->item(0)->text() == QStringLiteral("1. Original.nif") &&
                        entries->item(1)->text() == QStringLiteral("2. Whiterun firewood"),
                    "disabling a title restores its target fallback without changing another entry's title");
            saveWithoutDialog(save);
            auto second = legacy;
            second.insert(QStringLiteral("Title"), QStringLiteral("Whiterun firewood"));
            require(QJsonDocument::fromJson(readFile(path)).array() == QJsonArray{legacy, second},
                    "each entry saves its own title and disabled titles stay omitted");
            require(window.close(), "close the saved title fixture");
        }
        EditorWindow reopened(temporary.path());
        const auto* entries = reopened.findChild<QListWidget*>(QStringLiteral("swapEntries"));
        require(entries && entries->count() == 2 && entries->item(0)->text() == QStringLiteral("1. Original.nif") &&
                    entries->item(1)->text() == QStringLiteral("2. Whiterun firewood"),
                "saved entry titles and legacy fallbacks are restored when reopening the configuration");
        require(reopened.close(), "close the reloaded title fixture");
    }

    void testEntryLabels() {
        QTemporaryDir temporary;
        require(temporary.isValid(), "create entry-label fixtures");
        const auto path = QDir(temporary.path()).filePath(QStringLiteral("Targets_MS.json"));
        QJsonArray targets{
            QJsonObject{{"OriginalModel", "Original.nif"},
                        {"BaseFormID", QJsonArray{"Example.esp|001234", "Example.esp|001235"}}, {"ReferenceID", 4660}},
            QJsonObject{{"OriginalModel", QJsonArray{"", "Second.nif"}}, {"BaseFormID", "Example.esp|001234"}},
            QJsonObject{{"BaseFormID", QJsonArray{"Example.esp|001235", "Example.esp|001236"}}, {"ReferenceID", 4660}},
            QJsonObject{{"OriginalModel", ""}, {"BaseFormID", QJsonArray{}},
                        {"ReferenceID", QJsonArray{" ", "Example.esp|001237"}}},
            QJsonObject{{"BaseFormID", 4294967295.0}},
            QJsonObject{},
            QJsonObject{{"Title", "Named swap"}, {"OriginalModel", "Original.nif"}},
            QJsonObject{{"Title", "   "}, {"OriginalModel", "Original.nif"}},
            QJsonObject{{"Title", 123}, {"OriginalModel", "Original.nif"}}
        };
        for (qsizetype index = 0; index < targets.size(); ++index) {
            auto entry = targets.at(index).toObject();
            entry.insert(QStringLiteral("Variants"), QJsonArray{QStringLiteral("Replacement.nif")});
            targets.replace(index, entry);
        }
        const auto source = QJsonDocument(targets).toJson();
        writeFile(path, source);
        EditorWindow window(temporary.path());
        auto* entries = window.findChild<QListWidget*>(QStringLiteral("swapEntries"));
        auto* save = window.findChild<QAction*>(QStringLiteral("saveConfiguration"));
        require(entries && save && entries->count() == targets.size(), "load all entry-label fixtures");
        const QStringList descriptions{"Original.nif", "Second.nif", "Example.esp|001235",
                                       "Example.esp|001237", "4294967295", "New swap",
                                       "Named swap", "Original.nif", "Original.nif"};
        for (int row = 0; row < entries->count(); ++row) {
            require(entries->item(row)->text() == QStringLiteral("%1. %2").arg(row + 1).arg(descriptions.at(row)),
                    "entry names use the first populated enabled target, including scalar and list forms");
        }
        require(!save->isEnabled() && readFile(path) == source, "generating entry labels leaves JSON and dirty state unchanged");
        selectSettingsTab(window, QStringLiteral("Targets"));
        const auto enabled = [&window](const QString& name) -> QCheckBox* {
            for (auto* box : window.findChildren<QCheckBox*>()) {
                if (box->text() == name) return box;
            }
            require(false, "find target enable checkbox");
            return nullptr;
        };
        enabled(QStringLiteral("OriginalModel"))->setChecked(false);
        require(entries->item(0)->text() == "1. Example.esp|001234", "disabling OriginalModel falls back to the base target list");
        enabled(QStringLiteral("BaseFormID"))->setChecked(false);
        require(entries->item(0)->text() == "1. 4660", "disabling the base target falls back to the reference target");
        enabled(QStringLiteral("ReferenceID"))->setChecked(false);
        require(entries->item(0)->text() == "1. New swap", "disabled retained target values cannot name an entry");
        enabled(QStringLiteral("BaseFormID"))->setChecked(true);
        bool edited = false;
        for (auto* input : enabled(QStringLiteral("BaseFormID"))->parentWidget()->findChildren<QLineEdit*>()) {
            if (input->accessibleName() == QStringLiteral("Form ID")) {
                input->setText(QStringLiteral("001238"));
                edited = true;
                break;
            }
        }
        require(edited && entries->item(0)->text() == "1. Example.esp|001238", "editing a target list value updates the entry name immediately");
        enabled(QStringLiteral("OriginalModel"))->setChecked(true);
        require(entries->item(0)->text() == "1. Original.nif", "re-enabling a higher target restores its label priority");
        require(save->isEnabled() && readFile(path) == source, "target edits remain pending until Save");
        answerNextDialog(QMessageBox::Discard);
        require(window.close(), "discard entry-label fixture edits");
    }

    void testFileActions() {
        QTemporaryDir emptyDirectory;
        require(emptyDirectory.isValid(), "create empty configuration directory");
        EditorWindow emptyWindow(emptyDirectory.path());
        auto* emptyRename = emptyWindow.findChild<QAction*>(QStringLiteral("renameConfiguration"));
        auto* emptyDelete = emptyWindow.findChild<QAction*>(QStringLiteral("deleteConfiguration"));
        require(emptyRename && !emptyRename->isEnabled() && emptyRename->shortcut() == QKeySequence(Qt::Key_F2),
                "Rename has F2 shortcut and is disabled without a selected file");
        require(emptyDelete && !emptyDelete->isEnabled(), "Delete is disabled without a selected configuration");
        require(emptyWindow.close(), "close empty editor without a prompt");

        QTemporaryDir temporary;
        require(temporary.isValid(), "create file-action fixtures");
        auto currentPath = QDir(temporary.path()).filePath(QStringLiteral("A_Source_MS.json"));
        const auto collisionPath = QDir(temporary.path()).filePath(QStringLiteral("Z_Existing_MS.json"));
        const auto source = document(QStringLiteral("RenameSource.nif"));
        const auto collisionSource = document(QStringLiteral("KeepExisting.nif"));
        writeFile(currentPath, source);
        writeFile(collisionPath, collisionSource);
        EditorWindow window(temporary.path());
        auto* files = window.findChild<QListWidget*>(QStringLiteral("configurationFiles"));
        auto* entries = window.findChild<QListWidget*>(QStringLiteral("swapEntries"));
        auto* create = window.findChild<QAction*>(QStringLiteral("newConfiguration"));
        auto* rename = window.findChild<QAction*>(QStringLiteral("renameConfiguration"));
        auto* add = window.findChild<QAction*>(QStringLiteral("addEntry"));
        auto* save = window.findChild<QAction*>(QStringLiteral("saveConfiguration"));
        require(files && entries && create && rename && add && save && rename->isEnabled(),
                "find file-management actions for a selected configuration");
        const auto selectedPath = [&] {
            require(files->currentItem() != nullptr, "configuration remains selected");
            return files->currentItem()->data(Qt::UserRole).toString();
        };
        const auto checkSelection = [&] {
            require(selectedPath() == currentPath &&
                        files->currentItem()->text() == QFileInfo(currentPath).fileName() &&
                        window.windowTitle().contains(QFileInfo(currentPath).fileName()),
                    "file selection, displayed name, and window title follow the current path");
        };
        checkSelection();

        answerRenameDialog(QFileInfo(currentPath).fileName(), QStringLiteral("B_Renamed"));
        rename->trigger();
        const auto oldPath = currentPath;
        currentPath = QDir(temporary.path()).filePath(QStringLiteral("B_Renamed_MS.json"));
        require(!QFileInfo::exists(oldPath) && readFile(currentPath) == source && files->count() == 2 &&
                    entries->count() == 1 && !save->isEnabled(),
                "rename normalizes the suffix and preserves file bytes and clean document state");
        checkSelection();

        answerRenameDialog(QFileInfo(currentPath).fileName(), QStringLiteral("Cancelled"), false);
        rename->trigger();
        require(readFile(currentPath) == source && files->count() == 2 && !save->isEnabled(),
                "cancelled rename preserves the file and editor state");
        checkSelection();
        answerRenameDialog(QFileInfo(currentPath).fileName(), QFileInfo(collisionPath).fileName(), true, QMessageBox::Ok);
        rename->trigger();
        require(readFile(currentPath) == source && readFile(collisionPath) == collisionSource && files->count() == 2,
                "rename collision preserves both configurations");
        checkSelection();

        auto expectedEntry = QJsonDocument::fromJson(source).array().first().toObject();
        for (const auto& id : {QStringLiteral("222"), QStringLiteral("333")}) {
            editCondition(window, QStringLiteral("LocationID"), id);
            require(save->isEnabled(), "editing a renamed configuration enables Save");
            saveWithoutDialog(save);
            expectedEntry.insert(QStringLiteral("LocationID"), QStringLiteral("Example.esp|") + id);
            require(!save->isEnabled() && QJsonDocument::fromJson(readFile(currentPath)).array() == QJsonArray{expectedEntry},
                    "repeated saves after rename update the new path without false conflicts");
        }

        editCondition(window, QStringLiteral("LocationID"), QStringLiteral("444"));
        answerNextDialog(QMessageBox::Cancel);
        rename->trigger();
        require(save->isEnabled() && conditionInput(window, QStringLiteral("LocationID"))->text() == QStringLiteral("444"),
                "cancelling unsaved-change confirmation retains pending edits without renaming");
        checkSelection();
        confirmThenRename(QMessageBox::Discard, QFileInfo(currentPath).fileName(), QStringLiteral("C_Discarded"));
        rename->trigger();
        require(!QFileInfo::exists(currentPath), "discard-and-rename removes the previous path");
        currentPath = QDir(temporary.path()).filePath(QStringLiteral("C_Discarded_MS.json"));
        require(!save->isEnabled() && conditionInput(window, QStringLiteral("LocationID"))->text() == QStringLiteral("333") &&
                    QJsonDocument::fromJson(readFile(currentPath)).array() == QJsonArray{expectedEntry},
                "discard-and-rename reloads the saved bytes instead of retaining discarded edits");
        checkSelection();

        editCondition(window, QStringLiteral("LocationID"), QStringLiteral("555"));
        confirmThenRename(QMessageBox::Save, QFileInfo(currentPath).fileName(), QStringLiteral("D_Saved"));
        rename->trigger();
        require(!QFileInfo::exists(currentPath), "save-and-rename removes the previous path");
        currentPath = QDir(temporary.path()).filePath(QStringLiteral("D_Saved_MS.json"));
        expectedEntry.insert(QStringLiteral("LocationID"), QStringLiteral("Example.esp|555"));
        require(!save->isEnabled() && QJsonDocument::fromJson(readFile(currentPath)).array() == QJsonArray{expectedEntry},
                "save-and-rename writes pending edits before moving the configuration");
        checkSelection();
        editCondition(window, QStringLiteral("LocationID"), QStringLiteral("666"));
        saveWithoutDialog(save);
        expectedEntry.insert(QStringLiteral("LocationID"), QStringLiteral("Example.esp|666"));
        require(QJsonDocument::fromJson(readFile(currentPath)).array() == QJsonArray{expectedEntry},
                "saving after a confirmed rename uses the refreshed file baseline");

        answerNewFilenameDialog(QString(), false);
        create->trigger();
        require(files->count() == 2 && entries->count() == 1 && !save->isEnabled(),
                "cancelling New preserves the current configuration");
        checkSelection();
        answerNewFilenameDialog(QFileInfo(collisionPath).fileName(), true, QMessageBox::Ok);
        create->trigger();
        require(files->count() == 2 && entries->count() == 1 && !save->isEnabled() &&
                    readFile(collisionPath) == collisionSource,
                "failed New preserves the selected document and existing destination");
        checkSelection();

        answerNewFilenameDialog(QStringLiteral("Fresh"));
        create->trigger();
        currentPath = QDir(temporary.path()).filePath(QStringLiteral("Fresh_MS.json"));
        const auto emptySource = readFile(currentPath);
        const auto emptyDocument = QJsonDocument::fromJson(emptySource);
        require(emptyDocument.isArray() && emptyDocument.array().isEmpty() && files->count() == 3 &&
                    entries->count() == 0 && !save->isEnabled() && add->isEnabled() && rename->isEnabled(),
                "New immediately creates and selects a clean empty JSON array without adding a rule");
        checkSelection();
        add->trigger();
        require(entries->count() == 1 && save->isEnabled() && readFile(currentPath) == emptySource,
                "Add entry creates a pending rule while the new file remains empty on disk");
        const auto cancelledPath = QDir(temporary.path()).filePath(QStringLiteral("Aborted_MS.json"));
        answerNewFilenameDialog(QFileInfo(cancelledPath).fileName(), true, QMessageBox::Cancel);
        create->trigger();
        require(!QFileInfo::exists(cancelledPath) && files->count() == 3 && entries->count() == 1 && save->isEnabled(),
                "cancelling pending edits prevents New from creating or selecting another file");
        checkSelection();
        answerNextDialog(QMessageBox::Discard);
        require(window.close(), "discard pending blank entry when closing file-action fixture");
    }

    void testNewConfigurations() {
        QTemporaryDir temporary;
        require(temporary.isValid(), "create filename-only New fixtures");
        const QDir data(temporary.path());
        require(QDir().mkpath(data.filePath(QStringLiteral("Nested"))), "create selected configuration subfolder");
        const auto nestedPath = data.filePath(QStringLiteral("Nested/Source_MS.json"));
        const auto collisionPath = data.filePath(QStringLiteral("Existing_MS.json"));
        const auto nestedSource = document(QStringLiteral("NestedOriginal.nif"));
        const auto collisionSource = document(QStringLiteral("ExistingOriginal.nif"));
        writeFile(nestedPath, nestedSource);
        writeFile(collisionPath, collisionSource);
        EditorWindow window(data.path());
        auto* files = window.findChild<QListWidget*>(QStringLiteral("configurationFiles"));
        auto* entries = window.findChild<QListWidget*>(QStringLiteral("swapEntries"));
        auto* create = window.findChild<QAction*>(QStringLiteral("newConfiguration"));
        auto* save = window.findChild<QAction*>(QStringLiteral("saveConfiguration"));
        require(files && entries && create && save, "find filename-only New controls");
        const auto selectPath = [&](const QString& path) {
            for (int row = 0; row < files->count(); ++row) {
                if (files->item(row)->data(Qt::UserRole).toString() == path) {
                    files->setCurrentRow(row);
                    return;
                }
            }
            require(false, "select the requested New fixture");
        };
        const auto selectedPath = [&] {
            require(files->currentItem() != nullptr, "New keeps a configuration selected");
            return files->currentItem()->data(Qt::UserRole).toString();
        };
        selectPath(nestedPath);
        for (const auto& invalid : QStringList{
                 QString(), QStringLiteral("   "), QStringLiteral("."), QStringLiteral(".."),
                 QStringLiteral("Nested/Unexpected"), QStringLiteral("Nested\\Unexpected"),
                 QStringLiteral("../Unexpected"), QStringLiteral("C:\\Unexpected_MS.json"),
                 data.filePath(QStringLiteral("Absolute_MS.json")), QStringLiteral("Bad:name")}) {
            answerNewFilenameDialog(invalid, true, QMessageBox::Ok);
            create->trigger();
            require(files->count() == 2 && selectedPath() == nestedPath && entries->count() == 1 &&
                        !save->isEnabled() && readFile(nestedPath) == nestedSource && readFile(collisionPath) == collisionSource,
                    "empty, folder, absolute, traversal, and invalid names cannot create or switch configurations");
        }
        answerNewFilenameDialog(QStringLiteral("Existing.json"), true, QMessageBox::Ok);
        create->trigger();
        require(files->count() == 2 && selectedPath() == nestedPath && readFile(collisionPath) == collisionSource,
                "normalized New filename collisions preserve the selected nested file and existing destination");

        answerNewFilenameDialog(QStringLiteral("  Fresh.json  "));
        create->trigger();
        const auto freshPath = data.filePath(QStringLiteral("Fresh_MS.json"));
        require(selectedPath() == freshPath && files->currentItem()->text() == QStringLiteral("Fresh_MS.json") &&
                    QJsonDocument::fromJson(readFile(freshPath)).array().isEmpty() && entries->count() == 0 && !save->isEnabled() &&
                    !QFileInfo::exists(data.filePath(QStringLiteral("Nested/Fresh_MS.json"))),
                "New trims and normalizes the filename, immediately creating empty JSON in Data rather than the selected subfolder");
        answerNewFilenameDialog(QStringLiteral("Already_ms.JSON"));
        create->trigger();
        const auto normalizedPath = data.filePath(QStringLiteral("Already_MS.json"));
        const auto normalizedDocument = QJsonDocument::fromJson(readFile(normalizedPath));
        require(selectedPath() == normalizedPath && normalizedDocument.isArray() && normalizedDocument.array().isEmpty(),
                "New normalizes an existing case-insensitive suffix without adding it twice");

        selectPath(nestedPath);
        editCondition(window, QStringLiteral("LocationID"), QStringLiteral("777"));
        const auto cancelledPath = data.filePath(QStringLiteral("CancelPending_MS.json"));
        answerNewFilenameDialog(QStringLiteral("CancelPending"), true, QMessageBox::Cancel);
        create->trigger();
        require(!QFileInfo::exists(cancelledPath) && selectedPath() == nestedPath && save->isEnabled() &&
                    conditionInput(window, QStringLiteral("LocationID"))->text() == QStringLiteral("777") && readFile(nestedPath) == nestedSource,
                "Cancel on pending edits prevents immediate creation and retains the selected document and unsaved values");
        answerNewFilenameDialog(QStringLiteral("Ignored"), false);
        create->trigger();
        require(selectedPath() == nestedPath && save->isEnabled() && !QFileInfo::exists(data.filePath(QStringLiteral("Ignored_MS.json"))),
                "cancelling the filename prompt leaves pending edits intact without another confirmation");

        answerNewFilenameDialog(QStringLiteral("SavePending"), true, QMessageBox::Save);
        create->trigger();
        const auto savedPath = data.filePath(QStringLiteral("SavePending_MS.json"));
        const auto savedPrevious = readFile(nestedPath);
        const auto savedEmpty = QJsonDocument::fromJson(readFile(savedPath));
        require(selectedPath() == savedPath && entries->count() == 0 && !save->isEnabled() && savedEmpty.isArray() &&
                    savedEmpty.array().isEmpty() && QJsonDocument::fromJson(savedPrevious).array().first().toObject()
                        .value(QStringLiteral("LocationID")) == QStringLiteral("Example.esp|777"),
                "Save before New writes the previous edits and immediately selects a clean empty file in Data");
        selectPath(nestedPath);
        editCondition(window, QStringLiteral("LocationID"), QStringLiteral("888"));
        answerNewFilenameDialog(QStringLiteral("DiscardPending"), true, QMessageBox::Discard);
        create->trigger();
        const auto discardedPath = data.filePath(QStringLiteral("DiscardPending_MS.json"));
        const auto discardedEmpty = QJsonDocument::fromJson(readFile(discardedPath));
        require(selectedPath() == discardedPath && entries->count() == 0 && !save->isEnabled() && discardedEmpty.isArray() &&
                    discardedEmpty.array().isEmpty() && readFile(nestedPath) == savedPrevious,
                "Discard before New preserves the saved previous bytes while immediately creating the next empty file");
        require(window.close(), "close the clean filename-only New fixture");
    }

    void testDeleteConfigurations() {
        QTemporaryDir temporary;
        require(temporary.isValid(), "create isolated Delete fixtures");
        const QDir data(temporary.path());
        require(QDir().mkpath(data.filePath(QStringLiteral("Nested"))), "create another configuration subfolder for Delete");
        const auto firstPath = data.filePath(QStringLiteral("A_MS.json"));
        const auto secondPath = data.filePath(QStringLiteral("Nested/B_MS.json"));
        const auto firstSource = document(QStringLiteral("DeleteSelected.nif"));
        const auto secondSource = document(QStringLiteral("KeepOther.nif"));
        writeFile(firstPath, firstSource);
        writeFile(secondPath, secondSource);
        EditorWindow window(data.path());
        auto* files = window.findChild<QListWidget*>(QStringLiteral("configurationFiles"));
        auto* entries = window.findChild<QListWidget*>(QStringLiteral("swapEntries"));
        auto* deleteFile = window.findChild<QAction*>(QStringLiteral("deleteConfiguration"));
        auto* rename = window.findChild<QAction*>(QStringLiteral("renameConfiguration"));
        auto* save = window.findChild<QAction*>(QStringLiteral("saveConfiguration"));
        auto* add = window.findChild<QAction*>(QStringLiteral("addEntry"));
        auto* toolbar = window.findChild<QToolBar*>(QStringLiteral("fileActions"));
        require(files && entries && deleteFile && rename && save && add && toolbar && deleteFile->isEnabled(),
                "Delete is available for the selected saved configuration");
        require(files->actions().contains(deleteFile), "Delete is available in the file list context menu");
        bool inFileMenu = false;
        for (auto* menuAction : window.menuBar()->actions()) {
            if (auto* menu = menuAction->menu(); menu && menu->actions().contains(deleteFile)) inFileMenu = true;
        }
        require(inFileMenu, "Delete is available from the menu as well as the local file toolbar");
        auto* deleteButton = qobject_cast<QAbstractButton*>(toolbar->widgetForAction(deleteFile));
        require(deleteButton != nullptr, "find the actual local Delete button");
        answerDeleteDialog(QStringLiteral("A_MS.json"), false, QMessageBox::No);
        deleteButton->click();
        require(files->count() == 2 && entries->count() == 1 && !save->isEnabled() &&
                    files->currentItem()->data(Qt::UserRole).toString() == firstPath && readFile(firstPath) == firstSource &&
                    readFile(secondPath) == secondSource,
                "No on the actual Delete button keeps both files and the selected clean document unchanged");

        editCondition(window, QStringLiteral("LocationID"), QStringLiteral("234"));
        answerDeleteDialog(QStringLiteral("A_MS.json"), true, QMessageBox::No);
        deleteFile->trigger();
        require(save->isEnabled() && conditionInput(window, QStringLiteral("LocationID"))->text() == QStringLiteral("234") &&
                    readFile(firstPath) == firstSource,
                "No on dirty deletion preserves pending values and does not save them");
        QTimer unexpectedSavePrompt;
        QObject::connect(&unexpectedSavePrompt, &QTimer::timeout, [] {
            const auto* modal = QApplication::activeModalWidget();
            require(!modal || modal->windowTitle() == QStringLiteral("Delete configuration"),
                    "confirmed dirty Delete must discard directly without asking to save");
        });
        unexpectedSavePrompt.start(1);
        answerDeleteDialog(QStringLiteral("A_MS.json"), true, QMessageBox::Yes);
        deleteFile->trigger();
        unexpectedSavePrompt.stop();
        require(!QFileInfo::exists(firstPath) && readFile(secondPath) == secondSource && files->count() == 1 &&
                    files->currentItem()->data(Qt::UserRole).toString() == secondPath && entries->count() == 1 &&
                    entries->item(0)->text() == QStringLiteral("1. KeepOther.nif") && !save->isEnabled() && deleteFile->isEnabled(),
                "Yes permanently removes only the selected file, drops pending edits, and selects the remaining configuration cleanly");
        answerDeleteDialog(QStringLiteral("B_MS.json"), false, QMessageBox::Yes);
        deleteFile->trigger();
        require(!QFileInfo::exists(secondPath) && files->count() == 0 && files->currentItem() == nullptr && entries->count() == 0 &&
                    !deleteFile->isEnabled() && !rename->isEnabled() && !save->isEnabled() && !add->isEnabled(),
                "deleting the last configuration clears document controls and disables file and entry mutations");
        require(window.close(), "close the empty Delete fixture without an unsaved prompt");

        const auto conflictPath = data.filePath(QStringLiteral("Conflict_MS.json"));
        writeFile(conflictPath, firstSource);
        EditorWindow conflict(data.path());
        auto* conflictFiles = conflict.findChild<QListWidget*>(QStringLiteral("configurationFiles"));
        auto* conflictEntries = conflict.findChild<QListWidget*>(QStringLiteral("swapEntries"));
        auto* conflictDelete = conflict.findChild<QAction*>(QStringLiteral("deleteConfiguration"));
        auto* conflictSave = conflict.findChild<QAction*>(QStringLiteral("saveConfiguration"));
        auto* duplicate = conflict.findChild<QAction*>(QStringLiteral("duplicateEntry"));
        require(conflictFiles && conflictEntries && conflictDelete && conflictSave && duplicate, "find Delete conflict controls");
        editCondition(conflict, QStringLiteral("LocationID"), QStringLiteral("345"));
        duplicate->trigger();
        const auto changedSource = document(QStringLiteral("ChangedExternally.nif"));
        writeFromExternalProcess(conflictPath, changedSource);
        answerDeleteDialog(QStringLiteral("Conflict_MS.json"), true, QMessageBox::Yes, QMessageBox::Ok);
        conflictDelete->trigger();
        require(readFile(conflictPath) == changedSource && conflictFiles->count() == 1 &&
                    conflictFiles->currentItem()->data(Qt::UserRole).toString() == conflictPath && conflictEntries->count() == 2 &&
                    conflictEntries->currentRow() == 1 && conflictSave->isEnabled() && conflictDelete->isEnabled() &&
                    conditionInput(conflict, QStringLiteral("LocationID"))->text() == QStringLiteral("345"),
                "an external-source mismatch rejects Delete while preserving the external bytes, selected entry, and all pending edits");
        answerNextDialog(QMessageBox::Discard);
        require(conflict.close(), "discard only pending fixture edits after rejected Delete");
    }

    void renderPreview(const QString& screenshotPath) {
        QTemporaryDir previewDirectory;
        require(previewDirectory.isValid(), "create screenshot fixture directory");
        for (const auto& name : {QStringLiteral("Skyrim.esm"), QStringLiteral("Example.esp"), QStringLiteral("Example.esl")}) {
            writeFile(QDir(previewDirectory.path()).filePath(name), QByteArray());
        }
        const QJsonObject previewEntry{
            {QStringLiteral("Title"), QStringLiteral("Seasonal farmhouse")},
            {QStringLiteral("Priority"), 1.5},
            {QStringLiteral("OriginalModel"), QJsonArray{QStringLiteral("Architecture\\Farmhouse.nif"),
                                                       QStringLiteral("Architecture\\FarmhouseSnow.nif")}},
            {QStringLiteral("Variants"), QJsonArray{
                QJsonObject{{"Model", "ModelSwapper\\FarmhouseSummer.nif"}, {"Weight", 0.5}},
                QJsonObject{{"Model", "ModelSwapper\\FarmhouseAutumn.nif"}, {"Weight", 1.5}},
                QJsonObject{{"Model", "ModelSwapper\\FarmhouseWinter.nif"}, {"Weight", 0.0}}}},
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
        selectSettingsTab(preview, qEnvironmentVariable("MS_EDITOR_TEST_TAB", QStringLiteral("Basics")));
        preview.resize(1320, 860);
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
        require(screenshot.save(screenshotPath, "BMP"), "save editor preview BMP image");
    }
}
