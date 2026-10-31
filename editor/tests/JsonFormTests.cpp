#include "JsonForm.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QJsonArray>
#include <QLineEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QToolButton>

#include <array>
#include <iostream>
#include <stdexcept>

namespace {
const QStringList conditionIDFields{
    "LocationID", "RegionID", "KeywordID", "CellID", "WorldspaceID",
    "ExcludeLocationID", "ExcludeRegionID", "ExcludeKeywordID", "ExcludeCellID", "ExcludeWorldspaceID"
};

void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}

QGroupBox* field(JsonForm& form, const QString& name) {
    for (auto* check : form.findChildren<QCheckBox*>())
        if (check->text() == name) return qobject_cast<QGroupBox*>(check->parentWidget());
    throw std::runtime_error("Missing schema field");
}

QPushButton* button(QWidget* parent, const QString& name) {
    for (auto* control : parent->findChildren<QPushButton*>())
        if (control->text() == name && !control->isHidden()) return control;
    throw std::runtime_error("Missing form button");
}

QLineEdit* idText(QWidget* parent) {
    for (auto* text : parent->findChildren<QLineEdit*>())
        if (text->accessibleName() == "Form ID") return text;
    throw std::runtime_error("Missing Form ID text input");
}

QComboBox* idPlugin(QWidget* parent) {
    for (auto* combo : parent->findChildren<QComboBox*>())
        if (combo->accessibleName() == "Plugin filename") return combo;
    throw std::runtime_error("Missing plugin filename control");
}

QTabWidget* settingsTabs(JsonForm& form) {
    auto* tabs = form.findChild<QTabWidget*>(QStringLiteral("entryTabs"));
    require(tabs != nullptr, "Entry settings must have a tabbed form");
    return tabs;
}

QList<QToolButton*> removeButtons(QWidget* parent) {
    QList<QToolButton*> result;
    for (auto* control : parent->findChildren<QToolButton*>())
        if (control->text() == "Remove" && !control->isHidden()) result.append(control);
    return result;
}

void requireAddAboveInput(QWidget* group, const QString& name, QWidget* input) {
    auto* add = button(group, name);
    require(add->mapTo(group, QPoint(0, add->height())).y() <= input->mapTo(group, QPoint()).y(),
            "List add controls must appear above their editable items");
}

void processDeletes() {
    QCoreApplication::sendPostedEvents(nullptr, QEvent::DeferredDelete);
}

void testRoundTrip() {
    JsonForm form({"Skyrim.esm", "Installed.esp", "Light.esl"});
    const QJsonObject entry{
        {"OriginalModel", QJsonArray{"One.nif", "Two.nif"}},
        {"Variants", QJsonArray{"Variant.nif"}},
        {"BaseFormID", QJsonArray{4660, "0x01001234", "Missing.esp|001234"}},
        {"ReferenceID", "Light.esl|ABC"},
        {"ExcludeBaseFormID", QJsonArray{}},
        {"ExcludeReferenceID", "FF001234"},
        {"ActiveTimeSpan", "01-06~31-08"},
        {"Extension", QJsonObject{{"multiline", "first\nsecond"}, {"integer", QJsonValue(qint64(9007199254740993LL))},
                                  {"nested", QJsonArray{true, false, QJsonValue::Null, 2.25, QJsonObject{{"x", "y"}}}}}}
    };
    form.setValue(entry);
    require(form.value() == entry, "Form round trip must preserve every field and ID representation");
    require(form.errors().isEmpty(), "Valid form must have no numeric errors");
    auto* reference = field(form, "ReferenceID");
    bool foundInstalled = false;
    for (auto* combo : reference->findChildren<QComboBox*>())
        if (combo->accessibleName() == "Plugin filename") foundInstalled = combo->findText("Installed.esp") >= 0;
    require(foundInstalled, "Plugin dropdown must contain discovered installed plugins");
    for (auto* combo : form.findChildren<QComboBox*>())
        require(combo->accessibleName() == "Plugin filename", "ID controls must not expose alternative input formats");
}

void testEdits() {
    JsonForm form({"Skyrim.esm", "Installed.esp"});
    form.setValue({{"OriginalModel", "Original.nif"}, {"Variants", QJsonArray{"Variant.nif"}},
                   {"BaseFormID", "Missing.esp|001234"}, {"ReferenceID", 4660}});
    int changes = 0;
    QObject::connect(&form, &JsonForm::changed, [&changes] { ++changes; });
    auto* base = field(form, "BaseFormID");
    idText(base)->setText("001235");
    require(form.value().value("BaseFormID").toString() == "Missing.esp|001235", "Editing local ID must retain plugin name");
    for (auto* combo : base->findChildren<QComboBox*>())
        if (combo->accessibleName() == "Plugin filename") combo->setCurrentText("Installed.esp");
    require(form.value().value("BaseFormID").toString() == "Installed.esp|001235", "Plugin selection must update the combined ID");

    auto* reference = field(form, "ReferenceID");
    require(idPlugin(reference)->currentText().isEmpty() && idText(reference)->text().isEmpty() &&
                form.value().value("ReferenceID").toInteger() == 4660,
            "Legacy runtime IDs must be preserved without guessing a plugin or local ID");
    idPlugin(reference)->setCurrentText("Installed.esp");
    require(form.errors().join('\n').contains("ReferenceID"), "Replacing a legacy ID requires an explicit local ID too");
    idText(reference)->setText("001234");
    require(form.value().value("ReferenceID").toString() == "Installed.esp|001234" && form.errors().isEmpty(),
            "Explicit legacy replacements must use only plugin-qualified hexadecimal IDs");
    idText(reference)->clear();
    idPlugin(reference)->setCurrentText("");
    require(form.value().value("ReferenceID").toInteger() == 4660 && form.errors().isEmpty(),
            "Clearing both replacement fields restores the unchanged legacy ID");

    auto* original = field(form, "OriginalModel");
    button(original, "Use list")->click();
    processDeletes();
    require(form.value().value("OriginalModel").toArray() == QJsonArray{"Original.nif"}, "Single original must convert to list without losing it");
    button(original, "Add model")->click();
    processDeletes();
    require(form.value().value("OriginalModel").toArray().size() == 2, "Add model must append an editable path");

    auto* exclusion = field(form, "ExcludeBaseFormID");
    auto* include = exclusion->findChild<QCheckBox*>();
    require(include && !include->isChecked() && !form.value().contains("ExcludeBaseFormID"), "Absent option must stay omitted");
    include->setChecked(true);
    idText(exclusion)->setText("123");
    require(form.value().value("ExcludeBaseFormID").toString() == "Skyrim.esm|123", "Enabling option must include its editable value");
    include->setChecked(false);
    require(!form.value().contains("ExcludeBaseFormID"), "Disabling option must omit it again");
    require(changes >= 8, "Widget edits must notify the document about changes");
}

void testTitleEditing() {
    JsonForm form({"Skyrim.esm"});
    const QJsonObject legacy{{"OriginalModel", "Original.nif"}, {"Variants", QJsonArray{"Variant.nif"}},
                             {"Extension", QJsonObject{{"preserved", true}}}};
    form.setValue(legacy);
    int changes = 0;
    QObject::connect(&form, &JsonForm::changed, [&changes] { ++changes; });
    auto* title = field(form, "Title");
    auto* included = title->findChild<QCheckBox*>();
    auto* input = title->findChild<QLineEdit*>();
    require(included && !included->isChecked() && input && input->accessibleName() == "Title" &&
                !input->placeholderText().isEmpty() && form.value() == legacy,
            "Legacy entries must offer an optional single-line title without adding it to JSON");
    auto expected = legacy;
    field(form, "OriginalModel")->findChild<QLineEdit*>()->setText("EditedOriginal.nif");
    expected.insert("OriginalModel", "EditedOriginal.nif");
    require(form.value() == expected, "Unrelated edits must keep an absent Title omitted");

    included->setChecked(true);
    require(form.value().value("Title").isString() && form.value().value("Title").toString().isEmpty(),
            "Enabling Title must create a JSON string");
    input->setText(QString::fromUtf8("Summer carts \xE2\x80\x94 Riften"));
    expected.insert("Title", input->text());
    require(form.value() == expected && changes >= 3 && form.errors().isEmpty(),
            "Editing a title must notify the document and preserve its exact text and other properties");
    button(field(form, "OriginalModel"), "Use list")->click();
    processDeletes();
    expected.insert("OriginalModel", QJsonArray{"EditedOriginal.nif"});
    require(form.value() == expected, "Rebuilding other controls must retain the edited title");
    included->setChecked(false);
    require(!form.value().contains("Title"), "Disabling Title must omit it from the saved entry");
    included->setChecked(true);
    require(form.value() == expected, "Re-enabling Title must retain its previously entered text");
    form.setValue(expected);
    processDeletes();
    require(form.value() == expected && field(form, "Title")->findChild<QLineEdit*>()->text() == expected.value("Title").toString(),
            "An existing title must round trip through the single-line control");
    form.setValue(legacy);
    processDeletes();
    require(form.value() == legacy, "A previous entry's title must not leak into a legacy entry");
}

void testInvalidValuesCanBeRepaired() {
    JsonForm form({"Skyrim.esm"});
    form.setValue({{"OriginalModel", true}, {"Variants", "Replacement.nif"}, {"BaseFormID", false}, {"ActiveTimeSpan", 5}});
    const auto initial = form.value();
    require(initial.value("OriginalModel").isBool() && initial.value("Variants").isString(),
            "Malformed values must not be silently converted on load");
    button(field(form, "Variants"), "Use list")->click();
    processDeletes();
    require(form.value().value("Variants").toArray() == QJsonArray{"Replacement.nif"}, "Malformed scalar Variants can be repaired");
    button(field(form, "BaseFormID"), "Reset field")->click();
    processDeletes();
    require(form.value().value("BaseFormID").isString(), "Malformed ID type can be reset to an editable ID");
    for (const auto& malformed : QJsonArray{42, true, QJsonValue::Null, QJsonArray{"unexpected"}, QJsonObject{{"unexpected", true}}}) {
        const QJsonObject entry{{"Title", malformed}, {"OriginalModel", "Original.nif"}, {"Variants", QJsonArray{"Variant.nif"}}};
        form.setValue(entry);
        processDeletes();
        require(form.value() == entry, "Malformed titles must retain their original JSON type and value on load");
        button(field(form, "Title"), "Reset field")->click();
        processDeletes();
        require(form.value().value("Title").isString() && form.value().value("Title").toString().isEmpty() &&
                    field(form, "Title")->findChild<QLineEdit*>()->accessibleName() == "Title",
                "Malformed titles must reset explicitly to an editable empty string");
    }
}

void testLegacyIDReplacement() {
    JsonForm form({"Skyrim.esm", "Installed.esp"});
    for (const auto& raw : QJsonArray{4660, "0x01001234", "FF001234"}) {
        const QJsonObject entry{{"OriginalModel", "Original.nif"}, {"Variants", QJsonArray{"Variant.nif"}},
                               {"BaseFormID", raw}, {"LocationID", QJsonArray{raw, "Missing.esp|001234"}}};
        form.setValue(entry);
        processDeletes();
        auto* base = field(form, "BaseFormID");
        require(form.value() == entry && form.errors().isEmpty() &&
                    idPlugin(base)->currentText().isEmpty() && idText(base)->text().isEmpty(),
                "Legacy IDs must open unchanged with blank explicit replacement inputs");
        auto* included = base->findChild<QCheckBox*>();
        included->setChecked(false);
        included->setChecked(true);
        require(form.value() == entry, "Toggling a legacy ID must preserve its original JSON type and value");
        button(base, "Use list")->click();
        processDeletes();
        require(form.value().value("BaseFormID").toArray() == QJsonArray{raw},
                "Wrapping a legacy scalar in a list must preserve it exactly");
        button(base, "Add Form ID")->click();
        processDeletes();
        int inputIndex = 0;
        for (auto* input : base->findChildren<QLineEdit*>())
            if (input->accessibleName() == "Form ID" && inputIndex++ == 1) input->setText("ABC");
        require(form.value().value("BaseFormID").toArray() == QJsonArray{raw, "Skyrim.esm|ABC"},
                "Adding a plugin-local ID must not convert a legacy sibling");
        removeButtons(base).last()->click();
        processDeletes();
        button(base, "Use single value")->click();
        processDeletes();
        require(form.value() == entry, "Removing another item and unwrapping the list must retain exact legacy values");
        idText(base)->setText("001235");
        require(form.errors().join('\n').contains("BaseFormID"), "A local ID alone cannot replace a legacy runtime ID");
        idPlugin(base)->setCurrentText("Installed.esp");
        auto expected = entry;
        expected.insert("BaseFormID", "Installed.esp|001235");
        require(form.errors().isEmpty() && form.value() == expected,
                "Explicit plugin and local ID must replace only the requested legacy scalar");
        auto* locations = field(form, "LocationID");
        idPlugin(locations)->setCurrentText("Installed.esp");
        require(form.errors().join('\n').contains("LocationID[0]"), "Partial legacy list replacements must identify their item");
        idText(locations)->setText("001236");
        expected.insert("LocationID", QJsonArray{"Installed.esp|001236", "Missing.esp|001234"});
        require(form.errors().isEmpty() && form.value() == expected,
                "Replacing one legacy list item must preserve its other plugin-qualified values");
    }
}

void testConditionFields() {
    JsonForm form({"Skyrim.esm", "Installed.esp", "Light.esl"});
    const QJsonObject legacy{{"OriginalModel", "Original.nif"}, {"Variants", QJsonArray{"Variant.nif"}}};
    form.setValue(legacy);
    require(form.value() == legacy, "New optional controls must not add fields to legacy entries");

    for (const auto& name : conditionIDFields) {
        auto* group = field(form, name);
        auto* included = group->findChild<QCheckBox*>();
        require(included && !included->isChecked(), "Each condition must have a dedicated unchecked optional control");
        require(idPlugin(group) != nullptr, "Each condition must offer the plugin/local Form ID controls");
        bool installedPlugin = false;
        for (auto* combo : group->findChildren<QComboBox*>()) {
            if (combo->accessibleName() == "Plugin filename") {
                installedPlugin = combo->findText("Installed.esp") >= 0;
                combo->setCurrentText("Installed.esp");
            }
        }
        require(installedPlugin, "Every condition plugin dropdown must list discovered plugins");
        included->setChecked(true);
        idText(group)->setText("001234");
        require(form.value().value(name).toString() == "Installed.esp|001234", "Condition controls must save plugin-qualified Form IDs");

        idText(group)->setText("001235");
        require(form.value().value(name).toString() == "Installed.esp|001235" && form.errors().isEmpty(),
                "Editing any condition must retain its plugin and local hexadecimal ID format");

        button(group, "Use list")->click();
        processDeletes();
        require(form.value().value(name).toArray() == QJsonArray{"Installed.esp|001235"}, "Condition scalar-to-list conversion must preserve its plugin and ID");
        button(group, "Use single value")->click();
        processDeletes();
        require(form.value().value(name).toString() == "Installed.esp|001235", "Condition list-to-scalar conversion must preserve its ID");
        button(group, "Use list")->click();
        processDeletes();
        button(group, "Add Form ID")->click();
        processDeletes();
        int textIndex = 0;
        for (auto* text : group->findChildren<QLineEdit*>()) {
            if (text->accessibleName() == "Form ID" && textIndex++ == 1) text->setText("ABC");
        }
        require(form.value().value(name).toArray() == QJsonArray{"Installed.esp|001235", "Skyrim.esm|ABC"},
                "Every condition must add and edit only plugin-qualified IDs");
        included->setChecked(false);
        require(!form.value().contains(name), "Disabling any condition must omit it from the saved entry");
    }
    require(form.value() == legacy, "Editing and disabling conditions must preserve legacy properties");

    auto entry = legacy;
    const QJsonArray forms{QString("Missing.esp|001234"), QString("0x01001234"), 4660,
                          QJsonArray{}, QJsonArray{4660, "Light.esl|ABC", "0x01001234"}};
    for (qsizetype index = 0; index < conditionIDFields.size(); ++index)
        entry.insert(conditionIDFields.at(index), forms.at(index % forms.size()));
    entry.insert("Extension", QJsonObject{{"nested", QJsonArray{true, QJsonValue::Null, "unchanged"}}});
    form.setValue(entry);
    processDeletes();
    require(form.value() == entry && form.errors().isEmpty(),
            "Condition fields must round trip scalar, list, empty-list and unknown values unchanged");

    entry.insert("LocationID", false);
    form.setValue(entry);
    processDeletes();
    require(form.value().value("LocationID").isBool(), "Invalid condition types must remain repairable without silent conversion");
    button(field(form, "LocationID"), "Reset field")->click();
    processDeletes();
    require(form.value().value("LocationID").isString(), "Malformed conditions must offer the existing ID reset control");
}

void testUnknownPropertiesPreservedWithoutControls() {
    JsonForm form({"Skyrim.esm"});
    const QJsonObject legacy{{"OriginalModel", "Original.nif"}, {"Variants", QJsonArray{"Variant.nif"}}};
    const QJsonObject unknown{{"CustomFormID", "Unknown.esp|ABC"},
                             {"ExtraText", "first\nsecond"},
                             {"ExtraNested", QJsonObject{{"values", QJsonArray{true, QJsonValue::Null, 2.25,
                                 QJsonArray{"nested", QJsonObject{{"preserved", false}}}}}}},
                             {"ExtraInteger", QJsonValue(qint64(9007199254740993LL))}};
    auto expected = legacy;
    for (auto property = unknown.begin(); property != unknown.end(); ++property) expected.insert(property.key(), property.value());
    expected.insert("BaseFormID", "Skyrim.esm|001234");
    expected.insert("ActiveTimeSpan", "01-06~31-08");
    form.setValue(expected);
    require(form.value() == expected, "Unknown strings, IDs, nested lists and large integers must round trip unchanged");
    for (auto* check : form.findChildren<QCheckBox*>())
        require(!unknown.contains(check->text()), "Unknown JSON properties must not expose form controls");
    for (auto* control : form.findChildren<QPushButton*>())
        require(control->text() != "Add extra property" && control->text() != "Add property" && control->text() != "Add value",
                "Entry forms must not expose extra-property creation controls");

    idText(field(form, "BaseFormID"))->setText("001235");
    expected.insert("BaseFormID", "Skyrim.esm|001235");
    field(form, "OriginalModel")->findChild<QLineEdit*>()->setText("EditedOriginal.nif");
    expected.insert("OriginalModel", "EditedOriginal.nif");
    require(form.value() == expected, "Editing known fields must preserve every unknown JSON value verbatim");
    field(form, "BaseFormID")->findChild<QCheckBox*>()->setChecked(false);
    expected.remove("BaseFormID");
    field(form, "ActiveTimeSpan")->findChild<QCheckBox*>()->setChecked(false);
    expected.remove("ActiveTimeSpan");
    require(form.value() == expected, "Disabling known fields must remove their keys while retaining unknown properties");
    button(field(form, "OriginalModel"), "Use list")->click();
    processDeletes();
    expected.insert("OriginalModel", QJsonArray{"EditedOriginal.nif"});
    require(form.value() == expected, "Rebuilding a known list must retain hidden unknown data");
    form.setValue(expected);
    processDeletes();
    require(form.value() == expected, "Reloading the edited entry must preserve unknown JSON properties");
    form.setValue(legacy);
    processDeletes();
    require(form.value() == legacy, "Unknown properties from a previous entry must not leak into another entry");
}

void testTabPartitionAndNavigation() {
    JsonForm form({"Skyrim.esm", "Installed.esp"});
    QJsonObject entry{{"Title", "Named swap"}, {"OriginalModel", QJsonArray{"One.nif", "Two.nif"}},
                      {"Variants", QJsonArray{"Variant.nif"}},
                      {"BaseFormID", "Installed.esp|001234"}, {"ReferenceID", 4660},
                      {"ExcludeBaseFormID", QJsonArray{}}, {"ExcludeReferenceID", "FF001234"},
                      {"ActiveTimeSpan", "01-06~31-08"},
                      {"Extension", QJsonObject{{"nested", QJsonArray{true, QJsonValue::Null, "unchanged"}}}}};
    for (const auto& name : conditionIDFields) entry.insert(name, "Installed.esp|002345");
    form.setValue(entry);
    int changes = 0;
    QObject::connect(&form, &JsonForm::changed, [&changes] { ++changes; });
    auto* tabs = settingsTabs(form);
    const std::array<std::pair<QString, QStringList>, 4> sections{{
        {"Basics", {"Title", "Variants"}},
        {"Targets", {"OriginalModel", "BaseFormID", "ReferenceID"}},
        {"Inclusions", {"ActiveTimeSpan", "LocationID", "RegionID", "KeywordID", "CellID", "WorldspaceID"}},
        {"Exclusions", {"ExcludeBaseFormID", "ExcludeReferenceID", "ExcludeLocationID", "ExcludeRegionID",
                         "ExcludeKeywordID", "ExcludeCellID", "ExcludeWorldspaceID"}}
    }};
    require(tabs->count() == static_cast<int>(sections.size()), "Entry settings must contain exactly the four requested tabs");
    for (int index = 0; index < tabs->count(); ++index) {
        const auto& section = sections.at(static_cast<std::size_t>(index));
        require(tabs->tabText(index) == section.first, "Entry tabs must have the requested order and labels");
        auto* page = tabs->widget(index);
        const auto scrolls = page->findChildren<QScrollArea*>();
        require(scrolls.size() == 1 && scrolls.first()->widget() && scrolls.first()->widgetResizable(),
                "Each settings tab must own a scrollable, resizing content area");
        for (const auto& name : section.second)
            require(page->isAncestorOf(field(form, name)), "Each JSON field must appear in its intended settings tab");
        tabs->setCurrentIndex(index);
        QCoreApplication::processEvents();
        require(form.value() == entry && form.errors().isEmpty(), "Navigating tabs must preserve every field and JSON representation");
    }
    require(changes == 0, "Switching settings tabs must not mark the entry as edited");
    tabs->setCurrentIndex(2);
    idText(field(form, "LocationID"))->setText("002346");
    entry.insert("LocationID", "Installed.esp|002346");
    tabs->setCurrentIndex(1);
    field(form, "OriginalModel")->findChildren<QLineEdit*>().first()->setText("EditedOne.nif");
    entry.insert("OriginalModel", QJsonArray{"EditedOne.nif", "Two.nif"});
    const auto afterEdits = changes;
    for (int index = tabs->count() - 1; index >= 0; --index) tabs->setCurrentIndex(index);
    require(afterEdits >= 2 && changes == afterEdits && form.value() == entry,
            "Actual edits in different tabs must persist while later navigation remains clean");
    auto* optional = field(form, "LocationID")->findChild<QCheckBox*>();
    optional->setChecked(false);
    require(!form.value().contains("LocationID"), "Disabling an optional field must still omit its JSON property");
    tabs->setCurrentIndex(3);
    tabs->setCurrentIndex(2);
    optional->setChecked(true);
    require(form.value() == entry, "Hiding and showing optional content across tabs must retain its edited value");
}

void testListControlsPreserveUnknownData() {
    JsonForm form({"Skyrim.esm", "Installed.esp"});
    const QJsonObject initial{{"OriginalModel", QJsonArray{"One.nif", "Two.nif"}},
                             {"Variants", QJsonArray{"VariantA.nif", "VariantB.nif"}},
                             {"BaseFormID", QJsonArray{"Installed.esp|001234", "Installed.esp|001235"}},
                             {"ExtraList", QJsonArray{"first", "second"}}};
    form.setValue(initial);
    form.resize(1050, 700);
    form.show();
    QCoreApplication::processEvents();
    auto* tabs = settingsTabs(form);
    tabs->setCurrentIndex(1);
    QCoreApplication::processEvents();
    auto* original = field(form, "OriginalModel");
    const auto initialModelInputs = original->findChildren<QLineEdit*>();
    require(initialModelInputs.size() == 2, "Model lists must expose one editable path per item");
    require(original->findChildren<QGroupBox*>().isEmpty(), "Simple model lists must use compact rows rather than nested Value groups");
    requireAddAboveInput(original, "Add model", initialModelInputs.first());
    require(removeButtons(original).size() == 2, "Each compact model row must provide its own remove control");
    button(original, "Add model")->click();
    processDeletes();
    const auto modelInputs = original->findChildren<QLineEdit*>();
    require(modelInputs.size() == 3 && form.value().value("Variants") == initial.value("Variants"),
            "Adding an original model must append only to the original-model list");
    modelInputs.last()->setText("Three.nif");
    removeButtons(original).at(1)->click();
    processDeletes();
    require(form.value().value("OriginalModel").toArray() == QJsonArray{"One.nif", "Three.nif"} &&
            form.value().value("Variants") == initial.value("Variants"),
            "Removing a middle model must preserve the other items and replacement list");
    require(settingsTabs(form)->currentIndex() == 1, "Original-model list edits must keep the Targets tab selected");

    tabs->setCurrentIndex(1);
    QCoreApplication::processEvents();
    auto* base = field(form, "BaseFormID");
    requireAddAboveInput(base, "Add Form ID", idText(base));
    button(base, "Add Form ID")->click();
    processDeletes();
    require(form.value().value("BaseFormID").toArray().size() == 3 && settingsTabs(form)->currentIndex() == 1,
            "Adding a Form ID must modify its own list and retain the Targets tab");
    int idIndex = 0;
    for (auto* input : base->findChildren<QLineEdit*>())
        if (input->accessibleName() == "Form ID" && idIndex++ == 2) input->setText("001236");

    require(removeButtons(base).size() == 3, "Each Form ID list item must provide its own remove control");
    removeButtons(base).at(1)->click();
    processDeletes();
    auto expected = initial;
    expected.insert("OriginalModel", QJsonArray{"One.nif", "Three.nif"});
    expected.insert("BaseFormID", QJsonArray{"Installed.esp|001234", "Skyrim.esm|001236"});
    require(settingsTabs(form)->currentIndex() == 1 && form.value() == expected,
            "Form ID removal must retain its active tab, unrelated models and hidden unknown data");
    require(form.value().value("ExtraList") == initial.value("ExtraList"),
            "Known-list rebuilds must never discard or modify unknown JSON properties");
}
}

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        testRoundTrip();
        testEdits();
        testTitleEditing();
        testLegacyIDReplacement();
        testInvalidValuesCanBeRepaired();
        testConditionFields();
        testUnknownPropertiesPreservedWithoutControls();
        testTabPartitionAndNavigation();
        testListControlsPreserveUnknownData();
        std::cout << "JsonForm tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
