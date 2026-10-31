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
#include <QSignalBlocker>
#include <QToolButton>
#include <QVBoxLayout>

#include <functional>
#include <utility>
#include <vector>

namespace {
enum class Hint { Generic, Model, ID, Time };

const QStringList knownFields = {
    "OriginalModel", "Variants", "BaseFormID", "ReferenceID",
    "ExcludeBaseFormID", "ExcludeReferenceID", "ActiveTimeSpan"
};

Hint hintFor(const QString& key) {
    if (key == "OriginalModel" || key == "Variants") return Hint::Model;
    if (key.endsWith("FormID") || key == "ReferenceID" || key == "ExcludeReferenceID") return Hint::ID;
    if (key == "ActiveTimeSpan") return Hint::Time;
    return Hint::Generic;
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

// Recursive controls are also used for extension properties, so saving an entry
// does not discard settings that a later plugin version might introduce.
class ValueForm : public QWidget {
public:
    ValueForm(QJsonValue initial, QStringList plugins, Hint hint, bool entry,
              std::function<void()> changed, QWidget* parent = nullptr)
        : QWidget(parent), initial_(std::move(initial)), plugins_(std::move(plugins)),
          hint_(hint), entry_(entry), changed_(std::move(changed)), layout_(new QVBoxLayout(this)) {
        layout_->setContentsMargins(0, 0, 0, 0);
        rebuild(initial_);
    }

    QJsonValue value() const {
        if (initial_.isObject()) {
            QJsonObject object;
            for (const auto& child : children_) {
                if (!child.enabled || child.enabled->isChecked()) object.insert(child.key, child.form->value());
            }
            return object;
        }
        if (initial_.isArray()) {
            QJsonArray array;
            for (const auto& child : children_) array.append(child.form->value());
            return array;
        }
        if (idMode_) {
            const auto text = text_->text();
            if (idMode_->currentIndex() == 0) return plugin_->currentText() + "|" + text;
            if (idMode_->currentIndex() == 1) return text;
            bool ok = false;
            const auto number = text.toULongLong(&ok, 10);
            if (ok && number > 0 && number <= 0xFFFFFFFFull) return QJsonValue(static_cast<qint64>(number));
            return initial_;
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
        if (idMode_ && idMode_->currentIndex() == 2) {
            bool ok = false;
            const auto number = text_->text().toULongLong(&ok, 10);
            if (!ok || number == 0 || number > 0xFFFFFFFFull)
                result << path + ": enter a decimal integer between 1 and 4294967295.";
        } else if (text_ && initial_.isDouble() && !idMode_) {
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
        clearLayout(layout_);
        initial_ = std::move(initial);
        children_.clear();
        text_ = nullptr;
        multiline_ = nullptr;
        check_ = nullptr;
        idMode_ = nullptr;
        plugin_ = nullptr;
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
        QStringList keys = entry_ ? knownFields : QStringList{};
        for (auto i = object.begin(); i != object.end(); ++i) if (!keys.contains(i.key())) keys << i.key();
        for (const auto& key : keys) {
            const bool known = entry_ && knownFields.contains(key);
            const bool present = object.contains(key);
            auto* group = new QGroupBox(this);
            auto* groupLayout = new QVBoxLayout(group);
            auto* title = new QHBoxLayout;
            auto* enabled = new QCheckBox(key, group);
            enabled->setChecked(present);
            enabled->setToolTip("Include this property in the saved JSON");
            title->addWidget(enabled);
            title->addStretch();
            groupLayout->addLayout(title);
            auto* form = new ValueForm(present ? object.value(key) : defaultFor(key), plugins_,
                                       known ? hintFor(key) : Hint::Generic, false, [this] { notify(); }, group);
            form->setEnabled(present);
            groupLayout->addWidget(form);
            children_.push_back({key, form, enabled});
            connect(enabled, &QCheckBox::toggled, this, [this, form](bool checked) {
                form->setEnabled(checked);
                notify();
            });
            if (known && (key == "OriginalModel" || hintFor(key) == Hint::ID ||
                          (key == "Variants" && !form->value().isArray()))) {
                auto* list = new QPushButton(present && object.value(key).isArray() ? "Use single value" : "Use list", group);
                title->addWidget(list);
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
                title->addWidget(reset);
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
            layout_->addWidget(group);
        }
        auto* add = new QPushButton(entry_ ? "Add extra property" : "Add property", this);
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

    void buildArray() {
        const auto array = initial_.toArray();
        for (qsizetype index = 0; index < array.size(); ++index) {
            auto* group = new QGroupBox("Value " + QString::number(index + 1), this);
            auto* groupLayout = new QVBoxLayout(group);
            auto* row = new QHBoxLayout;
            auto* form = new ValueForm(array.at(index), plugins_, hint_, false, [this] { notify(); }, group);
            children_.push_back({{}, form});
            if (hint_ == Hint::Generic) addTypeSelector(form, row);
            row->addStretch();
            auto* remove = removeButton(group);
            row->addWidget(remove);
            connect(remove, &QToolButton::clicked, this, [this, index] {
                if (!canRestructure()) return;
                auto array = value().toArray();
                array.removeAt(index);
                replace(array);
            });
            groupLayout->addLayout(row);
            groupLayout->addWidget(form);
            layout_->addWidget(group);
        }
        auto* add = new QPushButton(hint_ == Hint::ID ? "Add Form ID" : hint_ == Hint::Model ? "Add model" : "Add value", this);
        layout_->addWidget(add, 0, Qt::AlignLeft);
        connect(add, &QPushButton::clicked, this, [this] {
            if (!canRestructure()) return;
            auto array = value().toArray();
            array.append(hint_ == Hint::ID ? QJsonValue((plugins_.isEmpty() ? QString("Skyrim.esm") : plugins_.first()) + "|")
                                         : QJsonValue(QString{}));
            replace(array);
        });
    }

    void buildID() {
        auto* row = new QHBoxLayout;
        idMode_ = new QComboBox(this);
        idMode_->addItems({"Plugin + local ID", "Runtime ID (hex)", "Runtime ID (decimal)"});
        idMode_->setAccessibleName("Form ID format");
        plugin_ = new QComboBox(this);
        plugin_->setEditable(true);
        plugin_->setInsertPolicy(QComboBox::NoInsert);
        plugin_->addItems(plugins_);
        plugin_->setAccessibleName("Plugin filename");
        plugin_->setToolTip("Installed .esm, .esp and .esl files; you can also type a filename");
        text_ = new QLineEdit(this);
        text_->setAccessibleName("Form ID");
        text_->setMinimumWidth(120);
        if (initial_.isDouble()) {
            idMode_->setCurrentIndex(2);
            const auto json = QJsonDocument(QJsonArray{initial_}).toJson(QJsonDocument::Compact);
            text_->setText(QString::fromUtf8(json.mid(1, json.size() - 2)));
        } else {
            const auto text = initial_.toString();
            const auto separator = text.indexOf('|');
            idMode_->setCurrentIndex(separator < 0 ? 1 : 0);
            if (separator >= 0) {
                plugin_->setCurrentText(text.left(separator));
                text_->setText(text.mid(separator + 1));
            } else text_->setText(text);
        }
        auto* label = new QLabel(this);
        auto updateMode = [this, label] {
            plugin_->setVisible(idMode_->currentIndex() == 0);
            label->setText(idMode_->currentIndex() == 2 ? "Form ID (decimal)" : "Form ID (hex)");
            text_->setPlaceholderText(idMode_->currentIndex() == 2 ? "4660" : "001234");
        };
        updateMode();
        row->addWidget(idMode_);
        row->addWidget(plugin_, 1);
        row->addWidget(label);
        row->addWidget(text_, 1);
        layout_->addLayout(row);
        connect(idMode_, &QComboBox::currentIndexChanged, this, [this, updateMode, previous = idMode_->currentIndex()](int current) mutable {
            // Changing between decimal and hex should preserve the ID's value.
            if ((previous == 1 && current == 2) || (previous == 2 && current == 1)) {
                bool ok = false;
                QString text = text_->text();
                if (previous == 1 && text.startsWith("0x", Qt::CaseInsensitive)) text.remove(0, 2);
                const auto id = text.toULongLong(&ok, previous == 2 ? 10 : 16);
                if (ok && id > 0 && id <= 0xFFFFFFFFull) {
                    const QSignalBlocker block(text_);
                    text_->setText(QString::number(id, current == 2 ? 10 : 16));
                }
            }
            previous = current;
            updateMode();
            notify();
        });
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
    QComboBox* idMode_ = nullptr;
    QComboBox* plugin_ = nullptr;
};

JsonForm::JsonForm(const QStringList& plugins, QWidget* parent)
    : QWidget(parent), plugins_(plugins), layout_(new QVBoxLayout(this)) {
    layout_->setContentsMargins(12, 12, 12, 12);
    layout_->setAlignment(Qt::AlignTop);
}

void JsonForm::setValue(const QJsonObject& entry) {
    clearLayout(layout_);
    fields_ = new ValueForm(entry, plugins_, Hint::Generic, true, [this] { emit changed(); }, this);
    layout_->addWidget(fields_);
}

QJsonObject JsonForm::value() const { return fields_ ? fields_->value().toObject() : QJsonObject{}; }

QStringList JsonForm::errors() const { return fields_ ? fields_->errors("Entry") : QStringList{}; }
