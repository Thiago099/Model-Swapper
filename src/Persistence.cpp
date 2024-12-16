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
            auto i = 0;
            for (auto item : data) {

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

                std::vector<variant*> models;
                std::string model = Str::processString(originalModel.get<std::string>());

                Time startDate, endDate;

                if (item.contains("ActiveTimeSpan")) {
                    auto pair = Time::fromDatePairString(item["ActiveTimeSpan"].get<std::string>());
                    startDate = pair.first;
                    endDate = pair.second;
                }

                auto j = 0;

                for (auto v : variants) {
                    if (!v.is_string()) {
                        logger::error("error processing item {} on file {} variant {} must be string", i, filename, j);
                        continue;
                    }
                    auto modelPath = strdup(v.get<std::string>().c_str());
                    auto key = strdup(Str::processString(v.get<std::string>()).c_str());
                    auto current = new variant();
                    current->startDate = startDate;
                    current->endDate = endDate;
                    current->model = modelPath;
                    current->key = key;
                    models.push_back(current);

                    ++j;
                }
                manager->Register(model, models);

                ++i;
            }

        } else {
            logger::error("file {}, must be an json array", filename);
        }
    });
}
