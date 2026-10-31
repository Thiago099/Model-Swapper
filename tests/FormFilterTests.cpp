#include "FormFilter.h"
#include "FilterContext.h"
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
        const std::array<const char*, 14> fields{
            "BaseFormID", "ReferenceID", "ExcludeBaseFormID", "ExcludeReferenceID",
            "LocationID", "RegionID", "KeywordID", "CellID", "WorldspaceID",
            "ExcludeLocationID", "ExcludeRegionID", "ExcludeKeywordID", "ExcludeCellID", "ExcludeWorldspaceID"};
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

    void TestContextFilters() {
        using Member = FormFilter::IDs FormFilter::*;
        struct ContextCase {
            Member include;
            Member exclude;
            std::function<void(FormFilterContext&, std::uint32_t)> populate;
        };
        const std::array<ContextCase, 5> cases{{
            {&FormFilter::locationID, &FormFilter::excludeLocationID,
                [](auto& context, auto id) { context.locations = {id}; }},
            {&FormFilter::regionID, &FormFilter::excludeRegionID,
                [](auto& context, auto id) { context.regions = {id}; }},
            {&FormFilter::keywordID, &FormFilter::excludeKeywordID,
                [](auto& context, auto id) { context.hasKeyword = [id](auto candidate) { return candidate == id; }; }},
            {&FormFilter::cellID, &FormFilter::excludeCellID,
                [](auto& context, auto id) { context.cellID = id; }},
            {&FormFilter::worldspaceID, &FormFilter::excludeWorldspaceID,
                [](auto& context, auto id) { context.worldspaces = {id}; }}
        }};
        for (const auto& test : cases) {
            FormFilter filter;
            FormFilterContext matching, other, empty;
            test.populate(matching, 0xE001);
            test.populate(other, 0xE002);
            filter.*test.include = std::vector<std::uint32_t>{0xE000, 0xE001};
            Require(filter.HasContextFilters() && filter.IsScoped(), "context inclusion enables scoped matching");
            Require(filter.Matches(1, 2, &matching), "each context filter accepts any matching ID in its array");
            Require(!filter.Matches(1, 2, &other), "each context filter rejects unrelated context");
            Require(!filter.Matches(1, 2, &empty), "each context inclusion rejects a missing context fact");
            Require(!filter.Matches(1, 2), "context inclusion is never silently ignored without reference context");
            filter.*test.exclude = std::vector<std::uint32_t>{0xE001};
            Require(!filter.Matches(1, 2, &matching), "each context exclusion overrides a positive match");
            (filter.*test.include).reset();
            Require(!filter.Matches(1, 2, &matching), "each exclusion works without a context inclusion");
            Require(filter.Matches(1, 2, &other), "each exclusion permits nonmatching contexts");
            Require(filter.Matches(1, 2, &empty), "missing contextual facts do not spuriously match exclusions");
            Require(!filter.Matches(1, 2), "context exclusions fail closed when no reference context is supplied");
            filter.*test.exclude = std::vector<std::uint32_t>{};
            Require(!filter.HasContextFilters() && !filter.IsScoped(), "empty contextual exclusions impose no scope");
            Require(filter.Matches(1, 2), "empty contextual exclusions retain legacy matching");
            filter.*test.include = std::vector<std::uint32_t>{};
            Require(filter.HasContextFilters() && filter.IsScoped(), "empty contextual inclusion remains an active restriction");
            Require(!filter.Matches(1, 2, &matching), "empty contextual inclusion matches nothing");
        }

        FormFilter combined;
        combined.locationID = std::vector<std::uint32_t>{0xE100};
        combined.regionID = std::vector<std::uint32_t>{0xE200};
        FormFilterContext context;
        context.regions = {0xE200};
        Require(combined.Matches(1, 2, &context), "different positive context categories use BOS any-match logic");
        context.regions.clear();
        context.locations = {0xE100};
        Require(combined.Matches(1, 2, &context), "a location alone can satisfy mixed positive conditions");
        context.locations.clear();
        Require(!combined.Matches(1, 2, &context), "mixed positive conditions reject when none match");
        context.regions = {0xE100};
        Require(!combined.Matches(1, 2, &context), "an ID in the wrong context category cannot satisfy a condition");
        context.regions = {0xE200};
        combined.locationID = std::vector<std::uint32_t>{};
        Require(combined.Matches(1, 2, &context), "an empty category does not suppress another positive context match");
        combined.baseFormID = std::vector<std::uint32_t>{0x123};
        combined.referenceID = std::vector<std::uint32_t>{0x456};
        Require(combined.Matches(0x123, 0x456, &context), "context conditions combine with existing base and reference gates");
        Require(!combined.Matches(0x124, 0x456, &context), "context match cannot bypass an existing base inclusion");
        Require(!combined.Matches(0x123, 0x457, &context), "context match cannot bypass an existing reference inclusion");
        combined.excludeCellID = std::vector<std::uint32_t>{0xE300};
        context.cellID = 0xE300;
        Require(!combined.Matches(0x123, 0x456, &context), "an exclusion in another context category vetoes positive matches");
    }

    void TestContextCapture() {
        static RE::TESBoundObject base;
        static RE::BGSKeyword baseKeyword, locationKeyword, referenceKeyword, parentOnlyKeyword, absentKeyword;
        static RE::BGSLocation location, parentLocation, ancestorLocation;
        static RE::TESRegion firstRegion, secondRegion, savedRegion;
        static RE::TESRegionList regions{&firstRegion, nullptr, &secondRegion}, savedRegions{&savedRegion};
        static RE::TESObjectCELL cell, savedCell;
        static RE::TESWorldSpace worldspace, parentWorld, ancestorWorld;
        static RE::TESObjectREFR ref;
        RE::TESForm::Register(base, 0xE000);
        RE::TESForm::Register(baseKeyword, 0xE010);
        RE::TESForm::Register(locationKeyword, 0xE011);
        RE::TESForm::Register(referenceKeyword, 0xE012);
        RE::TESForm::Register(parentOnlyKeyword, 0xE013);
        RE::TESForm::Register(absentKeyword, 0xE014);
        RE::TESForm::Register(location, 0xE100);
        RE::TESForm::Register(parentLocation, 0xE101);
        RE::TESForm::Register(ancestorLocation, 0xE102);
        RE::TESForm::Register(firstRegion, 0xE200);
        RE::TESForm::Register(secondRegion, 0xE201);
        RE::TESForm::Register(savedRegion, 0xE202);
        RE::TESForm::Register(cell, 0xE300);
        RE::TESForm::Register(savedCell, 0xE301);
        RE::TESForm::Register(worldspace, 0xE400);
        RE::TESForm::Register(parentWorld, 0xE401);
        RE::TESForm::Register(ancestorWorld, 0xE402);
        RE::TESForm::Register(ref, 0xE500);
        base.keywords = {&baseKeyword};
        location.keywords = {&locationKeyword};
        parentLocation.keywords = {&parentOnlyKeyword};
        location.parentLoc = &parentLocation;
        parentLocation.parentLoc = &ancestorLocation;
        cell.regionList = &regions;
        savedCell.regionList = &savedRegions;
        worldspace.parentWorld = &parentWorld;
        parentWorld.parentWorld = &ancestorWorld;
        ref.base = &base;
        ref.location = &location;
        ref.worldspace = &worldspace;
        ref.parentCell = &cell;
        ref.saveParentCell = &savedCell;
        ref.referenceKeywords = {&referenceKeyword};

        const auto context = FormFilters::CaptureContext(&ref);
        Require(context.locations == std::vector<std::uint32_t>{0xE100, 0xE101, 0xE102},
            "location context includes the object location and all ancestors");
        Require(context.cellID == 0xE300 && context.regions == std::vector<std::uint32_t>{0xE200, 0xE201},
            "cell and regions come from the object's current cell, ignoring null region entries");
        Require(context.worldspaces == std::vector<std::uint32_t>{0xE400, 0xE401},
            "worldspace context includes only the current and immediate parent worldspace");
        Require(context.hasKeyword && context.hasKeyword(0xE010), "keyword context uses the owning base object");
        Require(context.hasKeyword(0xE011), "keyword context uses the current location");
        Require(context.hasKeyword(0xE012), "keyword context preserves reference keyword behavior");
        Require(!context.hasKeyword(0xE013), "location keyword filters do not inherit parent-location keywords");
        Require(!context.hasKeyword(0xE014) && !context.hasKeyword(0xEFFF),
            "keyword context rejects absent and unresolved keyword records");
        Require(!context.hasKeyword(0xE100), "a non-keyword form ID cannot pass a keyword condition");
        ref.parentCell = nullptr;
        const auto saved = FormFilters::CaptureContext(&ref);
        Require(saved.cellID == 0xE301 && saved.regions == std::vector<std::uint32_t>{0xE202},
            "missing current cell falls back to saved cell and its regions");
        ref.saveParentCell = nullptr;
        ref.location = nullptr;
        ref.worldspace = nullptr;
        const auto missing = FormFilters::CaptureContext(&ref);
        Require(missing.cellID == 0 && missing.regions.empty() && missing.locations.empty() && missing.worldspaces.empty(),
            "references without geographic context are handled safely");
        Require(missing.hasKeyword && missing.hasKeyword(0xE010), "missing location does not discard object keywords");
        const auto null = FormFilters::CaptureContext(nullptr);
        Require(null.cellID == 0 && null.locations.empty() && null.regions.empty() && null.worldspaces.empty() &&
            (!null.hasKeyword || !null.hasKeyword(0xE010)), "null reference capture safely yields no matching facts");
        ancestorLocation.parentLoc = &location;
        ref.location = &location;
        const auto cyclic = FormFilters::CaptureContext(&ref);
        Require(cyclic.locations == std::vector<std::uint32_t>{0xE100, 0xE101, 0xE102},
            "a malformed location cycle terminates without duplicate ancestors");
        ancestorLocation.parentLoc = nullptr;

        FormFilter valid;
        valid.locationID = std::vector<std::uint32_t>{0xE100};
        valid.regionID = std::vector<std::uint32_t>{0xE200};
        valid.keywordID = std::vector<std::uint32_t>{0xE010};
        valid.cellID = std::vector<std::uint32_t>{0xE300};
        valid.worldspaceID = std::vector<std::uint32_t>{0xE400};
        valid.excludeLocationID = valid.locationID;
        valid.excludeRegionID = valid.regionID;
        valid.excludeKeywordID = valid.keywordID;
        valid.excludeCellID = valid.cellID;
        valid.excludeWorldspaceID = valid.worldspaceID;
        std::string_view invalidField;
        Require(FormFilters::ValidateContextIDs(valid, invalidField), "all typed inclusion and exclusion records validate");
        valid.locationID = std::vector<std::uint32_t>{0xE200};
        Require(!FormFilters::ValidateContextIDs(valid, invalidField) && invalidField == "LocationID",
            "wrong-type conditions are rejected with the offending field name");
        valid.locationID = std::vector<std::uint32_t>{0xEFFF};
        Require(!FormFilters::ValidateContextIDs(valid, invalidField), "unresolved context form IDs are rejected");
        valid.locationID.reset();
        valid.excludeRegionID = std::vector<std::uint32_t>{0xE010};
        Require(!FormFilters::ValidateContextIDs(valid, invalidField) && invalidField == "ExcludeRegionID",
            "wrong-type exclusions cannot silently become ineffective");
        FormFilter legacy;
        legacy.baseFormID = std::vector<std::uint32_t>{0xEFFF};
        legacy.referenceID = std::vector<std::uint32_t>{0xEFFE};
        Require(FormFilters::ValidateContextIDs(legacy, invalidField),
            "context record validation preserves the legacy raw base/reference ID behavior");
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

    void TestIDOnlySelection() {
        auto exact = MakeVariant("id-only-exact.nif", "id-only-exact.nif");
        auto independent = MakeVariant("id-only-any-path.nif", "id-only-any-path.nif", 0xD000);
        models choices{{"id-only-original.nif", {&exact}}, {"", {&independent}}};
        const std::array<variant*, 2> expected{&exact, &independent};
        for (std::uint32_t ref = 1; ref <= 64; ++ref) {
            std::mt19937 engine(ref);
            std::uniform_int_distribution<std::uint32_t> distribution(0, 1);
            Require(find(choices, "id-only-original.nif", ref, 0xD000) == expected[distribution(engine)],
                "model-specific and ID-only rules share one eligible pool");
            Require(find(choices, "id-only-original.nif", ref, 0xD001) == &exact,
                "nonmatching ID-only rule leaves model-specific choice unchanged");
        }
        Require(find(choices, "unlisted-path.nif", 0xD002, 0xD000) == &independent,
            "ID-only rule accepts an unlisted original model");
        Require(find(choices, nullptr, 0xD002, 0xD000) == &independent,
            "ID-only rule accepts a missing original model");
        Require(find(choices, "", 0xD002, 0xD000) == &independent,
            "ID-only rule accepts an empty original model");
        independent.filter.referenceID = std::vector<std::uint32_t>{0xD002, 0xD003};
        independent.filter.excludeReferenceID = std::vector<std::uint32_t>{0xD003};
        Require(!find(choices, "unlisted-path.nif", 0xD003, 0xD000),
            "ID-only rule preserves reference exclusions");
        Require(!find(choices, "unlisted-path.nif", 0xD004, 0xD000),
            "ID-only rule preserves reference inclusion");
        independent.startDate = Time::fromDatePairString("01-01~31-01").first;
        independent.endDate = Time::fromDatePairString("01-01~31-01").second;
        auto config = Config::GetSingleton();
        config->NowOverride = Time::fromString("2026-10-06 12:00:00");
        Require(!find(choices, "unlisted-path.nif", 0xD002, 0xD000),
            "ID-only rule preserves temporal inactivity");
        config->BypassTemporalActivation = true;
        Require(find(choices, "unlisted-path.nif", 0xD002, 0xD000) == &independent,
            "ID-only rule respects temporal bypass");
        Require(!find(choices, "unlisted-path.nif", 0xD003, 0xD000),
            "temporal bypass does not bypass ID-only exclusions");
        config->BypassTemporalActivation = false;
        config->NowOverride = Time::fromString("2026-01-06 12:00:00");
        Require(find(choices, "unlisted-path.nif", 0xD002, 0xD000) == &independent,
            "ID-only rule preserves temporal activation");
        config->NowOverride = {};
    }

    void TestMultipleOriginalRegistration() {
        auto manager = Manager::GetSingleton();
        static auto first = MakeVariant("multi-register-target-a.nif", "multi-register-target-a.nif", 0xD100);
        static auto second = MakeVariant("multi-register-target-b.nif", "multi-register-target-b.nif", 0xD100);
        manager->Register(std::vector<std::string>{"Meshes/Multi-Register-A.nif", "multi-register-b.nif",
            "meshes\\MULTI-REGISTER-A.nif", "multi-register-target-a.nif"}, {&first, &second});
        const std::array<const char*, 4> originals{"multi-register-a.nif", "multi-register-b.nif",
            "multi-register-target-a.nif", "multi-register-target-b.nif"};
        for (const auto original : originals) {
            RE::TESBoundObject base;
            base.formID = 0xD100;
            base.SetModel(original);
            for (std::uint32_t ref = 1; ref <= 64; ++ref) {
                std::mt19937 engine(ref);
                std::uniform_int_distribution<std::uint32_t> distribution(0, 1);
                const char* expected = distribution(engine) == 0 ? first.model : second.model;
                auto selected = manager->Swap(&base, ref);
                Require(std::string(base.GetModel()) == expected,
                    "multiple originals, duplicate paths, and aliases do not duplicate variant weights");
                selected->Reset();
            }
        }
    }

    void TestIDOnlyRestoration() {
        auto manager = Manager::GetSingleton();
        static auto rule = MakeVariant("id-only-restoration-target.nif", "id-only-restoration-target.nif", 0xD200, 0xD201);
        manager->Register(std::vector<std::string>{}, {&rule});
        Require(manager->HasScopedRules(), "ID-only registration enables scoped model loading");
        RE::TESBoundObject base;
        base.formID = 0xD200;
        {
            ModelSwapScope outer(0xD201);
            outer.Add(manager->Swap(&base, 0xD201));
            Require(std::string(base.GetModel()) == rule.model, "ID-only swap supplies a missing model");
            {
                ModelSwapScope inner(0xD202);
                inner.Add(manager->Swap(&base, 0xD202));
                Require(base.GetModel() == nullptr, "nested nonmatching reference sees missing baseline model");
            }
            Require(std::string(base.GetModel()) == rule.model, "nested return resumes ID-only replacement");
        }
        Require(base.GetModel() == nullptr, "ID-only scope restores missing original model");
        base.SetModel("id-only-restoration-original.nif");
        {
            ModelSwapScope scope(0xD201);
            scope.Add(manager->Swap(&base, 0xD201));
            Require(std::string(base.GetModel()) == rule.model, "ID-only swap also accepts an existing model");
        }
        Require(std::string(base.GetModel()) == "id-only-restoration-original.nif",
            "ID-only scope restores existing original model");

        RE::TESObjectARMA armor;
        armor.formID = 0xD299;
        const std::array<const char*, 4> originals{"id-only-male3.nif", "id-only-female3.nif", "id-only-male1.nif", nullptr};
        std::array<RE::TESModel*, 4> channels{&armor.bipedModels[0], &armor.bipedModels[1],
            &armor.bipedModel1stPersons[0], &armor.bipedModel1stPersons[1]};
        for (std::size_t i = 0; i < channels.size(); ++i) channels[i]->SetModel(originals[i]);
        {
            ModelSwapScope scope(0xD201);
            scope.Add(manager->Swap(&armor, 0xD201, 0xD200));
            for (auto channel : channels) {
                Require(std::string(channel->GetModel()) == rule.model,
                    "ID-only armor swap uses owning NPC base ID across all model channels");
            }
        }
        for (std::size_t i = 0; i < channels.size(); ++i) {
            Require(originals[i] ? std::string(channels[i]->GetModel()) == originals[i] : channels[i]->GetModel() == nullptr,
                "ID-only armor scope restores every original model channel");
        }
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
        Require(!manager->HasContextRules(), "legacy entries do not require reference context capture");
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

    void TestContextSelectionAndRestoration() {
        auto manager = Manager::GetSingleton();
        Require(!manager->HasContextRules(), "existing ID-only and legacy rules do not enable contextual loading");
        static auto empty = MakeVariant("context-empty-exclusions-target.nif", "context-empty-exclusions-target.nif");
        empty.filter.excludeLocationID = std::vector<std::uint32_t>{};
        empty.filter.excludeKeywordID = std::vector<std::uint32_t>{};
        manager->Register("context-empty-exclusions-original.nif", {&empty});
        Require(!manager->HasContextRules(), "empty context exclusions retain the legacy capture fast path");

        static auto conditional = MakeVariant("context-shared-target.nif", "context-shared-target.nif");
        conditional.filter.locationID = std::vector<std::uint32_t>{0xEE10};
        manager->Register("context-shared-original.nif", {&conditional});
        Require(manager->HasContextRules() && manager->HasScopedRules(),
            "a contextual rule with no base/reference filter enables scoped loading and capture");
        FormFilterContext matching, other;
        matching.locations = {0xEE10};
        other.locations = {0xEE11};
        RE::TESBoundObject base;
        base.formID = 0xEE00;
        base.SetModel("context-shared-original.nif");
        {
            ModelSwapScope outer(0xEE01);
            outer.Add(manager->Swap(&base, 0xEE01, &matching));
            Require(std::string(base.GetModel()) == conditional.model, "matching geographic context selects the conditioned model");
            {
                ModelSwapScope nested(0xEE02);
                nested.Add(manager->Swap(&base, 0xEE02, &other));
                Require(std::string(base.GetModel()) == "context-shared-original.nif",
                    "a different context sharing the base sees the original during a nested load");
            }
            Require(std::string(base.GetModel()) == conditional.model, "nested contextual load resumes its outer replacement");
        }
        Require(std::string(base.GetModel()) == "context-shared-original.nif", "contextual scope restores the shared base");
        auto withoutReference = manager->Swap(&base, 0xEE01);
        Require(std::string(base.GetModel()) == "context-shared-original.nif", "contextual rules cannot leak into legacy no-context calls");
        withoutReference->Reset();
        {
            ModelSwapScope scope(0xEE01);
            scope.Add(manager->Swap(&base, 0xEE01, &other));
            Require(std::string(base.GetModel()) == "context-shared-original.nif", "changed context is reevaluated for the same reference ID");
        }

        static auto aliasScoped = MakeVariant("context-alias-target.nif", "context-alias-shared.nif");
        aliasScoped.filter.locationID = std::vector<std::uint32_t>{0xEE10};
        static auto aliasGlobal = MakeVariant("context-alias-shared.nif", "context-alias-shared.nif");
        manager->Register("context-alias-conditioned.nif", {&aliasScoped});
        manager->Register("context-alias-global.nif", {&aliasGlobal});
        bool sawConditioned = false;
        for (std::uint32_t ref = 1; ref <= 64; ++ref) {
            base.SetModel("context-alias-shared.nif");
            auto selected = manager->Swap(&base, ref, &matching);
            sawConditioned = sawConditioned || std::string(base.GetModel()) == aliasScoped.model;
            selected->Reset();
            selected = manager->Swap(&base, ref, &other);
            Require(std::string(base.GetModel()) == aliasGlobal.model, "unmatched contextual aliases leave legacy choices eligible");
            selected->Reset();
        }
        Require(sawConditioned, "legacy alias registration preserves existing conditioned variants");

        static auto skinRule = MakeVariant("context-skin-target.nif", "context-skin-target.nif", 0xEF00);
        skinRule.filter.keywordID = std::vector<std::uint32_t>{0xEF10};
        manager->Register(std::vector<std::string>{}, {&skinRule});
        FormFilterContext npcContext;
        npcContext.hasKeyword = [](std::uint32_t id) { return id == 0xEF10; };
        RE::TESObjectARMA armor;
        armor.formID = 0xEF99;
        const std::array<const char*, 4> originals{"context-male3.nif", "context-female3.nif", "context-male1.nif", "context-female1.nif"};
        std::array<RE::TESModel*, 4> channels{&armor.bipedModels[0], &armor.bipedModels[1],
            &armor.bipedModel1stPersons[0], &armor.bipedModel1stPersons[1]};
        for (std::size_t i = 0; i < channels.size(); ++i) channels[i]->SetModel(originals[i]);
        {
            ModelSwapScope scope(0xEF01);
            scope.Add(manager->Swap(&armor, 0xEF01, 0xEF00, &npcContext));
            for (auto* channel : channels) Require(std::string(channel->GetModel()) == skinRule.model,
                "owning NPC context and base ID reach every skin model channel");
        }
        for (std::size_t i = 0; i < channels.size(); ++i) Require(std::string(channels[i]->GetModel()) == originals[i],
            "conditioned skin swaps restore all four model channels");
        auto wrongOwner = manager->Swap(&armor, 0xEF01, 0xEF99, &npcContext);
        for (std::size_t i = 0; i < channels.size(); ++i) Require(std::string(channels[i]->GetModel()) == originals[i],
            "skin context cannot override an owning-NPC base ID mismatch");
        wrongOwner->Reset();

        auto dated = MakeVariant("context-dated-target.nif", "context-dated-target.nif");
        dated.filter.locationID = std::vector<std::uint32_t>{0xEE10};
        const auto dates = Time::fromDatePairString("01-01~31-01");
        dated.startDate = dates.first;
        dated.endDate = dates.second;
        models datedModels{{"context-dated-original.nif", {&dated}}};
        auto config = Config::GetSingleton();
        config->NowOverride = Time::fromString("2026-10-06 12:00:00");
        Require(!find(datedModels, "context-dated-original.nif", 0xEE01, 0xEE00, &matching),
            "a context match preserves existing temporal inactivity");
        config->BypassTemporalActivation = true;
        Require(find(datedModels, "context-dated-original.nif", 0xEE01, 0xEE00, &matching) == &dated,
            "temporal bypass permits a matching contextual rule");
        Require(!find(datedModels, "context-dated-original.nif", 0xEE01, 0xEE00, &other),
            "temporal bypass never bypasses context restrictions");
        config->BypassTemporalActivation = false;
        config->NowOverride = {};
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

    void TestOriginalModelLoading() {
        const auto fixtureRoot = std::filesystem::path(__FILE__).parent_path().parent_path() / "build/tests/original-model-fixture";
        std::filesystem::create_directories(fixtureRoot / "Data/SKSE/Plugins");
        const auto previousDirectory = std::filesystem::current_path();
        std::filesystem::current_path(fixtureRoot);
        auto entries = json::parse(R"([
            {"Variants":["loader-id-only-base-target.nif"],"BaseFormID":"D300"},
            {"Variants":["loader-id-only-ref-target.nif"],"ReferenceID":"D301"},
            {"Variants":["loader-id-only-both-target.nif"],"BaseFormID":"D302","ReferenceID":"D303"},
            {"Variants":["loader-id-only-excluded-target.nif"],"BaseFormID":["D304","D305"],"ReferenceID":["D306","D307"],"ExcludeBaseFormID":"D305","ExcludeReferenceID":"D307"},
            {"Variants":["loader-id-only-plugin-target.nif"],"BaseFormID":"Example.esp|D30A"},
            {"Variants":["loader-id-only-empty-target.nif"],"BaseFormID":[]},
            {"OriginalModel":["Meshes/Loader-Multi-A.nif","loader-multi-b.nif"],"Variants":["loader-multi-target.nif"],"BaseFormID":"D309"},
            {"OriginalModel":["loader-multi-global-a.nif","loader-multi-global-b.nif"],"Variants":["loader-multi-global-target.nif"]},
            {"OriginalModel":"loader-empty-alias-original.nif","Variants":["","Meshes/"]},
            {"Variants":["loader-no-target.nif"]},
            {"Variants":["loader-exclusion-only-target.nif"],"ExcludeReferenceID":"D3FF"},
            {"Variants":["loader-invalid-id-only-target.nif"],"BaseFormID":["D30B","bogus"]},
            {"OriginalModel":"loader-after-bad-original.nif","Variants":["loader-after-bad-original-target.nif"]}
        ])");
        const std::vector<json> invalidOriginals{nullptr, true, 12, json::object(), "", "Meshes/", json::array(),
            json::array({"loader-invalid-original.nif", 12}),
            json::array({"loader-invalid-original.nif", nullptr}),
            json::array({"loader-invalid-original.nif", ""}),
            json::array({"loader-invalid-original.nif", json::array({"nested.nif"})})};
        for (const auto& original : invalidOriginals) {
            entries.push_back(json{{"OriginalModel", original}, {"BaseFormID", "D400"},
                {"Variants", json::array({"loader-invalid-original-target.nif"})}});
        }
        {
            std::ofstream fixture("Data/originals_MS.json");
            fixture << entries;
        }
        Persistence::Install();
        const auto check = [](const char* original, std::uint32_t baseID, std::uint32_t refID, const char* expected) {
            RE::TESBoundObject base;
            base.formID = baseID;
            base.SetModel(original);
            {
                ModelSwapScope scope(refID);
                scope.Add(Manager::GetSingleton()->Swap(&base, refID));
                Require(base.GetModel() && std::string(base.GetModel()) == expected,
                    "loader accepts only matching valid optional/string/array model rules");
            }
            Require(original ? base.GetModel() && std::string(base.GetModel()) == original : base.GetModel() == nullptr,
                "loaded optional model rule restores its original path");
        };
        check("arbitrary-base-original.nif", 0xD300, 0xD3FF, "loader-id-only-base-target.nif");
        check("other-base-original.nif", 0xD300, 0xD3FE, "loader-id-only-base-target.nif");
        check(nullptr, 0xD300, 0xD3FE, "loader-id-only-base-target.nif");
        check("arbitrary-base-original.nif", 0xD3FF, 0xD3FE, "arbitrary-base-original.nif");
        check("arbitrary-ref-original.nif", 0xD3FF, 0xD301, "loader-id-only-ref-target.nif");
        check("arbitrary-ref-original.nif", 0xD3FE, 0xD301, "loader-id-only-ref-target.nif");
        check("arbitrary-ref-original.nif", 0xD3FE, 0xD3FF, "arbitrary-ref-original.nif");
        check("arbitrary-both-original.nif", 0xD302, 0xD303, "loader-id-only-both-target.nif");
        check("arbitrary-both-original.nif", 0xD3FF, 0xD303, "arbitrary-both-original.nif");
        check("arbitrary-both-original.nif", 0xD302, 0xD3FF, "arbitrary-both-original.nif");
        check("arbitrary-excluded-original.nif", 0xD304, 0xD306, "loader-id-only-excluded-target.nif");
        check("arbitrary-excluded-original.nif", 0xD305, 0xD306, "arbitrary-excluded-original.nif");
        check("arbitrary-excluded-original.nif", 0xD304, 0xD307, "arbitrary-excluded-original.nif");
        check("arbitrary-plugin-original.nif", 0x0100D30A, 0xD3FF, "loader-id-only-plugin-target.nif");
        check("loader-multi-a.nif", 0xD309, 0xD3FF, "loader-multi-target.nif");
        check("Meshes\\LOADER-MULTI-B.nif", 0xD309, 0xD3FF, "loader-multi-target.nif");
        check("loader-multi-a.nif", 0xD3FF, 0xD3FF, "loader-multi-a.nif");
        check("loader-multi-unlisted.nif", 0xD309, 0xD3FF, "loader-multi-unlisted.nif");
        check("loader-multi-global-a.nif", 0xD3FF, 0xD3FF, "loader-multi-global-target.nif");
        check("loader-multi-global-b.nif", 0xD3FF, 0xD3FF, "loader-multi-global-target.nif");
        check("loader-invalid-original.nif", 0xD400, 0xD3FF, "loader-invalid-original.nif");
        check("loader-invalid-original-target.nif", 0xD400, 0xD3FF, "loader-invalid-original-target.nif");
        check("unlisted-invalid-original.nif", 0xD400, 0xD3FF, "unlisted-invalid-original.nif");
        check("arbitrary-invalid-id-original.nif", 0xD30B, 0xD3FF, "arbitrary-invalid-id-original.nif");
        check("unrelated-empty-alias.nif", 0xD500, 0xD501, "unrelated-empty-alias.nif");
        check("loader-after-bad-original.nif", 0xD3FF, 0xD3FF, "loader-after-bad-original-target.nif");
        std::filesystem::remove("Data/originals_MS.json");
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

    void TestContextConfigLoading() {
        static std::array<RE::BGSLocation, 2> locations;
        static std::array<RE::TESRegion, 2> regions;
        static std::array<RE::BGSKeyword, 2> keywords;
        static std::array<RE::TESObjectCELL, 2> cells;
        static std::array<RE::TESWorldSpace, 2> worldspaces;
        struct Category {
            const char* include;
            const char* exclude;
            std::uint32_t id;
            std::function<void(FormFilterContext&, std::uint32_t)> populate;
        };
        const std::array<Category, 5> categories{{
            {"LocationID", "ExcludeLocationID", 0x0100F100, [](auto& context, auto id) { context.locations = {id}; }},
            {"RegionID", "ExcludeRegionID", 0x0100F200, [](auto& context, auto id) { context.regions = {id}; }},
            {"KeywordID", "ExcludeKeywordID", 0x0100F300, [](auto& context, auto id) {
                context.hasKeyword = [id](auto candidate) { return candidate == id; };
            }},
            {"CellID", "ExcludeCellID", 0x0100F400, [](auto& context, auto id) { context.cellID = id; }},
            {"WorldspaceID", "ExcludeWorldspaceID", 0x0100F500, [](auto& context, auto id) { context.worldspaces = {id}; }}
        }};
        for (std::size_t i = 0; i < 2; ++i) {
            RE::TESForm::Register(locations[i], categories[0].id + static_cast<std::uint32_t>(i));
            RE::TESForm::Register(regions[i], categories[1].id + static_cast<std::uint32_t>(i));
            RE::TESForm::Register(keywords[i], categories[2].id + static_cast<std::uint32_t>(i));
            RE::TESForm::Register(cells[i], categories[3].id + static_cast<std::uint32_t>(i));
            RE::TESForm::Register(worldspaces[i], categories[4].id + static_cast<std::uint32_t>(i));
        }
        const auto qualified = [](std::uint32_t id) { return std::format("Example.esp|{:X}", id & 0xFFFFFF); };
        auto entries = json::array();
        const auto add = [&](const std::string& stem, const char* field, const json& ids) {
            entries.push_back(json{{"OriginalModel", stem + ".nif"},
                {"Variants", json::array({stem + "-target.nif"})}, {field, ids}});
        };
        for (const auto& category : categories) {
            for (const auto field : {category.include, category.exclude}) {
                const auto stem = std::string("loader-context-") + field;
                add(stem + "-scalar", field, qualified(category.id));
                add(stem + "-array", field, json::array({category.id + 1, qualified(category.id)}));
                add(stem + "-empty", field, json::array());
                add(stem + "-invalid", field, json::array({qualified(category.id), "WhiterunLocation"}));
                const auto wrongType = category.id == categories[2].id ? categories[1].id : categories[2].id;
                add(stem + "-wrong-type", field, json::array({qualified(category.id), wrongType}));
                add(stem + "-unresolved", field, category.id + 2);
            }
        }
        entries.push_back(json{{"OriginalModel", "loader-context-mixed.nif"},
            {"Variants", json::array({"loader-context-mixed-target.nif"})},
            {"BaseFormID", "F000"}, {"ReferenceID", "F001"},
            {"LocationID", qualified(categories[0].id)}, {"RegionID", qualified(categories[1].id)},
            {"ExcludeKeywordID", qualified(categories[2].id)}});
        entries.push_back(json{{"Variants", json::array({"loader-context-missing-target.nif"})},
            {"LocationID", qualified(categories[0].id)}});
        entries.push_back(json{{"OriginalModel", "loader-context-after-invalid.nif"},
            {"Variants", json::array({"loader-context-after-invalid-target.nif"})}});

        const auto fixtureRoot = std::filesystem::path(__FILE__).parent_path().parent_path() / "build/tests/context-fixture";
        std::filesystem::create_directories(fixtureRoot / "Data/SKSE/Plugins");
        const auto previousDirectory = std::filesystem::current_path();
        std::filesystem::current_path(fixtureRoot);
        {
            std::ofstream fixture("Data/context_MS.json");
            fixture << entries;
        }
        Persistence::Install();
        const auto check = [](const std::string& stem, const FormFilterContext* context, bool swaps,
                             std::uint32_t baseID = 0xF000, std::uint32_t refID = 0xF001) {
            RE::TESBoundObject base;
            base.formID = baseID;
            const auto original = stem + ".nif";
            const auto target = stem + "-target.nif";
            base.SetModel(original.c_str());
            {
                ModelSwapScope scope(refID);
                scope.Add(Manager::GetSingleton()->Swap(&base, refID, context));
                const auto expected = swaps ? target : original;
                if (std::string(base.GetModel()) != expected) {
                    std::cerr << "Context loader case " << stem << ": expected " << expected
                              << ", got " << base.GetModel() << '\n';
                }
                Require(std::string(base.GetModel()) == expected,
                    "production loader and matcher enforce every contextual field and its record type");
            }
            Require(std::string(base.GetModel()) == original, "loaded contextual entries restore the original model");
        };
        for (const auto& category : categories) {
            FormFilterContext matching, other, unresolved;
            category.populate(matching, category.id);
            category.populate(other, category.id + 2);
            category.populate(unresolved, category.id + 2);
            const bool deferredCell = std::string_view(category.include) == "CellID";
            if (deferredCell) {
                // This cell is deliberately created only after configuration loading
                // and is never registered in the engine's form lookup table.
                static RE::TESObjectCELL lateCell;
                static RE::TESObjectREFR lateReference;
                lateCell.formID = category.id + 2;
                lateReference.parentCell = &lateCell;
                unresolved = FormFilters::CaptureContext(&lateReference);
            }
            for (const bool exclusion : {false, true}) {
                const auto field = exclusion ? category.exclude : category.include;
                const auto stem = std::string("loader-context-") + field;
                for (const auto mode : {"-scalar", "-array"}) {
                    check(stem + mode, &matching, !exclusion);
                    check(stem + mode, &other, exclusion);
                    check(stem + mode, nullptr, false);
                }
                check(stem + "-empty", &matching, exclusion);
                check(stem + "-empty", nullptr, exclusion);
                check(stem + "-invalid", &matching, false);
                check(stem + "-wrong-type", &matching, false);
                check(stem + "-unresolved", &unresolved, deferredCell && !exclusion);
                check(stem + "-unresolved", &matching, deferredCell && exclusion);
            }
        }
        FormFilterContext locationOnly, regionOnly, neither, excluded;
        categories[0].populate(locationOnly, categories[0].id);
        categories[1].populate(regionOnly, categories[1].id);
        excluded = locationOnly;
        categories[2].populate(excluded, categories[2].id);
        check("loader-context-mixed", &locationOnly, true);
        check("loader-context-mixed", &regionOnly, true);
        check("loader-context-mixed", &neither, false);
        check("loader-context-mixed", &excluded, false);
        check("loader-context-mixed", &locationOnly, false, 0xF002, 0xF001);
        check("loader-context-mixed", &locationOnly, false, 0xF000, 0xF002);
        check("loader-context-unlisted", &locationOnly, false);
        check("loader-context-after-invalid", nullptr, true);
        std::filesystem::remove("Data/context_MS.json");
        std::filesystem::current_path(previousDirectory);
    }
}

int main() {
    TestFilter();
    TestIDParsing();
    TestContextFilters();
    TestContextCapture();
    TestSelection();
    TestIDOnlySelection();
    TestLegacyManagement();
    TestExclusionManagement();
    TestSharedBaseRestoration();
    TestNestedScopes();
    TestAliasRegistration();
    TestNullPathSnapshot();
    TestUnchangedModelPreservation();
    TestArmorRestoration();
    TestConfigLoading();
    TestMultipleOriginalRegistration();
    TestIDOnlyRestoration();
    TestOriginalModelLoading();
    TestContextSelectionAndRestoration();
    TestContextConfigLoading();
    std::cout << "Passed " << checks << " regression checks.\n";
}

