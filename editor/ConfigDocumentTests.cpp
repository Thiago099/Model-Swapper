#include "ConfigDocument.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>

#include <iostream>
#include <stdexcept>

namespace {
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
}

int main(int argc, char** argv) {
    QCoreApplication application(argc, argv);
    try {
        QTemporaryDir directory;
        require(directory.isValid(), "Temporary directory must exist");
        testDiscovery(directory.path());
        testValidation();
        testLoadingAndSaving(directory.path());
        std::cout << "ConfigDocument tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
