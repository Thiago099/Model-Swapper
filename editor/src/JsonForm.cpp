#include "JsonForm.h"

#include <QCheckBox>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QScrollArea>
#include <QTabWidget>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>
#include <utility>
#include <vector>

namespace {
enum class Hint { Generic, Model, ID, Time };

const QStringList knownFields = {
    "OriginalModel", "Variants", "BaseFormID", "ReferenceID",
    "ExcludeBaseFormID", "ExcludeReferenceID",
    "ActiveTimeSpan",
    "LocationID", "RegionID", "KeywordID", "CellID", "WorldspaceID",
    "ExcludeLocationID", "ExcludeRegionID", "ExcludeKeywordID", "ExcludeCellID", "ExcludeWorldspaceID"
};

Hint hintFor(const QString& key) {
    if (key == "OriginalModel" || key == "Variants") return Hint::Model;
    if (key.endsWith("FormID") || (knownFields.contains(key) && key.endsWith("ID"))) return Hint::ID;
    if (key == "ActiveTimeSpan") return Hint::Time;
    return Hint::Generic;
}

int tabFor(const QString& key) {
    if (key == "Variants") return 0;
    if (key == "OriginalModel" || key == "BaseFormID" || key == "ReferenceID") return 1;
    if (key.startsWith("Exclude")) return 3;
    return 2;
}

QJsonValue defaultFor(const QString& key) {
    if (key == "Variants") return QJsonArray{QString{}};
    if (hintFor(key) == Hint::ID) return QString("Skyrim.esm|");
    if (hintFor(key) == Hint::Time) return QString("01-01~31-12");
    return QString{};
}

QString typeName(const QJsonValue& value) {
    switch (value.type()) {
    case QJsonValue::Object: return "Object";
    case QJsonValue::Array: return "List";
    case QJsonValue::Bool: return "Boolean";
    case QJsonValue::Double: return "Number";
    case QJsonValue::Null: return "Null";
    default: return "Text";
    }
}

QJsonValue typeDefault(const QString& type) {
    if (type == "Object") return QJsonObject{};
    if (type == "List") return QJsonArray{};
    if (type == "Boolean") return false;
    if (type == "Number") return 0;
    if (type == "Null") return QJsonValue::Null;
    return QString{};
}

QToolButton* removeButton(QWidget* parent) {
    auto* button = new QToolButton(parent);
    button->setText("Remove");
    button->setToolTip("Remove this value");
    return button;
}

void clearLayout(QLayout* layout) {
    while (auto* item = layout->takeAt(0)) {
        if (item->widget()) {
            item->widget()->hide();
            item->widget()->deleteLater();
        }
        if (item->layout()) clearLayout(item->layout());
        delete item;
    }
}

bool parseNumber(const QString& text, QJsonValue& result) {
    const auto document = QJsonDocument::fromJson(("[" + text.trimmed() + "]").toUtf8());
    if (!document.isArray() || document.array().size() != 1 || !document.array().first().isDouble()) return false;
    result = document.array().first();
    return true;
}
}

// Recursive controls retain JSON representations while editing or repairing
// supported fields. Unknown entry properties are preserved without controls.
class ValueForm : public QWidget {
public:
    ValueForm(QJsonValue initial, QStringList plugins, Hint hint, bool entry,
              std::function<void()> changed, QWidget* parent = nullptr)
        : QWidget(parent), initial_(std::move(initial)), plugins_(std::move(plugins)),
          hint_(hint), entry_(entry), changed_(std::move(changed)), layout_(new QVBoxLayout(this)) {
        layout_->setContentsMargins(0, 0, 0, 0);
        if (!entry_) layout_->setAlignment(Qt::AlignTop);
        setSizePolicy(QSizePolicy::Expanding, entry_ ? QSizePolicy::Expanding : QSizePolicy::Preferred);
        rebuild(initial_);
    }

    QJsonValue value() const {
        if (initial_.isObject()) {
            // Unrecognized entry properties stay in the document without
            // exposing controls for settings the plugin does not support.
            QJsonObject object = entry_ ? initial_.toObject() : QJsonObject{};
            for (const auto& child : children_) {
                if (!child.enabled || child.enabled->isChecked()) object.insert(child.key, child.form->value());
                else object.remove(child.key);
            }
            return object;
        }
        if (initial_.isArray()) {
            QJsonArray array;
            for (const auto& child : children_) array.append(child.form->value());
            return array;
        }
        if (plugin_) {
            const auto plugin = plugin_->currentText();
            const auto localID = text_->text();
            if (isLegacyID() && plugin.isEmpty() && localID.isEmpty()) return initial_;
            return plugin + "|" + localID;
        }
        if (text_) {
            if (initial_.isDouble()) {
                QJsonValue number;
                return parseNumber(text_->text(), number) ? number : initial_;
            }
            return text_->text();
        }
        if (multiline_) return multiline_->toPlainText();
        if (check_) return check_->isChecked();
        return initial_;
    }

    QStringList errors(const QString& path) const {
        QStringList result;
        if (plugin_ && isLegacyID()) {
            const auto plugin = plugin_->currentText();
            const auto localID = text_->text();
            if (!(plugin.isEmpty() && localID.isEmpty()) &&
                (plugin.trimmed().isEmpty() || localID.trimmed().isEmpty())) {
                result << path + ": choose a plugin and enter its local hexadecimal Form ID, or clear both fields to keep the existing ID.";
            }
        } else if (text_ && initial_.isDouble() && !plugin_) {
            QJsonValue number;
            if (!parseNumber(text_->text(), number)) result << path + ": enter a valid JSON number.";
        }
        int index = 0;
        for (const auto& child : children_) {
            if (!child.enabled || child.enabled->isChecked())
                result.append(child.form->errors(initial_.isArray() ? path + "[" + QString::number(index) + "]"
                                                                  : path + "." + child.key));
            ++index;
        }
        return result;
    }

private:
    struct Child {
        QString key;
        ValueForm* form;
        QCheckBox* enabled = nullptr;
    };

    void notify() { if (changed_) changed_(); }

    bool isLegacyID() const {
        return initial_.isDouble() || (initial_.isString() && !initial_.toString().contains('|'));
    }

    bool canRestructure() const {
        const auto invalid = errors("Value");
        if (invalid.isEmpty()) return true;
        QMessageBox::warning(const_cast<ValueForm*>(this), "Correct these values", invalid.join('\n'));
        return false;
    }

    void replace(QJsonValue replacement) {
        rebuild(std::move(replacement));
        notify();
    }

    void rebuild(QJsonValue initial) {
        if (tabs_) selectedTab_ = tabs_->currentIndex();
        clearLayout(layout_);
        initial_ = std::move(initial);
        children_.clear();
        text_ = nullptr;
        multiline_ = nullptr;
        check_ = nullptr;
        plugin_ = nullptr;
        tabs_ = nullptr;
        if (initial_.isObject()) buildObject();
        else if (initial_.isArray()) buildArray();
        else if (hint_ == Hint::ID && (initial_.isString() || initial_.isDouble())) buildID();
        else if (initial_.isBool()) {
            check_ = new QCheckBox("Enabled", this);
            check_->setChecked(initial_.toBool());
            layout_->addWidget(check_);
            connect(check_, &QCheckBox::toggled, this, [this] { notify(); });
        } else if (initial_.isString() && hint_ == Hint::Generic) {
            multiline_ = new QPlainTextEdit(initial_.toString(), this);
            multiline_->setMaximumHeight(75);
            layout_->addWidget(multiline_);
            connect(multiline_, &QPlainTextEdit::textChanged, this, [this] { notify(); });
        } else if (initial_.isString() || initial_.isDouble()) {
            text_ = new QLineEdit(this);
            if (initial_.isDouble()) {
                const auto serialized = QJsonDocument(QJsonArray{initial_}).toJson(QJsonDocument::Compact);
                text_->setText(QString::fromUtf8(serialized.mid(1, serialized.size() - 2)));
            } else text_->setText(initial_.toString());
            if (hint_ == Hint::Model) text_->setPlaceholderText("Folder\\Model.nif");
            if (hint_ == Hint::Time) {
                text_->setPlaceholderText("dd-MM~dd-MM");
                text_->setToolTip("First day-month ~ last day-month, for example 01-06~31-08");
            }
            layout_->addWidget(text_);
            connect(text_, &QLineEdit::textChanged, this, [this] { notify(); });
        } else layout_->addWidget(new QLabel("null", this));
    }

    void addTypeSelector(ValueForm* form, QHBoxLayout* row) {
        auto* type = new QComboBox(this);
        type->addItems({"Text", "Number", "Boolean", "Null", "List", "Object"});
        type->setCurrentText(typeName(form->value()));
        type->setToolTip("JSON value type");
        row->addWidget(type);
        connect(type, &QComboBox::textActivated, form, [form](const QString& next) {
            if (next != typeName(form->value())) form->replace(typeDefault(next));
        });
    }

    void buildObject() {
        const auto object = initial_.toObject();
        std::vector<QVBoxLayout*> tabLayouts;
        if (entry_) {
            tabs_ = new QTabWidget(this);
            tabs_->setObjectName("entryTabs");
            tabs_->setDocumentMode(true);
            tabs_->setUsesScrollButtons(true);
            const QStringList tabNames{"Models", "Targets", "Conditions", "Exclusions"};
            for (const auto& name : tabNames) {
                auto* page = new QWidget(tabs_);
                auto* pageLayout = new QVBoxLayout(page);
                pageLayout->setContentsMargins(0, 0, 0, 0);
                auto* scroll = new QScrollArea(page);
                scroll->setObjectName("entryTabScroll" + name);
                scroll->setWidgetResizable(true);
                scroll->setFrameShape(QFrame::NoFrame);
                auto* content = new QWidget(scroll);
                auto* contentLayout = new QVBoxLayout(content);
                contentLayout->setContentsMargins(10, 12, 10, 12);
                contentLayout->setSpacing(10);
                contentLayout->setAlignment(Qt::AlignTop);
                scroll->setWidget(content);
                pageLayout->addWidget(scroll);
                tabs_->addTab(page, name);
                tabLayouts.push_back(contentLayout);
            }
            tabs_->setCurrentIndex(selectedTab_);
            connect(tabs_, &QTabWidget::currentChanged, this, [this](int index) { selectedTab_ = index; });
            layout_->addWidget(tabs_);
        }
        if (!entry_) {
            auto* add = new QPushButton("Add property", this);
            add->setAccessibleName(add->text());
            add->setToolTip("Add a property to this object");
            layout_->addWidget(add, 0, Qt::AlignLeft);
            connect(add, &QPushButton::clicked, this, [this] {
                if (!canRestructure()) return;
                bool ok = false;
                const auto key = QInputDialog::getText(this, "Add JSON property", "Property name:", QLineEdit::Normal, {}, &ok);
                if (!ok || key.isEmpty()) return;
                auto object = value().toObject();
                if (object.contains(key)) {
                    QMessageBox::warning(this, "Existing property", "That property already exists.");
                    return;
                }
                object.insert(key, defaultFor(key));
                replace(object);
            });
        }
        QStringList keys = entry_ ? knownFields : QStringList{};
        if (!entry_) {
            for (auto i = object.begin(); i != object.end(); ++i) keys << i.key();
        }
        for (const auto& key : keys) {
            const bool known = entry_ && knownFields.contains(key);
            const bool present = object.contains(key);
            auto* group = new QGroupBox(this);
            auto* groupLayout = new QVBoxLayout(group);
            groupLayout->setContentsMargins(10, 8, 10, 10);
            groupLayout->setSpacing(6);
            auto* title = new QHBoxLayout;
            auto* enabled = new QCheckBox(key, group);
            enabled->setChecked(present);
            enabled->setToolTip("Include this property in the saved JSON");
            const auto contextKey = key.startsWith("Exclude") ? key.mid(7) : key;
            if (QStringList{"LocationID", "RegionID", "KeywordID", "CellID", "WorldspaceID"}.contains(contextKey)) {
                enabled->setToolTip(key.startsWith("Exclude")
                    ? "Reject this entry if any listed condition matches. An empty list excludes nothing."
                    : "Match any included location, region, keyword, cell, or worldspace. Base and reference filters still apply. Empty inclusion lists alone match nothing.");
            }
            title->addWidget(enabled);
            title->addStretch();
            groupLayout->addLayout(title);
            auto* form = new ValueForm(present ? object.value(key) : defaultFor(key), plugins_,
                                       known ? hintFor(key) : Hint::Generic, false, [this] { notify(); }, group);
            form->setEnabled(present);
            form->setVisible(present);
            groupLayout->addWidget(form);
            children_.push_back({key, form, enabled});
            connect(enabled, &QCheckBox::toggled, this, [this, form](bool checked) {
                form->setEnabled(checked);
                form->setVisible(checked);
                notify();
            });
            if (known && (key == "OriginalModel" || hintFor(key) == Hint::ID ||
                          (key == "Variants" && !form->value().isArray()))) {
                auto* list = new QPushButton(present && object.value(key).isArray() ? "Use single value" : "Use list", group);
                list->setVisible(present);
                title->addWidget(list);
                connect(enabled, &QCheckBox::toggled, list, &QPushButton::setVisible);
                connect(list, &QPushButton::clicked, this, [this, form, list] {
                    if (!form->canRestructure()) return;
                    const auto current = form->value();
                    if (current.isArray()) {
                        if (current.toArray().size() > 1) {
                            QMessageBox::information(this, "Multiple values", "Remove all but one value before switching to a single value.");
                            return;
                        }
                        form->replace(current.toArray().isEmpty() ? QJsonValue(QString{}) : current.toArray().first());
                        list->setText("Use list");
                    } else {
                        form->replace(QJsonArray{current});
                        list->setText("Use single value");
                    }
                });
            }
            if (known && ((hintFor(key) == Hint::ID && !form->value().isString() &&
                           !form->value().isDouble() && !form->value().isArray()) ||
                          ((key == "OriginalModel" || key == "Variants") &&
                           !form->value().isString() && !form->value().isArray()) ||
                          (key == "ActiveTimeSpan" && !form->value().isString()))) {
                auto* reset = new QPushButton("Reset field", group);
                reset->setVisible(present);
                title->addWidget(reset);
                connect(enabled, &QCheckBox::toggled, reset, &QPushButton::setVisible);
                connect(reset, &QPushButton::clicked, form, [form, key] { form->replace(defaultFor(key)); });
            }
            if (!known) {
                addTypeSelector(form, title);
                auto* remove = removeButton(group);
                title->addWidget(remove);
                connect(remove, &QToolButton::clicked, this, [this, key] {
                    if (!canRestructure()) return;
                    auto object = value().toObject();
                    object.remove(key);
                    replace(object);
                });
            }
            (entry_ ? tabLayouts.at(tabFor(key)) : layout_)->addWidget(group);
        }
    }

    void buildArray() {
        const auto array = initial_.toArray();
        auto* add = new QPushButton(hint_ == Hint::ID ? "Add Form ID" : hint_ == Hint::Model ? "Add model" : "Add value", this);
        add->setObjectName("addListItem");
        add->setAccessibleName(add->text());
        add->setToolTip(hint_ == Hint::ID ? "Append a Form ID to this list" : hint_ == Hint::Model ? "Append a model path to this list" : "Append a value to this list");
        layout_->addWidget(add, 0, Qt::AlignLeft);
        connect(add, &QPushButton::clicked, this, [this] {
            if (!canRestructure()) return;
            auto array = value().toArray();
            array.append(hint_ == Hint::ID ? QJsonValue((plugins_.isEmpty() ? QString("Skyrim.esm") : plugins_.first()) + "|")
                                         : QJsonValue(QString{}));
            replace(array);
        });
        for (qsizetype index = 0; index < array.size(); ++index) {
            auto* item = new QWidget(this);
            item->setObjectName("listItem");
            auto* itemLayout = new QVBoxLayout(item);
            itemLayout->setContentsMargins(0, 0, 0, 0);
            itemLayout->setSpacing(4);
            auto* row = new QHBoxLayout;
            auto* number = new QLabel(QString::number(index + 1) + ".", item);
            number->setObjectName("listItemNumber");
            number->setMinimumWidth(20);
            row->addWidget(number);
            auto* form = new ValueForm(array.at(index), plugins_, hint_, false, [this] { notify(); }, item);
            children_.push_back({{}, form});
            if (hint_ == Hint::Generic) addTypeSelector(form, row);
            const bool compact = !array.at(index).isArray() && !array.at(index).isObject();
            if (compact) row->addWidget(form, 1);
            else row->addStretch();
            auto* remove = removeButton(item);
            remove->setToolTip("Remove item " + QString::number(index + 1));
            remove->setAccessibleName("Remove item " + QString::number(index + 1));
            row->addWidget(remove);
            connect(remove, &QToolButton::clicked, this, [this, index] {
                if (!canRestructure()) return;
                auto array = value().toArray();
                array.removeAt(index);
                replace(array);
            });
            itemLayout->addLayout(row);
            if (!compact) itemLayout->addWidget(form);
            layout_->addWidget(item);
        }
    }

    void buildID() {
        auto* row = new QHBoxLayout;
        plugin_ = new QComboBox(this);
        plugin_->setEditable(true);
        plugin_->lineEdit()->setPlaceholderText("Select or enter plugin");
        plugin_->setInsertPolicy(QComboBox::NoInsert);
        plugin_->addItems(plugins_);
        plugin_->setCurrentIndex(-1);
        plugin_->setEditText(QString{});
        plugin_->setAccessibleName("Plugin filename");
        plugin_->setToolTip("Installed .esm, .esp and .esl files; you can also type a filename");
        text_ = new QLineEdit(this);
        text_->setAccessibleName("Form ID");
        text_->setMinimumWidth(120);
        text_->setPlaceholderText("001234");
        if (!isLegacyID()) {
            const auto text = initial_.toString();
            const auto separator = text.indexOf('|');
            plugin_->setCurrentText(text.left(separator));
            text_->setText(text.mid(separator + 1));
        }
        auto* label = new QLabel("Local Form ID (hex)", this);
        row->addWidget(plugin_, 1);
        row->addWidget(label);
        row->addWidget(text_, 1);
        layout_->addLayout(row);
        if (isLegacyID()) {
            const auto json = QJsonDocument(QJsonArray{initial_}).toJson(QJsonDocument::Compact);
            const auto original = QString::fromUtf8(json.mid(1, json.size() - 2));
            auto* note = new QLabel("Existing ID: " + original + ". Fill both fields to replace it; leave both empty to keep it.", this);
            note->setObjectName("legacyFormIDNote");
            note->setTextFormat(Qt::PlainText);
            note->setTextInteractionFlags(Qt::TextSelectableByMouse);
            note->setWordWrap(true);
            layout_->addWidget(note);
        }
        connect(plugin_, &QComboBox::currentTextChanged, this, [this] { notify(); });
        connect(text_, &QLineEdit::textChanged, this, [this] { notify(); });
    }

    QJsonValue initial_;
    QStringList plugins_;
    Hint hint_;
    bool entry_;
    std::function<void()> changed_;
    QVBoxLayout* layout_;
    std::vector<Child> children_;
    QLineEdit* text_ = nullptr;
    QPlainTextEdit* multiline_ = nullptr;
    QCheckBox* check_ = nullptr;
    QComboBox* plugin_ = nullptr;
    QTabWidget* tabs_ = nullptr;
    int selectedTab_ = 0;
};

JsonForm::JsonForm(const QStringList& plugins, QWidget* parent)
    : QWidget(parent), plugins_(plugins), layout_(new QVBoxLayout(this)) {
    layout_->setContentsMargins(0, 0, 0, 0);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
}

void JsonForm::setValue(const QJsonObject& entry) {
    clearLayout(layout_);
    fields_ = new ValueForm(entry, plugins_, Hint::Generic, true, [this] { emit changed(); }, this);
    layout_->addWidget(fields_);
}

QJsonObject JsonForm::value() const { return fields_ ? fields_->value().toObject() : QJsonObject{}; }

QStringList JsonForm::errors() const { return fields_ ? fields_->errors("Entry") : QStringList{}; }
