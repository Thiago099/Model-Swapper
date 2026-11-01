#include "ConfigDocument.h"

#include <QDate>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QMap>
#include <QRegularExpression>
#include <QSaveFile>
#include <QSet>

#ifdef Q_OS_WIN
#include <qt_windows.h>
#endif

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <limits>
#include <vector>

namespace {
    QString absolutePath(const QString& path) {
        return QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    }

    QString pathKey(const QString& path) {
        const auto normalized = absolutePath(path);
#ifdef Q_OS_WIN
        return normalized.toCaseFolded();
#else
        return normalized;
#endif
    }

    bool isWithinDirectory(const QString& directory, const QString& path) {
        const auto relative = QDir(directory).relativeFilePath(path);
        return !QDir::isAbsolutePath(relative) && relative != QStringLiteral("..") &&
               !relative.startsWith(QStringLiteral("../"));
    }

    bool virtualFilesystemLoaded() {
#ifdef Q_OS_WIN
        return GetModuleHandleW(L"usvfs_x64.dll") || GetModuleHandleW(L"usvfs_x86.dll");
#else
        return false;
#endif
    }

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

    bool preserveFileIdentity(const QString& path) {
#ifdef Q_OS_WIN
        // MO2 redirects existing-file opens. Replacing a temporary file can
        // update a different backing file from the one read through its VFS.
        if (GetModuleHandleW(L"usvfs_x64.dll") || GetModuleHandleW(L"usvfs_x86.dll")) {
            return true;
        }
        const auto nativePath = std::filesystem::path(path.toStdWString());
#else
        const auto nativePath = std::filesystem::path(QFile::encodeName(path).constData());
#endif
        std::error_code error;
        const auto links = std::filesystem::hard_link_count(nativePath, error);
        return !error && links > 1;
    }

    bool checkSource(QFile& file, const QByteArray& expected, QString& error,
                     const QString& operation = QStringLiteral("saving")) {
        const auto bytes = file.readAll();
        if (file.error() != QFileDevice::NoError) {
            error = QStringLiteral("Cannot read the configuration before %1: %2").arg(operation, file.errorString());
            return false;
        }
        if (bytes != expected) {
            error = QStringLiteral("The file changed outside the editor. Reload it before %1 to avoid overwriting those changes.").arg(operation);
            return false;
        }
        return true;
    }

    bool writeInPlace(QFile& file, const QByteArray& bytes) {
        // Do not truncate on open: compare the original bytes first, then
        // shorten only after all replacement bytes have been written.
        return file.seek(0) && file.write(bytes) == bytes.size() &&
               file.resize(bytes.size()) && file.flush();
    }

    bool verifySavedFile(const QString& path, const QByteArray& expected, QString& error) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("Cannot verify the saved configuration: %1").arg(file.errorString());
            return false;
        }
        const auto bytes = file.readAll();
        if (file.error() != QFileDevice::NoError) {
            error = QStringLiteral("Cannot read the saved configuration: %1").arg(file.errorString());
            return false;
        }
        if (bytes != expected) {
            error = QStringLiteral("The saved contents are not visible at %1. Your edits are still open; check the mod manager's file mapping before retrying.")
                        .arg(QDir::toNativeSeparators(path));
            return false;
        }
        return true;
    }

    bool pathExists(const QString& path) {
        const QFileInfo file(path);
        return file.exists() || file.isSymLink();
    }

    bool renameWithoutOverwrite(const QString& oldPath, const QString& newPath, QString& error) {
#ifdef Q_OS_WIN
        // Use MO2's hooked native rename without QFile's copy-and-delete
        // fallback, which could change the backing file or its identity.
        if (MoveFileW(reinterpret_cast<LPCWSTR>(oldPath.utf16()), reinterpret_cast<LPCWSTR>(newPath.utf16()))) {
            return true;
        }
        const auto code = GetLastError();
        wchar_t message[1024]{};
        const auto length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                           nullptr, code, 0, message, 1024, nullptr);
        error = length ? QString::fromWCharArray(message, static_cast<int>(length)).trimmed()
                       : QStringLiteral("Windows error %1").arg(code);
        return false;
#else
        QFile file(oldPath);
        if (file.rename(newPath)) return true;
        error = file.errorString();
        return false;
#endif
    }

    bool hasContents(const QString& path, const QByteArray& expected) {
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly)) return false;
        const auto bytes = file.readAll();
        return file.error() == QFileDevice::NoError && bytes == expected;
    }

    bool removeSingleFile(const QString& path, QString& error) {
#ifdef Q_OS_WIN
        // Use MO2's hooked deletion for the selected name. Never delete a
        // resolved backing path or use a copy-and-delete fallback.
        if (DeleteFileW(reinterpret_cast<LPCWSTR>(path.utf16()))) return true;
        const auto code = GetLastError();
        wchar_t message[1024]{};
        const auto length = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                                           nullptr, code, 0, message, 1024, nullptr);
        error = length ? QString::fromWCharArray(message, static_cast<int>(length)).trimmed()
                       : QStringLiteral("Windows error %1").arg(code);
        return false;
#else
        QFile file(path);
        if (file.remove()) return true;
        error = file.errorString();
        return false;
#endif
    }
}

namespace ConfigFiles {
    QString backingFilePath(const QString& path) {
#ifdef Q_OS_WIN
        const auto handle = CreateFileW(reinterpret_cast<LPCWSTR>(path.utf16()), FILE_READ_ATTRIBUTES,
                                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                       nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
        if (handle != INVALID_HANDLE_VALUE) {
            QString resolved;
            const auto required = GetFinalPathNameByHandleW(handle, nullptr, 0, FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
            if (required > 0 && required < static_cast<DWORD>(std::numeric_limits<int>::max())) {
                std::vector<wchar_t> buffer(static_cast<std::size_t>(required) + 1);
                const auto length = GetFinalPathNameByHandleW(handle, buffer.data(), static_cast<DWORD>(buffer.size()),
                                                             FILE_NAME_NORMALIZED | VOLUME_NAME_DOS);
                if (length > 0 && length < buffer.size()) {
                    resolved = QString::fromWCharArray(buffer.data(), static_cast<int>(length));
                }
            }
            CloseHandle(handle);
            if (!resolved.isEmpty()) {
                if (resolved.startsWith(QStringLiteral("\\\\?\\UNC\\"), Qt::CaseInsensitive)) {
                    resolved = QStringLiteral("\\\\") + resolved.mid(8);
                } else if (resolved.startsWith(QStringLiteral("\\\\?\\"))) {
                    resolved.remove(0, 4);
                }
                return QDir::cleanPath(QDir::fromNativeSeparators(resolved));
            }
        }
#endif
        const auto canonical = QFileInfo(path).canonicalFilePath();
        return canonical.isEmpty() ? path : QDir::cleanPath(canonical);
    }

    QStringList discoverFiles(const QString& dataDirectory, QString* error, QList<SourceDirectory>* sourceDirectories) {
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
        const auto dataPath = directory.absoluteFilePath();
        QStringList paths;
        QDirIterator files(dataPath, QDir::Files | QDir::Hidden | QDir::NoSymLinks,
                           QDirIterator::Subdirectories);
        while (files.hasNext()) {
            const auto path = files.next();
            if (files.fileName().endsWith(QStringLiteral("_MS.json"), Qt::CaseInsensitive)) {
                paths.append(QFileInfo(path).absoluteFilePath());
            }
        }
        sortPaths(paths);
        if (!sourceDirectories) return paths;

        QList<SourceDirectory> anchors;
        QSet<QString> anchorKeys;
        const auto addAnchor = [&](const SourceDirectory& source) {
            if (source.sourcePath.isEmpty() || source.virtualPath.isEmpty()) return;
            const SourceDirectory normalized{absolutePath(source.sourcePath), absolutePath(source.virtualPath)};
            if (!isWithinDirectory(dataPath, normalized.virtualPath)) return;
            const auto key = pathKey(normalized.sourcePath) + QLatin1Char('\n') + pathKey(normalized.virtualPath);
            if (!anchorKeys.contains(key)) {
                anchorKeys.insert(key);
                anchors.append(normalized);
            }
        };
        for (const auto& source : *sourceDirectories) addAnchor(source);

        QSet<QString> virtualAliases;
        QSet<QString> backingFiles;
        QStringList uniquePaths;
        const bool learnSources = virtualFilesystemLoaded();
        for (const auto& path : paths) {
            virtualAliases.insert(pathKey(path));
            const auto backing = backingFilePath(path);
            // USVFS can retain a directory entry after its source was removed.
            // Keep its alias reserved without showing a file that cannot open.
            if (!QFileInfo::exists(backing)) continue;
            const auto backingKey = pathKey(backing);
            if (learnSources && backingKey != pathKey(path)) {
                addAnchor({QFileInfo(backing).absolutePath(), QFileInfo(path).absolutePath()});
            }
            if (!backingFiles.contains(backingKey)) {
                backingFiles.insert(backingKey);
                uniquePaths.append(path);
            }
        }
        // Keep proven source anchors even when the last mapped file was renamed
        // or removed. A later rescan can still find new files in that source.
        *sourceDirectories = anchors;

        struct Candidate {
            QString path;
            QString backingKey;
            bool ambiguous = false;
        };
        QMap<QString, Candidate> candidates;
        for (const auto& source : anchors) {
            if (pathKey(source.sourcePath) == pathKey(source.virtualPath)) continue;
            const QFileInfo sourceDirectory(source.sourcePath);
            if (!sourceDirectory.isDir() || !sourceDirectory.isReadable()) continue;
            const QDir sourceRoot(source.sourcePath);
            const QDir virtualRoot(source.virtualPath);
            QDirIterator sourceFiles(source.sourcePath, QDir::Files | QDir::Hidden | QDir::NoSymLinks,
                                     QDirIterator::Subdirectories);
            while (sourceFiles.hasNext()) {
                const auto path = absolutePath(sourceFiles.next());
                if (!sourceFiles.fileName().endsWith(QStringLiteral("_MS.json"), Qt::CaseInsensitive)) continue;
                if (!isWithinDirectory(source.sourcePath, path)) continue;
                const auto alias = absolutePath(virtualRoot.filePath(sourceRoot.relativeFilePath(path)));
                const auto aliasKey = pathKey(alias);
                // The active virtual view owns its existing filenames. Never
                // expose an overridden source just because its bytes differ.
                if (virtualAliases.contains(aliasKey)) continue;
                const auto backingKey = pathKey(backingFilePath(path));
                auto existing = candidates.find(aliasKey);
                if (existing == candidates.end()) {
                    candidates.insert(aliasKey, {path, backingKey});
                } else if (existing->backingKey != backingKey) {
                    // There is no known MO2 priority for a new alias exposed by
                    // multiple source folders. Leave it out rather than guess.
                    existing->ambiguous = true;
                }
            }
        }
        for (const auto& candidate : candidates) {
            if (candidate.ambiguous || backingFiles.contains(candidate.backingKey)) continue;
            backingFiles.insert(candidate.backingKey);
            uniquePaths.append(candidate.path);
        }
        sortPaths(uniquePaths);
        return uniquePaths;
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
                                   QStringLiteral("ExcludeBaseFormID"), QStringLiteral("ExcludeReferenceID"),
                                   QStringLiteral("LocationID"), QStringLiteral("RegionID"), QStringLiteral("KeywordID"),
                                   QStringLiteral("CellID"), QStringLiteral("WorldspaceID"),
                                   QStringLiteral("ExcludeLocationID"), QStringLiteral("ExcludeRegionID"),
                                   QStringLiteral("ExcludeKeywordID"), QStringLiteral("ExcludeCellID"),
                                   QStringLiteral("ExcludeWorldspaceID")};
        for (qsizetype index = 0; index < entries.size(); ++index) {
            const auto prefix = QStringLiteral("Entry [%1]").arg(index);
            if (!entries.at(index).isObject()) {
                errors.append(prefix + QStringLiteral(": use a JSON object."));
                continue;
            }
            const auto entry = entries.at(index).toObject();
            if (entry.contains(QStringLiteral("Priority"))) {
                const auto priority = entry.value(QStringLiteral("Priority"));
                if (!priority.isDouble() || !std::isfinite(priority.toDouble())) {
                    errors.append(prefix + QStringLiteral(".Priority: use a finite floating-point number, for example 1.5."));
                }
            }
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
              QByteArray& savedSource, QString& error, SaveMode mode) {
        error.clear();
        const auto errors = validate(entries);
        if (!errors.isEmpty()) {
            error = errors.join(QLatin1Char('\n'));
            return false;
        }
        const auto bytes = QJsonDocument(entries).toJson(QJsonDocument::Indented);
        const bool inPlace = mode == SaveMode::InPlace || preserveFileIdentity(path);
        QFile original(path);
        if (expectedSource.isNull()) {
            if (QFileInfo::exists(path)) {
                error = QStringLiteral("A file already exists at this path. Open it before editing it.");
                return false;
            }
        } else {
            const auto openMode = inPlace ? QIODevice::ReadWrite | QIODevice::ExistingOnly : QIODevice::ReadOnly;
            if (!original.open(openMode)) {
                error = QStringLiteral("Cannot verify the original file: %1. Reload before saving.").arg(original.errorString());
                return false;
            }
            if (!checkSource(original, expectedSource, error)) {
                return false;
            }
        }

        if (inPlace) {
            const bool creating = expectedSource.isNull();
            if (creating && !original.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
                error = QStringLiteral("Cannot create %1: %2").arg(QDir::toNativeSeparators(path), original.errorString());
                return false;
            }
            if (!writeInPlace(original, bytes)) {
                const auto writeError = original.errorString();
                // Preserve the old snapshot on failure and restore the same
                // open file where possible, without changing its mapping.
                bool restored = false;
                if (!creating) {
                    original.unsetError();
                    restored = writeInPlace(original, expectedSource);
                }
                original.close();
                error = QStringLiteral("Cannot write the complete configuration: %1").arg(writeError);
                if (creating && !original.remove()) {
                    error += QStringLiteral(" The incomplete new file could not be removed: %1").arg(original.errorString());
                }
                if (!creating && !restored) {
                    error += QStringLiteral(" The previous contents could not be restored; check the file before retrying.");
                }
                return false;
            }
            original.close();
        } else {
            original.close();
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
        }
        if (!verifySavedFile(path, bytes, error)) {
            return false;
        }
        savedSource = bytes;
        return true;
    }

    bool create(const QString& path, QByteArray& savedSource, QString& error) {
        return save(path, QJsonArray{}, QByteArray{}, savedSource, error, SaveMode::InPlace);
    }

    bool remove(const QString& path, const QByteArray& expectedSource, QString& error) {
        error.clear();
        if (path.isEmpty() || expectedSource.isNull()) {
            error = QStringLiteral("Open the configuration before deleting it so its original contents can be verified.");
            return false;
        }
        const auto source = absolutePath(path);
        const QFileInfo selected(source);
        if (!selected.isFile() || selected.isSymLink()) {
            error = QStringLiteral("The selected configuration is missing or is not a regular file. Rescan before deleting it.");
            return false;
        }
        QFile original(source);
        if (!original.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("Cannot read the configuration before deleting: %1").arg(original.errorString());
            return false;
        }
        if (!checkSource(original, expectedSource, error, QStringLiteral("deleting"))) return false;
        // A lower-priority file may become visible at the virtual name after
        // deletion. Verify the selected source, not that newly visible file.
        const auto backing = backingFilePath(source);
        original.close();
        QString removeError;
        if (!removeSingleFile(source, removeError)) {
            error = QStringLiteral("Cannot delete the configuration: %1").arg(removeError);
            return false;
        }
        if (pathExists(backing)) {
            error = QStringLiteral("The selected configuration is still visible after deletion. Rescan and check the mod manager's file mapping before retrying.");
            return false;
        }
        return true;
    }

    bool rename(const QString& oldPath, const QString& newPath, const QByteArray& expectedSource, QString& error) {
        error.clear();
        if (oldPath.isEmpty() || newPath.isEmpty()) {
            error = QStringLiteral("Choose a filename before renaming the configuration.");
            return false;
        }
        const auto source = QDir::cleanPath(QFileInfo(oldPath).absoluteFilePath());
        const auto destination = QDir::cleanPath(QFileInfo(newPath).absoluteFilePath());
#ifdef Q_OS_WIN
        constexpr auto pathCase = Qt::CaseInsensitive;
        const bool caseOnly = source != destination && source.compare(destination, pathCase) == 0;
#else
        constexpr auto pathCase = Qt::CaseSensitive;
        constexpr bool caseOnly = false;
#endif
        if (QFileInfo(source).absolutePath().compare(QFileInfo(destination).absolutePath(), pathCase) != 0) {
            error = QStringLiteral("Rename the configuration within its current folder.");
            return false;
        }
        QFile original(source);
        if (!original.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("Cannot read the original configuration before renaming: %1").arg(original.errorString());
            return false;
        }
        if (!checkSource(original, expectedSource, error, QStringLiteral("renaming"))) return false;
        original.close();
        if (source == destination) return true;
        if (!caseOnly && pathExists(destination)) {
            error = QStringLiteral("A file already exists with that name. Choose another filename.");
            return false;
        }
        QString renameError;
        if (!renameWithoutOverwrite(source, destination, renameError)) {
            error = QStringLiteral("Cannot rename the configuration: %1").arg(renameError);
            return false;
        }
        const bool destinationMatches = hasContents(destination, expectedSource);
        const bool sourceRemains = !caseOnly && pathExists(source);
        if (destinationMatches && !sourceRemains) return true;

        // Only move back a destination whose bytes still match the snapshot,
        // and never replace another file that appeared under the old name.
        QString rollbackError;
        if (!pathExists(source) && hasContents(destination, expectedSource) &&
            renameWithoutOverwrite(destination, source, rollbackError) &&
            hasContents(source, expectedSource) && !pathExists(destination)) {
            error = QStringLiteral("The rename could not be verified. The original filename was restored.");
        } else {
            error = QStringLiteral("The rename could not be verified. Check %1 and %2 before retrying; no additional files were overwritten or removed.")
                        .arg(QDir::toNativeSeparators(source), QDir::toNativeSeparators(destination));
        }
        return false;
    }
}
