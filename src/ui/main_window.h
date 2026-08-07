#pragma once

#include "../core/config_store.h"
#include "../core/libraries.h"
#include "../core/wallpaper_service.h"

#include <QFutureWatcher>
#include <QMainWindow>

class QLabel;
class QListWidget;
class QListWidgetItem;
class QTableWidget;
class QTableWidgetItem;
class QComboBox;
class QFontComboBox;
class QSpinBox;
class QCheckBox;
class QPushButton;
class QSystemTrayIcon;
class QTimer;
class QCloseEvent;
class QResizeEvent;
class QTabWidget;
class QToolButton;
class QDialog;

namespace poetry {

class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

    void allowQuit();

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private slots:
    void addImageDirectory();
    void removeImageDirectory();
    void imageDirectoryItemChanged(QListWidgetItem *item);
    void rescanImages();
    void selectImageFile();
    void imageFileSelectionChanged();
    void useSelectedImagePreview();
    void setSelectedImageWallpaper();
    void importPoetry();
    void addPoem();
    void editSelectedPoem();
    void deleteSelectedPoem();
    void poemItemChanged(QTableWidgetItem *item);
    void useSelectedPoemPreview();
    void randomSwitchWallpaper();
    void applyCurrentPreview();
    void refreshPreview();
    void previewDebounceTimeout();
    void timerTimeout();
    void scanFinished();
    void previewFinished();
    void switchFinished();
    void applyFinished();
    void chooseTextColor();
    void chooseTitleColor();
    void chooseMetadataColor();
    void choosePanelColor();
    void typographyPresetChanged(int index);
    void displaySettingsChanged();
    void toggleSchedule(bool enabled);
    void saveScheduleSettings();
    void clearCache();
    void openCacheDirectory();
    void openLogDirectory();
    void resetDisplaySettings();
    void showWindow();
    void quitFromTray();

private:
    enum class ApplyKind { Preview, OriginalImage };

    void setupUi();
    void setupTray();
    void loadState();
    void saveState();
    void persistConfig();
    void updateAllUi();
    void updateImageUi();
    void updateImageFilesUi();
    void updateSelectedImageInfo();
    void updatePoemUi();
    void updateSettingsUi();
    void updateStatusUi();
    void updatePreview(const QString &path);
    void updatePreviewSummary();
    void updateScheduleTimer();
    void startImageScan();
    void schedulePreviewRefresh();
    void startPreviewRender();
    void requestPreviewFor(const Poem &poem, const ImageItem &image);
    void startApply(const QString &path, const Poem &poem, const ImageItem &image, ApplyKind kind);
    void setPreviewState(const SwitchResult &result, bool applied);
    void hydratePreviewState();
    void appendLog(const QString &level, const QString &message);
    QSize targetWallpaperSize() const;
    QString colorButtonStyle(const QColor &color) const;
    QVector<Poem> enabledPoems() const;
    Poem selectedOrCurrentPoem(bool *ok = nullptr) const;
    ImageItem selectedImage(bool *ok = nullptr) const;
    void setTopStatus(const QString &text, const QColor &color);
    bool editPoemDialog(Poem &poem, bool creating);

    ConfigStore m_store;
    AppConfig m_config;
    ImageLibrary m_imageLibrary;
    RandomPicker m_picker;
    QTimer *m_timer = nullptr;
    QTimer *m_previewDebounce = nullptr;
    QSystemTrayIcon *m_tray = nullptr;
    QTabWidget *m_tabs = nullptr;

    bool m_allowQuit = false;
    bool m_desktopBusy = false;
    bool m_previewBusy = false;
    bool m_previewPending = false;
    bool m_ignorePoemChanges = false;
    bool m_ignoreImageChanges = false;
    bool m_hasPreviewContent = false;
    bool m_previewApplied = false;
    ApplyKind m_applyKind = ApplyKind::Preview;
    quint64 m_previewGeneration = 0;
    quint64 m_previewStartedGeneration = 0;
    quint64 m_switchStartedGeneration = 0;

    QString m_previewPath;
    QString m_selectedImagePath;
    Poem m_previewPoem;
    ImageItem m_previewImage;
    Poem m_pendingPreviewPoem;
    ImageItem m_pendingPreviewImage;
    bool m_hasPendingPreviewSelection = false;

    QLabel *m_statusLabel = nullptr;
    QLabel *m_desktopStateLabel = nullptr;
    QLabel *m_countsLabel = nullptr;
    QLabel *m_scheduleSummaryLabel = nullptr;
    QLabel *m_previewLabel = nullptr;
    QLabel *m_previewHintLabel = nullptr;
    QLabel *m_previewContentLabel = nullptr;
    QLabel *m_imageScanLabel = nullptr;
    QLabel *m_selectedImageInfoLabel = nullptr;
    QLabel *m_poemCountLabel = nullptr;
    QLabel *m_scheduleNextLabel = nullptr;

    QListWidget *m_imageDirectories = nullptr;
    QListWidget *m_imageFiles = nullptr;
    QTableWidget *m_poemTable = nullptr;

    QComboBox *m_fitCombo = nullptr;
    QComboBox *m_anchorCombo = nullptr;
    QFontComboBox *m_fontCombo = nullptr;
    QSpinBox *m_fontSizeSpin = nullptr;
    QComboBox *m_typographyPreset = nullptr;
    QPushButton *m_textColorButton = nullptr;
    QPushButton *m_titleColorButton = nullptr;
    QPushButton *m_metadataColorButton = nullptr;
    QPushButton *m_panelColorButton = nullptr;
    QCheckBox *m_panelCheck = nullptr;
    QCheckBox *m_shadowCheck = nullptr;
    QCheckBox *m_scheduleCheck = nullptr;
    QSpinBox *m_intervalSpin = nullptr;

    QPushButton *m_switchButton = nullptr;
    QPushButton *m_applyPreviewButton = nullptr;
    QPushButton *m_refreshPreviewButton = nullptr;
    QPushButton *m_pauseButton = nullptr;
    QPushButton *m_addImageDirectoryButton = nullptr;
    QPushButton *m_selectImageButton = nullptr;
    QPushButton *m_setSelectedImageButton = nullptr;
    QPushButton *m_useSelectedImagePreviewButton = nullptr;
    QPushButton *m_clearCacheButton = nullptr;

    QFutureWatcher<ImageLibrary::ScanResult> m_scanWatcher;
    QFutureWatcher<SwitchResult> m_previewWatcher;
    QFutureWatcher<SwitchResult> m_switchWatcher;
    QFutureWatcher<SwitchResult> m_applyWatcher;
};

} // namespace poetry


