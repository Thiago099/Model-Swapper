#include "ConfigDocument.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace {
    const QStringList conditionIDFields{
        "LocationID", "RegionID", "KeywordID", "CellID", "WorldspaceID",
        "ExcludeLocationID", "ExcludeRegionID", "ExcludeKeywordID", "ExcludeCellID", "ExcludeWorldspaceID"
    };

    void require(bool condition, const char* message) {
        if (!condition) {
            throw std::runtime_error(message);
        }
    }

    void writeFile(const QString& path, const QByteArray& bytes) {
        QFile file(path);
        require(file.open(QIODevice::WriteOnly), "Fixture must open for writing");
        require(file.write(bytes) == bytes.size(), "Fixture must write completely");
    }

    QByteArray readFile(const QString& path) {
        QFile file(path);
        require(file.open(QIODevice::ReadOnly), "Fixture must open for reading");
        return file.readAll();
    }

    QJsonObject validEntry() {
        return {{QStringLiteral("OriginalModel"), QStringLiteral("Example\\Original.nif")},
                {QStringLiteral("Variants"), QJsonArray{QStringLiteral("Example\\Replacement.nif")}}};
    }

    bool accepts(const QJsonObject& entry) {
        return ConfigFiles::validate(QJsonArray{entry}).isEmpty();
    }

    void testDiscovery(const QString& root) {
        require(QDir(root).mkpath(QStringLiteral("nested")), "Nested fixture directory must exist");
        writeFile(root + QStringLiteral("/a_MS.json"), "[]");
        writeFile(root + QStringLiteral("/nested/b_ms.JSON"), "[]");
        writeFile(root + QStringLiteral("/ignored.json"), "[]");
        writeFile(root + QStringLiteral("/not_MS.json.bak"), "[]");
        writeFile(root + QStringLiteral("/Skyrim.esm"), "");
        writeFile(root + QStringLiteral("/Light.ESL"), "");
        writeFile(root + QStringLiteral("/Patch.esp"), "");
        writeFile(root + QStringLiteral("/nested/NotInstalled.esp"), "");
        QString error;
        const auto files = ConfigFiles::discoverFiles(root, &error);
        require(error.isEmpty() && files.size() == 2, "Discover only matching JSON files, recursively and case-insensitively");
        require(files.at(0).endsWith(QStringLiteral("/a_MS.json")), "Discovered files must be sorted");
        require(files.at(1).endsWith(QStringLiteral("/nested/b_ms.JSON")), "Nested JSON must be discovered");
        const auto plugins = ConfigFiles::discoverPlugins(root);
        require(plugins == QStringList{QStringLiteral("Light.ESL"), QStringLiteral("Patch.esp"), QStringLiteral("Skyrim.esm")},
                "Plugin discovery includes ESP/ESM/ESL in the Data root only");
        require(ConfigFiles::discoverFiles(root + QStringLiteral("/missing"), &error).isEmpty() && !error.isEmpty(),
                "Unavailable Data directories must report an error");
    }

    void testSourceDirectoryRescan(const QString& root) {
        const auto fixture = root + QStringLiteral("/source-rescan");
        const auto data = fixture + QStringLiteral("/Data");
        const auto source = fixture + QStringLiteral("/Known mod");
        require(QDir().mkpath(data) && QDir().mkpath(source + QStringLiteral("/nested")), "Create isolated source-rescan directories");
        const auto visible = data + QStringLiteral("/WoodCarts_MS.json");
        const auto mappedSource = source + QStringLiteral("/WoodCarts_MS.json");
        writeFile(visible, "[]");
        writeFile(mappedSource, "[ ]");
        QList<ConfigFiles::SourceDirectory> sources{{source, data}};
        QString error;
        require(ConfigFiles::discoverFiles(data, &error, &sources) == QStringList{visible} && error.isEmpty(),
                "A source anchor must not duplicate an already visible virtual filename");

        const auto external = source + QStringLiteral("/aaa_MS.json");
        const auto nested = source + QStringLiteral("/nested/Seasonal_ms.JSON");
        writeFile(external, "[]");
        writeFile(nested, "[]");
        writeFile(source + QStringLiteral("/ignored.json"), "[]");
        const auto refreshed = ConfigFiles::discoverFiles(data, &error, &sources);
        require(error.isEmpty() && refreshed.size() == 3 && refreshed.contains(visible) &&
                    refreshed.contains(external) && refreshed.contains(nested),
                "A second scan must discover externally added source files, including nested files");
        require(ConfigFiles::discoverFiles(data) == QStringList{visible}, "The legacy discovery API must retain its original scope");
        require(sources.size() == 1 && sources.first().sourcePath == source && sources.first().virtualPath == data,
                "Rescanning must retain the known physical-to-virtual directory anchor");
        require(readFile(visible) == "[]" && readFile(mappedSource) == "[ ]", "Discovery must never change virtual or source file contents");

        require(QFile::remove(visible) && QFile::remove(mappedSource) && QFile::remove(external) && QFile::remove(nested),
                "Remove all configuration fixtures while retaining their source directory");
        require(ConfigFiles::discoverFiles(data, &error, &sources).isEmpty() && sources.size() == 1,
                "Removing the last mapped configuration must not discard its source anchor");
        const auto later = source + QStringLiteral("/Later_MS.json");
        writeFile(later, "[]");
        require(ConfigFiles::discoverFiles(data, &error, &sources) == QStringList{later},
                "A retained source anchor must discover later additions without reopening the editor");

        const auto actual = ConfigFiles::backingFilePath(later);
#ifdef Q_OS_WIN
        require(actual.compare(QFileInfo(later).canonicalFilePath(), Qt::CaseInsensitive) == 0,
                "Backing file resolution must return the real normalized path for an existing file");
#else
        require(actual == QFileInfo(later).canonicalFilePath(), "Backing file resolution must return the canonical path");
#endif
        const auto missing = source + QStringLiteral("/Missing_MS.json");
        require(ConfigFiles::backingFilePath(missing) == missing, "An unresolved backing file must retain its original path");
        const auto otherData = fixture + QStringLiteral("/OtherData");
        require(QDir().mkpath(otherData), "Create an unrelated Data directory");
        require(ConfigFiles::discoverFiles(otherData, &error, &sources).isEmpty() && sources.isEmpty(),
                "Source anchors must never expose files outside their selected virtual Data directory");
    }

    void testSourceDirectoryConflicts(const QString& root) {
        const auto fixture = root + QStringLiteral("/source-conflicts");
        const auto data = fixture + QStringLiteral("/Data");
        const auto first = fixture + QStringLiteral("/FirstMod");
        const auto second = fixture + QStringLiteral("/SecondMod");
        require(QDir().mkpath(data) && QDir().mkpath(first + QStringLiteral("/nested")) && QDir().mkpath(second),
                "Create isolated overlapping source-directory fixtures");
        const auto visible = data + QStringLiteral("/Existing_MS.json");
        writeFile(visible, "[]");
        writeFile(first + QStringLiteral("/Existing_MS.json"), "[ ]");
        writeFile(second + QStringLiteral("/Existing_MS.json"), "[\n]");
        const auto ambiguousFirst = first + QStringLiteral("/UnknownPriority_MS.json");
        const auto ambiguousSecond = second + QStringLiteral("/UnknownPriority_MS.json");
        writeFile(ambiguousFirst, "[]");
        writeFile(ambiguousSecond, "[]");
        const auto uniqueFirst = first + QStringLiteral("/UniqueFirst_MS.json");
        const auto uniqueSecond = second + QStringLiteral("/UniqueSecond_MS.json");
        const auto nested = first + QStringLiteral("/nested/Nested_MS.json");
        writeFile(uniqueFirst, "[]");
        writeFile(uniqueSecond, "[]");
        writeFile(nested, "[]");
        QList<ConfigFiles::SourceDirectory> sources{{first, data}, {second, data}, {first, data},
            {first + QStringLiteral("/nested"), data + QStringLiteral("/nested")}};
        QString error;
        const auto files = ConfigFiles::discoverFiles(data, &error, &sources);
        require(error.isEmpty() && files.size() == 4 && files.contains(visible) && files.contains(uniqueFirst) &&
                    files.contains(uniqueSecond) && files.contains(nested),
                "Virtual filenames must win, unique source files must appear once, and ambiguous new aliases must be omitted");
        require(!files.contains(ambiguousFirst) && !files.contains(ambiguousSecond),
                "Discovery must never guess a mod priority for colliding new aliases");
        require(sources.size() == 3, "Duplicate source anchors must not accumulate between rescans");
        require(ConfigFiles::discoverFiles(data, &error, &sources) == files && sources.size() == 3,
                "Repeated source discovery must be stable and free of duplicate files or anchors");
        const auto nowVisible = data + QStringLiteral("/UniqueFirst_MS.json");
        writeFile(nowVisible, "[]");
        const auto afterMapping = ConfigFiles::discoverFiles(data, &error, &sources);
        require(afterMapping.size() == 4 && afterMapping.contains(nowVisible) && !afterMapping.contains(uniqueFirst),
                "A newly available virtual filename must take precedence over its physical fallback");
    }

    void testValidation() {
        require(accepts(validEntry()), "Legacy entries must validate");
        auto entry = validEntry();
        entry.remove(QStringLiteral("OriginalModel"));
        entry.insert(QStringLiteral("BaseFormID"), QJsonArray{4660, QStringLiteral("0x01001234"),
                                                             QStringLiteral("Uninstalled.esp|001234"), QStringLiteral("Light.esl|FFF")});
        entry.insert(QStringLiteral("ReferenceID"), QStringLiteral("Unknown.esm|0xABCDEF"));
        entry.insert(QStringLiteral("ExcludeBaseFormID"), QJsonArray{});
        entry.insert(QStringLiteral("ExcludeReferenceID"), QJsonArray{QStringLiteral("FF001234"), 4294967295.0});
        entry.insert(QStringLiteral("ActiveTimeSpan"), QStringLiteral("29-02~31-12"));
        require(accepts(entry), "ID-only entries, mixed ID types, missing plugins, empty filters and leap day must validate");
        entry.insert(QStringLiteral("OriginalModel"), QJsonArray{QStringLiteral("One.nif"), QStringLiteral("Two.nif")});
        require(accepts(entry), "Multiple original model paths must validate");

        const QJsonArray invalidIDs{0, -1, 4294967296.0, 1.5, true, QJsonValue::Null,
                                   QStringLiteral("0x0"), QStringLiteral("FFFFFFFFF"), QStringLiteral("Plugin.esp|1000000"),
                                   QStringLiteral("Light.esl|1000"), QStringLiteral("Plugin.esp|"), QStringLiteral("|123"),
                                   QStringLiteral("Plugin.esp|1|2"), QStringLiteral("1 xyz"), QStringLiteral("123\n"), QJsonArray{1}};
        for (const auto& id : invalidIDs) {
            auto invalid = validEntry();
            if (!id.isArray()) {
                invalid.insert(QStringLiteral("BaseFormID"), id);
                require(!accepts(invalid), "Invalid scalar IDs must fail validation");
            }
            invalid.insert(QStringLiteral("BaseFormID"), QJsonArray{QStringLiteral("Plugin.esp|123"), id});
            require(!accepts(invalid), "Any invalid ID array element must fail the entry");
        }
        auto invalid = validEntry();
        invalid.remove(QStringLiteral("OriginalModel"));
        invalid.insert(QStringLiteral("ExcludeReferenceID"), QStringLiteral("Plugin.esp|123"));
        require(!accepts(invalid), "Exclusion-only entries still require OriginalModel");
        invalid = validEntry();
        invalid.insert(QStringLiteral("OriginalModel"), QJsonArray{});
        require(!accepts(invalid), "An explicitly empty model array must fail");
        invalid.insert(QStringLiteral("OriginalModel"), QJsonArray{QStringLiteral("One.nif"), 2});
        require(!accepts(invalid), "Non-string original paths must fail");
        invalid = validEntry();
        invalid.insert(QStringLiteral("Variants"), QJsonArray{});
        require(!accepts(invalid), "An entry with no variants must fail");
        invalid.insert(QStringLiteral("Variants"), QJsonArray{QStringLiteral(" ")});
        require(!accepts(invalid), "Blank variants must fail");
        invalid = validEntry();
        for (const auto& time : {QStringLiteral("31-04~01-05"), QStringLiteral("01-13~02-12"), QStringLiteral("bad")}) {
            invalid.insert(QStringLiteral("ActiveTimeSpan"), time);
            require(!accepts(invalid), "Invalid calendar spans must fail");
        }
        const auto errors = ConfigFiles::validate(QJsonArray{QJsonObject{}});
        require(errors.size() == 2 && errors.first().contains(QStringLiteral("Entry [0].Variants")),
                "Validation must identify the entry and offending field");
    }

    void testLoadingAndSaving(const QString& root) {
        const auto path = root + QStringLiteral("/Editing_MS.json");
        auto entry = validEntry();
        entry.insert(QStringLiteral("BaseFormID"), QJsonArray{4660, QStringLiteral("Unknown.esp|001234")});
        const QJsonArray conditionForms{QString("Unknown.esp|001234"), QString("0x01001234"), 4660,
                                       QJsonArray{}, QJsonArray{4660, "Light.esl|ABC", "0x01001234"}};
        for (qsizetype index = 0; index < conditionIDFields.size(); ++index)
            entry.insert(conditionIDFields.at(index), conditionForms.at(index % conditionForms.size()));
        entry.insert(QStringLiteral("Extra"), QJsonObject{{QStringLiteral("nested"), QJsonArray{true, QJsonValue::Null, 2.25}},
                                                          {QStringLiteral("largeInteger"), QJsonValue(qint64(9007199254740993LL))}});
        const QJsonArray expected{entry};
        const auto source = QJsonDocument(expected).toJson(QJsonDocument::Compact);
        writeFile(path, source);
        QJsonArray entries;
        QByteArray loaded;
        QString error;
        require(ConfigFiles::load(path, entries, loaded, error), "A valid document must load");
        require(entries == expected && loaded == source, "Loading must preserve JSON data and exact source bytes");
        QByteArray saved;
        require(ConfigFiles::save(path, entries, loaded, saved, error), "An unchanged document must save atomically");
        require(saved == readFile(path), "Returned saved bytes must match the file");
        require(QJsonDocument::fromJson(saved).array() == expected, "Saving must preserve unknown values and ID representations");
        const auto external = QByteArray("[\n]\n");
        writeFile(path, external);
        require(!ConfigFiles::save(path, entries, saved, loaded, error) && error.contains(QStringLiteral("changed outside")),
                "Saving must detect concurrent external changes");
        require(readFile(path) == external, "Conflict detection must not overwrite external changes");
        require(!ConfigFiles::save(path, expected, QByteArray(), loaded, error), "Creating a document must never overwrite an existing file");
        const auto newPath = root + QStringLiteral("/New_MS.json");
        require(ConfigFiles::save(newPath, expected, QByteArray(), loaded, error), "Saving a new document must succeed");
        const auto newBytes = readFile(newPath);
        require(!ConfigFiles::save(newPath, QJsonArray{QJsonObject{}}, loaded, saved, error), "Saving invalid entries must fail");
        require(readFile(newPath) == newBytes, "Validation failure must leave the original file intact");

        writeFile(path, "[{broken]");
        require(!ConfigFiles::load(path, entries, loaded, error) && error.contains(QStringLiteral("byte")),
                "Malformed JSON must report the parse position");
        writeFile(path, "{}");
        require(!ConfigFiles::load(path, entries, loaded, error), "The document root must be an array");
        writeFile(path, "[1]");
        require(!ConfigFiles::load(path, entries, loaded, error), "Entries must be objects");
        writeFile(path, "[{}]");
        require(ConfigFiles::load(path, entries, loaded, error) && !ConfigFiles::validate(entries).isEmpty(),
                "Invalid entry fields must still load so they can be repaired in the editor");
    }

    void testConditionValidation() {
        const QJsonArray validIDs{4660, 4294967295.0, QString("0x01001234"), QString("FFFFFFFF"),
                                 QString("Missing.esp|FFFFFF"), QString("Light.esl|FFF")};
        const QJsonArray invalidIDs{0, -1, 4294967296.0, 1.5, true, QJsonValue::Null, QJsonObject{},
                                   QString("WhiterunLocation"), QString(""), QString("0x0"),
                                   QString("FFFFFFFFF"), QString("Missing.esp|1000000"), QString("Light.esl|1000"),
                                   QString("Missing.esp|"), QString("|123"), QString("Missing.esp|1|2"),
                                   QString("123\n"), QJsonArray{1}};
        for (const auto& field : conditionIDFields) {
            auto entry = validEntry();
            for (const auto& id : validIDs) {
                entry.insert(field, id);
                require(accepts(entry), "Every condition field must accept existing scalar ID representations");
            }
            entry.insert(field, validIDs);
            require(accepts(entry), "Every condition field must accept mixed ID arrays");
            entry.insert(field, QJsonArray{});
            require(accepts(entry), "Empty condition arrays must remain valid and preserve their runtime semantics");
            for (const auto& id : invalidIDs) {
                if (!id.isArray()) {
                    entry.insert(field, id);
                    const auto errors = ConfigFiles::validate(QJsonArray{entry});
                    require(!errors.isEmpty() && errors.join('\n').contains(field),
                            "Invalid condition IDs must be rejected with a field-specific error");
                }
                entry.insert(field, QJsonArray{QString("Missing.esp|123"), id});
                require(!accepts(entry), "Any invalid condition array element must reject the entry");
            }
            entry.insert(field, QString("Missing.esp|123"));
            entry.remove(QStringLiteral("OriginalModel"));
            require(!accepts(entry), "Context conditions must not replace the existing model/base/reference target requirement");
            entry.insert(QStringLiteral("ReferenceID"), QString("Missing.esp|456"));
            require(accepts(entry), "Context conditions must work alongside existing ID-only targets");
        }
    }

    void testRepeatedSaveSnapshots(const QString& root) {
        const auto path = root + QStringLiteral("/RepeatedSave_MS.json");
        writeFile(path, QJsonDocument(QJsonArray{validEntry()}).toJson(QJsonDocument::Compact));
        QJsonArray entries;
        QByteArray snapshot;
        QString error;
        require(ConfigFiles::load(path, entries, snapshot, error), "Repeated-save fixture must load");
        for (int revision = 1; revision <= 3; ++revision) {
            auto entry = entries.first().toObject();
            entry.insert(QStringLiteral("Variants"), QJsonArray{QStringLiteral("Example\\Revision%1.nif").arg(revision)});
            entry.insert(QStringLiteral("LocationID"), QStringLiteral("Example.esp|%1").arg(revision, 6, 16, QLatin1Char('0')));
            entries[0] = entry;
            QByteArray saved;
            require(ConfigFiles::save(path, entries, snapshot, saved, error), "Successive edits to one loaded document must save");
            require(saved == readFile(path), "Each successful save must return the exact bytes visible on disk");
            require(QJsonDocument::fromJson(saved).array() == entries, "Every revision must actually replace the file's JSON contents");
            snapshot = saved;
        }
        require(ConfigFiles::save(path, entries, snapshot, snapshot, error),
                "Using the same source snapshot for input and output must remain safe");
        require(snapshot == readFile(path), "An aliased source snapshot must track the final disk bytes");
        const auto beforeConflict = snapshot;
        const QByteArray external("[\n]\n");
        writeFile(path, external);
        require(!ConfigFiles::save(path, entries, snapshot, snapshot, error) && error.contains(QStringLiteral("changed outside")),
                "Repeated saves must retain protection against a later external edit");
        require(snapshot == beforeConflict && readFile(path) == external,
                "A conflicting save must preserve both the source snapshot and external contents");
        std::cout << "Repeated save snapshots passed\n";
    }

    void testInPlaceSaves(const QString& root) {
        const auto path = root + QStringLiteral("/InPlace_MS.json");
        QJsonArray entries{validEntry()};
        QByteArray snapshot;
        QString error;
        require(ConfigFiles::save(path, entries, QByteArray(), snapshot, error, ConfigFiles::SaveMode::InPlace),
                "In-place mode must create a new configuration exclusively");
        require(snapshot == readFile(path), "New in-place files must return their visible disk bytes");
        QByteArray unchangedOutput("unchanged output");
        require(!ConfigFiles::save(path, entries, QByteArray(), unchangedOutput, error, ConfigFiles::SaveMode::InPlace),
                "In-place new-file mode must never overwrite an existing configuration");
        require(unchangedOutput == "unchanged output" && readFile(path) == snapshot,
                "A rejected exclusive create must leave both output and file contents intact");

        for (const int length : {16384, 3, 32768, 0}) {
            auto entry = entries.first().toObject();
            entry.insert(QStringLiteral("PreservedExtra"), QString(length, QLatin1Char('x')));
            entries[0] = entry;
            QByteArray saved;
            require(ConfigFiles::save(path, entries, snapshot, saved, error, ConfigFiles::SaveMode::InPlace),
                    "Successive in-place edits must support both growing and shrinking documents");
            const auto diskBytes = readFile(path);
            require(saved == diskBytes && diskBytes.size() == QJsonDocument(entries).toJson(QJsonDocument::Indented).size(),
                    "In-place saves must write completely and truncate old trailing bytes");
            require(QJsonDocument::fromJson(diskBytes).array() == entries,
                    "Growing and shrinking in-place saves must preserve exactly the edited JSON");
            snapshot = saved;
        }
        require(ConfigFiles::save(path, entries, snapshot, snapshot, error, ConfigFiles::SaveMode::InPlace) && snapshot == readFile(path),
                "In-place saving must support the same input and output snapshot object");
        const auto beforeConflict = snapshot;
        const QByteArray external("[\n]\n");
        writeFile(path, external);
        require(!ConfigFiles::save(path, entries, snapshot, snapshot, error, ConfigFiles::SaveMode::InPlace) &&
                error.contains(QStringLiteral("changed outside")), "In-place saves must reject a real external edit before writing");
        require(snapshot == beforeConflict && readFile(path) == external,
                "An in-place conflict must preserve the external edit and caller snapshot");

        const auto missingPath = root + QStringLiteral("/DeletedBeforeInPlaceSave_MS.json");
        writeFile(missingPath, beforeConflict);
        require(QFile::remove(missingPath), "Deleted-file fixture must be removed before its next save");
        unchangedOutput = "unchanged output";
        require(!ConfigFiles::save(missingPath, entries, beforeConflict, unchangedOutput, error, ConfigFiles::SaveMode::InPlace),
                "An existing-file in-place save must not silently recreate a deleted configuration");
        require(!QFile::exists(missingPath) && unchangedOutput == "unchanged output",
                "A missing original must leave the path missing and output snapshot unchanged");
        std::cout << "In-place repeated saves passed\n";
    }

    void testImmediateCreation(const QString& root) {
        const auto path = root + QStringLiteral("/ImmediatelyCreated_MS.json");
        QByteArray source("unchanged output");
        QString error;
        require(ConfigFiles::create(path, source, error), "Creating a configuration must immediately create its file");
        require(QFile::exists(path) && !source.isNull() && source == readFile(path),
                "Creation must return the exact bytes already visible on disk");
        const auto document = QJsonDocument::fromJson(source);
        require(document.isArray() && document.array().isEmpty(), "A new configuration must contain an empty valid JSON array");
        QJsonArray entries;
        QByteArray loaded;
        require(ConfigFiles::load(path, entries, loaded, error) && entries.isEmpty() && loaded == source,
                "A newly created empty configuration must be loadable before adding entries");

        QByteArray rejectedSource("unchanged output");
        require(!ConfigFiles::create(path, rejectedSource, error) && !error.isEmpty(),
                "Repeated creation must refuse to overwrite an existing file");
        require(readFile(path) == source && rejectedSource == "unchanged output",
                "A rejected create must preserve file contents and the caller output snapshot");
        const auto unavailablePath = root + QStringLiteral("/missing-create-directory/Unavailable_MS.json");
        require(!ConfigFiles::create(unavailablePath, rejectedSource, error) && !error.isEmpty(),
                "Creating inside an unavailable directory must fail clearly");
        require(!QFile::exists(unavailablePath) && rejectedSource == "unchanged output",
                "A failed create must not report a source snapshot or leave a file");

        const auto renamedPath = root + QStringLiteral("/RenamedEmpty_MS.json");
        require(ConfigFiles::rename(path, renamedPath, source, error), "An immediately created empty configuration must be renameable");
        require(!QFile::exists(path) && readFile(renamedPath) == source, "Renaming an empty configuration must move its existing file");
        entries = QJsonArray{validEntry()};
        QByteArray saved;
        require(ConfigFiles::save(renamedPath, entries, source, saved, error),
                "The empty creation snapshot must remain valid for saving entries after rename");
        require(ConfigFiles::load(renamedPath, entries, loaded, error) && entries == QJsonArray{validEntry()} && loaded == saved,
                "A renamed new configuration must persist and reload its first entry normally");
    }

    void testRemoving(const QString& root) {
        const auto fixture = root + QStringLiteral("/removal");
        require(QDir().mkpath(fixture), "Create an isolated file-deletion fixture directory");
        const auto path = fixture + QStringLiteral("/Selected_MS.json");
        const auto unrelated = fixture + QStringLiteral("/Unrelated_MS.json");
        const QByteArray source("[\r\n  {\"Title\":\"Original\",\"Unknown\":true}\r\n]\r\n");
        const QByteArray unrelatedSource("[ ]\n");
        writeFile(path, source);
        writeFile(unrelated, unrelatedSource);
        QString error("previous error");
        require(!ConfigFiles::remove(path, QByteArray{}, error) && !error.isEmpty() && readFile(path) == source,
                "Deletion must require a loaded source snapshot and preserve an unverified file");
        require(!ConfigFiles::remove(QString{}, source, error) && !error.isEmpty(),
                "Deletion must refuse an empty path");
        const QByteArray external("[\n]\n");
        writeFile(path, external);
        require(!ConfigFiles::remove(path, source, error) && error.contains(QStringLiteral("changed outside")),
                "Deletion must refuse a configuration changed after loading");
        require(readFile(path) == external && readFile(unrelated) == unrelatedSource,
                "A deletion conflict must preserve the external edit and unrelated configurations");
        writeFile(path, source);
        error = "previous error";
        require(ConfigFiles::remove(path, source, error) && error.isEmpty() && !QFile::exists(path),
                "Deleting a verified configuration must remove exactly its selected name and clear prior errors");
        require(readFile(unrelated) == unrelatedSource, "Successful deletion must leave unrelated files untouched");
        require(!ConfigFiles::remove(path, source, error) && !error.isEmpty() && !QFile::exists(path),
                "Deleting an already missing configuration must fail without recreating it");

        const auto directory = fixture + QStringLiteral("/Directory_MS.json");
        const auto child = directory + QStringLiteral("/Child_MS.json");
        require(QDir().mkpath(directory), "Create a directory named like a configuration");
        writeFile(child, source);
        require(!ConfigFiles::remove(directory, source, error) && !error.isEmpty() &&
                    QFileInfo(directory).isDir() && readFile(child) == source && readFile(unrelated) == unrelatedSource,
                "Deletion must refuse directories and preserve every child and unrelated file");
    }

    void testRenaming(const QString& root) {
        const auto oldPath = root + QStringLiteral("/BeforeRename_MS.json");
        const auto newPath = root + QStringLiteral("/AfterRename_MS.json");
        auto entry = validEntry();
        entry.insert("Unknown", QJsonObject{{"text", "first\nsecond"}, {"values", QJsonArray{true, QJsonValue::Null}},
                                           {"largeInteger", QJsonValue(qint64(9007199254740993LL))}});
        auto source = QJsonDocument(QJsonArray{entry}).toJson(QJsonDocument::Indented);
        source.replace("\n", "\r\n");
        writeFile(oldPath, source);
        QString error;
        require(ConfigFiles::rename(oldPath, oldPath, source, error), "Renaming to the identical path must be a safe no-op");
        require(readFile(oldPath) == source, "An identical-name rename must not rewrite formatting or data");
        require(ConfigFiles::rename(oldPath, newPath, source, error), "Renaming a loaded configuration must succeed");
        require(!QFile::exists(oldPath) && QFile::exists(newPath) && readFile(newPath) == source,
                "Rename must remove the old name and preserve exact bytes, unknown properties and CRLF formatting");
        QJsonArray entries;
        QByteArray loaded;
        require(ConfigFiles::load(newPath, entries, loaded, error) && entries == QJsonArray{entry} && loaded == source,
                "The renamed file must load with its original source snapshot and JSON values");

        const auto collisionPath = root + QStringLiteral("/RenameCollision_MS.json");
        const QByteArray collisionContents("[\r\n]\r\n");
        writeFile(collisionPath, collisionContents);
        require(!ConfigFiles::rename(newPath, collisionPath, source, error) && !error.isEmpty(),
                "Rename must refuse an existing destination");
        require(readFile(newPath) == source && readFile(collisionPath) == collisionContents,
                "A rename collision must preserve both source and destination files");

        const auto externalTarget = root + QStringLiteral("/ExternalRenameTarget_MS.json");
        const QByteArray external("[]\n");
        writeFile(newPath, external);
        require(!ConfigFiles::rename(newPath, externalTarget, source, error) && error.contains(QStringLiteral("changed outside")),
                "Rename must detect external changes to the loaded source");
        require(readFile(newPath) == external && !QFile::exists(externalTarget),
                "A refused stale-source rename must preserve the external contents and original filename");
        require(!ConfigFiles::rename(newPath, newPath, source, error),
                "Even an identical-name no-op must not accept a stale source snapshot");
        writeFile(newPath, source);

        const auto missingPath = root + QStringLiteral("/MissingRenameSource_MS.json");
        const auto missingTarget = root + QStringLiteral("/MissingRenameTarget_MS.json");
        require(!ConfigFiles::rename(missingPath, missingTarget, source, error) && !error.isEmpty(),
                "Renaming a missing source must fail clearly");
        require(!QFile::exists(missingPath) && !QFile::exists(missingTarget), "A missing-source rename must create neither path");

        require(QDir(root).mkpath(QStringLiteral("rename-other-directory")), "Rename directory fixture must exist");
        const auto otherDirectoryTarget = root + QStringLiteral("/rename-other-directory/Moved_MS.json");
        require(!ConfigFiles::rename(newPath, otherDirectoryTarget, source, error) && !error.isEmpty(),
                "Configuration renaming must not move files to another directory");
        require(readFile(newPath) == source && !QFile::exists(otherDirectoryTarget),
                "A refused directory change must leave the source and destination intact");
        const auto nestedSource = root + QStringLiteral("/rename-other-directory/NestedSource_MS.json");
        const auto traversalTarget = root + QStringLiteral("/rename-other-directory/../TraversalEscape_MS.json");
        writeFile(nestedSource, source);
        require(!ConfigFiles::rename(nestedSource, traversalTarget, source, error) && !error.isEmpty(),
                "A parent-directory traversal must not escape the source directory during rename");
        require(readFile(nestedSource) == source && !QFile::exists(traversalTarget),
                "A rejected traversal must preserve its source and create no destination");
    }

#if defined(Q_OS_WIN)
    void testHardLinkedRemoval(const QString& root) {
        const auto selected = root + QStringLiteral("/DeleteSelectedAlias_MS.json");
        const auto retained = root + QStringLiteral("/DeleteRetainedAlias_MS.json");
        const QByteArray source("[\r\n]\r\n");
        writeFile(selected, source);
        std::error_code linkError;
        std::filesystem::create_hard_link(std::filesystem::path(selected.toStdWString()),
                                          std::filesystem::path(retained.toStdWString()), linkError);
        require(!linkError, "Create an isolated hard-linked deletion fixture");
        QString error;
        require(ConfigFiles::remove(selected, source, error) && error.isEmpty() && !QFile::exists(selected),
                "Deleting a hard-linked configuration must remove the selected alias only");
        require(readFile(retained) == source &&
                    std::filesystem::hard_link_count(std::filesystem::path(retained.toStdWString())) == 1,
                "Deleting one alias must preserve the other alias and its exact contents");
    }

    void testCaseOnlyRename(const QString& root) {
        const auto oldPath = root + QStringLiteral("/CaseOriginal_MS.json");
        const auto newPath = root + QStringLiteral("/caseoriginal_MS.json");
        const auto source = QJsonDocument(QJsonArray{validEntry()}).toJson(QJsonDocument::Compact);
        writeFile(oldPath, source);
        QString error;
        const bool renamed = ConfigFiles::rename(oldPath, newPath, source, error);
        const auto names = QDir(root).entryList(QDir::Files);
        if (renamed) {
            require(readFile(newPath) == source && names.contains(QStringLiteral("caseoriginal_MS.json")) &&
                    !names.contains(QStringLiteral("CaseOriginal_MS.json")),
                    "A successful case-only rename must update filename spelling while preserving contents");
        } else {
            require(!error.isEmpty() && readFile(oldPath) == source && names.contains(QStringLiteral("CaseOriginal_MS.json")),
                    "An unsupported case-only rename must fail clearly and retain its original name and contents");
        }
    }

    void testHardLinkedRename(const QString& root) {
        const auto stagingPath = root + QStringLiteral("/RenameStaging_MS.json");
        const auto deployedPath = root + QStringLiteral("/RenameDeployed_MS.json");
        const auto renamedPath = root + QStringLiteral("/RenamedDeployed_MS.json");
        const auto nativeStaging = std::filesystem::path(stagingPath.toStdWString());
        const auto nativeRenamed = std::filesystem::path(renamedPath.toStdWString());
        const auto source = QJsonDocument(QJsonArray{validEntry()}).toJson(QJsonDocument::Compact);
        writeFile(stagingPath, source);
        std::error_code linkError;
        std::filesystem::create_hard_link(nativeStaging, std::filesystem::path(deployedPath.toStdWString()), linkError);
        require(!linkError, "Hard-linked rename fixture must be created on the same volume");
        QString error;
        require(ConfigFiles::rename(deployedPath, renamedPath, source, error), "A deployed hard-linked configuration must be renameable");
        require(!QFile::exists(deployedPath) && readFile(renamedPath) == source && readFile(stagingPath) == source,
                "Renaming one deployment alias must preserve its bytes and the untouched staging alias");
        require(std::filesystem::hard_link_count(nativeStaging) == 2 && std::filesystem::hard_link_count(nativeRenamed) == 2,
                "Renaming a hard-linked file must preserve its shared backing file");
        auto entry = validEntry();
        entry.insert("Variants", QJsonArray{"Example\\RenamedLinkedReplacement.nif"});
        QByteArray saved;
        require(ConfigFiles::save(renamedPath, QJsonArray{entry}, source, saved, error),
                "A renamed deployment alias must remain editable with the loaded snapshot");
        require(readFile(renamedPath) == saved && readFile(stagingPath) == saved,
                "Saving a renamed deployment alias must still update its staging file");
    }

    void testHardLinkedSave(const QString& root) {
        const auto stagingPath = root + QStringLiteral("/Staging_MS.json");
        const auto deployedPath = root + QStringLiteral("/Deployed_MS.json");
        const auto nativeStaging = std::filesystem::path(stagingPath.toStdWString());
        const auto nativeDeployed = std::filesystem::path(deployedPath.toStdWString());
        writeFile(stagingPath, QJsonDocument(QJsonArray{validEntry()}).toJson(QJsonDocument::Compact));
        std::error_code linkError;
        std::filesystem::create_hard_link(nativeStaging, nativeDeployed, linkError);
        if (linkError) std::cerr << "Cannot create hard-link fixture: " << linkError.message() << " (" << linkError.value() << ")\n";
        require(!linkError, "Windows hard-link fixture must be created on the same volume");
        require(std::filesystem::hard_link_count(nativeStaging) == 2, "Fixture paths must initially refer to the same file");
        QJsonArray entries;
        QByteArray snapshot;
        QString error;
        require(ConfigFiles::load(deployedPath, entries, snapshot, error), "Deployed hard-linked configuration must load");
        auto entry = entries.first().toObject();
        entry.insert(QStringLiteral("Variants"), QJsonArray{QStringLiteral("Example\\FirstLinkedRevision.nif")});
        entries[0] = entry;
        QByteArray firstSaved;
        require(ConfigFiles::save(deployedPath, entries, snapshot, firstSaved, error), "Saving a deployed hard-linked file must succeed");
        const bool deployedUpdated = readFile(deployedPath) == firstSaved;
        const bool stagingUpdated = readFile(stagingPath) == firstSaved;
        std::cout << "Hard-link first save: deployed updated=" << deployedUpdated
                  << ", staging updated=" << stagingUpdated << '\n';
        require(deployedUpdated && stagingUpdated, "Saving through one hard-link path must update both deployed and staging contents");
        require(std::filesystem::hard_link_count(nativeDeployed) == 2,
                "Saving a hard-linked configuration must preserve its deployment aliases");
        snapshot = firstSaved;
        entry.insert(QStringLiteral("Variants"), QJsonArray{QStringLiteral("Example\\SecondLinkedRevision.nif")});
        entries[0] = entry;
        QByteArray secondSaved;
        require(ConfigFiles::save(stagingPath, entries, snapshot, secondSaved, error),
                "A second edit through the other hard-link alias must save without a false conflict");
        require(readFile(stagingPath) == secondSaved && readFile(deployedPath) == secondSaved,
                "Successive hard-linked saves must stay visible through both aliases");
        const QByteArray external("[\n]\n");
        writeFile(stagingPath, external);
        QByteArray rejectedSnapshot("unchanged output");
        require(!ConfigFiles::save(deployedPath, entries, secondSaved, rejectedSnapshot, error) &&
                error.contains(QStringLiteral("changed outside")), "External edits through an alias must still cause a conflict");
        require(readFile(stagingPath) == external && readFile(deployedPath) == external && rejectedSnapshot == "unchanged output",
                "Conflict handling must preserve external alias contents and the caller's saved snapshot");
    }
#endif
}

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    try {
        QTemporaryDir directory;
        require(directory.isValid(), "Temporary directory must exist");
        testDiscovery(directory.path());
        testSourceDirectoryRescan(directory.path());
        testSourceDirectoryConflicts(directory.path());
        testValidation();
        testConditionValidation();
        testLoadingAndSaving(directory.path());
        testRepeatedSaveSnapshots(directory.path());
        testInPlaceSaves(directory.path());
        testImmediateCreation(directory.path());
        testRemoving(directory.path());
        testRenaming(directory.path());
#if defined(Q_OS_WIN)
        testHardLinkedRemoval(directory.path());
        testCaseOnlyRename(directory.path());
        testHardLinkedRename(directory.path());
        testHardLinkedSave(directory.path());
#endif
        std::cout << "ConfigDocument tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
