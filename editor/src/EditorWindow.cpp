#include "EditorWindow.h"

#include "ConfigDocument.h"
#include "JsonForm.h"

#include <QAction>
#include <QCloseEvent>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QJsonObject>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenuBar>
#include <QMessageBox>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSplitter>
#include <QStatusBar>
#include <QStyle>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
    QJsonObject blankEntry() {
        return {{QStringLiteral("OriginalModel"), QString()},
                {QStringLiteral("Variants"), QJsonArray{QString()}}};
    }

    QString configurationPath(QString path) {
        if (path.endsWith(QStringLiteral("_MS.json"), Qt::CaseInsensitive)) {
            path.chop(8);
        } else if (path.endsWith(QStringLiteral(".json"), Qt::CaseInsensitive)) {
            path.chop(5);
        }
        return path + QStringLiteral("_MS.json");
    }

    QToolBar* actionBar(QWidget* parent, const QString& name) {
        auto* bar = new QToolBar(parent);
        bar->setObjectName(name);
        bar->setMovable(false);
        bar->setFloatable(false);
        bar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        bar->setIconSize(QSize(16, 16));
        return bar;
    }

    void addPanelAction(QToolBar* bar, QAction* action, const QString& text,
                        QStyle::StandardPixmap icon) {
        action->setIcon(bar->style()->standardIcon(icon));
        action->setIconText(text);
        action->setToolTip(action->text().remove(QLatin1Char('&')));
        bar->addAction(action);
        if (auto* button = qobject_cast<QToolButton*>(bar->widgetForAction(action))) {
            button->setText(text);
            button->setToolTip(action->text().remove(QLatin1Char('&')));
            button->setAccessibleName(button->toolTip());
        }
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
    renameAction_ = fileMenu->addAction(tr("&Rename configuration…"));
    renameAction_->setObjectName(QStringLiteral("renameConfiguration"));
    renameAction_->setShortcut(QKeySequence(Qt::Key_F2));
    connect(renameAction_, &QAction::triggered, this, &EditorWindow::renameFile);
    saveAction_ = fileMenu->addAction(tr("&Save"));
    saveAction_->setObjectName(QStringLiteral("saveConfiguration"));
    saveAction_->setShortcut(QKeySequence::Save);
    connect(saveAction_, &QAction::triggered, this, [this] { saveFile(); });
    fileMenu->addSeparator();
    auto* directoryAction = fileMenu->addAction(tr("Choose &Data folder…"));
    directoryAction->setObjectName(QStringLiteral("chooseDataDirectory"));
    connect(directoryAction, &QAction::triggered, this, &EditorWindow::chooseDirectory);
    auto* rescanAction = fileMenu->addAction(tr("&Rescan files"));
    rescanAction->setObjectName(QStringLiteral("rescanConfigurations"));
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
    auto* fileActions = actionBar(filePanel, QStringLiteral("fileActions"));
    fileActions->setToolButtonStyle(Qt::ToolButtonTextOnly);
    addPanelAction(fileActions, newAction, tr("New"), QStyle::SP_FileIcon);
    addPanelAction(fileActions, renameAction_, tr("Rename"), QStyle::SP_FileDialogInfoView);
    addPanelAction(fileActions, rescanAction, tr("Rescan"), QStyle::SP_BrowserReload);
    addPanelAction(fileActions, directoryAction, tr("Folder…"), QStyle::SP_DirOpenIcon);
    fileLayout->addWidget(fileActions);
    search_ = new QLineEdit(filePanel);
    search_->setPlaceholderText(tr("Filter files…"));
    search_->setClearButtonEnabled(true);
    fileLayout->addWidget(search_);
    files_ = new QListWidget(filePanel);
    files_->setObjectName(QStringLiteral("configurationFiles"));
    files_->setContextMenuPolicy(Qt::ActionsContextMenu);
    files_->addAction(renameAction_);
    fileLayout->addWidget(files_);
    auto* discoveryHelp = new QLabel(tr("Finds all *_MS.json files below this folder.\n"
                                       "The plugin loads configurations in Data itself."), filePanel);
    discoveryHelp->setWordWrap(true);
    fileLayout->addWidget(discoveryHelp);

    auto* entryPanel = new QWidget(splitter);
    auto* entryLayout = new QVBoxLayout(entryPanel);
    entryLayout->setContentsMargins(0, 0, 0, 0);
    entryLayout->addWidget(new QLabel(tr("Swap entries"), entryPanel));
    auto* entryActions = actionBar(entryPanel, QStringLiteral("entryActions"));
    entryActions->setToolButtonStyle(Qt::ToolButtonTextOnly);
    addPanelAction(entryActions, addAction_, tr("Add"), QStyle::SP_FileDialogNewFolder);
    addPanelAction(entryActions, duplicateAction_, tr("Duplicate"), QStyle::SP_FileDialogDetailedView);
    addPanelAction(entryActions, removeAction_, tr("Remove"), QStyle::SP_TrashIcon);
    entryLayout->addWidget(entryActions);
    entryList_ = new QListWidget(entryPanel);
    entryList_->setObjectName(QStringLiteral("swapEntries"));
    entryList_->setWordWrap(true);
    entryLayout->addWidget(entryList_);

    auto* formPanel = new QWidget(splitter);
    auto* formLayout = new QVBoxLayout(formPanel);
    formLayout->setContentsMargins(0, 0, 0, 0);
    documentLabel_ = new QLabel(formPanel);
    documentLabel_->setWordWrap(true);
    auto* documentHeading = new QHBoxLayout;
    documentHeading->addWidget(documentLabel_, 1);
    auto* documentActions = actionBar(formPanel, QStringLiteral("documentActions"));
    addPanelAction(documentActions, saveAction_, tr("Save"), QStyle::SP_DialogSaveButton);
    documentHeading->addWidget(documentActions);
    formLayout->addLayout(documentHeading);
    editorLayout_ = new QVBoxLayout;
    editorLayout_->setContentsMargins(0, 0, 0, 0);
    formLayout->addLayout(editorLayout_, 1);
    splitter->addWidget(filePanel);
    splitter->addWidget(entryPanel);
    splitter->addWidget(formPanel);
    splitter->setStretchFactor(0, 0);
    splitter->setStretchFactor(1, 0);
    splitter->setStretchFactor(2, 1);
    splitter->setSizes({260, 260, 800});
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
    path = configurationPath(path);
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
    QString error;
    QByteArray createdSource;
    if (!ConfigFiles::create(path, createdSource, error)) {
        QMessageBox::warning(this, tr("Cannot create configuration"), error);
        return;
    }
    currentPath_ = path;
    originalSource_ = createdSource;
    entries_ = QJsonArray{};
    dirty_ = false;
    if (!discoveredFiles_.contains(path)) discoveredFiles_.append(path);
    discoveredFiles_.sort(Qt::CaseInsensitive);
    search_->clear();
    populateFiles();
    populateEntries();
    updateState();
    statusBar()->showMessage(tr("Created %1").arg(QFileInfo(path).fileName()), 6000);
}

void EditorWindow::renameFile() {
    if (currentPath_.isEmpty() || !confirmUnsavedChanges()) {
        return;
    }
    bool accepted = false;
    auto name = QInputDialog::getText(this, tr("Rename configuration"), tr("New filename:"),
                                     QLineEdit::Normal, QFileInfo(currentPath_).fileName(), &accepted).trimmed();
    if (!accepted) {
        return;
    }
    static const QRegularExpression invalidName(QStringLiteral(R"([<>:"/\\|?*\x00-\x1F])"));
    if (name.isEmpty() || name == QStringLiteral(".") || name == QStringLiteral("..") || name.contains(invalidName)) {
        QMessageBox::warning(this, tr("Invalid filename"), tr("Enter a filename without folder paths or special filename characters."));
        return;
    }
    name = configurationPath(name);
    const auto newPath = QFileInfo(currentPath_).dir().absoluteFilePath(name);
    QString error;
    if (!ConfigFiles::rename(currentPath_, newPath, originalSource_, error)) {
        QMessageBox::warning(this, tr("Cannot rename configuration"), error);
        return;
    }

    discoveredFiles_.removeAll(currentPath_);
    if (!discoveredFiles_.contains(newPath)) discoveredFiles_.append(newPath);
    discoveredFiles_.sort(Qt::CaseInsensitive);
    currentPath_ = newPath;
    // A Discard response drops pending edits only once the rename succeeds.
    entries_ = QJsonDocument::fromJson(originalSource_).array();
    dirty_ = false;
    search_->clear();
    populateFiles();
    populateEntries(currentEntry_);
    updateState();
    statusBar()->showMessage(tr("Renamed to %1").arg(name), 6000);
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
    for (const auto& key : {QStringLiteral("OriginalModel"), QStringLiteral("BaseFormID"), QStringLiteral("ReferenceID")}) {
        const auto target = entry.value(key);
        const auto values = target.isArray() ? target.toArray() : QJsonArray{target};
        for (const auto& value : values) {
            if (value.isString() && !value.toString().trimmed().isEmpty()) {
                description = value.toString();
            } else if (key != QStringLiteral("OriginalModel") && value.isDouble()) {
                description = QString::number(value.toInteger());
            }
            if (!description.isEmpty()) {
                break;
            }
        }
        if (!description.isEmpty()) {
            break;
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
    int activeTab = 0;
    if (form_) {
        if (auto* tabs = form_->findChild<QTabWidget*>(QStringLiteral("entryTabs"))) {
            activeTab = tabs->currentIndex();
        }
    }
    delete editorContent_;
    editorContent_ = nullptr;
    form_ = nullptr;
    if (row >= 0 && row < entries_.size()) {
        form_ = new JsonForm(plugins_);
        form_->setValue(entries_.at(row).toObject());
        editorContent_ = form_;
        if (auto* tabs = form_->findChild<QTabWidget*>(QStringLiteral("entryTabs"))) {
            tabs->setCurrentIndex(activeTab);
        }
        connect(form_, &JsonForm::changed, this, &EditorWindow::formChanged);
    } else {
        auto* empty = new QLabel(currentPath_.isEmpty()
                                    ? tr("Select a configuration file or create a new one.")
                                    : tr("This configuration has no entries. Use Add entry to create a swap."));
        empty->setWordWrap(true);
        empty->setAlignment(Qt::AlignCenter);
        editorContent_ = empty;
    }
    editorLayout_->addWidget(editorContent_);
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
    populateEntries(-1);
}

void EditorWindow::updateState() {
    const bool open = !currentPath_.isEmpty();
    const bool selected = currentEntry_ >= 0 && currentEntry_ < entries_.size();
    saveAction_->setEnabled(open && dirty_);
    renameAction_->setEnabled(open);
    addAction_->setEnabled(open);
    duplicateAction_->setEnabled(selected);
    removeAction_->setEnabled(selected);
    directoryLabel_->setText(tr("Data folder: %1").arg(QDir::toNativeSeparators(dataDirectory_)));
    directoryLabel_->setToolTip(QDir::toNativeSeparators(dataDirectory_));
    const auto name = open ? QFileInfo(currentPath_).fileName() : tr("No configuration selected");
    setWindowTitle(tr("%1%2 — Model Swapper Editor").arg(dirty_ ? QStringLiteral("* ") : QString(), name));
    documentLabel_->setText(open
                               ? tr("%1 · %2 %3").arg(name)
                                     .arg(entries_.size()).arg(entries_.size() == 1 ? tr("entry") : tr("entries"))
                               : name);
}

void EditorWindow::closeEvent(QCloseEvent* event) {
    if (confirmUnsavedChanges()) {
        event->accept();
    } else {
        event->ignore();
    }
}
