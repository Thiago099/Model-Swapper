#pragma once

#include <QJsonObject>
#include <QStringList>
#include <QWidget>

class QVBoxLayout;
class ValueForm;

// An entry form retains the JSON types, optional fields, and unknown properties.
class JsonForm : public QWidget {
    Q_OBJECT

public:
    explicit JsonForm(const QStringList& plugins, QWidget* parent = nullptr);
    void setValue(const QJsonObject& entry);
    QJsonObject value() const;
    QStringList errors() const;

signals:
    void changed();

private:
    QStringList plugins_;
    QVBoxLayout* layout_;
    ValueForm* fields_ = nullptr;
};
