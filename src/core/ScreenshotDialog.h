#ifndef CORE_SCREENSHOTDIALOG_H
#define CORE_SCREENSHOTDIALOG_H

#include <QDialog>
#include <QPixmap>
#include <QLabel>
#include <QPushButton>

namespace Core {

class ScreenshotDialog : public QDialog
{
    Q_OBJECT

public:
    explicit ScreenshotDialog(const QPixmap &screenshot, QWidget *parent = nullptr);

private slots:
    void copyToClipboard();
    void openInEditor();

private:
    QPixmap m_screenshot;
    QLabel *m_previewLabel;
    QPushButton *m_copyButton;
    QPushButton *m_editorButton;
    QPushButton *m_cancelButton;
};

} // namespace Core

#endif // CORE_SCREENSHOTDIALOG_H
