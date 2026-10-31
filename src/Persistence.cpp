#include "Persistence.h"

const std::string pattern = "_MS.json";
const std::string directory = "Data/";



void EachConfigFile(std::function<void(std::string, json)> callback) {
    auto manager = Manager::GetSingleton();


    for (const auto& entry : std::filesystem::directory_iterator(directory)) {
        if (entry.is_regular_file() && entry.path().extension() == ".json") {
            const std::string filename = entry.path().filename().string();

            if (filename.size() >= pattern.size() && filename.substr(filename.size() - pattern.size()) == pattern) {
                std::ifstream f(entry.path());
                if (!f) {
                    continue;
                }

                try {
                    json data = json::parse(f);

                    callback(filename, data);

                } catch (const json::parse_error& e) {
                    logger::error("error parsing file {}: {}", filename, e.what());
                }
            }
        }
    }
}


void Persistence::Install() {

    auto ini = Ini::IniReader("ModelSwapper.ini");
    auto config = Config::GetSingleton();
    ini.SetSection("TemporalActivation");
    config->BypassTemporalActivation = ini.GetBool("BypassTemporalActivation", false);
    config->NowOverride = Time::fromString(ini.GetString("NowOverride", ""));
    

    config->NowOverride.log("now override");

    EachConfigFile([](std::string filename, json data) {
        auto manager = Manager::GetSingleton();
        if (data.is_array()) {
            for (std::size_t i = 0; i < data.size(); ++i) {
                const auto& item = data[i];

                if (!item.contains("OriginalModel")) {
                    logger::error("error processing item {} on file {} OriginalModel not found", i, filename);
                    continue;
                }

                if (!item.contains("Variants")) {
                    logger::error("error processing item {} on file {} Variants not found", i, filename);
                    continue;
                }

                auto originalModel = item["OriginalModel"];
                auto variants = item["Variants"];


                if (!originalModel.is_string()) {
                    logger::error("error processing item {} on file {} OriginalModel must be string", i, filename);
                    continue;
                }
                if (!variants.is_array()) {
                    logger::error("error processing item {} on file {} Variants must be variants", i, filename);
                    continue;
                }

                FormFilter filter;
                const FormFilters::Resolver resolve = [](std::uint32_t localID, std::string_view plugin) -> std::uint32_t {
                    auto handler = RE::TESDataHandler::GetSingleton();
                    if (!handler) {
                        return 0;
                    }
                    auto file = handler->LookupModByName(plugin);
                    if (!file || file->GetCompileIndex() == 0xFF || (file->IsLight() && localID > 0xFFF)) {
                        return 0;
                    }
                    return handler->LookupFormID(localID, plugin);
                };
                if (!FormFilters::ReadIDs(item, "BaseFormID", filter.baseFormID, resolve)) {
                    logger::error("error processing item {} on file {}: invalid or unresolved BaseFormID", i, filename);
                    continue;
                }
                if (!FormFilters::ReadIDs(item, "ReferenceID", filter.referenceID, resolve)) {
                    logger::error("error processing item {} on file {}: invalid or unresolved ReferenceID", i, filename);
                    continue;
                }
                if (!FormFilters::ReadIDs(item, "ExcludeBaseFormID", filter.excludeBaseFormID, resolve)) {
                    logger::error("error processing item {} on file {}: invalid or unresolved ExcludeBaseFormID", i, filename);
                    continue;
                }
                if (!FormFilters::ReadIDs(item, "ExcludeReferenceID", filter.excludeReferenceID, resolve)) {
                    logger::error("error processing item {} on file {}: invalid or unresolved ExcludeReferenceID", i, filename);
                    continue;
                }

                std::vector<variant*> models;
                std::string model = Str::processString(originalModel.get<std::string>());

                Time startDate, endDate;

                if (item.contains("ActiveTimeSpan")) {
                    if (!item["ActiveTimeSpan"].is_string()) {
                        logger::error("error processing item {} on file {} ActiveTimeSpan must be string", i, filename);
                        continue;
                    }
                    auto pair = Time::fromDatePairString(item["ActiveTimeSpan"].get<std::string>());
                    startDate = pair.first;
                    endDate = pair.second;
                }

                for (std::size_t j = 0; j < variants.size(); ++j) {
                    const auto& v = variants[j];
                    if (!v.is_string()) {
                        logger::error("error processing item {} on file {} variant {} must be string", i, filename, j);
                        continue;
                    }
                    auto modelPath = strdup(v.get<std::string>().c_str());
                    auto key = strdup(Str::processString(v.get<std::string>()).c_str());
                    auto current = new variant();
                    current->startDate = startDate;
                    current->endDate = endDate;
                    current->filter = filter;
                    current->model = modelPath;
                    current->key = key;
                    models.push_back(current);

                }
                manager->Register(model, models);
            }

        } else {
            logger::error("file {}, must be an json array", filename);
        }
    });
}
