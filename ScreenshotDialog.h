#ifndef SCREENSHOTDIALOG_H
#define SCREENSHOTDIALOG_H

#include <QDialog>
#include <QPixmap>
#include <QLabel>
#include <QPushButton>

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

#endif // SCREENSHOTDIALOG_H
