#include "ConfigDocument.h"

#include <QDate>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QRegularExpression>
#include <QSaveFile>

#include <algorithm>
#include <cmath>
#include <limits>

namespace {
    void sortPaths(QStringList& paths) {
        std::sort(paths.begin(), paths.end(), [](const QString& left, const QString& right) {
            const auto order = QString::compare(left, right, Qt::CaseInsensitive);
            return order == 0 ? left < right : order < 0;
        });
    }

    bool validHex(QString text, quint32 maximum) {
        if (text.startsWith(QStringLiteral("0x"), Qt::CaseInsensitive)) {
            text.remove(0, 2);
        }
        static const QRegularExpression hex(QStringLiteral("\\A[0-9A-Fa-f]+\\z"));
        if (!hex.match(text).hasMatch()) {
            return false;
        }
        // Leading zeroes do not change the ID or count towards its width.
        while (text.size() > 1 && text.front() == QLatin1Char('0')) {
            text.remove(0, 1);
        }
        if (text.size() > 8) {
            return false;
        }
        bool ok = false;
        const auto id = text.toUInt(&ok, 16);
        return ok && id != 0 && id <= maximum;
    }

    bool validID(const QJsonValue& value) {
        if (value.isDouble()) {
            const double number = value.toDouble();
            return std::isfinite(number) && std::trunc(number) == number && number > 0 &&
                   number <= std::numeric_limits<quint32>::max();
        }
        if (!value.isString()) {
            return false;
        }
        const auto text = value.toString();
        const auto separator = text.indexOf(QLatin1Char('|'));
        if (separator < 0) {
            return validHex(text, std::numeric_limits<quint32>::max());
        }
        const auto plugin = text.left(separator);
        if (plugin.trimmed().isEmpty() || plugin.contains(QLatin1Char('/')) ||
            plugin.contains(QLatin1Char('\\')) || text.indexOf(QLatin1Char('|'), separator + 1) >= 0) {
            return false;
        }
        const auto maximum = plugin.endsWith(QStringLiteral(".esl"), Qt::CaseInsensitive) ? 0xFFFu : 0xFFFFFFu;
        return validHex(text.mid(separator + 1), maximum);
    }

    void validateModels(const QJsonValue& value, const QString& path, bool permitSingle, QStringList& errors) {
        if (permitSingle && value.isString()) {
            if (value.toString().trimmed().isEmpty()) {
                errors.append(path + QStringLiteral(": enter a nonempty model path."));
            }
            return;
        }
        if (!value.isArray()) {
            errors.append(path + (permitSingle ? QStringLiteral(": use a model path or an array of model paths.")
                                               : QStringLiteral(": use an array of model paths.")));
            return;
        }
        const auto models = value.toArray();
        if (models.isEmpty()) {
            errors.append(path + QStringLiteral(": add at least one model path."));
        }
        for (qsizetype index = 0; index < models.size(); ++index) {
            const auto model = models.at(index);
            if (!model.isString() || model.toString().trimmed().isEmpty()) {
                errors.append(path + QStringLiteral("[%1]: enter a nonempty model path.").arg(index));
            }
        }
    }

    bool validTimeSpan(const QJsonValue& value) {
        if (!value.isString()) {
            return false;
        }
        static const QRegularExpression pattern(QStringLiteral("\\A(\\d{1,2})-(\\d{1,2})~(\\d{1,2})-(\\d{1,2})\\z"));
        const auto match = pattern.match(value.toString());
        return match.hasMatch() && QDate::isValid(2000, match.captured(2).toInt(), match.captured(1).toInt()) &&
               QDate::isValid(2000, match.captured(4).toInt(), match.captured(3).toInt());
    }
}

namespace ConfigFiles {
    QStringList discoverFiles(const QString& dataDirectory, QString* error) {
        if (error) {
            error->clear();
        }
        const QFileInfo directory(dataDirectory);
        if (!directory.isDir() || !directory.isReadable()) {
            if (error) {
                *error = QStringLiteral("Cannot read Data folder: %1").arg(QDir::toNativeSeparators(dataDirectory));
            }
            return {};
        }
        QStringList paths;
        QDirIterator files(directory.absoluteFilePath(), QDir::Files | QDir::Hidden | QDir::NoSymLinks,
                           QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const auto path = files.next();
            if (files.fileName().endsWith(QStringLiteral("_MS.json"), Qt::CaseInsensitive)) {
                paths.append(QFileInfo(path).absoluteFilePath());
            }
        }
        sortPaths(paths);
        return paths;
    }

    QStringList discoverPlugins(const QString& dataDirectory) {
        QStringList plugins;
        const auto entries = QDir(dataDirectory).entryInfoList(QDir::Files | QDir::Hidden | QDir::NoSymLinks);
        for (const auto& entry : entries) {
            const auto extension = entry.suffix().toLower();
            if (extension == QStringLiteral("esp") || extension == QStringLiteral("esm") || extension == QStringLiteral("esl")) {
                plugins.append(entry.fileName());
            }
        }
        sortPaths(plugins);
        return plugins;
    }

    bool load(const QString& path, QJsonArray& entries, QByteArray& source, QString& error) {
        error.clear();
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("Cannot open %1: %2").arg(QDir::toNativeSeparators(path), file.errorString());
            return false;
        }
        const auto bytes = file.readAll();
        if (file.error() != QFileDevice::NoError) {
            error = QStringLiteral("Cannot read %1: %2").arg(QDir::toNativeSeparators(path), file.errorString());
            return false;
        }
        QJsonParseError parseError;
        const auto document = QJsonDocument::fromJson(bytes, &parseError);
        if (parseError.error != QJsonParseError::NoError) {
            error = QStringLiteral("Invalid JSON at byte %1: %2").arg(parseError.offset).arg(parseError.errorString());
            return false;
        }
        if (!document.isArray()) {
            error = QStringLiteral("The JSON file must contain an array of swap entries.");
            return false;
        }
        const auto parsedEntries = document.array();
        for (qsizetype index = 0; index < parsedEntries.size(); ++index) {
            if (!parsedEntries.at(index).isObject()) {
                error = QStringLiteral("Entry [%1] must be a JSON object.").arg(index);
                return false;
            }
        }
        entries = parsedEntries;
        source = bytes;
        return true;
    }

    QStringList validate(const QJsonArray& entries) {
        QStringList errors;
        const QStringList idFields{QStringLiteral("BaseFormID"), QStringLiteral("ReferenceID"),
                                   QStringLiteral("ExcludeBaseFormID"), QStringLiteral("ExcludeReferenceID")};
        for (qsizetype index = 0; index < entries.size(); ++index) {
            const auto prefix = QStringLiteral("Entry [%1]").arg(index);
            if (!entries.at(index).isObject()) {
                errors.append(prefix + QStringLiteral(": use a JSON object."));
                continue;
            }
            const auto entry = entries.at(index).toObject();
            validateModels(entry.value(QStringLiteral("Variants")), prefix + QStringLiteral(".Variants"), false, errors);
            if (entry.contains(QStringLiteral("OriginalModel"))) {
                validateModels(entry.value(QStringLiteral("OriginalModel")), prefix + QStringLiteral(".OriginalModel"), true, errors);
            } else if (!entry.contains(QStringLiteral("BaseFormID")) && !entry.contains(QStringLiteral("ReferenceID"))) {
                errors.append(prefix + QStringLiteral(": include OriginalModel, BaseFormID, or ReferenceID."));
            }
            for (const auto& field : idFields) {
                if (!entry.contains(field)) {
                    continue;
                }
                const auto value = entry.value(field);
                const auto path = prefix + QLatin1Char('.') + field;
                if (value.isArray()) {
                    const auto ids = value.toArray();
                    for (qsizetype idIndex = 0; idIndex < ids.size(); ++idIndex) {
                        if (!validID(ids.at(idIndex))) {
                            errors.append(path + QStringLiteral("[%1]: use a positive 32-bit ID or Plugin.esp|local hex ID.").arg(idIndex));
                        }
                    }
                } else if (!validID(value)) {
                    errors.append(path + QStringLiteral(": use a positive 32-bit ID or Plugin.esp|local hex ID."));
                }
            }
            if (entry.contains(QStringLiteral("ActiveTimeSpan")) && !validTimeSpan(entry.value(QStringLiteral("ActiveTimeSpan")))) {
                errors.append(prefix + QStringLiteral(".ActiveTimeSpan: use valid day-month dates, for example 01-01~31-12."));
            }
        }
        return errors;
    }

    bool save(const QString& path, const QJsonArray& entries, const QByteArray& expectedSource,
              QByteArray& savedSource, QString& error) {
        error.clear();
        const auto errors = validate(entries);
        if (!errors.isEmpty()) {
            error = errors.join(QLatin1Char('\n'));
            return false;
        }
        QFile original(path);
        if (expectedSource.isNull()) {
            if (QFileInfo::exists(path)) {
                error = QStringLiteral("A file already exists at this path. Open it before editing it.");
                return false;
            }
        } else {
            if (!original.open(QIODevice::ReadOnly)) {
                error = QStringLiteral("Cannot verify the original file: %1. Reload before saving.").arg(original.errorString());
                return false;
            }
            const auto currentSource = original.readAll();
            if (original.error() != QFileDevice::NoError || currentSource != expectedSource) {
                error = QStringLiteral("The file changed outside the editor. Reload it before saving to avoid overwriting those changes.");
                return false;
            }
            original.close();
        }
        const auto bytes = QJsonDocument(entries).toJson(QJsonDocument::Indented);
        QSaveFile file(path);
        file.setDirectWriteFallback(false);
        if (!file.open(QIODevice::WriteOnly)) {
            error = QStringLiteral("Cannot save %1: %2").arg(QDir::toNativeSeparators(path), file.errorString());
            return false;
        }
        if (file.write(bytes) != bytes.size()) {
            error = QStringLiteral("Cannot write the complete file: %1").arg(file.errorString());
            file.cancelWriting();
            return false;
        }
        if (!file.commit()) {
            error = QStringLiteral("Cannot replace the file: %1").arg(file.errorString());
            return false;
        }
        savedSource = bytes;
        return true;
    }
}
