#include "FormFilter.h"
#include "Manager.h"
#include "Persistence.h"

using json = nlohmann::json;

namespace {
    int checks = 0;

    void Require(bool condition, const char* message) {
        ++checks;
        if (!condition) {
            std::cerr << "FAIL: " << message << '\n';
            std::exit(1);
        }
    }

    variant MakeVariant(const char* model, const char* key,
        std::optional<std::uint32_t> base = {}, std::optional<std::uint32_t> ref = {}) {
        variant result{};
        result.model = model;
        result.key = key;
        if (base) result.filter.baseFormID = std::vector<std::uint32_t>{*base};
        if (ref) result.filter.referenceID = std::vector<std::uint32_t>{*ref};
        return result;
    }

    void TestFilter() {
        FormFilter filter;
        Require(!filter.IsScoped(), "omitted filters remain unrestricted");
        Require(filter.Matches(1, 2), "unrestricted filter accepts existing configs");
        filter.baseFormID = std::vector<std::uint32_t>{0x123};
        Require(filter.IsScoped(), "base filter is scoped");
        Require(filter.Matches(0x123, 2), "base filter accepts matching base");
        Require(!filter.Matches(0x124, 2), "base filter rejects other base");
        filter.referenceID = std::vector<std::uint32_t>{0x456};
        Require(filter.Matches(0x123, 0x456), "combined filters accept both matches");
        Require(!filter.Matches(0x123, 0x457), "combined filters reject other reference");
        Require(!filter.Matches(0x124, 0x456), "combined filters reject other base");
        filter.baseFormID.reset();
        Require(filter.Matches(0x124, 0x456), "reference filter accepts any matching base");
        Require(!filter.Matches(0x124, 0x457), "reference filter rejects other reference");

        filter.baseFormID = std::vector<std::uint32_t>{0x123, 0x124};
        filter.referenceID = std::vector<std::uint32_t>{0x456, 0x457};
        Require(filter.Matches(0x123, 0x457), "base and reference lists accept independent matches");
        Require(filter.Matches(0x124, 0x456), "all list members can match");
        Require(!filter.Matches(0x125, 0x456), "base outside inclusion list rejects entry");
        Require(!filter.Matches(0x123, 0x458), "reference outside inclusion list rejects entry");
        filter.excludeBaseFormID = std::vector<std::uint32_t>{0x124, 0x125};
        filter.excludeReferenceID = std::vector<std::uint32_t>{0x457, 0x458};
        Require(filter.Matches(0x123, 0x456), "nonexcluded included pair remains eligible");
        Require(!filter.Matches(0x124, 0x456), "base exclusion overrides base inclusion");
        Require(!filter.Matches(0x123, 0x457), "reference exclusion overrides reference inclusion");
        filter.baseFormID.reset();
        filter.referenceID.reset();
        Require(filter.IsScoped(), "exclusion-only entry is scoped");
        Require(filter.Matches(0x126, 0x459), "exclusion-only entry accepts other bases and references");
        Require(!filter.Matches(0x125, 0x459), "any excluded base rejects entry without inclusion");
        Require(!filter.Matches(0x126, 0x458), "any excluded reference rejects entry without inclusion");
        filter.excludeBaseFormID = std::vector<std::uint32_t>{};
        filter.excludeReferenceID = std::vector<std::uint32_t>{};
        Require(!filter.IsScoped(), "empty exclusion lists impose no scope");
        Require(filter.Matches(0x124, 0x457), "empty exclusion lists exclude nothing");
        filter.baseFormID = std::vector<std::uint32_t>{};
        Require(filter.IsScoped(), "empty base inclusion still scopes entry");
        Require(!filter.Matches(0x124, 0x457), "empty base inclusion matches nothing");
        filter.baseFormID.reset();
        filter.referenceID = std::vector<std::uint32_t>{};
        Require(!filter.Matches(0x124, 0x457), "empty reference inclusion matches nothing");
    }

    void TestIDParsing() {
        std::uint32_t id = 0;
        Require(FormFilters::ParseID(json(1), id) && id == 1, "positive integer parses");
        Require(FormFilters::ParseID(json(std::uint64_t{0xFFFFFFFF}), id) && id == 0xFFFFFFFF,
            "maximum form ID parses without overflow");
        Require(FormFilters::ParseID(json("000123AB"), id) && id == 0x123AB, "hex string parses");
        Require(FormFilters::ParseID(json("0x000123ab"), id) && id == 0x123AB, "prefixed hex string parses");
        Require(FormFilters::ParseID(json("0XFFFFFFFF"), id) && id == 0xFFFFFFFF, "uppercase prefix parses");
        const std::vector<json> invalid{nullptr, true, false, 0, -1, 1.5,
            std::uint64_t{0x100000000}, "", "0", "0x0", "0x", "xyz", "123junk", "100000000",
            json::array({1}), json::object({{"id", 1}})};
        for (const auto& value : invalid) {
            Require(!FormFilters::ParseID(value, id), "invalid ID fails closed");
        }
        std::uint32_t seenLocal = 0;
        std::string seenPlugin;
        const auto resolver = [&](std::uint32_t local, std::string_view plugin) {
            seenLocal = local;
            seenPlugin = plugin;
            return plugin == "Example.esp" ? 0xFE001ABC : 0u;
        };
        Require(FormFilters::ParseID(json("Example.esp|0x1ABC"), id, resolver) && id == 0xFE001ABC,
            "plugin-qualified ID resolves to loaded form ID");
        Require(seenLocal == 0x1ABC && seenPlugin == "Example.esp", "resolver receives plugin and local ID");
        Require(!FormFilters::ParseID(json("Missing.esp|1ABC"), id, resolver), "missing plugin rejects scoped rule");
        Require(!FormFilters::ParseID(json("Example.esp|1ABC"), id), "plugin form requires resolver");
        Require(!FormFilters::ParseID(json("|1ABC"), id, resolver), "empty plugin rejects scoped rule");
        Require(!FormFilters::ParseID(json("Example.esp|"), id, resolver), "empty local ID rejects scoped rule");
        Require(!FormFilters::ParseID(json("Example.esp|1000000"), id, resolver), "plugin local ID rejects 24-bit overflow");
        const auto lightResolver = [](std::uint32_t local, std::string_view plugin) {
            return plugin == "Light.esl" && local <= 0xFFF ? 0xFE001000 | local : 0u;
        };
        Require(FormFilters::ParseID(json("Light.esl|FFF"), id, lightResolver) && id == 0xFE001FFF,
            "light plugin accepts largest 12-bit local ID");
        Require(!FormFilters::ParseID(json("Light.esl|1000"), id, lightResolver), "light plugin rejects 12-bit overflow");
        std::optional<std::vector<std::uint32_t>> parsed;
        const auto legacy = json::parse(R"([{"OriginalModel":"old.nif","Variants":["new.nif"]}])");
        Require(legacy.is_array(), "legacy format remains a root array");
        Require(FormFilters::ReadIDs(legacy[0], "BaseFormID", parsed) && !parsed,
            "legacy entry missing BaseFormID remains unrestricted");
        Require(FormFilters::ReadIDs(json{{"ReferenceID", "0xABC"}}, "ReferenceID", parsed) &&
            parsed == std::vector<std::uint32_t>{0xABC},
            "optional field parses in existing entry");
        Require(!FormFilters::ReadIDs(json{{"ReferenceID", "bogus"}}, "ReferenceID", parsed),
            "invalid present field rejects entry");

        const auto arrayResolver = [](std::uint32_t local, std::string_view plugin) {
            if (plugin == "Example.esp") return 0x01000000 | local;
            if (plugin == "Light.esl" && local <= 0xFFF) return 0xFE001000 | local;
            return 0u;
        };
        const std::array<const char*, 4> fields{"BaseFormID", "ReferenceID", "ExcludeBaseFormID", "ExcludeReferenceID"};
        for (const auto field : fields) {
            Require(FormFilters::ReadIDs(json{{field, "0x123"}}, field, parsed) &&
                parsed == std::vector<std::uint32_t>{0x123}, "every filter field accepts a scalar string");
            Require(FormFilters::ReadIDs(json{{field, 291}}, field, parsed) &&
                parsed == std::vector<std::uint32_t>{0x123}, "every filter field preserves decimal integer support");
            Require(FormFilters::ReadIDs(json{{field, json::array({"123", "0X456", 1929,
                "Example.esp|ABC", "Light.esl|DEF"})}}, field, parsed, arrayResolver) &&
                parsed == std::vector<std::uint32_t>{0x123, 0x456, 0x789, 0x01000ABC, 0xFE001DEF},
                "every filter field parses mixed raw and plugin-qualified ID arrays");
            Require(FormFilters::ReadIDs(json{{field, json::array()}}, field, parsed) && parsed && parsed->empty(),
                "empty array remains distinct from omitted filter");
            Require(FormFilters::ReadIDs(json::object(), field, parsed) && !parsed,
                "omitted field clears previous list");
            const std::vector<json> invalidLists{nullptr, true, json::object(),
                json::array({"123", "bogus"}), json::array({"123", 0}), json::array({"123", -1}),
                json::array({"123", std::uint64_t{0x100000000}}), json::array({"123", nullptr}),
                json::array({"123", json::array({"456"})}), json::array({"123", "Missing.esp|ABC"}),
                json::array({"123", "Light.esl|1000"})};
            for (const auto& value : invalidLists) {
                Require(!FormFilters::ReadIDs(json{{field, value}}, field, parsed, arrayResolver),
                    "invalid or unresolved ID anywhere in a filter rejects whole entry");
            }
        }
    }

    void TestSelection() {
        auto first = MakeVariant("a.nif", "a.nif");
        auto second = MakeVariant("b.nif", "b.nif");
        auto third = MakeVariant("c.nif", "c.nif");
        models legacy{{"legacy.nif", {&first, &second, &third}}};
        const std::array<variant*, 3> expected{&first, &second, &third};
        for (std::uint32_t ref = 1; ref <= 128; ++ref) {
            std::mt19937 engine(ref);
            std::uniform_int_distribution<std::uint32_t> dist(0, 2);
            Require(find(legacy, "Meshes/LEGACY.nif", ref, 0xAAA) == expected[dist(engine)],
                "legacy selection preserves original reference-seeded choice");
        }
        auto scoped = MakeVariant("scoped.nif", "scoped.nif", 0x123, 0x456);
        models onlyScoped{{"scoped-original.nif", {&scoped}}};
        Require(find(onlyScoped, "scoped-original.nif", 0x456, 0x123) == &scoped, "matching IDs choose scoped variant");
        Require(!find(onlyScoped, "scoped-original.nif", 0x457, 0x123), "other reference gets no scoped variant");
        Require(!find(onlyScoped, "scoped-original.nif", 0x456, 0x124), "other base gets no scoped variant");
        models mixed{{"mixed.nif", {&scoped, &first}}};
        for (std::uint32_t ref = 1; ref <= 128; ++ref) {
            Require(find(mixed, "mixed.nif", ref, 0x124) == &first,
                "nonmatching scoped candidate does not suppress legacy candidate");
        }
        models empty{{"empty.nif", {}}};
        Require(!find(empty, "empty.nif", 1, 1), "empty variants safely return no swap");
        Require(!find(empty, nullptr, 1, 1), "missing model path safely returns no swap");

        auto config = Config::GetSingleton();
        config->BypassTemporalActivation = true;
        Require(!find(onlyScoped, "scoped-original.nif", 0x457, 0x123), "temporal bypass keeps reference restriction");
        auto exclusionOnly = MakeVariant("selection-exclude-target.nif", "selection-exclude-target.nif");
        exclusionOnly.filter.excludeBaseFormID = std::vector<std::uint32_t>{0x124};
        exclusionOnly.filter.excludeReferenceID = std::vector<std::uint32_t>{0x457};
        models exclusions{{"selection-exclude.nif", {&exclusionOnly}}};
        Require(find(exclusions, "selection-exclude.nif", 0x456, 0x123) == &exclusionOnly,
            "temporal bypass permits nonexcluded variant");
        Require(!find(exclusions, "selection-exclude.nif", 0x456, 0x124), "temporal bypass preserves base exclusion");
        Require(!find(exclusions, "selection-exclude.nif", 0x457, 0x123), "temporal bypass preserves reference exclusion");
        config->BypassTemporalActivation = false;
        scoped.startDate = Time::fromDatePairString("01-01~31-01").first;
        scoped.endDate = Time::fromDatePairString("01-01~31-01").second;
        config->NowOverride = Time::fromString("2026-10-06 12:00:00");
        Require(!find(onlyScoped, "scoped-original.nif", 0x456, 0x123), "matching IDs preserve temporal inactivity");
        config->NowOverride = Time::fromString("2026-01-06 12:00:00");
        Require(find(onlyScoped, "scoped-original.nif", 0x456, 0x123) == &scoped, "matching IDs preserve temporal activation");
        config->NowOverride = {};
    }

    void TestSharedBaseRestoration() {
        auto manager = Manager::GetSingleton();
        static RE::TESBoundObject base;
        base.formID = 0x123;
        base.SetModel("shared-original.nif");
        static auto scoped = MakeVariant("shared-target.nif", "shared-target.nif", {}, 0x456);
        manager->Register("shared-original.nif", {&scoped});
        auto matching = manager->Swap(&base, 0x456);
        Require(std::string(base.GetModel()) == "shared-target.nif", "targeted reference gets swap");
        matching->Reset();
        Require(std::string(base.GetModel()) == "shared-original.nif", "shared base restores for other reference");
        auto other = manager->Swap(&base, 0x457);
        Require(std::string(base.GetModel()) == "shared-original.nif", "other reference keeps original");
        other->Reset();
        matching = manager->Swap(&base, 0x456);
        Require(std::string(base.GetModel()) == "shared-target.nif", "targeted reference remains repeatable after restore");
        matching->Reset();
        other = manager->Swap(&base, 0x457);
        Require(std::string(base.GetModel()) == "shared-original.nif", "repeated other reference keeps original");
        other->Reset();
        Require(!manager->Swap(static_cast<RE::TESBoundObject*>(nullptr), 0x456), "null object base safely returns");
        Require(!manager->Swap(static_cast<RE::TESObjectARMA*>(nullptr), 0x456, 0x123), "null armor base safely returns");
    }

    void TestNestedScopes() {
        static RE::TESBoundObject base;
        base.formID = 0x500;
        base.SetModel("nested-original.nif");
        static auto outerRule = MakeVariant("nested-outer.nif", "nested-outer.nif", {}, 0x501);
        static auto innerRule = MakeVariant("nested-inner.nif", "nested-inner.nif", {}, 0x502);
        auto manager = Manager::GetSingleton();
        manager->Register("nested-original.nif", {&outerRule});
        manager->Register("nested-original.nif", {&innerRule});
        {
            ModelSwapScope outer;
            outer.Add(manager->Swap(&base, 0x501));
            Require(std::string(base.GetModel()) == "nested-outer.nif", "outer scope applies its variant");
            {
                ModelSwapScope inner;
                inner.Add(manager->Swap(&base, 0x503));
                inner.Add(nullptr);
                Require(std::string(base.GetModel()) == "nested-original.nif", "nested unrelated reference sees baseline");
            }
            Require(std::string(base.GetModel()) == "nested-outer.nif", "nested unrelated reference resumes outer variant");
            {
                ModelSwapScope inner;
                inner.Add(manager->Swap(&base, 0x502));
                Require(std::string(base.GetModel()) == "nested-inner.nif", "nested targeted reference sees its variant");
                {
                    ModelSwapScope deepest;
                    deepest.Add(manager->Swap(&base, 0x503));
                    Require(std::string(base.GetModel()) == "nested-original.nif", "three nested scopes keep baseline isolation");
                }
                Require(std::string(base.GetModel()) == "nested-inner.nif", "deepest scope resumes inner variant");
            }
            Require(std::string(base.GetModel()) == "nested-outer.nif", "inner target resumes outer variant");
        }
        Require(std::string(base.GetModel()) == "nested-original.nif", "outer scope restores original after nested loads");

        try {
            ModelSwapScope throwing;
            throwing.Add(manager->Swap(&base, 0x501));
            throw 1;
        } catch (int) {
        }
        Require(std::string(base.GetModel()) == "nested-original.nif", "exception unwinding restores original path");

        {
            ModelSwapScope outer(0x501);
            outer.Add(manager->Swap(&base, 0x501));
            {
                ModelSwapScope continuation(0x501);
                Require(std::string(base.GetModel()) == "nested-outer.nif", "same-reference continuation retains outer variant");
                {
                    ModelSwapScope different(0x503);
                    different.Add(manager->Swap(&base, 0x503));
                    Require(std::string(base.GetModel()) == "nested-original.nif", "different reference inside continuation sees baseline");
                }
                Require(std::string(base.GetModel()) == "nested-outer.nif", "different reference resumes continuation variant");
            }
            Require(std::string(base.GetModel()) == "nested-outer.nif", "continuation return retains outer variant");
        }
        Require(std::string(base.GetModel()) == "nested-original.nif", "same-reference chain eventually restores original");
    }

    void TestAliasRegistration() {
        auto manager = Manager::GetSingleton();
        std::uint32_t matchingRef = 1;
        for (;; ++matchingRef) {
            std::mt19937 engine(matchingRef);
            std::uniform_int_distribution<std::uint32_t> distribution(0, 3);
            if (distribution(engine) == 1) break;
        }
        static auto scopedFirst = MakeVariant("alias-shared.nif", "alias-shared.nif");
        static auto scopedSecond = MakeVariant("alias-scoped-other.nif", "alias-scoped-other.nif");
        scopedFirst.filter.referenceID = std::vector<std::uint32_t>{matchingRef};
        scopedSecond.filter.referenceID = std::vector<std::uint32_t>{matchingRef};
        static auto globalFirst = MakeVariant("alias-shared.nif", "alias-shared.nif");
        static auto globalSecond = MakeVariant("alias-global-other.nif", "alias-global-other.nif");
        manager->Register("alias-scoped-original.nif", {&scopedFirst, &scopedSecond});
        manager->Register("alias-global-original.nif", {&globalFirst, &globalSecond});
        static RE::TESBoundObject base;
        base.formID = 0x600;
        base.SetModel("alias-shared.nif");
        auto matching = manager->Swap(&base, matchingRef);
        Require(std::string(base.GetModel()) == "alias-scoped-other.nif", "global alias registration preserves scoped variants");
        matching->Reset();
        for (std::uint32_t ref = matchingRef + 1; ref < matchingRef + 65; ++ref) {
            std::mt19937 engine(ref);
            std::uniform_int_distribution<std::uint32_t> distribution(0, 1);
            const char* expected = distribution(engine) == 0 ? globalFirst.model : globalSecond.model;
            auto other = manager->Swap(&base, ref);
            Require(std::string(base.GetModel()) == expected, "mixed alias filters scoped rules before global selection");
            other->Reset();
        }

        static auto self = MakeVariant("alias-self.nif", "alias-self.nif", 0x601);
        static auto alternative = MakeVariant("alias-self-other.nif", "alias-self-other.nif", 0x601);
        manager->Register("alias-self.nif", {&self, &alternative});
        static RE::TESBoundObject selfBase;
        selfBase.formID = 0x601;
        selfBase.SetModel("alias-self.nif");
        for (std::uint32_t ref = 1; ref < 65; ++ref) {
            std::mt19937 engine(ref);
            std::uniform_int_distribution<std::uint32_t> distribution(0, 1);
            const char* expected = distribution(engine) == 0 ? self.model : alternative.model;
            auto selected = manager->Swap(&selfBase, ref);
            Require(std::string(selfBase.GetModel()) == expected, "self alias does not duplicate scoped candidate pool");
            selected->Reset();
        }
    }

    void TestNullPathSnapshot() {
        static RE::TESBoundObject base;
        base.formID = 0x700;
        Require(base.GetModel() == nullptr, "runtime stub preserves missing model path");
        {
            ModelSwapScope scope;
            scope.Add(Manager::GetSingleton()->Swap(&base, 0x701));
        }
        Require(base.GetModel() == nullptr, "scope keeps missing model path unchanged");
    }

    void TestExclusionManagement() {
        auto manager = Manager::GetSingleton();
        static auto empty = MakeVariant("exclude-empty-target.nif", "exclude-empty-target.nif");
        empty.filter.excludeBaseFormID = std::vector<std::uint32_t>{};
        empty.filter.excludeReferenceID = std::vector<std::uint32_t>{};
        manager->Register("exclude-empty-original.nif", {&empty});
        Require(!manager->HasScopedRules(), "empty exclusion-only entry retains legacy mode");

        static auto excluded = MakeVariant("exclude-scope-target.nif", "exclude-scope-target.nif");
        excluded.filter.excludeReferenceID = std::vector<std::uint32_t>{0xA02, 0xA03};
        manager->Register("exclude-scope-original.nif", {&excluded});
        Require(manager->HasScopedRules(), "exclusion-only entry enables scoped hooks");
        static RE::TESBoundObject base;
        base.formID = 0xA00;
        base.SetModel("exclude-scope-original.nif");
        {
            ModelSwapScope outer(0xA01);
            outer.Add(manager->Swap(&base, 0xA01));
            Require(std::string(base.GetModel()) == "exclude-scope-target.nif", "nonexcluded reference gets shared-base swap");
            {
                ModelSwapScope inner(0xA02);
                inner.Add(manager->Swap(&base, 0xA02));
                Require(std::string(base.GetModel()) == "exclude-scope-original.nif", "nested excluded reference sees original shared base");
            }
            Require(std::string(base.GetModel()) == "exclude-scope-target.nif", "excluded nested reference resumes outer variant");
        }
        Require(std::string(base.GetModel()) == "exclude-scope-original.nif", "exclusion scope restores original shared base");

        static auto scoped = MakeVariant("exclude-alias-scoped-target.nif", "exclude-alias-shared.nif");
        static auto global = MakeVariant("exclude-alias-shared.nif", "exclude-alias-shared.nif");
        scoped.filter.excludeReferenceID = std::vector<std::uint32_t>{0xB02};
        manager->Register("exclude-alias-scoped.nif", {&scoped});
        manager->Register("exclude-alias-global.nif", {&global});
        bool sawScoped = false;
        for (std::uint32_t ref = 1; ref <= 64; ++ref) {
            RE::TESBoundObject aliasBase;
            aliasBase.formID = 0xB00;
            aliasBase.SetModel("exclude-alias-shared.nif");
            auto selected = manager->Swap(&aliasBase, ref);
            sawScoped = sawScoped || std::string(aliasBase.GetModel()) == scoped.model;
            selected->Reset();
        }
        Require(sawScoped, "global alias registration preserves exclusion-only candidate");
        RE::TESBoundObject excludedAliasBase;
        excludedAliasBase.formID = 0xB00;
        excludedAliasBase.SetModel("exclude-alias-shared.nif");
        auto selected = manager->Swap(&excludedAliasBase, 0xB02);
        Require(std::string(excludedAliasBase.GetModel()) == global.model,
            "excluded alias candidate leaves unrelated global candidate eligible");
        selected->Reset();
    }

    void TestUnchangedModelPreservation() {
        RE::TESBoundObject base;
        base.formID = 0x800;
        base.SetModel("unchanged-original.nif");
        {
            ModelSwapScope scope(0x801);
            scope.Add(Manager::GetSingleton()->Swap(&base, 0x801));
            base.SetModel("other-hook-edit.nif");
        }
        Require(std::string(base.GetModel()) == "other-hook-edit.nif", "scope keeps other hooks' changes to unmatched model");
    }

    void TestLegacyManagement() {
        auto manager = Manager::GetSingleton();
        manager->Register("legacy-empty.nif", {});
        Require(!manager->HasScopedRules(), "empty and legacy entries do not enable scoped hooks");
        static auto first = MakeVariant("legacy-process-a.nif", "legacy-process-a.nif");
        static auto second = MakeVariant("legacy-process-b.nif", "legacy-process-b.nif");
        manager->Register("legacy-process-original.nif", {&first, &second});
        Require(!manager->HasScopedRules(), "unscoped legacy registration retains legacy mode");
        RE::TESBoundObject base;
        base.formID = 0x900;
        base.SetModel("legacy-process-original.nif");
        std::mt19937 engine(0x901);
        std::uniform_int_distribution<std::uint32_t> distribution(0, 1);
        const char* expected = distribution(engine) == 0 ? first.model : second.model;
        manager->Process(&base, 0x901);
        Require(std::string(base.GetModel()) == expected, "legacy Process preserves persistent replacement behavior");
    }

    void TestConfigLoading() {
        const auto fixtureRoot = std::filesystem::path(__FILE__).parent_path().parent_path() / "build/tests/loader-fixture";
        std::filesystem::create_directories(fixtureRoot / "Data/SKSE/Plugins");
        const auto previousDirectory = std::filesystem::current_path();
        std::filesystem::current_path(fixtureRoot);
        {
            std::ofstream fixture("Data/regression_MS.json");
            fixture << R"([
                {"OriginalModel":"loader-legacy.nif","Variants":["loader-legacy-target.nif"]},
                {"OriginalModel":"loader-base.nif","Variants":["loader-base-target.nif"],"BaseFormID":"0x123"},
                {"OriginalModel":"loader-ref.nif","Variants":["loader-ref-target.nif"],"ReferenceID":1110},
                {"OriginalModel":"loader-both.nif","Variants":["loader-both-target.nif"],"BaseFormID":"123","ReferenceID":"456"},
                {"OriginalModel":"loader-plugin.nif","Variants":["loader-plugin-target.nif"],"BaseFormID":"Example.esp|ABC"},
                {"OriginalModel":"loader-light.nif","Variants":["loader-light-target.nif"],"BaseFormID":"Light.esl|FFF"},
                {"OriginalModel":"loader-arrays.nif","Variants":["loader-arrays-target.nif"],"BaseFormID":["123","Example.esp|ABC"],"ReferenceID":["456","Light.esl|DEF"]},
                {"OriginalModel":"loader-exclude-base.nif","Variants":["loader-exclude-base-target.nif"],"ExcludeBaseFormID":"Example.esp|ABC"},
                {"OriginalModel":"loader-exclude-ref.nif","Variants":["loader-exclude-ref-target.nif"],"ExcludeReferenceID":"456"},
                {"OriginalModel":"loader-exclude-arrays.nif","Variants":["loader-exclude-arrays-target.nif"],"ExcludeBaseFormID":["123","Example.esp|ABC"],"ExcludeReferenceID":[1110,"Light.esl|DEF"]},
                {"OriginalModel":"loader-include-exclude.nif","Variants":["loader-include-exclude-target.nif"],"BaseFormID":["123","124"],"ReferenceID":["456","457"],"ExcludeBaseFormID":"124","ExcludeReferenceID":["457"]},
                {"OriginalModel":"loader-empty-base.nif","Variants":["loader-empty-base-target.nif"],"BaseFormID":[]},
                {"OriginalModel":"loader-empty-ref.nif","Variants":["loader-empty-ref-target.nif"],"ReferenceID":[]},
                {"OriginalModel":"loader-empty-exclusions.nif","Variants":["loader-empty-exclusions-target.nif"],"ExcludeBaseFormID":[],"ExcludeReferenceID":[]},
                {"OriginalModel":"loader-invalid-array-base.nif","Variants":["loader-invalid-array-base-target.nif"],"BaseFormID":["123","bogus"]},
                {"OriginalModel":"loader-invalid-array-ref.nif","Variants":["loader-invalid-array-ref-target.nif"],"ReferenceID":["456","Missing.esp|ABC"]},
                {"OriginalModel":"loader-invalid-exclude-base.nif","Variants":["loader-invalid-exclude-base-target.nif"],"ExcludeBaseFormID":["123","bogus"]},
                {"OriginalModel":"loader-invalid-exclude-ref.nif","Variants":["loader-invalid-exclude-ref-target.nif"],"ExcludeReferenceID":["456","Missing.esp|ABC"]},
                {"OriginalModel":"loader-invalid-scalar-exclude-base.nif","Variants":["loader-invalid-scalar-exclude-base-target.nif"],"ExcludeBaseFormID":null},
                {"OriginalModel":"loader-invalid-scalar-exclude-ref.nif","Variants":["loader-invalid-scalar-exclude-ref-target.nif"],"ExcludeReferenceID":false},
                {"OriginalModel":"loader-bad-light.nif","Variants":["loader-bad-light-target.nif"],"BaseFormID":"Light.esl|1000"},
                {"OriginalModel":"loader-missing-plugin.nif","Variants":["loader-missing-plugin-target.nif"],"BaseFormID":"Missing.esp|ABC"},
                {"OriginalModel":"loader-inactive-plugin.nif","Variants":["loader-inactive-plugin-target.nif"],"BaseFormID":"Inactive.esp|ABC"},
                {"OriginalModel":"loader-bad-id.nif","Variants":["loader-bad-id-target.nif"],"ReferenceID":"bogus"},
                {"OriginalModel":"loader-zero.nif","Variants":["loader-zero-target.nif"],"BaseFormID":0},
                {"OriginalModel":"loader-empty.nif","Variants":[],"ReferenceID":"456"},
                {"OriginalModel":"loader-bad-time.nif","Variants":["loader-bad-time-target.nif"],"ActiveTimeSpan":5},
                {"OriginalModel":"loader-after-invalid.nif","Variants":[1,"loader-after-invalid-target.nif"]}
            ])";
        }
        { std::ofstream malformed("Data/malformed_MS.json"); malformed << "{broken"; }
        { std::ofstream ignored("Data/unrelated.json"); ignored << R"([{"OriginalModel":"loader-ignored.nif","Variants":["loader-ignored-target.nif"]}])"; }
        Persistence::Install();
        const auto check = [](const char* original, std::uint32_t baseID, std::uint32_t refID, const char* expected) {
            RE::TESBoundObject base;
            base.formID = baseID;
            base.SetModel(original);
            ModelSwapScope scope(refID);
            scope.Add(Manager::GetSingleton()->Swap(&base, refID));
            Require(std::string(base.GetModel()) == expected, "production config loader applies only valid matching rules");
        };
        check("loader-legacy.nif", 0x124, 0x457, "loader-legacy-target.nif");
        check("loader-base.nif", 0x123, 0x457, "loader-base-target.nif");
        check("loader-base.nif", 0x124, 0x457, "loader-base.nif");
        check("loader-ref.nif", 0x124, 0x456, "loader-ref-target.nif");
        check("loader-ref.nif", 0x124, 0x457, "loader-ref.nif");
        check("loader-both.nif", 0x123, 0x456, "loader-both-target.nif");
        check("loader-both.nif", 0x123, 0x457, "loader-both.nif");
        check("loader-both.nif", 0x124, 0x456, "loader-both.nif");
        check("loader-plugin.nif", 0x01000ABC, 0x456, "loader-plugin-target.nif");
        check("loader-plugin.nif", 0x02000ABC, 0x456, "loader-plugin.nif");
        check("loader-light.nif", 0xFE001FFF, 0x456, "loader-light-target.nif");
        check("loader-arrays.nif", 0x123, 0x456, "loader-arrays-target.nif");
        check("loader-arrays.nif", 0x01000ABC, 0xFE001DEF, "loader-arrays-target.nif");
        check("loader-arrays.nif", 0x123, 0xFE001DEF, "loader-arrays-target.nif");
        check("loader-arrays.nif", 0x124, 0x456, "loader-arrays.nif");
        check("loader-arrays.nif", 0x123, 0x457, "loader-arrays.nif");
        check("loader-exclude-base.nif", 0x01000ABC, 0x456, "loader-exclude-base.nif");
        check("loader-exclude-base.nif", 0x02000ABC, 0x456, "loader-exclude-base-target.nif");
        check("loader-exclude-ref.nif", 0x123, 0x456, "loader-exclude-ref.nif");
        check("loader-exclude-ref.nif", 0x123, 0x457, "loader-exclude-ref-target.nif");
        check("loader-exclude-arrays.nif", 0x123, 0x457, "loader-exclude-arrays.nif");
        check("loader-exclude-arrays.nif", 0x01000ABC, 0x457, "loader-exclude-arrays.nif");
        check("loader-exclude-arrays.nif", 0x124, 0x456, "loader-exclude-arrays.nif");
        check("loader-exclude-arrays.nif", 0x124, 0xFE001DEF, "loader-exclude-arrays.nif");
        check("loader-exclude-arrays.nif", 0x124, 0x457, "loader-exclude-arrays-target.nif");
        check("loader-include-exclude.nif", 0x123, 0x456, "loader-include-exclude-target.nif");
        check("loader-include-exclude.nif", 0x124, 0x456, "loader-include-exclude.nif");
        check("loader-include-exclude.nif", 0x123, 0x457, "loader-include-exclude.nif");
        check("loader-include-exclude.nif", 0x125, 0x456, "loader-include-exclude.nif");
        check("loader-include-exclude.nif", 0x123, 0x458, "loader-include-exclude.nif");
        check("loader-empty-base.nif", 0x123, 0x456, "loader-empty-base.nif");
        check("loader-empty-ref.nif", 0x123, 0x456, "loader-empty-ref.nif");
        check("loader-empty-exclusions.nif", 0x123, 0x456, "loader-empty-exclusions-target.nif");
        check("loader-invalid-array-base.nif", 0x123, 0x456, "loader-invalid-array-base.nif");
        check("loader-invalid-array-ref.nif", 0x123, 0x456, "loader-invalid-array-ref.nif");
        check("loader-invalid-exclude-base.nif", 0x124, 0x457, "loader-invalid-exclude-base.nif");
        check("loader-invalid-exclude-ref.nif", 0x124, 0x457, "loader-invalid-exclude-ref.nif");
        check("loader-invalid-scalar-exclude-base.nif", 0x124, 0x457, "loader-invalid-scalar-exclude-base.nif");
        check("loader-invalid-scalar-exclude-ref.nif", 0x124, 0x457, "loader-invalid-scalar-exclude-ref.nif");
        check("loader-bad-light.nif", 0xFE001000, 0x456, "loader-bad-light.nif");
        check("loader-missing-plugin.nif", 0x01000ABC, 0x456, "loader-missing-plugin.nif");
        check("loader-inactive-plugin.nif", 0x01000ABC, 0x456, "loader-inactive-plugin.nif");
        check("loader-bad-id.nif", 0x123, 0x456, "loader-bad-id.nif");
        check("loader-zero.nif", 0x123, 0x456, "loader-zero.nif");
        check("loader-empty.nif", 0x123, 0x456, "loader-empty.nif");
        check("loader-bad-time.nif", 0x123, 0x456, "loader-bad-time.nif");
        check("loader-after-invalid.nif", 0x123, 0x456, "loader-after-invalid-target.nif");
        check("loader-ignored.nif", 0x123, 0x456, "loader-ignored.nif");
        std::filesystem::remove("Data/regression_MS.json");
        std::filesystem::remove("Data/malformed_MS.json");
        std::filesystem::remove("Data/unrelated.json");
        std::filesystem::current_path(previousDirectory);
    }

    void TestArmorRestoration() {
        static RE::TESObjectARMA armor;
        armor.formID = 0x789;
        const std::array<const char*, 4> originals{"armor-male3.nif", "armor-female3.nif", "armor-male1.nif", "armor-female1.nif"};
        const std::array<const char*, 4> replacements{"armor-target-male3.nif", "armor-target-female3.nif", "armor-target-male1.nif", "armor-target-female1.nif"};
        const std::array<const char*, 4> keys{"armor-target-male3.nif", "armor-target-female3.nif", "armor-target-male1.nif", "armor-target-female1.nif"};
        static std::array<variant, 4> scoped;
        std::array<RE::TESModel*, 4> channels{&armor.bipedModels[0], &armor.bipedModels[1], &armor.bipedModel1stPersons[0], &armor.bipedModel1stPersons[1]};
        auto manager = Manager::GetSingleton();
        for (std::size_t i = 0; i < channels.size(); ++i) {
            channels[i]->SetModel(originals[i]);
            scoped[i] = MakeVariant(replacements[i], keys[i], 0x789, 0x456);
            manager->Register(originals[i], {&scoped[i]});
        }
        auto matching = manager->Swap(&armor, 0x456, 0x789);
        for (std::size_t i = 0; i < channels.size(); ++i) {
            Require(std::string(channels[i]->GetModel()) == replacements[i], "armor channel swaps for matching reference");
        }
        matching->Reset();
        auto other = manager->Swap(&armor, 0x457, 0x789);
        for (std::size_t i = 0; i < channels.size(); ++i) {
            Require(std::string(channels[i]->GetModel()) == originals[i], "armor channel restores for other reference");
        }
        other->Reset();
    }
}

int main() {
    TestFilter();
    TestIDParsing();
    TestSelection();
    TestLegacyManagement();
    TestExclusionManagement();
    TestSharedBaseRestoration();
    TestNestedScopes();
    TestAliasRegistration();
    TestNullPathSnapshot();
    TestUnchangedModelPreservation();
    TestArmorRestoration();
    TestConfigLoading();
    std::cout << "Passed " << checks << " regression checks.\n";
}

