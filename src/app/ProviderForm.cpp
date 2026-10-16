#include "app/ProviderForm.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSignalBlocker>
#include <QSpinBox>

namespace App {

using Upload::FieldKind;
using Upload::FieldSpec;

ProviderForm::ProviderForm(const Upload::ProviderSpec &spec, QWidget *parent)
    : QWidget(parent), m_spec(spec)
{
    m_form = new QFormLayout(this);
    m_form->setContentsMargins(0, 0, 0, 0);

    for (const FieldSpec &field : m_spec.fields) {
        if (field.kind == FieldKind::Secret) {
            m_secretLabel = new QLabel(this);
            m_secretEdit = new QLineEdit(this);
            m_secretEdit->setEchoMode(QLineEdit::Password);
            m_form->addRow(m_secretLabel, m_secretEdit);
            continue;
        }

        Row row;
        row.field = &field;
        switch (field.kind) {
        case FieldKind::Port:
            row.spin = new QSpinBox(this);
            row.spin->setRange(0, 65535);
            row.spin->setSpecialValueText(field.placeholder);   // shown at 0
            connect(row.spin, &QSpinBox::valueChanged, this, &ProviderForm::refresh);
            row.rowWidget = row.spin;
            m_form->addRow(field.label, row.spin);
            break;
        case FieldKind::Flag:
            row.check = new QCheckBox(field.label, this);
            connect(row.check, &QCheckBox::toggled, this, &ProviderForm::refresh);
            row.rowWidget = row.check;
            m_form->addRow(QString(), row.check);
            break;
        case FieldKind::Choice:
            row.combo = new QComboBox(this);
            for (const auto &[value, label] : field.choices)
                row.combo->addItem(label, value);
            connect(row.combo, &QComboBox::currentIndexChanged, this, &ProviderForm::refresh);
            row.rowWidget = row.combo;
            m_form->addRow(field.label, row.combo);
            break;
        case FieldKind::FilePath: {
            // Path and Browse share one container, so hiding the row hides both.
            auto *box = new QWidget(this);
            auto *layout = new QHBoxLayout(box);
            layout->setContentsMargins(0, 0, 0, 0);
            row.edit = new QLineEdit(box);
            row.edit->setPlaceholderText(field.placeholder);
            auto *browse = new QPushButton(tr("Browse…"), box);
            browse->setMaximumWidth(90);
            layout->addWidget(row.edit);
            layout->addWidget(browse);
            QLineEdit *edit = row.edit;
            connect(browse, &QPushButton::clicked, this, [this, edit, &field] {
                const QString current = edit->text().trimmed();
                const QString start = !current.isEmpty() ? current
                                      : field.placeholder.isEmpty()
                                          ? QDir::homePath()
                                          : QFileInfo(field.placeholder).absolutePath();
                const QString path = QFileDialog::getOpenFileName(this, field.browseTitle, start);
                if (!path.isEmpty())
                    edit->setText(path);
            });
            connect(row.edit, &QLineEdit::textChanged, this, &ProviderForm::refresh);
            row.rowWidget = box;
            m_form->addRow(field.label, box);
            break;
        }
        case FieldKind::Text:
        case FieldKind::Secret:
            row.edit = new QLineEdit(this);
            row.edit->setPlaceholderText(field.placeholder);
            connect(row.edit, &QLineEdit::textChanged, this, &ProviderForm::refresh);
            row.rowWidget = row.edit;
            m_form->addRow(field.label, row.edit);
            break;
        }
        m_rows.push_back(row);
    }
    refresh();
}

void ProviderForm::setRowValue(const Row &row, const QVariant &value)
{
    if (row.edit)
        row.edit->setText(value.toString());
    else if (row.spin)
        row.spin->setValue(value.toInt());
    else if (row.check)
        row.check->setChecked(value.toBool());
    else if (const int index = row.combo->findData(value.toString()); index != -1)
        row.combo->setCurrentIndex(index);
}

void ProviderForm::bind(const Upload::UploadProfile &p, bool hasSecret)
{
    m_secretStored = hasSecret;
    if (m_secretEdit)
        m_secretEdit->clear();
    for (const Row &row : m_rows)
        setRowValue(row, p.value(row.field->key));
    refresh();
}

void ProviderForm::clear()
{
    m_secretStored = false;
    if (m_secretEdit)
        m_secretEdit->clear();
    for (const Row &row : m_rows)
        setRowValue(row, row.combo ? row.field->defaultValue : QVariant());
    refresh();
}

void ProviderForm::flush(Upload::UploadProfile &p) const
{
    for (const Row &row : m_rows) {
        QVariant value;
        if (row.edit)
            value = row.edit->text().trimmed();
        else if (row.spin)
            value = row.spin->value();          // 0 = the protocol default
        else if (row.check)
            value = row.check->isChecked();
        else
            value = row.combo->currentData().toString();
        p.values.insert(row.field->key, value);
    }
}

QString ProviderForm::secret() const
{
    return m_secretEdit ? m_secretEdit->text() : QString();
}

void ProviderForm::clearSecret()
{
    if (m_secretEdit)
        m_secretEdit->clear();
}

void ProviderForm::refresh()
{
    Upload::UploadProfile shown;
    shown.type = m_spec.id;
    flush(shown);
    for (const Row &row : m_rows)
        if (row.field->visibleWhen)
            m_form->setRowVisible(row.rowWidget, row.field->visibleWhen(shown));
    if (!m_secretEdit)
        return;
    m_secretLabel->setText(m_spec.secretLabel ? m_spec.secretLabel(shown) : QString());
    // The hint follows the values, but never replaces the "stored" marker.
    const QString hint = m_spec.secretHint ? m_spec.secretHint(shown) : QString();
    m_secretEdit->setPlaceholderText(m_secretStored ? tr("•••••••• (stored)") : hint);
}

} // namespace App
