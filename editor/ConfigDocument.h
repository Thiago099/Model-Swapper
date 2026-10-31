#pragma once

#include <QByteArray>
#include <QJsonArray>
#include <QStringList>

namespace ConfigFiles {
    QStringList discoverFiles(const QString& dataDirectory, QString* error = nullptr);
    QStringList discoverPlugins(const QString& dataDirectory);

    // Loading permits invalid entry fields so that the editor can repair them.
    bool load(const QString& path, QJsonArray& entries, QByteArray& source, QString& error);
    QStringList validate(const QJsonArray& entries);

    // A null expectedSource creates a new file; otherwise its current bytes must
    // match the version loaded by the editor before it may be replaced.
    bool save(const QString& path, const QJsonArray& entries, const QByteArray& expectedSource,
              QByteArray& savedSource, QString& error);
}
