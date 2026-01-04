#ifndef EDITOR_VIDEO_VIDEOEDITOR_H
#define EDITOR_VIDEO_VIDEOEDITOR_H

#include <QMainWindow>
#include <QMediaPlayer>
#include <memory>

class QAudioOutput;
class QLabel;
class QToolBar;
class QVideoWidget;

namespace Editor::Video {

class TrimTimeline;
class VideoExporter;

/**
 * The post-recording window: a finished recording opens here (instead of a bare
 * save dialog) for preview, trimming, and a decision — Save (save-as), Copy (file
 * lands in the recordings folder + clipboard), or Discard. The editor owns the
 * recording's temp file: every path through save/copy/close either consumes it or
 * deletes it, so nothing accumulates in the temp directory.
 *
 * Trimmed saves export via VideoExporter (AVAssetExportSession remux on macOS);
 * untrimmed saves are a plain file move. If the preview fails (codec/corrupt
 * file), trimming is disabled but Save still works — it never decodes.
 */
class VideoEditor : public QMainWindow
{
    Q_OBJECT

public:
    explicit VideoEditor(const QString &tempPath, QWidget *parent = nullptr);
    ~VideoEditor() override;

signals:
    void recordingSaved(const QString &finalPath);   // the app shows the tray balloon
    // The app owns the upload (outlives this window) and deletes the temp when done.
    void uploadRequested(const QString &localPath, const QString &suggestedName, bool deleteWhenDone);

protected:
    void closeEvent(QCloseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private slots:
    void onSave();
    void onCopy();
    void onExportGif();
    void onUpload();
    void togglePlayPause();
    void onDurationChanged(qint64 durationMs);
    void onPositionChanged(qint64 positionMs);
    void onMediaStatusChanged(QMediaPlayer::MediaStatus status);
    void onExporterFinished(const QString &exportedPath);
    void onExporterFailed(const QString &error);
    void onExportProgress(int done, int total);

private:
    enum class Pending { None, SaveMove, Copy, ExportGif, Upload };

    void setupUi();
    void previewFailed();
    void setBusy(bool busy);
    void updateTimeLabel();
    void updatePlayPauseIcon();
    [[nodiscard]] QIcon themedIcon(const QString &svgPath) const;
    [[nodiscard]] QString suggestedFileName() const;
    [[nodiscard]] QString suggestedGifFileName() const;
    [[nodiscard]] QString recordingsDir() const;     // ensured to exist
    bool moveFileTo(const QString &source, const QString &dest);
    void finishSaved(const QString &finalPath);      // mark saved, notify, close
    VideoExporter *exporter();                       // lazily created + wired
    static void putOnClipboard(const QString &path);

    QString m_tempPath;            // the recording, owned by this editor
    QString m_exportTempPath;      // trim output while a SaveMove export runs
    QString m_pendingDest;         // chosen destination awaiting export completion
    Pending m_pending = Pending::None;
    bool m_previewOk = true;
    bool m_saved = false;          // suppresses the discard prompt on close

    QMediaPlayer *m_player = nullptr;
    QAudioOutput *m_audio = nullptr;
    QVideoWidget *m_videoWidget = nullptr;
    TrimTimeline *m_timeline = nullptr;
    QLabel *m_timeLabel = nullptr;
    QLabel *m_statusLabel = nullptr;
    QToolBar *m_toolbar = nullptr;
    QAction *m_playPauseAction = nullptr;
    QAction *m_saveAction = nullptr;
    QAction *m_copyAction = nullptr;
    QAction *m_gifAction = nullptr;
    QAction *m_uploadAction = nullptr;
    QAction *m_discardAction = nullptr;
    std::unique_ptr<VideoExporter> m_exporter;
};

} // namespace Editor::Video

#endif // EDITOR_VIDEO_VIDEOEDITOR_H
