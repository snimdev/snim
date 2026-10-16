#ifndef APP_PROVIDERFORM_H
#define APP_PROVIDERFORM_H

#include "upload/ProviderRegistry.h"
#include "upload/UploadProfiles.h"

#include <QVector>
#include <QWidget>

class QCheckBox;
class QComboBox;
class QFormLayout;
class QLabel;
class QLineEdit;
class QSpinBox;

namespace App {

/**
 * One provider's page of the Upload settings, built from its Upload::ProviderSpec: a row
 * per field, plus the write-only secret edit (never filled, only typed into; it goes to
 * the keychain on Apply). Rows with a visibleWhen and the secret's label and hint follow
 * the values on screen as they change.
 */
class ProviderForm : public QWidget
{
    Q_OBJECT

public:
    explicit ProviderForm(const Upload::ProviderSpec &spec, QWidget *parent = nullptr);

    [[nodiscard]] const Upload::ProviderSpec &spec() const { return m_spec; }

    // Show p's values; hasSecret marks the secret as already stored (or staged).
    void bind(const Upload::UploadProfile &p, bool hasSecret);
    // Blank every row (a Choice back to its default) and the secret.
    void clear();
    // Write what is on screen into p.values, text trimmed. The secret is not included.
    void flush(Upload::UploadProfile &p) const;

    [[nodiscard]] QString secret() const;
    void clearSecret();

private:
    struct Row {
        const Upload::FieldSpec *field = nullptr;
        QWidget *rowWidget = nullptr;      // what setRowVisible hides
        QLineEdit *edit = nullptr;         // Text, FilePath
        QSpinBox *spin = nullptr;          // Port
        QCheckBox *check = nullptr;        // Flag
        QComboBox *combo = nullptr;        // Choice
    };

    void setRowValue(const Row &row, const QVariant &value);
    // Re-evaluates visibleWhen and the secret's label and hint against the screen.
    void refresh();

    const Upload::ProviderSpec &m_spec;
    QFormLayout *m_form = nullptr;
    QVector<Row> m_rows;
    QLabel *m_secretLabel = nullptr;
    QLineEdit *m_secretEdit = nullptr;
    bool m_secretStored = false;
};

} // namespace App

#endif // APP_PROVIDERFORM_H
