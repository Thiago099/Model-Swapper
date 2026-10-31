#pragma once

#include "ConfigDocument.h"

#include <QByteArray>
#include <QJsonArray>
#include <QMainWindow>
#include <QString>
#include <QStringList>

class QAction;
class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QVBoxLayout;
class JsonForm;

class EditorWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit EditorWindow(const QString& dataDirectory, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    void buildUi();
    void scanFiles();
    void newFile();
    void renameFile();
    void deleteFile();
    bool saveFile();
    bool confirmUnsavedChanges();
    bool reportFormErrors();
    bool openFile(const QString& path, int selectedEntry = 0);
    void selectFile(QListWidgetItem* current, QListWidgetItem* previous);
    void populateFiles();
    void restoreFileSelection();
    void filterFiles();
    void populateEntries(int selected = 0);
    void selectEntry(int row);
    void addEntry();
    void duplicateEntry();
    void removeEntry();
    void formChanged();
    void clearDocument();
    void updateState();
    QString entryLabel(int row) const;
    QString configurationLabel(const QString& path) const;

    QString dataDirectory_;
    QString currentPath_;
    QStringList discoveredFiles_;
    QList<ConfigFiles::SourceDirectory> sourceDirectories_;
    QStringList plugins_;
    QJsonArray entries_;
    QByteArray originalSource_;
    int currentEntry_ = -1;
    bool dirty_ = false;
    bool loading_ = false;

    QListWidget* files_ = nullptr;
    QListWidget* entryList_ = nullptr;
    QLineEdit* search_ = nullptr;
    QLabel* directoryLabel_ = nullptr;
    QLabel* documentLabel_ = nullptr;
    QVBoxLayout* editorLayout_ = nullptr;
    QWidget* editorContent_ = nullptr;
    JsonForm* form_ = nullptr;
    QAction* saveAction_ = nullptr;
    QAction* renameAction_ = nullptr;
    QAction* deleteAction_ = nullptr;
    QAction* addAction_ = nullptr;
    QAction* duplicateAction_ = nullptr;
    QAction* removeAction_ = nullptr;
};
