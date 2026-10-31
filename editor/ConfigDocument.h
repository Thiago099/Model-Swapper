#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QStringList>

namespace ConfigFiles {
    enum class SaveMode { Automatic, InPlace };

    QStringList discoverFiles(const QString& dataDirectory, QString* error = nullptr);
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
}
