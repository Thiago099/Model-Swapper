#include "JsonForm.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QGroupBox>
#include <QJsonArray>
#include <QLineEdit>
#include <QPushButton>

#include <iostream>
#include <stdexcept>

namespace {
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
}

int main(int argc, char** argv) {
    QApplication application(argc, argv);
    try {
        testRoundTrip();
        testEdits();
        testInvalidValuesCanBeRepaired();
        std::cout << "JsonForm tests passed\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return 1;
    }
}
