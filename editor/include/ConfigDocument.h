#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QList>
#include <QStringList>

namespace ConfigFiles {
    enum class SaveMode { Automatic, InPlace };

    struct SourceDirectory {
        QString sourcePath;
        QString virtualPath;
    };

    // Optional source anchors retain verified backing directories between
    // rescans so externally created files are visible without remapping MO2.
    QStringList discoverFiles(const QString& dataDirectory, QString* error = nullptr,
                             QList<SourceDirectory>* sourceDirectories = nullptr);
    QString backingFilePath(const QString& path);
    QStringList discoverPlugins(const QString& dataDirectory);

    // Loading permits invalid entry fields so that the editor can repair them.
    bool load(const QString& path, QJsonArray& entries, QByteArray& source, QString& error);
    QStringList validate(const QJsonArray& entries);

    // A null expectedSource creates a new file; otherwise its current bytes must
    // match the version loaded by the editor before it may be updated.
    // Automatic preserves file identity under MO2 and for hard-linked files;
    // ordinary files use atomic replacement. InPlace also supports testing the
    // virtual-filesystem write path without an injected mod manager.
    bool save(const QString& path, const QJsonArray& entries, const QByteArray& expectedSource,
              QByteArray& savedSource, QString& error, SaveMode mode = SaveMode::Automatic);

    // Create an empty, valid configuration exclusively; an existing file is
    // never overwritten, including if it appears after the initial check.
    bool create(const QString& path, QByteArray& savedSource, QString& error);

    // Rename within the same directory only after confirming the loaded bytes.
    // The destination is never replaced, and success verifies the moved file.
    bool rename(const QString& oldPath, const QString& newPath, const QByteArray& expectedSource, QString& error);
}
