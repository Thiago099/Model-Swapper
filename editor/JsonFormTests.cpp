#include "JsonForm.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QInputDialog>
#include <QJsonArray>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QTimer>

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

QComboBox* idMode(QWidget* parent) {
    for (auto* combo : parent->findChildren<QComboBox*>())
        if (combo->accessibleName() == "Form ID format") return combo;
    throw std::runtime_error("Missing Form ID format control");
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
    auto* plugin = reference->findChild<QComboBox*>(QString(), Qt::FindChildrenRecursively);
    Q_UNUSED(plugin);
    bool foundInstalled = false;
    for (auto* combo : reference->findChildren<QComboBox*>())
        if (combo->accessibleName() == "Plugin filename") foundInstalled = combo->findText("Installed.esp") >= 0;
    require(foundInstalled, "Plugin dropdown must contain discovered installed plugins");
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
    idMode(reference)->setCurrentIndex(1);
    require(form.value().value("ReferenceID").toString() == "1234", "Decimal to hex conversion must preserve the ID");
    idMode(reference)->setCurrentIndex(2);
    require(form.value().value("ReferenceID").toInteger() == 4660, "Hex to decimal conversion must preserve the ID");
    idText(reference)->setText("1.5");
    require(!form.errors().isEmpty(), "Invalid decimal ID input must be reported");
    idText(reference)->setText("4294967295");
    require(form.errors().isEmpty(), "Maximum uint32 ID must be accepted");

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
        require(idMode(group)->currentIndex() == 0, "Each condition must offer the plugin/local Form ID controls");
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

        idMode(group)->setCurrentIndex(1);
        idText(group)->setText("1234");
        idMode(group)->setCurrentIndex(2);
        require(form.value().value(name).toInteger() == 4660, "Every condition must support decimal runtime IDs");
        idText(group)->setText("4294967296");
        require(form.errors().join('\n').contains(name), "Out-of-range condition input must identify the field");
        idText(group)->setText("4660");
        require(form.errors().isEmpty(), "Correcting condition input must clear its error");

        button(group, "Use list")->click();
        processDeletes();
        require(form.value().value(name).toArray() == QJsonArray{4660}, "Condition scalar-to-list conversion must preserve its type and ID");
        button(group, "Use single value")->click();
        processDeletes();
        require(form.value().value(name).toInteger() == 4660, "Condition list-to-scalar conversion must preserve its ID");
        button(group, "Use list")->click();
        processDeletes();
        button(group, "Add Form ID")->click();
        processDeletes();
        int textIndex = 0;
        for (auto* text : group->findChildren<QLineEdit*>()) {
            if (text->accessibleName() == "Form ID" && textIndex++ == 1) text->setText("ABC");
        }
        require(form.value().value(name).toArray() == QJsonArray{4660, "Skyrim.esm|ABC"},
                "Every condition must support editing mixed ID lists");
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

void testUnknownFormIDProperty() {
    JsonForm form({"Skyrim.esm"});
    const QJsonObject legacy{{"OriginalModel", "Original.nif"}, {"Variants", QJsonArray{"Variant.nif"}}};
    form.setValue(legacy);
    QTimer::singleShot(0, [] {
        auto* dialog = qobject_cast<QInputDialog*>(QApplication::activeModalWidget());
        require(dialog != nullptr, "Adding an extra property must open its name dialog");
        dialog->setTextValue("CustomFormID");
        dialog->accept();
    });
    button(&form, "Add extra property")->click();
    processDeletes();
    require(form.value().value("CustomFormID").toString() == "Skyrim.esm|",
            "Unknown FormID properties must retain their existing plugin/local ID default");
    auto* text = field(form, "CustomFormID")->findChild<QPlainTextEdit*>();
    require(text != nullptr, "Unknown properties must retain generic editing controls");
    text->setPlainText("Unknown.esp|ABC");
    auto expected = legacy;
    expected.insert("CustomFormID", "Unknown.esp|ABC");
    require(form.value() == expected, "Editing a custom FormID property must preserve it and the legacy entry");
    form.setValue(expected);
    processDeletes();
    require(form.value() == expected, "Custom FormID properties must round trip unchanged");
}
}

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        testRoundTrip();
        testEdits();
        testInvalidValuesCanBeRepaired();
        testConditionFields();
        testUnknownFormIDProperty();
        std::cout << "JsonForm tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
