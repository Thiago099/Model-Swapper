#include "EditorWindow.h"

#include "ConfigDocument.h"
#include "JsonForm.h"

#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QToolBar>
#include <QVBoxLayout>

namespace {
    QJsonObject blankEntry() {
        return {{QStringLiteral("OriginalModel"), QString()},
                {QStringLiteral("Variants"), QJsonArray{QString()}}};
    }
}

EditorWindow::EditorWindow(const QString& dataDirectory, QWidget* parent)
    : QMainWindow(parent), dataDirectory_(QDir(dataDirectory).absolutePath()) {
    buildUi();
    scanFiles();
}

void EditorWindow::buildUi() {
    setWindowTitle(tr("Model Swapper Editor"));
    resize(1320, 860);
    setMinimumSize(900, 600);

    auto* fileMenu = menuBar()->addMenu(tr("&File"));
    auto* newAction = fileMenu->addAction(tr("&New configuration…"));
    newAction->setObjectName(QStringLiteral("newConfiguration"));
    newAction->setShortcut(QKeySequence::New);
    connect(newAction, &QAction::triggered, this, &EditorWindow::newFile);
    saveAction_ = fileMenu->addAction(tr("&Save"));
    saveAction_->setObjectName(QStringLiteral("saveConfiguration"));
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this, [this] { saveFile(); });
    fileMenu->addSeparator();
    auto* directoryAction = fileMenu->addAction(tr("Choose &Data folder…"));
    connect(directoryAction, &QAction::triggered, this, &EditorWindow::chooseDirectory);
    auto* rescanAction = fileMenu->addAction(tr("&Rescan files"));
    rescanAction->setShortcut(QKeySequence::Refresh);
    connect(rescanAction, &QAction::triggered, this, &EditorWindow::scanFiles);
    fileMenu->addSeparator();
    auto* quitAction = fileMenu->addAction(tr("&Quit"));
    quitAction->setShortcut(QKeySequence::Quit);
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    auto* entryMenu = menuBar()->addMenu(tr("&Entry"));
    addAction_ = entryMenu->addAction(tr("&Add entry"));
    duplicateAction_ = entryMenu->addAction(tr("&Duplicate entry"));
    removeAction_ = entryMenu->addAction(tr("&Remove entry"));
    addAction_->setObjectName(QStringLiteral("addEntry"));
    duplicateAction_->setObjectName(QStringLiteral("duplicateEntry"));
    removeAction_->setObjectName(QStringLiteral("removeEntry"));
    connect(addAction_, &QAction::triggered, this, &EditorWindow::addEntry);
    connect(duplicateAction_, &QAction::triggered, this, &EditorWindow::duplicateEntry);
    connect(removeAction_, &QAction::triggered, this, &EditorWindow::removeEntry);

    auto* toolbar = addToolBar(tr("Configuration"));
    toolbar->setMovable(false);
    toolbar->setToolButtonStyle(Qt::ToolButtonTextOnly);
    toolbar->addAction(newAction);
    toolbar->addAction(saveAction_);
    toolbar->addSeparator();
    toolbar->addAction(rescanAction);
    toolbar->addAction(directoryAction);
    toolbar->addSeparator();
    toolbar->addAction(addAction_);
    toolbar->addAction(duplicateAction_);
    toolbar->addAction(removeAction_);

    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    directoryLabel_ = new QLabel(central);
    directoryLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    directoryLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(directoryLabel_);

    auto* splitter = new QSplitter(Qt::Horizontal, central);
    auto* filePanel = new QWidget(splitter);
    auto* fileLayout = new QVBoxLayout(filePanel);
    fileLayout->setContentsMargins(0, 0, 0, 0);
    fileLayout->addWidget(new QLabel(tr("Configuration files"), filePanel));
    search_ = new QLineEdit(filePanel);
    search_->setPlaceholderText(tr("Filter files…"));
    search_->setClearButtonEnabled(true);
    fileLayout->addWidget(search_);
    files_ = new QListWidget(filePanel);
    files_->setObjectName(QStringLiteral("configurationFiles"));
    fileLayout->addWidget(files_);
    auto* discoveryHelp = new QLabel(tr("Finds all *_MS.json files below this folder.\n"
                                       "The plugin loads configurations in Data itself."), filePanel);
    discoveryHelp->setWordWrap(true);
    fileLayout->addWidget(discoveryHelp);

    auto* entryPanel = new QWidget(splitter);
    auto* entryLayout = new QVBoxLayout(entryPanel);
    entryLayout->setContentsMargins(0, 0, 0, 0);
    entryLayout->addWidget(new QLabel(tr("Swap entries"), entryPanel));
    entryList_ = new QListWidget(entryPanel);
    entryList_->setObjectName(QStringLiteral("swapEntries"));
    entryList_->setWordWrap(true);
    entryLayout->addWidget(entryList_);

    auto* formPanel = new QWidget(splitter);
    auto* formLayout = new QVBoxLayout(formPanel);
    formLayout->setContentsMargins(0, 0, 0, 0);
    documentLabel_ = new QLabel(formPanel);
    documentLabel_->setWordWrap(true);
    formLayout->addWidget(documentLabel_);
    scroll_ = new QScrollArea(formPanel);
    scroll_->setWidgetResizable(true);
    scroll_->setFrameShape(QFrame::NoFrame);
    formLayout->addWidget(scroll_);
    splitter->addWidget(filePanel);
    splitter->addWidget(entryPanel);
    splitter->addWidget(formPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 0);
    splitter->setStretchFactor(2, 1);
    splitter->setSizes({240, 240, 800});
    layout->addWidget(splitter, 1);
    setCentralWidget(central);

    connect(search_, &QLineEdit::textChanged, this, &EditorWindow::filterFiles);
    connect(files_, &QListWidget::currentItemChanged, this, &EditorWindow::selectFile);
    connect(entryList_, &QListWidget::currentRowChanged, this, [this](int row) {
        if (row != currentEntry_ && reportFormErrors()) {
            const QSignalBlocker blocker(entryList_);
            entryList_->setCurrentRow(currentEntry_);
            return;
        }
        selectEntry(row);
    });
    selectEntry(-1);
}

void EditorWindow::scanFiles() {
    if (!confirmUnsavedChanges()) {
        return;
    }
    QString error;
    auto discovered = ConfigFiles::discoverFiles(dataDirectory_, &error);
    if (!error.isEmpty()) {
        QMessageBox::warning(this, tr("Cannot scan Data folder"), error);
        return;
    }
    discoveredFiles_ = discovered;
    plugins_ = ConfigFiles::discoverPlugins(dataDirectory_);
    if (!currentPath_.isEmpty()) {
        if (QFileInfo::exists(currentPath_)) {
            openFile(currentPath_);
        } else {
            clearDocument();
        }
    }
    populateFiles();
    if (currentPath_.isEmpty() && !discoveredFiles_.isEmpty()) {
        files_->setCurrentRow(0);
    }
    statusBar()->showMessage(tr("Found %1 configuration file(s) and %2 plugin(s).")
                                 .arg(discoveredFiles_.size()).arg(plugins_.size()));
    updateState();
}

void EditorWindow::chooseDirectory() {
    const auto directory = QFileDialog::getExistingDirectory(this, tr("Choose Skyrim Data folder"), dataDirectory_);
    if (directory.isEmpty() || !confirmUnsavedChanges()) {
        return;
    }
    clearDocument();
    dataDirectory_ = QDir(directory).absolutePath();
    discoveredFiles_.clear();
    plugins_.clear();
    populateFiles();
    scanFiles();
}

void EditorWindow::populateFiles() {
    const QSignalBlocker blocker(files_);
    files_->clear();
    auto paths = discoveredFiles_;
    if (!currentPath_.isEmpty() && !paths.contains(currentPath_)) {
        paths.append(currentPath_);
    }
    for (const auto& path : paths) {
        auto* item = new QListWidgetItem(QDir(dataDirectory_).relativeFilePath(path), files_);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        if (path == currentPath_) {
            files_->setCurrentItem(item);
        }
    }
    filterFiles();
}

void EditorWindow::restoreFileSelection() {
    const QSignalBlocker blocker(files_);
    files_->setCurrentItem(nullptr);
    for (int row = 0; row < files_->count(); ++row) {
        auto* item = files_->item(row);
        if (item->data(Qt::UserRole).toString() == currentPath_) {
            files_->setCurrentItem(item);
            return;
        }
    }
}

void EditorWindow::filterFiles() {
    for (int row = 0; row < files_->count(); ++row) {
        auto* item = files_->item(row);
        item->setHidden(!item->text().contains(search_->text(), Qt::CaseInsensitive));
    }
}

void EditorWindow::selectFile(QListWidgetItem* current, QListWidgetItem*) {
    if (!current || loading_) {
        return;
    }
    const auto path = current->data(Qt::UserRole).toString();
    if (path == currentPath_) {
        return;
    }
    if (!confirmUnsavedChanges()) {
        restoreFileSelection();
        return;
    }
    openFile(path);
    restoreFileSelection();
}

bool EditorWindow::openFile(const QString& path) {
    QJsonArray loaded;
    QByteArray source;
    QString error;
    if (!ConfigFiles::load(path, loaded, source, error)) {
        QMessageBox::warning(this, tr("Cannot open configuration"), error);
        return false;
    }
    entries_ = loaded;
    originalSource_ = source;
    currentPath_ = path;
    dirty_ = false;
    newDocument_ = false;
    populateEntries();
    updateState();
    return true;
}

void EditorWindow::newFile() {
    auto path = QFileDialog::getSaveFileName(this, tr("New Model Swapper configuration"),
                                           QDir(dataDirectory_).filePath(QStringLiteral("New_MS.json")),
                                           tr("Model Swapper configurations (*_MS.json)"));
    if (path.isEmpty()) {
        return;
    }
    if (!path.endsWith(QStringLiteral("_MS.json"), Qt::CaseInsensitive)) {
        if (path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
            path.chop(5);
        }
        path += QStringLiteral("_MS.json");
    }
    path = QFileInfo(path).absoluteFilePath();
    if (QFileInfo::exists(path)) {
        QMessageBox::warning(this, tr("File already exists"),
                             tr("Choose a new filename. Select an existing configuration from the file list to edit it."));
        return;
    }
    if (!confirmUnsavedChanges()) {
        return;
    }
    // Saving the previous document may have just created the requested path.
    if (QFileInfo::exists(path)) {
        QMessageBox::warning(this, tr("File already exists"), tr("Choose a new filename for the new configuration."));
        return;
    }
    currentPath_ = path;
    originalSource_ = QByteArray();
    entries_ = QJsonArray{blankEntry()};
    newDocument_ = true;
    dirty_ = true;
    populateFiles();
    populateEntries();
    updateState();
}

bool EditorWindow::saveFile() {
    if (currentPath_.isEmpty()) {
        return false;
    }
    if (reportFormErrors()) {
        return false;
    }
    QString error;
    QByteArray savedSource;
    if (!ConfigFiles::save(currentPath_, entries_, originalSource_, savedSource, error)) {
        QMessageBox::warning(this, tr("Cannot save configuration"), error);
        return false;
    }
    originalSource_ = savedSource;
    dirty_ = false;
    newDocument_ = false;
    if (!discoveredFiles_.contains(currentPath_)) {
        discoveredFiles_.append(currentPath_);
        discoveredFiles_.sort(Qt::CaseInsensitive);
        populateFiles();
    }
    updateState();
    statusBar()->showMessage(tr("Saved %1").arg(QFileInfo(currentPath_).fileName()), 6000);
    return true;
}

bool EditorWindow::confirmUnsavedChanges() {
    if (!dirty_) {
        return true;
    }
    const auto response = QMessageBox::warning(
        this, tr("Unsaved changes"), tr("Save changes to %1?").arg(QFileInfo(currentPath_).fileName()),
        QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
    if (response == QMessageBox::Save) {
        return saveFile();
    }
    return response == QMessageBox::Discard;
}

bool EditorWindow::reportFormErrors() {
    if (!form_) {
        return false;
    }
    const auto errors = form_->errors();
    if (errors.isEmpty()) {
        return false;
    }
    QMessageBox::warning(this, tr("Correct these values"), errors.join(QLatin1Char('\n')));
    return true;
}

QString EditorWindow::entryLabel(int row) const {
    const auto entry = entries_.at(row).toObject();
    QString description;
    const auto original = entry.value(QStringLiteral("OriginalModel"));
    if (original.isString()) {
        description = original.toString();
    } else if (original.isArray() && !original.toArray().isEmpty()) {
        description = original.toArray().first().toString();
    }
    if (description.isEmpty()) {
        for (const auto& key : {QStringLiteral("BaseFormID"), QStringLiteral("ReferenceID")}) {
            const auto id = entry.value(key);
            if (id.isString()) {
                description = id.toString();
            } else if (id.isDouble()) {
                description = QString::number(id.toInteger());
            } else if (id.isArray() && !id.toArray().isEmpty()) {
                description = tr("%1 filter").arg(key);
            }
            if (!description.isEmpty()) {
                break;
            }
        }
    }
    if (description.isEmpty()) {
        description = tr("New swap");
    }
    return tr("%1. %2").arg(row + 1).arg(description);
}

void EditorWindow::populateEntries(int selected) {
    {
        const QSignalBlocker blocker(entryList_);
        entryList_->clear();
        for (int row = 0; row < entries_.size(); ++row) {
            entryList_->addItem(entryLabel(row));
        }
        selected = entries_.isEmpty() ? -1 : qBound(0, selected, static_cast<int>(entries_.size()) - 1);
        entryList_->setCurrentRow(selected);
    }
    selectEntry(selected);
}

void EditorWindow::selectEntry(int row) {
    if (loading_) {
        return;
    }
    loading_ = true;
    currentEntry_ = row;
    delete scroll_->takeWidget();
    form_ = nullptr;
    if (row >= 0 && row < entries_.size()) {
        form_ = new JsonForm(plugins_);
        form_->setValue(entries_.at(row).toObject());
        scroll_->setWidget(form_);
        connect(form_, &JsonForm::changed, this, &EditorWindow::formChanged);
    } else {
        auto* empty = new QLabel(currentPath_.isEmpty()
                                    ? tr("Select a configuration file or create a new one.")
                                    : tr("This configuration has no entries. Use Add entry to create a swap."));
        empty->setWordWrap(true);
        empty->setAlignment(Qt::AlignCenter);
        scroll_->setWidget(empty);
    }
    loading_ = false;
    updateState();
}

void EditorWindow::formChanged() {
    if (loading_ || !form_ || currentEntry_ < 0 || currentEntry_ >= entries_.size()) {
        return;
    }
    entries_.replace(currentEntry_, form_->value());
    dirty_ = true;
    entryList_->item(currentEntry_)->setText(entryLabel(currentEntry_));
    updateState();
}

void EditorWindow::addEntry() {
    if (currentPath_.isEmpty() || reportFormErrors()) {
        return;
    }
    entries_.append(blankEntry());
    dirty_ = true;
    populateEntries(static_cast<int>(entries_.size()) - 1);
}

void EditorWindow::duplicateEntry() {
    if (currentEntry_ < 0 || currentEntry_ >= entries_.size() || reportFormErrors()) {
        return;
    }
    const int selected = currentEntry_ + 1;
    entries_.insert(selected, entries_.at(currentEntry_));
    dirty_ = true;
    populateEntries(selected);
}

void EditorWindow::removeEntry() {
    if (currentEntry_ < 0 || currentEntry_ >= entries_.size()) {
        return;
    }
    const auto response = QMessageBox::question(this, tr("Remove entry"),
                                               tr("Remove swap entry %1?").arg(currentEntry_ + 1),
                                               QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (response != QMessageBox::Yes) {
        return;
    }
    const int previous = currentEntry_;
    entries_.removeAt(currentEntry_);
    dirty_ = true;
    populateEntries(previous);
}

void EditorWindow::clearDocument() {
    currentPath_.clear();
    entries_ = QJsonArray();
    originalSource_ = QByteArray();
    dirty_ = false;
    newDocument_ = false;
    populateEntries(-1);
}

void EditorWindow::updateState() {
    const bool open = !currentPath_.isEmpty();
    const bool selected = currentEntry_ >= 0 && currentEntry_ < entries_.size();
    saveAction_->setEnabled(open && dirty_);
    addAction_->setEnabled(open);
    duplicateAction_->setEnabled(selected);
    removeAction_->setEnabled(selected);
    directoryLabel_->setText(tr("Data folder: %1").arg(QDir::toNativeSeparators(dataDirectory_)));
    directoryLabel_->setToolTip(QDir::toNativeSeparators(dataDirectory_));
    const auto name = open ? QFileInfo(currentPath_).fileName() : tr("No configuration selected");
    setWindowTitle(tr("%1%2 — Model Swapper Editor").arg(dirty_ ? QStringLiteral("* ") : QString(), name));
    documentLabel_->setText(open
                               ? tr("%1%2 · %3 entry/entries").arg(name, newDocument_ ? tr(" (new)") : QString())
                                     .arg(entries_.size())
                               : name);
}

void EditorWindow::closeEvent(QCloseEvent* event) {
    if (confirmUnsavedChanges()) {
        event->accept();
    } else {
        event->ignore();
    }
}
