
#include "main_window.h"

#include "../platform/windows_autostart.h"
#include "../platform/windows_wallpaper_setter.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QAbstractItemView>
#include <QApplication>
#include <QButtonGroup>
#include <QCheckBox>
#include <QClipboard>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDir>
#include <QEvent>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QImage>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPalette>
#include <QPushButton>
#include <QRadioButton>
#include <QScreen>
#include <QScrollArea>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStandardPaths>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextEdit>
#include <QTextStream>
#include <QTimer>
#include <QToolButton>
#include <QUrl>
#include <QUuid>
#include <QVBoxLayout>
#include <QWizard>
#include <QWizardPage>

namespace poetry {

namespace {
// Each settings page scrolls rather than being squeezed: a QFormLayout starved of
// vertical height collapses its rows and paints labels over each other, which no
// per-widget size check reveals. Returns the inner content widget to lay out into.
QWidget *makeTabPage(QTabWidget *tabs, const QString &title) {
    auto *page = new QWidget(tabs);
    page->setObjectName(title + QStringLiteral("Page"));
    auto *scroll = new QScrollArea(page);
    scroll->setObjectName(title + QStringLiteral("Scroll"));
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    auto *content = new QWidget(scroll);
    content->setObjectName(title + QStringLiteral("Content"));
    scroll->setWidget(content);
    auto *outer = new QVBoxLayout(page);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->addWidget(scroll);
    tabs->addTab(page, title);
    return content;
}
QString imageFilter() { return QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.webp)"); }

QString normalizedPath(const QString &path) {
    return QDir::cleanPath(QFileInfo(path).absoluteFilePath()).toCaseFolded();
}

bool isExcludedImage(const QStringList &excluded, const QString &path) {
    if (path.isEmpty()) return false;
    const QString normalized = normalizedPath(path);
    for (const QString &candidate : excluded) {
        if (normalizedPath(candidate) == normalized) return true;
    }
    return false;
}

bool sameWallpaperPair(const WallpaperRecord &a, const WallpaperRecord &b) {
    if (a.poemId != b.poemId || a.imagePath.isEmpty() || b.imagePath.isEmpty()) return false;
    return normalizedPath(a.imagePath) == normalizedPath(b.imagePath);
}

bool isFavoriteRecord(const QVector<WallpaperRecord> &favorites, const WallpaperRecord &record) {
    for (const WallpaperRecord &favorite : favorites) {
        if (sameWallpaperPair(favorite, record)) return true;
    }
    return false;
}

QString poemFormLabel(const Poem &poem) {
    if (!poem.isPaired()) return QStringLiteral("单首");
    const QString key = poem.relation.toLower();
    if (key == QLatin1String("gift")) return QStringLiteral("赠答");
    if (key == QLatin1String("harmony")) return QStringLiteral("唱和");
    if (key == QLatin1String("qa")) return QStringLiteral("问答");
    return QStringLiteral("双首");
}

// The relation choices, in the order the editor combo shows them.
struct RelationChoice { const char *value; const char *label; };
const RelationChoice kRelationChoices[] = {
    {"", "单首"},
    {"gift", "赠答（一赠一酬）"},
    {"harmony", "唱和（原唱·奉和）"},
    {"qa", "问答（一问一答）"},
};

class OnboardingIntroPage final : public QWizardPage {
public:
    explicit OnboardingIntroPage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(QStringLiteral("欢迎使用诗词壁纸"));
        setSubTitle(QStringLiteral("用你的图片做底，让诗词落在桌面上。"));
        auto *layout = new QVBoxLayout(this);
        layout->setSpacing(10);
        auto *body = new QLabel(QStringLiteral(
            "三步就能开始：\n\n"
            "1. 选择底图：内置 3 张示例图，也可以指向你自己的图片文件夹。\n"
            "2. 选择诗词：已内置示例诗词，其中一首是赠答双诗，可在「诗词库」里增删改。\n"
            "3. 点击「随机换一张壁纸」生成并应用；左侧预览的改动不会自动修改桌面。\n\n"
            "所有图片和配置都保存在本机，不会上传网络。"), this);
        body->setWordWrap(true);
        body->setTextFormat(Qt::PlainText);
        layout->addWidget(body);
        auto *hint = new QLabel(QStringLiteral("下一步先准备底图。"), this);
        hint->setObjectName(QStringLiteral("fieldHintLabel"));
        layout->addWidget(hint);
        layout->addStretch();
    }
};

class OnboardingImagePage final : public QWizardPage {
public:
    explicit OnboardingImagePage(QWidget *parent = nullptr) : QWizardPage(parent) {
        setTitle(QStringLiteral("选择底图"));
        setSubTitle(QStringLiteral("先准备几张图片，之后可以随时在「图片库」里增删。"));

        auto *layout = new QVBoxLayout(this);
        layout->setSpacing(8);
        auto *group = new QButtonGroup(this);
        m_samples = new QRadioButton(QStringLiteral("使用内置示例图片（3 张，推荐）"), this);
        m_current = new QRadioButton(QStringLiteral("使用我当前的桌面壁纸"), this);
        m_folder = new QRadioButton(QStringLiteral("使用我的图片文件夹"), this);
        m_skip = new QRadioButton(QStringLiteral("暂不添加，稍后在「图片库」中设置"), this);
        m_samples->setChecked(true);
        for (QRadioButton *button : {m_samples, m_current, m_folder, m_skip}) group->addButton(button);
        layout->addWidget(m_samples);
        layout->addWidget(m_current);

        auto *folderRow = new QHBoxLayout();
        folderRow->setSpacing(8);
        folderRow->addWidget(m_folder);
        m_folderEdit = new QLineEdit(this);
        m_folderEdit->setPlaceholderText(QStringLiteral("选择包含图片的文件夹"));
        m_folderEdit->setEnabled(false);
        auto *browseButton = new QPushButton(QStringLiteral("浏览…"), this);
        browseButton->setEnabled(false);
        folderRow->addWidget(m_folderEdit, 1);
        folderRow->addWidget(browseButton);
        layout->addLayout(folderRow);
        layout->addWidget(m_skip);

        auto *hint = new QLabel(QStringLiteral("示例图片和当前桌面壁纸会复制一份到应用数据目录，不会改动原图。"), this);
        hint->setObjectName(QStringLiteral("fieldHintLabel"));
        hint->setWordWrap(true);
        layout->addWidget(hint);

        layout->addSpacing(6);
        m_autostart = new QCheckBox(QStringLiteral("登录 Windows 后自动启动（静默驻留托盘，可随时关闭）"), this);
        layout->addWidget(m_autostart);
        layout->addStretch();

        connect(m_folder, &QRadioButton::toggled, this, [this, browseButton](bool checked) {
            m_folderEdit->setEnabled(checked);
            browseButton->setEnabled(checked);
            emit completeChanged();
        });
        connect(m_folderEdit, &QLineEdit::textChanged, this, [this] { emit completeChanged(); });
        connect(browseButton, &QPushButton::clicked, this, [this] {
            const QString start = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
            const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择图片文件夹"), start);
            if (path.isEmpty()) return;
            m_folderEdit->setText(QDir::cleanPath(QFileInfo(path).absoluteFilePath()));
            m_folder->setChecked(true);
        });
    }

    bool isComplete() const override {
        return !m_folder->isChecked() || !m_folderEdit->text().trimmed().isEmpty();
    }

    bool wantsSamples() const { return m_samples->isChecked(); }
    bool wantsCurrentWallpaper() const { return m_current->isChecked(); }
    QString folderPath() const { return m_folder->isChecked() ? m_folderEdit->text().trimmed() : QString(); }
    bool wantsAutostart() const { return m_autostart->isChecked(); }

private:
    QRadioButton *m_samples = nullptr;
    QRadioButton *m_current = nullptr;
    QRadioButton *m_folder = nullptr;
    QRadioButton *m_skip = nullptr;
    QLineEdit *m_folderEdit = nullptr;
    QCheckBox *m_autostart = nullptr;
};
}

MainWindow::MainWindow(QWidget *parent, bool enableOnboarding)
    : QMainWindow(parent), m_enableOnboarding(enableOnboarding),
      m_picker(static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() & 0xffffffffu)) {
    setWindowTitle(QStringLiteral("诗词壁纸"));
    setMinimumSize(900, 640);
    resize(1440, 900);
    setupUi();
    setupTray();
    loadState();
    startImageScan();
    updateScheduleTimer();
    updateAllUi();
    if (m_firstRun && m_enableOnboarding) QTimer::singleShot(0, this, &MainWindow::showOnboarding);
}

MainWindow::~MainWindow() {
    if (m_timer) m_timer->stop();
    if (m_previewDebounce) m_previewDebounce->stop();
    if (m_scanWatcher.isRunning()) m_scanWatcher.waitForFinished();
    if (m_previewWatcher.isRunning()) m_previewWatcher.waitForFinished();
    if (m_switchWatcher.isRunning()) m_switchWatcher.waitForFinished();
    if (m_applyWatcher.isRunning()) m_applyWatcher.waitForFinished();
    saveState();
}

void MainWindow::setupUi() {
    auto *central = new QWidget(this);
    central->setObjectName(QStringLiteral("rootCanvas"));
    // 12 pt keeps every control legible at 100 % DPI. The type ramp below is
    // applied through object-name selectors rather than by shrinking the base
    // font, so nothing depends on stylesheet inheritance to stay readable.
    QFont uiFont = font();
    uiFont.setPointSize(12);
    setFont(uiFont);
    QPalette base = palette();
    base.setColor(QPalette::Window, QColor(QStringLiteral("#eef2f7")));
    setPalette(base);
    setStyleSheet(QStringLiteral(R"(
        QWidget#rootCanvas { background: #eef2f7; }

        QLabel { color: #1f2937; font-size: 11pt; }
        QLabel#appTitleLabel { font-size: 20pt; font-weight: 700; color: #0f172a; }
        QLabel#appSubtitleLabel { font-size: 10pt; color: #64748b; }
        QLabel#resourceSummaryLabel, QLabel#scheduleSummaryLabel { font-size: 11pt; color: #475569; }
        QLabel#previewHintLabel, QLabel#statusLabel, QLabel#imageScanStatusLabel,
        QLabel#selectedImageInfoLabel, QLabel#poemCountLabel, QLabel#scheduleNextLabel,
        QLabel#fieldHintLabel { font-size: 10pt; color: #64748b; }
        QLabel#previewContentLabel { font-size: 11pt; color: #334155; }
        QLabel#previewLabel { background: #10151d; color: #8d99a8; border: 1px solid #2a3441; border-radius: 10px; }

        QGroupBox { background: #ffffff; border: 1px solid #e2e8f0; border-radius: 12px;
                    margin-top: 11px; padding: 11px 12px 10px; font-size: 11pt; font-weight: 700; color: #334155; }
        QGroupBox::title { subcontrol-origin: margin; left: 12px; padding: 0 5px; }

        QPushButton { min-height: 28px; padding: 2px 11px; border: 1px solid #cbd5e1; border-radius: 7px;
                      background: #ffffff; color: #1f2937; font-size: 11pt; }
        QPushButton:hover { background: #f1f5f9; border-color: #94a3b8; }
        QPushButton:pressed { background: #e2e8f0; }
        QPushButton:disabled { color: #a3afbd; background: #f4f6f9; border-color: #e4e9ef; }
        QPushButton#switchWallpaperButton { min-height: 44px; padding: 4px 18px; background: #2563eb; border: none;
                                            color: #ffffff; font-size: 12pt; font-weight: 700; }
        QPushButton#switchWallpaperButton:hover { background: #1d4ed8; }
        QPushButton#switchWallpaperButton:pressed { background: #1e40af; }
        QPushButton#switchWallpaperButton:disabled { background: #c3d3f2; color: #f2f6fd; }
        QPushButton#applyPreviewButton { border-color: #2563eb; color: #1d4ed8; font-weight: 600; }
        QPushButton#applyPreviewButton:hover { background: #eaf1fe; }
        QPushButton#applyPreviewButton:disabled { border-color: #e4e9ef; color: #a3afbd; font-weight: 400; }
        QPushButton#removeImageDirectoryButton, QPushButton#deletePoemButton, QPushButton#clearCacheButton { color: #b42318; border-color: #e6c8c5; }
        QPushButton#removeImageDirectoryButton:hover, QPushButton#deletePoemButton:hover, QPushButton#clearCacheButton:hover { background: #fdf1f0; border-color: #d99b95; }
        QPushButton#resetDisplaySettingsButton { border-color: transparent; background: transparent; color: #475569; }
        QPushButton#resetDisplaySettingsButton:hover { background: #f1f5f9; border-color: #cbd5e1; }

        QToolButton#moreMenuButton, QToolButton#exportMenuButton { min-height: 28px; padding: 2px 11px;
                                     border: 1px solid #cbd5e1; border-radius: 7px; background: #ffffff;
                                     color: #1f2937; font-size: 11pt; }
        QToolButton#moreMenuButton:hover, QToolButton#exportMenuButton:hover { background: #f1f5f9; border-color: #94a3b8; }
        QToolButton#exportMenuButton:disabled { color: #a3afbd; background: #f4f6f9; border-color: #e4e9ef; }

        QComboBox, QFontComboBox, QSpinBox, QLineEdit { min-height: 28px; padding: 1px 8px;
                                                        border: 1px solid #cbd5e1; border-radius: 7px;
                                                        background: #ffffff; color: #1f2937; font-size: 11pt; }
        QComboBox:focus, QFontComboBox:focus, QSpinBox:focus, QLineEdit:focus { border-color: #2563eb; }

        QListWidget, QTableWidget, QTextEdit { border: 1px solid #e2e8f0; border-radius: 8px;
                                               background: #ffffff; color: #1f2937; font-size: 11pt; }
        QListWidget::item { min-height: 24px; padding: 2px 4px; }
        QListWidget::item:selected { background: #e0eafe; color: #14336b; }
        QTableWidget { gridline-color: #eef2f6; alternate-background-color: #f8fafc; }
        QTableWidget::item:selected { background: #e0eafe; color: #14336b; }
        QHeaderView::section { min-height: 26px; padding: 3px 8px; border: none;
                               border-bottom: 1px solid #e2e8f0; background: #f8fafc;
                               font-size: 10pt; font-weight: 600; color: #475569; }
        QCheckBox { font-size: 11pt; color: #1f2937; spacing: 8px; }

        QTabWidget::pane { border: none; }
        QTabBar { background: transparent; }
        QTabBar::tab { min-height: 26px; padding: 3px 12px; margin-right: 5px;
                       border: 1px solid transparent; border-radius: 7px;
                       background: transparent; color: #5b6b7f; font-size: 11pt; }
        QTabBar::tab:selected { background: #ffffff; border-color: #dde5ee; color: #0f172a; font-weight: 600; }
        QTabBar::tab:!selected:hover { color: #1f2937; }

        QMenu { background: #ffffff; border: 1px solid #dde5ee; border-radius: 8px; padding: 4px; }
        QMenu::item { padding: 6px 20px; border-radius: 6px; }
        QMenu::item:selected { background: #eaf1fe; color: #14336b; }
        QMenu::separator { height: 1px; background: #e8edf3; margin: 4px 8px; }
    )"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(20, 16, 20, 12);
    root->setSpacing(12);

    auto *topBar = new QHBoxLayout();
    topBar->setSpacing(12);
    auto *brandCol = new QVBoxLayout();
    brandCol->setSpacing(1);
    auto *title = new QLabel(QStringLiteral("诗词壁纸"), central);
    title->setObjectName(QStringLiteral("appTitleLabel"));
    auto *subtitle = new QLabel(QStringLiteral("本地图片 × 诗词，合成桌面壁纸"), central);
    subtitle->setObjectName(QStringLiteral("appSubtitleLabel"));
    brandCol->addWidget(title);
    brandCol->addWidget(subtitle);
    m_desktopStateLabel = new QLabel(QStringLiteral("尚未生成预览"), central);
    m_desktopStateLabel->setObjectName(QStringLiteral("desktopStateLabel"));
    m_countsLabel = new QLabel(central);
    m_countsLabel->setObjectName(QStringLiteral("resourceSummaryLabel"));
    m_scheduleSummaryLabel = new QLabel(central);
    m_scheduleSummaryLabel->setObjectName(QStringLiteral("scheduleSummaryLabel"));
    auto *moreButton = new QToolButton(central);
    moreButton->setObjectName(QStringLiteral("moreMenuButton"));
    moreButton->setText(QStringLiteral("更多"));
    moreButton->setPopupMode(QToolButton::InstantPopup);
    auto *moreMenu = new QMenu(moreButton);
    auto *openCacheAction = moreMenu->addAction(QStringLiteral("打开缓存目录"));
    auto *openLogAction = moreMenu->addAction(QStringLiteral("打开日志目录"));
    auto *clearCacheAction = moreMenu->addAction(QStringLiteral("清理缓存"));
    auto *onboardingAction = moreMenu->addAction(QStringLiteral("首次使用引导"));
    moreMenu->addSeparator();
    auto *quitAction = moreMenu->addAction(QStringLiteral("退出"));
    moreButton->setMenu(moreMenu);
    connect(openCacheAction, &QAction::triggered, this, &MainWindow::openCacheDirectory);
    connect(openLogAction, &QAction::triggered, this, &MainWindow::openLogDirectory);
    connect(clearCacheAction, &QAction::triggered, this, &MainWindow::clearCache);
    connect(onboardingAction, &QAction::triggered, this, &MainWindow::showOnboarding);
    connect(quitAction, &QAction::triggered, this, &MainWindow::quitFromTray);
    topBar->addLayout(brandCol);
    topBar->addSpacing(6);
    topBar->addWidget(m_desktopStateLabel);
    topBar->addStretch();
    topBar->addWidget(m_countsLabel);
    topBar->addSpacing(14);
    topBar->addWidget(m_scheduleSummaryLabel);
    topBar->addWidget(moreButton);
    root->addLayout(topBar);

    auto *body = new QSplitter(Qt::Horizontal, central);
    body->setObjectName(QStringLiteral("mainContentSplitter"));
    body->setChildrenCollapsible(false);
    body->setHandleWidth(12);

    auto *previewGroup = new QGroupBox(QStringLiteral("壁纸预览"), body);
    previewGroup->setMinimumWidth(520);
    auto *previewLayout = new QVBoxLayout(previewGroup);
    previewLayout->setSpacing(10);
    m_previewLabel = new QLabel(QStringLiteral("还没有可用预览\n请先添加图片目录并导入诗词库"), previewGroup);
    m_previewLabel->setObjectName(QStringLiteral("previewLabel"));
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(520, 300);
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewLabel->installEventFilter(this);
    previewLayout->addWidget(m_previewLabel, 1);

    auto *actionBar = new QHBoxLayout();
    actionBar->setSpacing(8);
    m_switchButton = new QPushButton(QStringLiteral("随机换一张壁纸"), previewGroup);
    m_switchButton->setObjectName(QStringLiteral("switchWallpaperButton"));
    m_switchButton->setToolTip(QStringLiteral("\u968f\u673a\u9009\u62e9\u4e00\u5f20\u56fe\u7247\u548c\u4e00\u9996\u542f\u7528\u8bd7\u8bcd\uff0c\u751f\u6210\u5e76\u8bbe\u7f6e\u684c\u9762\u58c1\u7eb8"));
    m_switchButton->setMinimumHeight(44);
    m_applyPreviewButton = new QPushButton(QStringLiteral("应用当前预览"), previewGroup);
    m_applyPreviewButton->setObjectName(QStringLiteral("applyPreviewButton"));
    m_applyPreviewButton->setToolTip(QStringLiteral("\u628a\u5f53\u524d\u9884\u89c8\u76f4\u63a5\u5e94\u7528\u5230 Windows \u684c\u9762\uff0c\u4e0d\u91cd\u65b0\u968f\u673a\u5185\u5bb9"));
    m_refreshPreviewButton = new QPushButton(QStringLiteral("重新预览"), previewGroup);
    m_refreshPreviewButton->setObjectName(QStringLiteral("refreshPreviewButton"));
    m_pauseButton = new QPushButton(QStringLiteral("开启定时"), previewGroup);
    m_pauseButton->setObjectName(QStringLiteral("pauseScheduleButton"));
    m_pauseButton->setToolTip(QStringLiteral("\u5feb\u901f\u5f00\u542f\u6216\u505c\u6b62\u5b9a\u65f6\u5207\u6362"));
    m_exportMenuButton = new QToolButton(previewGroup);
    m_exportMenuButton->setObjectName(QStringLiteral("exportMenuButton"));
    m_exportMenuButton->setText(QStringLiteral("导出"));
    m_exportMenuButton->setToolTip(QStringLiteral("把当前成品保存为图片，或复制到剪贴板"));
    m_exportMenuButton->setPopupMode(QToolButton::InstantPopup);
    auto *exportMenu = new QMenu(m_exportMenuButton);
    auto *saveImageAction = exportMenu->addAction(QStringLiteral("导出图片…"));
    auto *copyImageAction = exportMenu->addAction(QStringLiteral("复制图片到剪贴板"));
    m_exportMenuButton->setMenu(exportMenu);
    connect(saveImageAction, &QAction::triggered, this, &MainWindow::exportPreviewImage);
    connect(copyImageAction, &QAction::triggered, this, &MainWindow::copyPreviewImage);
    actionBar->addWidget(m_switchButton);
    actionBar->addWidget(m_applyPreviewButton);
    actionBar->addWidget(m_refreshPreviewButton);
    actionBar->addWidget(m_pauseButton);
    actionBar->addWidget(m_exportMenuButton);
    actionBar->addStretch();
    previewLayout->addLayout(actionBar);

    auto *captionRow = new QHBoxLayout();
    captionRow->setSpacing(10);
    m_previewContentLabel = new QLabel(QStringLiteral("暂无预览内容"), previewGroup);
    m_previewContentLabel->setObjectName(QStringLiteral("previewContentLabel"));
    m_previewHintLabel = new QLabel(QStringLiteral("预览不会自动修改桌面"), previewGroup);
    m_previewHintLabel->setObjectName(QStringLiteral("previewHintLabel"));
    m_previewHintLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    captionRow->addWidget(m_previewContentLabel, 1);
    captionRow->addWidget(m_previewHintLabel);
    previewLayout->addLayout(captionRow);
    body->addWidget(previewGroup);

    auto *rail = new QWidget(body);
    rail->setObjectName(QStringLiteral("sideRail"));
    rail->setMinimumWidth(430);
    auto *railLayout = new QVBoxLayout(rail);
    railLayout->setContentsMargins(0, 0, 0, 0);

    m_tabs = new QTabWidget(rail);
    m_tabs->setObjectName(QStringLiteral("resourceSettingsTabs"));
    m_tabs->setDocumentMode(true);
    m_tabs->setUsesScrollButtons(true);
    m_tabs->setMinimumHeight(260);
    railLayout->addWidget(m_tabs, 1);

    body->addWidget(rail);
    body->setStretchFactor(0, 7);
    body->setStretchFactor(1, 4);
    body->setSizes({900, 520});
    root->addWidget(body, 1);

    auto *statusStrip = new QHBoxLayout();
    m_statusLabel = new QLabel(QStringLiteral("准备就绪"), central);
    m_statusLabel->setObjectName(QStringLiteral("statusLabel"));
    statusStrip->addWidget(m_statusLabel, 1);
    root->addLayout(statusStrip);
    setCentralWidget(central);


    auto *imagePage = makeTabPage(m_tabs, QStringLiteral("图片库"));
    auto *imageRoot = new QVBoxLayout(imagePage);
    auto *imageToolbar = new QGridLayout();
    imageToolbar->setHorizontalSpacing(8);
    imageToolbar->setVerticalSpacing(8);
    m_selectImageButton = new QPushButton(QStringLiteral("选择图片…"), imagePage);
    m_selectImageButton->setObjectName(QStringLiteral("selectImageButton"));
    m_addImageDirectoryButton = new QPushButton(QStringLiteral("添加图片目录"), imagePage);
    m_addImageDirectoryButton->setObjectName(QStringLiteral("addImageDirectoryButton"));
    auto *removeDirectoryButton = new QPushButton(QStringLiteral("移除目录"), imagePage);
    removeDirectoryButton->setObjectName(QStringLiteral("removeImageDirectoryButton"));
    auto *rescanButton = new QPushButton(QStringLiteral("重新扫描"), imagePage);
    rescanButton->setObjectName(QStringLiteral("rescanImagesButton"));
    imageToolbar->addWidget(m_selectImageButton, 0, 0);
    imageToolbar->addWidget(m_addImageDirectoryButton, 0, 1);
    imageToolbar->addWidget(removeDirectoryButton, 1, 0);
    imageToolbar->addWidget(rescanButton, 1, 1);
    imageRoot->addLayout(imageToolbar);
    auto *imageSplitter = new QSplitter(Qt::Vertical, imagePage);
    auto *directoryGroup = new QGroupBox(QStringLiteral("图片来源目录"), imageSplitter);
    auto *directoryLayout = new QVBoxLayout(directoryGroup);
    m_imageDirectories = new QListWidget(directoryGroup);
    m_imageDirectories->setObjectName(QStringLiteral("imageDirectoryList"));
    m_imageDirectories->setSelectionMode(QAbstractItemView::SingleSelection);
    directoryLayout->addWidget(m_imageDirectories);
    auto *fileGroup = new QGroupBox(QStringLiteral("可用图片（可直接选择）"), imageSplitter);
    auto *fileLayout = new QVBoxLayout(fileGroup);
    m_imageFiles = new QListWidget(fileGroup);
    m_imageFiles->setObjectName(QStringLiteral("imageFileList"));
    m_imageFiles->setSelectionMode(QAbstractItemView::SingleSelection);
    m_imageFiles->setTextElideMode(Qt::ElideMiddle);
    m_imageFiles->setMinimumHeight(140);
    fileLayout->addWidget(m_imageFiles);
    imageSplitter->addWidget(directoryGroup);
    imageSplitter->addWidget(fileGroup);
    imageSplitter->setStretchFactor(0, 2);
    imageSplitter->setStretchFactor(1, 3);
    imageRoot->addWidget(imageSplitter, 1);
    m_selectedImageInfoLabel = new QLabel(QStringLiteral("请选择一张图片"), imagePage);
    m_selectedImageInfoLabel->setObjectName(QStringLiteral("selectedImageInfoLabel"));
    imageRoot->addWidget(m_selectedImageInfoLabel);
    auto *imageActionRow = new QHBoxLayout();
    m_useSelectedImagePreviewButton = new QPushButton(QStringLiteral("使用选中图片预览"), imagePage);
    m_useSelectedImagePreviewButton->setObjectName(QStringLiteral("useSelectedImagePreviewButton"));
    m_useSelectedImagePreviewButton->setToolTip(QStringLiteral("只更新左侧预览，不会修改桌面。"));
    m_setSelectedImageButton = new QPushButton(QStringLiteral("将选中图片设为壁纸"), imagePage);
    m_setSelectedImageButton->setObjectName(QStringLiteral("setSelectedImageButton"));
    m_setSelectedImageButton->setToolTip(QStringLiteral("直接把这张原图设为桌面壁纸，不叠加诗词。"));
    m_excludeImageButton = new QPushButton(QStringLiteral("排除选中图片"), imagePage);
    m_excludeImageButton->setObjectName(QStringLiteral("excludeImageButton"));
    m_excludeImageButton->setToolTip(QStringLiteral("从随机换壁纸中排除这张图片，不会删除原文件。"));
    imageActionRow->addWidget(m_useSelectedImagePreviewButton);
    imageActionRow->addWidget(m_setSelectedImageButton);
    imageActionRow->addWidget(m_excludeImageButton);
    imageActionRow->addStretch();
    imageRoot->addLayout(imageActionRow);
    m_imageScanLabel = new QLabel(imagePage);
    m_imageScanLabel->setObjectName(QStringLiteral("imageScanStatusLabel"));
    imageRoot->addWidget(m_imageScanLabel);
    auto *poemPage = makeTabPage(m_tabs, QStringLiteral("诗词库"));
    auto *poemRoot = new QVBoxLayout(poemPage);
    auto *poemToolbar = new QGridLayout();
    poemToolbar->setHorizontalSpacing(8);
    poemToolbar->setVerticalSpacing(8);
    auto *addPoemButton = new QPushButton(QStringLiteral("\u6dfb\u52a0\u8bd7\u8bcd"), poemPage);
    addPoemButton->setObjectName(QStringLiteral("addPoemButton"));
    auto *editPoemButton = new QPushButton(QStringLiteral("\u7f16\u8f91\u9009\u4e2d"), poemPage);
    editPoemButton->setObjectName(QStringLiteral("editPoemButton"));
    auto *importButton = new QPushButton(QStringLiteral("导入诗词 JSON"), poemPage);
    importButton->setObjectName(QStringLiteral("importPoetryButton"));
    auto *togglePoemButton = new QPushButton(QStringLiteral("启用/停用"), poemPage);
    auto *deletePoemButton = new QPushButton(QStringLiteral("删除选中"), poemPage);
    deletePoemButton->setObjectName(QStringLiteral("deletePoemButton"));
    auto *usePoemButton = new QPushButton(QStringLiteral("使用选中诗词预览"), poemPage);
    usePoemButton->setObjectName(QStringLiteral("useSelectedPoemPreviewButton"));
    poemToolbar->addWidget(addPoemButton, 0, 0);
    poemToolbar->addWidget(editPoemButton, 0, 1);
    poemToolbar->addWidget(importButton, 0, 2);
    poemToolbar->addWidget(togglePoemButton, 1, 0);
    poemToolbar->addWidget(deletePoemButton, 1, 1);
    poemToolbar->addWidget(usePoemButton, 1, 2);
    poemRoot->addLayout(poemToolbar);
    m_poemTable = new QTableWidget(poemPage);
    m_poemTable->setObjectName(QStringLiteral("poemTable"));
    m_poemTable->setColumnCount(5);
    m_poemTable->setHorizontalHeaderLabels({QStringLiteral("启用"), QStringLiteral("标题"), QStringLiteral("作者"),
                                            QStringLiteral("正文摘要"), QStringLiteral("形式")});
    m_poemTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_poemTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_poemTable->verticalHeader()->setDefaultSectionSize(32);
    m_poemTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_poemTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_poemTable->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    poemRoot->addWidget(m_poemTable, 1);
    m_poemCountLabel = new QLabel(poemPage);
    m_poemCountLabel->setObjectName(QStringLiteral("poemCountLabel"));
    poemRoot->addWidget(m_poemCountLabel);

    auto *historyPage = makeTabPage(m_tabs, QStringLiteral("历史"));
    auto *historyRoot = new QVBoxLayout(historyPage);
    auto *historyToolbar = new QHBoxLayout();
    historyToolbar->setSpacing(8);
    m_historyFilter = new QComboBox(historyPage);
    m_historyFilter->setObjectName(QStringLiteral("historyFilterComboBox"));
    m_historyFilter->addItem(QStringLiteral("全部记录"), QStringLiteral("all"));
    m_historyFilter->addItem(QStringLiteral("仅收藏"), QStringLiteral("favorites"));
    m_applyHistoryButton = new QPushButton(QStringLiteral("重新应用"), historyPage);
    m_applyHistoryButton->setObjectName(QStringLiteral("applyHistoryButton"));
    m_applyHistoryButton->setToolTip(QStringLiteral("用当前排版重新生成这张壁纸并应用到桌面。"));
    m_favoriteHistoryButton = new QPushButton(QStringLiteral("收藏"), historyPage);
    m_favoriteHistoryButton->setObjectName(QStringLiteral("favoriteHistoryButton"));
    m_favoriteHistoryButton->setToolTip(QStringLiteral("收藏的搭配会偶尔出现在随机结果中。"));
    m_excludeHistoryImageButton = new QPushButton(QStringLiteral("排除底图"), historyPage);
    m_excludeHistoryImageButton->setObjectName(QStringLiteral("excludeHistoryImageButton"));
    m_excludeHistoryImageButton->setToolTip(QStringLiteral("不再随机使用这张底图，原文件不会被删除。"));
    historyToolbar->addWidget(m_historyFilter);
    historyToolbar->addWidget(m_applyHistoryButton);
    historyToolbar->addWidget(m_favoriteHistoryButton);
    historyToolbar->addWidget(m_excludeHistoryImageButton);
    historyToolbar->addStretch();
    historyRoot->addLayout(historyToolbar);
    m_historyTable = new QTableWidget(historyPage);
    m_historyTable->setObjectName(QStringLiteral("historyTable"));
    m_historyTable->setColumnCount(4);
    m_historyTable->setHorizontalHeaderLabels({QStringLiteral("收藏"), QStringLiteral("诗词"),
                                               QStringLiteral("底图"), QStringLiteral("时间")});
    m_historyTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_historyTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_historyTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_historyTable->verticalHeader()->setDefaultSectionSize(32);
    m_historyTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_historyTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_historyTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Stretch);
    m_historyTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    historyRoot->addWidget(m_historyTable, 1);
    m_historySummaryLabel = new QLabel(historyPage);
    m_historySummaryLabel->setObjectName(QStringLiteral("historySummaryLabel"));
    historyRoot->addWidget(m_historySummaryLabel);
    auto *historyHint = new QLabel(QStringLiteral("保留最近 20 次；收藏会长期保留。重新应用会按当前排版重新生成。"), historyPage);
    historyHint->setObjectName(QStringLiteral("fieldHintLabel"));
    historyHint->setWordWrap(true);
    historyRoot->addWidget(historyHint);

    auto *displayPage = makeTabPage(m_tabs, QStringLiteral("显示设置"));
    auto *displayRoot = new QVBoxLayout(displayPage);
    displayRoot->setSpacing(10);
    m_fontCombo = new QFontComboBox(displayPage);
    m_fontCombo->setObjectName(QStringLiteral("fontComboBox"));
    m_fontSizeSpin = new QSpinBox(displayPage);
    m_fontSizeSpin->setObjectName(QStringLiteral("fontSizeSpinBox"));
    m_fontSizeSpin->setRange(10, 120);
    m_fontSizeSpin->setSuffix(QStringLiteral(" pt"));
    m_typographyPreset = new QComboBox(displayPage);
    m_typographyPreset->setObjectName(QStringLiteral("typographyPresetComboBox"));
    m_typographyPreset->addItem(QStringLiteral("\u96c5\u81f4\u5c42\u7ea7\uff08\u63a8\u8350\uff09"), QStringLiteral("elegant"));
    m_typographyPreset->addItem(QStringLiteral("\u7b80\u6d01\u5c42\u7ea7"), QStringLiteral("simple"));
    m_typographyPreset->addItem(QStringLiteral("\u81ea\u5b9a\u4e49"), QStringLiteral("custom"));
    m_textColorButton = new QPushButton(displayPage);
    m_textColorButton->setObjectName(QStringLiteral("textColorButton"));
    m_titleColorButton = new QPushButton(displayPage);
    m_titleColorButton->setObjectName(QStringLiteral("titleColorButton"));
    m_metadataColorButton = new QPushButton(displayPage);
    m_metadataColorButton->setObjectName(QStringLiteral("metadataColorButton"));
    m_dynastyColorButton = new QPushButton(displayPage);
    m_dynastyColorButton->setObjectName(QStringLiteral("dynastyColorButton"));
    m_anchorCombo = new QComboBox(displayPage);
    m_anchorCombo->setObjectName(QStringLiteral("anchorComboBox"));
    m_anchorCombo->addItem(QStringLiteral("左上"), QStringLiteral("topLeft"));
    m_anchorCombo->addItem(QStringLiteral("右上"), QStringLiteral("topRight"));
    m_anchorCombo->addItem(QStringLiteral("居中"), QStringLiteral("center"));
    m_anchorCombo->addItem(QStringLiteral("左下"), QStringLiteral("bottomLeft"));
    m_anchorCombo->addItem(QStringLiteral("右下"), QStringLiteral("bottomRight"));
    m_fitCombo = new QComboBox(displayPage);
    m_fitCombo->setObjectName(QStringLiteral("fitModeComboBox"));
    m_fitCombo->addItem(QStringLiteral("铺满（Fill）"), QStringLiteral("fill"));
    m_fitCombo->addItem(QStringLiteral("适应（Fit）"), QStringLiteral("fit"));
    m_fitCombo->addItem(QStringLiteral("拉伸（Stretch）"), QStringLiteral("stretch"));
    m_panelCheck = new QCheckBox(QStringLiteral("文字底板"), displayPage);
    m_panelCheck->setObjectName(QStringLiteral("panelCheckBox"));
    m_panelColorButton = new QPushButton(displayPage);
    m_panelColorButton->setObjectName(QStringLiteral("panelColorButton"));
    m_shadowCheck = new QCheckBox(QStringLiteral("文字阴影"), displayPage);
    m_shadowCheck->setObjectName(QStringLiteral("shadowCheckBox"));
    m_relationTagCheck = new QCheckBox(QStringLiteral("双诗显示酬答标签"), displayPage);
    m_relationTagCheck->setObjectName(QStringLiteral("relationTagCheckBox"));
    m_pairLayoutPanelCombo = new QComboBox(displayPage);
    m_pairLayoutPanelCombo->setObjectName(QStringLiteral("pairLayoutPanelComboBox"));
    m_pairLayoutPanelCombo->addItem(QStringLiteral("合并为一个面板"), QStringLiteral("merged"));
    m_pairLayoutPanelCombo->addItem(QStringLiteral("各自独立成面板"), QStringLiteral("separate"));
    m_pairSpacingSpin = new QSpinBox(displayPage);
    m_pairSpacingSpin->setObjectName(QStringLiteral("pairSpacingSpinBox"));
    m_pairSpacingSpin->setRange(0, 400);
    m_pairSpacingSpin->setSuffix(QStringLiteral(" px"));
    auto *typeCard = new QGroupBox(QStringLiteral("排版"), displayPage);
    auto *typeForm = new QFormLayout(typeCard);
    typeForm->addRow(QStringLiteral("字体族"), m_fontCombo);
    typeForm->addRow(QStringLiteral("正文基准字号"), m_fontSizeSpin);
    typeForm->addRow(QStringLiteral("排版预设"), m_typographyPreset);
    displayRoot->addWidget(typeCard);

    auto *colorCard = new QGroupBox(QStringLiteral("颜色"), displayPage);
    auto *colorGrid = new QGridLayout(colorCard);
    colorGrid->setHorizontalSpacing(8);
    colorGrid->setVerticalSpacing(6);
    const auto addSwatch = [](QGridLayout *grid, int row, int column, const QString &caption, QPushButton *button) {
        auto *cell = new QVBoxLayout();
        cell->setSpacing(2);
        auto *captionLabel = new QLabel(caption);
        captionLabel->setObjectName(QStringLiteral("fieldHintLabel"));
        cell->addWidget(captionLabel);
        cell->addWidget(button);
        grid->addLayout(cell, row, column);
    };
    addSwatch(colorGrid, 0, 0, QStringLiteral("标题"), m_titleColorButton);
    addSwatch(colorGrid, 0, 1, QStringLiteral("作者"), m_metadataColorButton);
    addSwatch(colorGrid, 1, 0, QStringLiteral("朝代"), m_dynastyColorButton);
    addSwatch(colorGrid, 1, 1, QStringLiteral("正文"), m_textColorButton);
    displayRoot->addWidget(colorCard);

    auto *layoutCard = new QGroupBox(QStringLiteral("版面"), displayPage);
    auto *layoutForm = new QFormLayout(layoutCard);
    layoutForm->addRow(QStringLiteral("文字位置"), m_anchorCombo);
    layoutForm->addRow(QStringLiteral("图片适配"), m_fitCombo);
    auto *panelRow = new QHBoxLayout();
    panelRow->setSpacing(8);
    panelRow->addWidget(m_panelCheck);
    panelRow->addWidget(m_panelColorButton);
    panelRow->addStretch();
    layoutForm->addRow(QStringLiteral("文字底板"), panelRow);
    layoutForm->addRow(QString(), m_shadowCheck);
    layoutForm->addRow(QStringLiteral("双诗间隔"), m_pairSpacingSpin);
    layoutForm->addRow(QStringLiteral("双诗面板"), m_pairLayoutPanelCombo);
    layoutForm->addRow(QString(), m_relationTagCheck);
    layoutForm->addRow(QStringLiteral("提示"), new QLabel(QStringLiteral("双诗间隔与酬答标签只在酬答／唱和／问答体裁下生效。"), displayPage));
    displayRoot->addWidget(layoutCard);

    auto *displayHint = new QLabel(QStringLiteral("改动会在约 220 ms 后刷新左侧预览，不会自动修改桌面。"), displayPage);
    displayHint->setObjectName(QStringLiteral("fieldHintLabel"));
    displayHint->setWordWrap(true);
    displayRoot->addWidget(displayHint);
    auto *resetDisplayButton = new QPushButton(QStringLiteral("恢复默认显示设置"), displayPage);
    resetDisplayButton->setObjectName(QStringLiteral("resetDisplaySettingsButton"));
    displayRoot->addWidget(resetDisplayButton, 0, Qt::AlignLeft);
    displayRoot->addStretch();

    auto *schedulePage = makeTabPage(m_tabs, QStringLiteral("定时与高级"));
    auto *scheduleRoot = new QVBoxLayout(schedulePage);
    scheduleRoot->setSpacing(10);
    m_scheduleCheck = new QCheckBox(QStringLiteral("开启定时切换"), schedulePage);
    m_scheduleCheck->setObjectName(QStringLiteral("scheduleCheckBox"));
    m_intervalSpin = new QSpinBox(schedulePage);
    m_intervalSpin->setObjectName(QStringLiteral("intervalSpinBox"));
    m_intervalSpin->setRange(1, 1440);
    m_intervalSpin->setSuffix(QStringLiteral(" 分钟"));
    auto *scheduleCard = new QGroupBox(QStringLiteral("定时切换"), schedulePage);
    auto *scheduleForm = new QFormLayout(scheduleCard);
    scheduleForm->addRow(QString(), m_scheduleCheck);
    scheduleForm->addRow(QStringLiteral("切换间隔"), m_intervalSpin);
    m_scheduleNextLabel = new QLabel(schedulePage);
    m_scheduleNextLabel->setObjectName(QStringLiteral("scheduleNextLabel"));
    scheduleForm->addRow(QStringLiteral("下次切换"), m_scheduleNextLabel);
    m_autostartCheck = new QCheckBox(QStringLiteral("登录 Windows 后自动启动（静默驻留托盘）"), schedulePage);
    m_autostartCheck->setObjectName(QStringLiteral("autostartCheckBox"));
    m_autostartCheck->setToolTip(QStringLiteral("写入当前用户的启动项；不需要管理员权限，可随时关闭。"));
    scheduleForm->addRow(QString(), m_autostartCheck);
    scheduleRoot->addWidget(scheduleCard);
    auto *scheduleHint = new QLabel(QStringLiteral("程序重启后按启动时间重新计时，不补偿错过的切换；开启上面的开机自启才能在重启后继续。"), schedulePage);
    scheduleHint->setObjectName(QStringLiteral("fieldHintLabel"));
    scheduleHint->setWordWrap(true);
    scheduleRoot->addWidget(scheduleHint);
    auto *advancedGroup = new QGroupBox(QStringLiteral("高级"), schedulePage);
    auto *advancedLayout = new QHBoxLayout(advancedGroup);
    auto *openCacheButton = new QPushButton(QStringLiteral("打开缓存目录"), advancedGroup);
    auto *openLogButton = new QPushButton(QStringLiteral("打开日志目录"), advancedGroup);
    m_clearCacheButton = new QPushButton(QStringLiteral("清理缓存"), advancedGroup);
    m_clearCacheButton->setObjectName(QStringLiteral("clearCacheButton"));
    advancedLayout->addWidget(openCacheButton);
    advancedLayout->addWidget(m_clearCacheButton);
    advancedLayout->addWidget(openLogButton);
    advancedLayout->addStretch();
    scheduleRoot->addWidget(advancedGroup);
    scheduleRoot->addStretch();

    connect(m_switchButton, &QPushButton::clicked, this, &MainWindow::randomSwitchWallpaper);
    connect(m_applyPreviewButton, &QPushButton::clicked, this, &MainWindow::applyCurrentPreview);
    connect(m_refreshPreviewButton, &QPushButton::clicked, this, &MainWindow::refreshPreview);
    connect(m_pauseButton, &QPushButton::clicked, this, [this] { toggleSchedule(!m_config.scheduleEnabled); });
    connect(m_addImageDirectoryButton, &QPushButton::clicked, this, &MainWindow::addImageDirectory);
    connect(m_selectImageButton, &QPushButton::clicked, this, &MainWindow::selectImageFile);
    connect(removeDirectoryButton, &QPushButton::clicked, this, &MainWindow::removeImageDirectory);
    connect(rescanButton, &QPushButton::clicked, this, &MainWindow::rescanImages);
    connect(m_imageDirectories, &QListWidget::itemChanged, this, &MainWindow::imageDirectoryItemChanged);
    connect(m_imageFiles, &QListWidget::currentRowChanged, this, &MainWindow::imageFileSelectionChanged);
    connect(m_useSelectedImagePreviewButton, &QPushButton::clicked, this, &MainWindow::useSelectedImagePreview);
    connect(m_setSelectedImageButton, &QPushButton::clicked, this, &MainWindow::setSelectedImageWallpaper);
    connect(m_excludeImageButton, &QPushButton::clicked, this, &MainWindow::toggleSelectedImageExclusion);
    connect(addPoemButton, &QPushButton::clicked, this, &MainWindow::addPoem);
    connect(editPoemButton, &QPushButton::clicked, this, &MainWindow::editSelectedPoem);
    connect(importButton, &QPushButton::clicked, this, &MainWindow::importPoetry);
    connect(togglePoemButton, &QPushButton::clicked, this, [this] {
        const int row = m_poemTable->currentRow();
        if (row >= 0 && m_poemTable->item(row, 0)) {
            auto *item = m_poemTable->item(row, 0);
            item->setCheckState(item->checkState() == Qt::Checked ? Qt::Unchecked : Qt::Checked);
        }
    });
    connect(deletePoemButton, &QPushButton::clicked, this, &MainWindow::deleteSelectedPoem);
    connect(usePoemButton, &QPushButton::clicked, this, &MainWindow::useSelectedPoemPreview);
    connect(m_poemTable, &QTableWidget::itemChanged, this, &MainWindow::poemItemChanged);
    connect(m_historyFilter, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::historyFilterChanged);
    connect(m_historyTable, &QTableWidget::itemSelectionChanged, this, &MainWindow::historySelectionChanged);
    connect(m_applyHistoryButton, &QPushButton::clicked, this, &MainWindow::applySelectedHistory);
    connect(m_favoriteHistoryButton, &QPushButton::clicked, this, &MainWindow::toggleSelectedFavorite);
    connect(m_excludeHistoryImageButton, &QPushButton::clicked, this, &MainWindow::excludeSelectedHistoryImage);
    connect(m_textColorButton, &QPushButton::clicked, this, &MainWindow::chooseTextColor);
    connect(m_titleColorButton, &QPushButton::clicked, this, &MainWindow::chooseTitleColor);
    connect(m_metadataColorButton, &QPushButton::clicked, this, &MainWindow::chooseMetadataColor);
    connect(m_dynastyColorButton, &QPushButton::clicked, this, &MainWindow::chooseDynastyColor);
    connect(m_panelColorButton, &QPushButton::clicked, this, &MainWindow::choosePanelColor);
    connect(m_typographyPreset, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::typographyPresetChanged);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, &MainWindow::displaySettingsChanged);
    connect(m_fontSizeSpin, &QSpinBox::valueChanged, this, &MainWindow::displaySettingsChanged);
    connect(m_panelCheck, &QCheckBox::toggled, this, &MainWindow::displaySettingsChanged);
    connect(m_shadowCheck, &QCheckBox::toggled, this, &MainWindow::displaySettingsChanged);
    connect(m_relationTagCheck, &QCheckBox::toggled, this, &MainWindow::displaySettingsChanged);
    connect(m_pairSpacingSpin, &QSpinBox::valueChanged, this, &MainWindow::displaySettingsChanged);
    connect(m_pairLayoutPanelCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::displaySettingsChanged);
    connect(m_anchorCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::displaySettingsChanged);
    connect(m_fitCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::displaySettingsChanged);
    connect(resetDisplayButton, &QPushButton::clicked, this, &MainWindow::resetDisplaySettings);
    connect(m_scheduleCheck, &QCheckBox::toggled, this, &MainWindow::toggleSchedule);
    connect(m_intervalSpin, &QSpinBox::valueChanged, this, &MainWindow::saveScheduleSettings);
    connect(m_autostartCheck, &QCheckBox::toggled, this, &MainWindow::toggleAutostart);
    connect(openCacheButton, &QPushButton::clicked, this, &MainWindow::openCacheDirectory);
    connect(openLogButton, &QPushButton::clicked, this, &MainWindow::openLogDirectory);
    connect(m_clearCacheButton, &QPushButton::clicked, this, &MainWindow::clearCache);

    m_timer = new QTimer(this);
    m_timer->setSingleShot(true);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::timerTimeout);
    m_previewDebounce = new QTimer(this);
    m_previewDebounce->setSingleShot(true);
    m_previewDebounce->setInterval(220);
    connect(m_previewDebounce, &QTimer::timeout, this, &MainWindow::previewDebounceTimeout);
    connect(&m_scanWatcher, &QFutureWatcher<ImageLibrary::ScanResult>::finished, this, &MainWindow::scanFinished);
    connect(&m_previewWatcher, &QFutureWatcher<SwitchResult>::finished, this, &MainWindow::previewFinished);
    connect(&m_switchWatcher, &QFutureWatcher<SwitchResult>::finished, this, &MainWindow::switchFinished);
    connect(&m_applyWatcher, &QFutureWatcher<SwitchResult>::finished, this, &MainWindow::applyFinished);
}

void MainWindow::setupTray() {
    m_tray = new QSystemTrayIcon(this);
    m_tray->setIcon(QIcon(QStringLiteral(":/app_icon.ico")));
    m_tray->setToolTip(QStringLiteral("诗词壁纸"));
    auto *menu = new QMenu(this);
    auto *openAction = menu->addAction(QStringLiteral("打开主窗口"));
    auto *nextAction = menu->addAction(QStringLiteral("下一张诗词壁纸"));
    auto *pauseAction = menu->addAction(QStringLiteral("暂停/恢复定时"));
    menu->addSeparator();
    auto *clearAction = menu->addAction(QStringLiteral("清理缓存"));
    auto *quitAction = menu->addAction(QStringLiteral("退出"));
    connect(openAction, &QAction::triggered, this, &MainWindow::showWindow);
    connect(nextAction, &QAction::triggered, this, &MainWindow::randomSwitchWallpaper);
    connect(pauseAction, &QAction::triggered, this, [this] { toggleSchedule(!m_config.scheduleEnabled); });
    connect(clearAction, &QAction::triggered, this, &MainWindow::clearCache);
    connect(quitAction, &QAction::triggered, this, &MainWindow::quitFromTray);
    connect(m_tray, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::DoubleClick) showWindow();
    });
    m_tray->setContextMenu(menu);
    m_tray->show();
}
void MainWindow::loadState() {
    const bool hadConfigFile = QFile::exists(m_store.configPath());
    m_firstRun = !hadConfigFile;
    QStringList warnings;
    m_store.load(m_config, &warnings);
    if (!hadConfigFile) {
        m_config.poems = PoetryLibrary::defaultPoems();
        persistConfig();
    }
    // Repair the Run entry on every start: the executable may have moved, and a
    // stale quoted path would fail silently at login.
    if (m_config.autostartEnabled) {
        QString error;
        if (!WindowsAutoStart::setEnabled(true, &error)) {
            m_config.autostartEnabled = false;
            warnings.append(QStringLiteral("开机自启未能生效：%1").arg(error));
            persistConfig();
        }
    }
    m_picker.rememberPoem(m_config.runtime.lastPoemId);
    m_picker.rememberImage(m_config.runtime.lastImagePath);
    hydratePreviewState();
    updateAllUi();
    for (const QString &warning : warnings) appendLog(QStringLiteral("WARN"), warning);
    if (!warnings.isEmpty()) m_statusLabel->setText(warnings.join(QStringLiteral("；")));
}

void MainWindow::hydratePreviewState() {
    const QString previewPath = !m_config.runtime.currentPreviewWallpaper.isEmpty()
        ? m_config.runtime.currentPreviewWallpaper : m_config.runtime.lastSuccessfulWallpaper;
    const QString poemId = !m_config.runtime.currentPreviewPoemId.isEmpty()
        ? m_config.runtime.currentPreviewPoemId : m_config.runtime.lastPoemId;
    const QString imagePath = !m_config.runtime.currentPreviewImagePath.isEmpty()
        ? m_config.runtime.currentPreviewImagePath : m_config.runtime.lastImagePath;
    for (const Poem &poem : m_config.poems) {
        if (poem.id == poemId) {
            m_previewPoem = poem;
            break;
        }
    }
    if (!imagePath.isEmpty() && QFileInfo::exists(imagePath)) {
        m_previewImage = ImageItem{imagePath, QFileInfo(imagePath).size(), QFileInfo(imagePath).lastModified(), true, {}};
        m_selectedImagePath = imagePath;
    }
    if (!previewPath.isEmpty() && QFileInfo::exists(previewPath)) {
        m_previewPath = previewPath;
        m_hasPreviewContent = true;
        m_previewApplied = m_config.runtime.currentPreviewWallpaper.isEmpty()
            ? true : m_config.runtime.currentPreviewApplied;
        updatePreview(previewPath);
    }
}

void MainWindow::saveState() { persistConfig(); }

void MainWindow::persistConfig() {
    QString error;
    if (!m_store.save(m_config, &error)) {
        if (m_statusLabel) m_statusLabel->setText(QStringLiteral("配置保存失败：%1").arg(error));
        appendLog(QStringLiteral("ERROR"), error);
    }
}

void MainWindow::updateAllUi() {
    updateImageUi();
    updateImageFilesUi();
    updatePoemUi();
    updateHistoryUi();
    updateSettingsUi();
    updatePreviewSummary();
    updateStatusUi();
}

void MainWindow::updateImageUi() {
    m_ignoreImageChanges = true;
    m_imageDirectories->clear();
    for (const ImageSource &source : m_config.imageSources) {
        const QString label = source.isFile
            ? QStringLiteral("图片  %1").arg(QFileInfo(source.path).fileName())
            : QStringLiteral("目录  %1").arg(source.path);
        auto *item = new QListWidgetItem(label, m_imageDirectories);
        item->setToolTip(source.path);
        item->setData(Qt::UserRole, source.path);
        item->setCheckState(source.enabled ? Qt::Checked : Qt::Unchecked);
    }
    m_ignoreImageChanges = false;
    m_imageScanLabel->setText(QStringLiteral("扫描状态：有效图片 %1 张").arg(m_imageLibrary.validItems().size()));
}

void MainWindow::updateImageFilesUi() {
    const QString previous = m_selectedImagePath;
    QSignalBlocker blocker(m_imageFiles);
    m_imageFiles->clear();
    int selectedRow = -1;
    const QVector<ImageItem> items = m_imageLibrary.validItems();
    for (int i = 0; i < items.size(); ++i) {
        const ImageItem &image = items.at(i);
        const bool excluded = isExcludedImage(m_config.excludedImages, image.path);
        const QString name = QFileInfo(image.path).fileName();
        auto *item = new QListWidgetItem(excluded ? QStringLiteral("%1（已排除）").arg(name) : name, m_imageFiles);
        item->setData(Qt::UserRole, image.path);
        item->setToolTip(excluded ? QStringLiteral("%1\n已排除，仍可通过按钮恢复参与随机。").arg(image.path) : image.path);
        if (excluded) item->setForeground(QColor(QStringLiteral("#94a3b8")));
        if (image.path == previous) selectedRow = i;
    }
    if (selectedRow < 0 && !previous.isEmpty() && QFileInfo::exists(previous)) {
        auto *item = new QListWidgetItem(QStringLiteral("临时图片：%1").arg(QFileInfo(previous).fileName()), m_imageFiles);
        item->setData(Qt::UserRole, previous);
        item->setToolTip(previous);
        selectedRow = m_imageFiles->count() - 1;
    }
    if (selectedRow >= 0) m_imageFiles->setCurrentRow(selectedRow);
    updateSelectedImageInfo();
}

void MainWindow::updatePoemUi() {
    m_ignorePoemChanges = true;
    m_poemTable->setRowCount(0);
    int enabledCount = 0;
    int selectedRow = -1;
    for (const Poem &poem : m_config.poems) {
        const int row = m_poemTable->rowCount();
        m_poemTable->insertRow(row);
        auto *enabled = new QTableWidgetItem();
        enabled->setCheckState(poem.enabled ? Qt::Checked : Qt::Unchecked);
        enabled->setData(Qt::UserRole, poem.id);
        m_poemTable->setItem(row, 0, enabled);
        QString title = poem.title;
        if (poem.isPaired()) {
            const QString second = poem.secondTitle.isEmpty() ? poem.secondContent.left(8) : poem.secondTitle;
            title = QStringLiteral("%1 ↔ %2").arg(poem.title, second);
        }
        m_poemTable->setItem(row, 1, new QTableWidgetItem(title));
        QString author = poem.author;
        if (poem.isPaired() && !poem.secondAuthor.isEmpty()) {
            author = QStringLiteral("%1 / %2").arg(poem.author, poem.secondAuthor);
        }
        m_poemTable->setItem(row, 2, new QTableWidgetItem(author));
        QString content = poem.content;
        content.replace(QStringLiteral("\n"), QStringLiteral(" / "));
        if (poem.isPaired()) {
            QString secondContent = poem.secondContent;
            secondContent.replace(QStringLiteral("\n"), QStringLiteral(" / "));
            content += QStringLiteral(" ↔ ") + secondContent;
        }
        m_poemTable->setItem(row, 3, new QTableWidgetItem(content));
        auto *formItem = new QTableWidgetItem(poemFormLabel(poem));
        if (poem.isPaired()) formItem->setForeground(QColor(QStringLiteral("#1d4ed8")));
        m_poemTable->setItem(row, 4, formItem);
        if (poem.enabled) ++enabledCount;
        if (poem.id == m_previewPoem.id) selectedRow = row;
    }
    m_ignorePoemChanges = false;
    if (selectedRow >= 0) m_poemTable->selectRow(selectedRow);
    m_poemCountLabel->setText(QStringLiteral("共 %1 条 · 启用 %2 条").arg(m_config.poems.size()).arg(enabledCount));
}

void MainWindow::updateSettingsUi() {
    const QSignalBlocker b1(m_fontCombo);
    const QSignalBlocker b2(m_fontSizeSpin);
    const QSignalBlocker b3(m_typographyPreset);
    const QSignalBlocker b4(m_textColorButton);
    const QSignalBlocker b5(m_titleColorButton);
    const QSignalBlocker b6(m_metadataColorButton);
    const QSignalBlocker b7(m_panelColorButton);
    const QSignalBlocker b14(m_dynastyColorButton);
    const QSignalBlocker b8(m_panelCheck);
    const QSignalBlocker b9(m_shadowCheck);
    const QSignalBlocker b10(m_anchorCombo);
    const QSignalBlocker b11(m_fitCombo);
    m_fontCombo->setCurrentFont(QFont(m_config.render.fontFamily));
    m_fontSizeSpin->setValue(m_config.render.fontPointSize);

    const bool elegant = qFuzzyCompare(m_config.render.titleScale, 1.50)
        && qFuzzyCompare(m_config.render.authorScale, 0.72)
        && qFuzzyCompare(m_config.render.dynastyScale, 0.68)
        && m_config.render.titleWeight == 600;
    const bool simple = qFuzzyCompare(m_config.render.titleScale, 1.35)
        && qFuzzyCompare(m_config.render.authorScale, 0.78)
        && qFuzzyCompare(m_config.render.dynastyScale, 0.74)
        && m_config.render.titleWeight == 500;
    m_typographyPreset->setCurrentIndex(elegant ? 0 : (simple ? 1 : 2));

    const QColor contentColor = m_config.render.contentColor.isValid() ? m_config.render.contentColor : m_config.render.textColor;
    m_textColorButton->setText(contentColor.name(QColor::HexArgb));
    m_textColorButton->setStyleSheet(colorButtonStyle(contentColor));
    m_titleColorButton->setText(m_config.render.titleColor.name(QColor::HexArgb));
    m_titleColorButton->setStyleSheet(colorButtonStyle(m_config.render.titleColor));
    m_metadataColorButton->setText(m_config.render.metadataColor.name(QColor::HexArgb));
    m_metadataColorButton->setStyleSheet(colorButtonStyle(m_config.render.metadataColor));
    m_dynastyColorButton->setText(m_config.render.dynastyColor.name(QColor::HexArgb));
    m_dynastyColorButton->setStyleSheet(colorButtonStyle(m_config.render.dynastyColor));
    m_panelColorButton->setText(m_config.render.panelColor.name(QColor::HexArgb));
    m_panelColorButton->setStyleSheet(colorButtonStyle(m_config.render.panelColor));
    m_panelCheck->setChecked(m_config.render.panelEnabled);
    m_shadowCheck->setChecked(m_config.render.shadowEnabled);
    const QSignalBlocker b15(m_relationTagCheck);
    const QSignalBlocker b16(m_pairSpacingSpin);
    const QSignalBlocker b17(m_pairLayoutPanelCombo);
    m_relationTagCheck->setChecked(m_config.render.relationTagEnabled);
    m_pairSpacingSpin->setValue(m_config.render.pairSpacing);
    m_pairLayoutPanelCombo->setCurrentIndex(
        qMax(0, m_pairLayoutPanelCombo->findData(pairLayoutPanelToString(m_config.render.pairLayoutPanel))));
    m_anchorCombo->setCurrentIndex(m_anchorCombo->findData(anchorToString(m_config.render.anchor)));
    m_fitCombo->setCurrentIndex(m_fitCombo->findData(fitModeToString(m_config.render.fitMode)));
    const QSignalBlocker b12(m_scheduleCheck);
    const QSignalBlocker b13(m_intervalSpin);
    const QSignalBlocker b18(m_autostartCheck);
    m_scheduleCheck->setChecked(m_config.scheduleEnabled);
    m_intervalSpin->setValue(m_config.intervalMinutes);
    m_autostartCheck->setChecked(m_config.autostartEnabled);
}
void MainWindow::updateStatusUi() {
    const int validImages = availableImages().size();
    const int validPoems = enabledPoems().size();
    m_countsLabel->setText(QStringLiteral("图片 %1 · 诗词 %2").arg(validImages).arg(validPoems));
    if (m_desktopBusy || m_previewBusy) {
        setTopStatus(QStringLiteral("正在生成"), QColor(40, 110, 210));
    } else if (validImages == 0 || validPoems == 0) {
        setTopStatus(QStringLiteral("资源不足"), QColor(190, 120, 20));
    } else if (m_hasPreviewContent && m_previewApplied && !m_previewPending) {
        setTopStatus(QStringLiteral("已应用到桌面"), QColor(30, 135, 70));
    } else if (m_hasPreviewContent) {
        setTopStatus(QStringLiteral("预览未应用"), QColor(190, 120, 20));
    } else {
        setTopStatus(QStringLiteral("尚未生成预览"), QColor(100, 100, 100));
    }
    m_switchButton->setEnabled(!m_desktopBusy && !m_previewBusy && validImages > 0 && validPoems > 0);
    m_applyPreviewButton->setEnabled(!m_desktopBusy && !m_previewBusy && !m_previewPending && m_hasPreviewContent && QFileInfo::exists(m_previewPath) && !m_previewApplied);
    m_refreshPreviewButton->setEnabled(!m_desktopBusy && !m_previewBusy && (m_hasPreviewContent || m_imageFiles->currentRow() >= 0) && validPoems > 0);
    const bool hasImage = !selectedImage().path.isEmpty();
    m_useSelectedImagePreviewButton->setEnabled(!m_desktopBusy && !m_previewBusy && hasImage && validPoems > 0);
    m_setSelectedImageButton->setEnabled(!m_desktopBusy && hasImage);
    m_excludeImageButton->setEnabled(hasImage);
    m_excludeImageButton->setText(isExcludedImage(m_config.excludedImages, m_selectedImagePath)
        ? QStringLiteral("恢复选中图片") : QStringLiteral("排除选中图片"));
    m_exportMenuButton->setEnabled(m_hasPreviewContent && QFileInfo::exists(m_previewPath));
    WallpaperRecord record;
    bool favorite = false;
    const bool recordSelected = selectedHistoryRecord(&record, &favorite);
    m_applyHistoryButton->setEnabled(recordSelected && !m_desktopBusy && !m_previewBusy);
    m_excludeHistoryImageButton->setEnabled(recordSelected && !m_desktopBusy);
    m_favoriteHistoryButton->setEnabled(recordSelected);
    m_favoriteHistoryButton->setText(favorite ? QStringLiteral("取消收藏") : QStringLiteral("收藏"));
    m_pauseButton->setText(m_config.scheduleEnabled ? QStringLiteral("暂停定时") : QStringLiteral("开启定时"));
    m_scheduleSummaryLabel->setText(m_config.scheduleEnabled ? QStringLiteral("定时已开启") : QStringLiteral("定时已关闭"));
}

void MainWindow::updatePreview(const QString &path) {
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        m_previewPixmap = QPixmap();
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText(QStringLiteral("还没有可用预览\n请先添加图片目录并导入诗词库"));
        return;
    }
    const QPixmap pixmap(path);
    if (pixmap.isNull()) return;
    m_previewPixmap = pixmap;
    m_previewLabel->setText(QString());
    m_previewLabel->setToolTip(path);
    rescalePreview();
}

// Scaled from the cached full-size image, never re-decoded per frame.
void MainWindow::rescalePreview() {
    if (m_rescalingPreview || m_previewPixmap.isNull()) return;
    m_rescalingPreview = true;
    m_previewLabel->setPixmap(m_previewPixmap.scaled(m_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_rescalingPreview = false;
}

void MainWindow::updatePreviewSummary() {
    if (!m_hasPreviewContent) {
        m_previewContentLabel->setText(QStringLiteral("暂无预览内容"));
        m_previewContentLabel->setToolTip(QString());
        return;
    }
    const QString poemTitle = m_previewPoem.title.isEmpty() ? m_previewPoem.content.left(20) : m_previewPoem.title;
    const QString imageName = m_previewImage.path.isEmpty() ? QStringLiteral("未知图片") : QFileInfo(m_previewImage.path).fileName();
    QStringList parts;
    parts << poemTitle;
    if (!m_previewPoem.author.isEmpty()) parts << m_previewPoem.author;
    if (m_previewPoem.isPaired()) {
        parts << QStringLiteral("↔");
        parts << (m_previewPoem.secondTitle.isEmpty() ? m_previewPoem.secondContent.left(12) : m_previewPoem.secondTitle);
        if (!m_previewPoem.secondAuthor.isEmpty()) parts << m_previewPoem.secondAuthor;
    }
    parts << imageName;
    m_previewContentLabel->setText(QStringLiteral("当前预览：%1").arg(parts.join(QStringLiteral(" · "))));
    m_previewContentLabel->setToolTip(m_previewImage.path);
}

void MainWindow::updateScheduleTimer() {
    if (!m_config.scheduleEnabled) {
        m_timer->stop();
        m_scheduleNextLabel->setText(QStringLiteral("定时已关闭"));
        return;
    }
    const int interval = qBound(1, m_config.intervalMinutes, 1440);
    m_timer->start(interval * 60 * 1000);
    const QDateTime next = QDateTime::currentDateTime().addSecs(interval * 60);
    m_scheduleNextLabel->setText(next.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
    m_scheduleSummaryLabel->setText(QStringLiteral("每 %1 分钟 · 下次 %2").arg(interval).arg(next.toString(QStringLiteral("HH:mm"))));
}

void MainWindow::updateHistoryUi() {
    if (!m_historyTable || !m_historyFilter) return;

    QString selectedPoemId;
    QString selectedImagePath;
    const int oldRow = m_historyTable->currentRow();
    if (oldRow >= 0 && oldRow < m_visibleHistory.size()) {
        selectedPoemId = m_visibleHistory.at(oldRow).poemId;
        selectedImagePath = m_visibleHistory.at(oldRow).imagePath;
    }

    const bool favoritesOnly = m_historyFilter->currentData().toString() == QStringLiteral("favorites");
    m_visibleHistory = favoritesOnly ? m_config.favorites : m_config.history;

    {
        const QSignalBlocker blocker(m_historyTable);
        m_historyTable->setRowCount(0);
        for (int row = 0; row < m_visibleHistory.size(); ++row) {
            const WallpaperRecord &record = m_visibleHistory.at(row);
            m_historyTable->insertRow(row);

            const bool favorite = isFavoriteRecord(m_config.favorites, record);
            auto *star = new QTableWidgetItem(favorite ? QStringLiteral("★") : QStringLiteral("☆"));
            star->setTextAlignment(Qt::AlignCenter);
            star->setForeground(QColor(favorite ? QStringLiteral("#d97706") : QStringLiteral("#94a3b8")));
            m_historyTable->setItem(row, 0, star);

            auto *poemItem = new QTableWidgetItem(record.poemTitle.isEmpty()
                ? QStringLiteral("（未命名诗词）") : record.poemTitle);
            poemItem->setToolTip(record.poemId);
            m_historyTable->setItem(row, 1, poemItem);

            const bool imageExists = QFileInfo::exists(record.imagePath);
            const QString imageName = record.imagePath.isEmpty()
                ? QStringLiteral("（底图已移除）") : QFileInfo(record.imagePath).fileName();
            auto *imageItem = new QTableWidgetItem(imageExists ? imageName
                : QStringLiteral("%1（缺失）").arg(imageName));
            imageItem->setToolTip(record.imagePath);
            if (!imageExists) imageItem->setForeground(QColor(QStringLiteral("#b42318")));
            m_historyTable->setItem(row, 2, imageItem);

            auto *timeItem = new QTableWidgetItem(record.appliedAt.isValid()
                ? record.appliedAt.toString(QStringLiteral("MM-dd HH:mm")) : QStringLiteral("—"));
            timeItem->setForeground(QColor(QStringLiteral("#64748b")));
            m_historyTable->setItem(row, 3, timeItem);
        }
    }

    if (m_visibleHistory.isEmpty()) {
        m_historySummaryLabel->setText(favoritesOnly
            ? QStringLiteral("还没有收藏。应用一张壁纸后可在历史里收藏。")
            : QStringLiteral("还没有历史。成功应用一张壁纸后会出现在这里。"));
    } else {
        m_historySummaryLabel->setText(favoritesOnly
            ? QStringLiteral("收藏 %1 条").arg(m_config.favorites.size())
            : QStringLiteral("最近 %1 条 · 收藏 %2 条").arg(m_config.history.size()).arg(m_config.favorites.size()));
    }

    int selectRow = -1;
    if (!selectedPoemId.isEmpty()) {
        for (int row = 0; row < m_visibleHistory.size(); ++row) {
            const WallpaperRecord &record = m_visibleHistory.at(row);
            if (record.poemId == selectedPoemId
                && normalizedPath(record.imagePath) == normalizedPath(selectedImagePath)) {
                selectRow = row;
                break;
            }
        }
    }
    if (selectRow < 0 && !m_visibleHistory.isEmpty()) selectRow = 0;
    if (selectRow >= 0) m_historyTable->selectRow(selectRow);
}

QVector<ImageItem> MainWindow::availableImages() const {
    return m_imageLibrary.availableItems(m_config.excludedImages);
}

void MainWindow::rememberWallpaper(const Poem &poem, const ImageItem &image, const QString &path) {
    if (path.isEmpty() || poem.id.isEmpty() || image.path.isEmpty()) return;
    WallpaperRecord record{poem.id, poem.title, image.path, path, QDateTime::currentDateTime()};
    for (int i = 0; i < m_config.history.size(); ++i) {
        if (sameWallpaperPair(m_config.history.at(i), record)) {
            m_config.history.removeAt(i);
            break;
        }
    }
    m_config.history.prepend(record);
    while (m_config.history.size() > 20) m_config.history.removeLast();
    for (WallpaperRecord &favorite : m_config.favorites) {
        if (sameWallpaperPair(favorite, record)) {
            favorite.poemTitle = record.poemTitle;
            favorite.wallpaperPath = record.wallpaperPath;
            favorite.appliedAt = record.appliedAt;
            break;
        }
    }
}

bool MainWindow::selectedHistoryRecord(WallpaperRecord *record, bool *favorite) const {
    if (favorite) *favorite = false;
    if (!m_historyTable) return false;
    const int row = m_historyTable->currentRow();
    if (row < 0 || row >= m_visibleHistory.size()) return false;
    const WallpaperRecord &selected = m_visibleHistory.at(row);
    if (record) *record = selected;
    if (favorite) *favorite = isFavoriteRecord(m_config.favorites, selected);
    return true;
}

void MainWindow::historyFilterChanged() {
    if (m_historyTable) m_historyTable->clearSelection();
    updateHistoryUi();
    updateStatusUi();
}

void MainWindow::historySelectionChanged() {
    updateStatusUi();
}

void MainWindow::toggleSelectedFavorite() {
    WallpaperRecord record;
    bool favorite = false;
    if (!selectedHistoryRecord(&record, &favorite)) return;
    if (favorite) {
        for (int i = 0; i < m_config.favorites.size(); ++i) {
            if (sameWallpaperPair(m_config.favorites.at(i), record)) {
                m_config.favorites.removeAt(i);
                break;
            }
        }
        m_statusLabel->setText(QStringLiteral("已取消收藏"));
    } else {
        m_config.favorites.prepend(record);
        while (m_config.favorites.size() > 50) m_config.favorites.removeLast();
        m_statusLabel->setText(QStringLiteral("已收藏：%1").arg(record.poemTitle.isEmpty()
            ? QStringLiteral("未命名诗词") : record.poemTitle));
    }
    persistConfig();
    updateHistoryUi();
    updateStatusUi();
}

void MainWindow::excludeSelectedHistoryImage() {
    WallpaperRecord record;
    if (!selectedHistoryRecord(&record, nullptr) || record.imagePath.isEmpty()) return;
    if (isExcludedImage(m_config.excludedImages, record.imagePath)) {
        m_statusLabel->setText(QStringLiteral("这张底图已经在排除列表中"));
        return;
    }
    m_config.excludedImages.append(QDir::cleanPath(QFileInfo(record.imagePath).absoluteFilePath()));
    persistConfig();
    updateImageFilesUi();
    updateHistoryUi();
    updateStatusUi();
    m_statusLabel->setText(QStringLiteral("已从随机切换中排除底图：%1").arg(QFileInfo(record.imagePath).fileName()));
}

void MainWindow::applySelectedHistory() {
    WallpaperRecord record;
    if (!selectedHistoryRecord(&record, nullptr)) return;
    const Poem *poem = nullptr;
    for (const Poem &candidate : m_config.poems) {
        if (candidate.id == record.poemId && candidate.enabled && !candidate.content.trimmed().isEmpty()) {
            poem = &candidate;
            break;
        }
    }
    if (!poem) {
        m_statusLabel->setText(QStringLiteral("对应的诗词已被删除或停用，无法重新应用"));
        return;
    }
    if (!QFileInfo::exists(record.imagePath)) {
        m_statusLabel->setText(QStringLiteral("对应的底图已不存在，无法重新应用"));
        return;
    }
    const ImageItem image{record.imagePath, QFileInfo(record.imagePath).size(),
                          QFileInfo(record.imagePath).lastModified(), true, {}};
    startRenderedApply(*poem, image);
}

void MainWindow::toggleSelectedImageExclusion() {
    if (m_selectedImagePath.isEmpty() || !QFileInfo::exists(m_selectedImagePath)) return;
    int existing = -1;
    for (int i = 0; i < m_config.excludedImages.size(); ++i) {
        if (normalizedPath(m_config.excludedImages.at(i)) == normalizedPath(m_selectedImagePath)) {
            existing = i;
            break;
        }
    }
    if (existing >= 0) {
        m_config.excludedImages.removeAt(existing);
        m_statusLabel->setText(QStringLiteral("已恢复这张图片参与随机切换"));
    } else {
        m_config.excludedImages.append(QDir::cleanPath(QFileInfo(m_selectedImagePath).absoluteFilePath()));
        m_statusLabel->setText(QStringLiteral("已排除这张图片，不会再随机使用"));
    }
    persistConfig();
    updateImageFilesUi();
    updateHistoryUi();
    updateStatusUi();
}

void MainWindow::startImageScan() {
    if (m_scanWatcher.isRunning()) {
        // Onboarding or a source edit can land while the startup scan is still
        // walking the old source list; scan again when it finishes.
        m_scanPending = true;
        return;
    }
    m_imageScanLabel->setText(QStringLiteral("正在扫描，新文件将在完成后更新…"));
    const QVector<ImageSource> sources = m_config.imageSources;
    m_scanWatcher.setFuture(QtConcurrent::run([sources] { return ImageLibrary::scan(sources); }));
}

void MainWindow::scanFinished() {
    const ImageLibrary::ScanResult result = m_scanWatcher.result();
    m_imageLibrary.setItems(result.items);
    if (!m_selectedImagePath.isEmpty() && !QFileInfo::exists(m_selectedImagePath)) m_selectedImagePath.clear();
    updateImageUi();
    updateImageFilesUi();
    hydratePreviewState();
    m_imageScanLabel->setText(QStringLiteral("扫描完成：有效图片 %1 张，跳过 %2 张").arg(result.items.size()).arg(result.skipped));
    if (!result.errors.isEmpty()) appendLog(QStringLiteral("WARN"), result.errors.mid(0, 20).join(QStringLiteral("；")));
    if (m_scanPending) {
        m_scanPending = false;
        startImageScan();
        updateStatusUi();
        return;
    }
    if (m_pendingFirstWallpaper) {
        m_pendingFirstWallpaper = false;
        if (availableImages().isEmpty()) {
            m_statusLabel->setText(QStringLiteral("还没有可用图片，添加图片目录后会继续生成第一张壁纸"));
        } else {
            randomSwitchWallpaper();
            return;
        }
    }
    updateStatusUi();
}
void MainWindow::addImageDirectory() {
    const QString path = QFileDialog::getExistingDirectory(this, QStringLiteral("选择图片目录"));
    if (path.isEmpty()) return;
    const QString normalized = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    for (const ImageSource &source : m_config.imageSources) {
        if (QDir::cleanPath(QFileInfo(source.path).absoluteFilePath()) == normalized) {
            m_statusLabel->setText(QStringLiteral("该图片目录已经存在"));
            return;
        }
    }
    m_config.imageSources.append(ImageSource{normalized, true, true, false});
    persistConfig();
    updateImageUi();
    startImageScan();
}

void MainWindow::removeImageDirectory() {
    const int row = m_imageDirectories->currentRow();
    if (row < 0 || row >= m_config.imageSources.size()) return;
    m_config.imageSources.removeAt(row);
    persistConfig();
    startImageScan();
}

void MainWindow::imageDirectoryItemChanged(QListWidgetItem *item) {
    if (m_ignoreImageChanges || !item) return;
    const int row = m_imageDirectories->row(item);
    if (row < 0 || row >= m_config.imageSources.size()) return;
    m_config.imageSources[row].enabled = item->checkState() == Qt::Checked;
    persistConfig();
    startImageScan();
}

void MainWindow::rescanImages() { startImageScan(); }

void MainWindow::selectImageFile() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("选择图片"), QString(), imageFilter());
    if (path.isEmpty()) return;
    QImageReader reader(path);
    reader.setAutoTransform(true);
    if (!reader.canRead() || reader.read().isNull()) {
        m_statusLabel->setText(QStringLiteral("图片无法读取，请选择其他图片"));
        return;
    }
    const QString normalized = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    bool exists = false;
    for (const ImageSource &source : m_config.imageSources) {
        if (source.path == normalized) { exists = true; break; }
    }
    if (!exists) {
        m_config.imageSources.append(ImageSource{normalized, true, false, true});
        persistConfig();
        startImageScan();
    } else {
        m_selectedImagePath = normalized;
        updateImageFilesUi();
        updateStatusUi();
    }
    m_statusLabel->setText(QStringLiteral("\u5df2\u6dfb\u52a0\u56fe\u7247\uff1a%1").arg(QFileInfo(normalized).fileName()));
}

void MainWindow::imageFileSelectionChanged() {
    const auto *item = m_imageFiles->currentItem();
    m_selectedImagePath = item ? item->data(Qt::UserRole).toString() : QString();
    updateSelectedImageInfo();
    updateStatusUi();
}

void MainWindow::updateSelectedImageInfo() {
    bool ok = false;
    const ImageItem image = selectedImage(&ok);
    if (!ok) {
        m_selectedImageInfoLabel->setText(QStringLiteral("请选择一张图片"));
        return;
    }
    const QFileInfo info(image.path);
    QImageReader reader(image.path);
    const QSize size = reader.size();
    m_selectedImageInfoLabel->setText(QStringLiteral("当前选中：%1 · %2 × %3 · %4 KB")
        .arg(info.fileName()).arg(size.width()).arg(size.height()).arg(info.size() / 1024));
    m_selectedImageInfoLabel->setToolTip(image.path);
}

void MainWindow::useSelectedImagePreview() {
    bool imageOk = false;
    const ImageItem image = selectedImage(&imageOk);
    bool poemOk = false;
    const Poem poem = selectedOrCurrentPoem(&poemOk);
    if (!imageOk || !poemOk) {
        m_statusLabel->setText(QStringLiteral("请先选择有效图片和可用诗词"));
        return;
    }
    requestPreviewFor(poem, image);
}

void MainWindow::setSelectedImageWallpaper() {
    bool ok = false;
    const ImageItem image = selectedImage(&ok);
    if (!ok) {
        m_statusLabel->setText(QStringLiteral("请先选择一张有效图片"));
        return;
    }
    startApply(image.path, Poem{}, image, ApplyKind::OriginalImage);
}

bool MainWindow::editPoemDialog(Poem &poem, bool creating) {
    QDialog dialog(this);
    dialog.setWindowTitle(creating ? QStringLiteral("新增诗词") : QStringLiteral("编辑诗词"));
    dialog.setMinimumSize(620, 640);
    auto *layout = new QVBoxLayout(&dialog);

    const auto isPairedChoice = [](const QComboBox *combo) { return !combo->currentData().toString().isEmpty(); };

    auto *form = new QFormLayout();
    auto *relationCombo = new QComboBox(&dialog);
    relationCombo->setObjectName(QStringLiteral("relationComboBox"));
    for (const RelationChoice &choice : kRelationChoices) {
        relationCombo->addItem(QString::fromUtf8(choice.label), QString::fromLatin1(choice.value));
    }
    relationCombo->setCurrentIndex(qMax(0, relationCombo->findData(poem.relation)));
    form->addRow(QStringLiteral("体裁"), relationCombo);
    layout->addLayout(form);

    auto *firstGroup = new QGroupBox(QStringLiteral("上首"), &dialog);
    auto *firstForm = new QFormLayout(firstGroup);
    auto *titleEdit = new QLineEdit(poem.title, firstGroup);
    auto *authorEdit = new QLineEdit(poem.author, firstGroup);
    auto *dynastyEdit = new QLineEdit(poem.dynasty, firstGroup);
    auto *contentEdit = new QTextEdit(poem.content, firstGroup);
    contentEdit->setMinimumHeight(150);
    contentEdit->setPlaceholderText(QStringLiteral("正文支持换行，每行会按壁纸安全区域自动换行。"));
    firstForm->addRow(QStringLiteral("标题"), titleEdit);
    firstForm->addRow(QStringLiteral("作者"), authorEdit);
    firstForm->addRow(QStringLiteral("朝代"), dynastyEdit);
    firstForm->addRow(QStringLiteral("正文"), contentEdit);
    layout->addWidget(firstGroup);

    auto *secondGroup = new QGroupBox(QStringLiteral("下首"), &dialog);
    auto *secondForm = new QFormLayout(secondGroup);
    auto *secondTitleEdit = new QLineEdit(poem.secondTitle, secondGroup);
    auto *secondAuthorEdit = new QLineEdit(poem.secondAuthor, secondGroup);
    auto *secondDynastyEdit = new QLineEdit(poem.secondDynasty, secondGroup);
    auto *secondContentEdit = new QTextEdit(poem.secondContent, secondGroup);
    secondContentEdit->setMinimumHeight(150);
    secondForm->addRow(QStringLiteral("标题"), secondTitleEdit);
    secondForm->addRow(QStringLiteral("作者"), secondAuthorEdit);
    secondForm->addRow(QStringLiteral("朝代"), secondDynastyEdit);
    secondForm->addRow(QStringLiteral("正文"), secondContentEdit);
    layout->addWidget(secondGroup);
    secondGroup->setEnabled(isPairedChoice(relationCombo));
    connect(relationCombo, &QComboBox::currentIndexChanged, &dialog,
            [relationCombo, secondGroup, isPairedChoice](int) {
                secondGroup->setEnabled(isPairedChoice(relationCombo));
            });

    auto *hint = new QLabel(QStringLiteral("建议填写标题、作者和朝代；正文不能为空。标题、作者／朝代、正文会使用不同字号和颜色。"
                                          "选酬答类体裁时，壁纸会分上下两块展示，并各自标出「赠／酬」一类关系标签。"), &dialog);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#5b6b7b;"));
    layout->addWidget(hint);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("保存"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("取消"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, relationCombo, contentEdit, secondContentEdit] {
        if (contentEdit->toPlainText().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("无法保存"), QStringLiteral("诗词正文不能为空。"));
            return;
        }
        if (!relationCombo->currentData().toString().isEmpty() && secondContentEdit->toPlainText().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("无法保存"), QStringLiteral("酬答类体裁需要填写下首正文，或把体裁改回「单首」。"));
            return;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return false;

    poem.relation = relationCombo->currentData().toString();
    poem.title = titleEdit->text().trimmed();
    poem.author = authorEdit->text().trimmed();
    poem.dynasty = dynastyEdit->text().trimmed();
    poem.content = contentEdit->toPlainText().trimmed();
    if (poem.relation.isEmpty()) {
        // Dropping back to 单首 must not leave a hidden second half behind.
        poem.secondTitle.clear();
        poem.secondAuthor.clear();
        poem.secondDynasty.clear();
        poem.secondContent.clear();
    } else {
        poem.secondTitle = secondTitleEdit->text().trimmed();
        poem.secondAuthor = secondAuthorEdit->text().trimmed();
        poem.secondDynasty = secondDynastyEdit->text().trimmed();
        poem.secondContent = secondContentEdit->toPlainText().trimmed();
    }
    if (poem.id.isEmpty()) poem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    poem.enabled = true;
    return true;
}

void MainWindow::addPoem() {
    Poem poem;
    if (!editPoemDialog(poem, true)) return;
    m_config.poems.append(poem);
    persistConfig();
    updatePoemUi();
    updateStatusUi();
    m_statusLabel->setText(QStringLiteral("\u5df2\u6dfb\u52a0\u8bd7\u8bcd\uff1a%1").arg(poem.title.isEmpty() ? QStringLiteral("\u672a\u547d\u540d") : poem.title));
}

void MainWindow::editSelectedPoem() {
    const int row = m_poemTable->currentRow();
    if (row < 0 || row >= m_config.poems.size()) {
        m_statusLabel->setText(QStringLiteral("\u8bf7\u5148\u9009\u62e9\u8981\u7f16\u8f91\u7684\u8bd7\u8bcd\u3002"));
        return;
    }
    Poem poem = m_config.poems.at(row);
    if (!editPoemDialog(poem, false)) return;
    m_config.poems[row] = poem;
    persistConfig();
    updatePoemUi();
    updateStatusUi();
    m_statusLabel->setText(QStringLiteral("\u5df2\u66f4\u65b0\u8bd7\u8bcd\uff1a%1").arg(poem.title.isEmpty() ? QStringLiteral("\u672a\u547d\u540d") : poem.title));
}

void MainWindow::importPoetry() {
    const QString path = QFileDialog::getOpenFileName(this, QStringLiteral("导入诗词 JSON"), QString(), QStringLiteral("JSON 文件 (*.json)"));
    if (path.isEmpty()) return;
    PoetryLibrary library = PoetryLibrary::fromPoems(m_config.poems);
    const ImportResult result = library.importJson(path, true);
    if (!result.success) {
        const QString message = QStringLiteral("诗词导入失败：%1").arg(result.errors.isEmpty() ? QStringLiteral("JSON 无效") : result.errors.first());
        m_statusLabel->setText(message);
        appendLog(QStringLiteral("ERROR"), message);
        return;
    }
    m_config.poems = library.all();
    m_config.poetryLibraryPath = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    persistConfig();
    updatePoemUi();
    updateStatusUi();
    QString message = QStringLiteral("导入完成：有效 %1，跳过 %2").arg(result.imported).arg(result.skipped);
    if (!result.errors.isEmpty()) message += QStringLiteral("；%1").arg(result.errors.mid(0, 3).join(QStringLiteral("；")));
    m_statusLabel->setText(message);
    appendLog(result.errors.isEmpty() ? QStringLiteral("INFO") : QStringLiteral("WARN"), message);
}

void MainWindow::deleteSelectedPoem() {
    const int row = m_poemTable->currentRow();
    if (row < 0 || row >= m_config.poems.size()) return;
    m_config.poems.removeAt(row);
    persistConfig();
    updatePoemUi();
    updateStatusUi();
}

void MainWindow::poemItemChanged(QTableWidgetItem *item) {
    if (m_ignorePoemChanges || !item || item->column() != 0) return;
    const QString id = item->data(Qt::UserRole).toString();
    PoetryLibrary library = PoetryLibrary::fromPoems(m_config.poems);
    if (library.setEnabled(id, item->checkState() == Qt::Checked)) {
        m_config.poems = library.all();
        persistConfig();
        updatePoemUi();
        updateStatusUi();
    }
}

void MainWindow::useSelectedPoemPreview() {
    bool poemOk = false;
    const Poem poem = selectedOrCurrentPoem(&poemOk);
    bool imageOk = false;
    const ImageItem image = selectedImage(&imageOk);
    if (!poemOk || !imageOk) {
        m_statusLabel->setText(QStringLiteral("请先选择可用诗词和图片"));
        return;
    }
    requestPreviewFor(poem, image);
}

void MainWindow::randomSwitchWallpaper() {
    if (m_desktopBusy || m_previewBusy) return;
    const QVector<Poem> poems = enabledPoems();
    const QVector<ImageItem> images = availableImages();
    if (poems.isEmpty() || images.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("缺少可用图片或诗词，无法随机换一张壁纸"));
        updateStatusUi();
        return;
    }

    struct FavoritePair { Poem poem; ImageItem image; };
    QVector<FavoritePair> playableFavorites;
    for (const WallpaperRecord &favorite : m_config.favorites) {
        const Poem *poem = nullptr;
        for (const Poem &candidate : poems) {
            if (candidate.id == favorite.poemId) { poem = &candidate; break; }
        }
        if (!poem) continue;
        const ImageItem *image = nullptr;
        for (const ImageItem &candidate : images) {
            if (normalizedPath(candidate.path) == normalizedPath(favorite.imagePath)) { image = &candidate; break; }
        }
        if (image) playableFavorites.append(FavoritePair{*poem, *image});
    }

    // A quarter of the draws replay a starred pairing. This keeps favourites in
    // rotation without turning the app into a fixed playlist.
    const bool useFavorite = !playableFavorites.isEmpty() && m_picker.pickIndex(4, -1) == 0;
    const int poemIndex = useFavorite ? -1 : m_picker.pickPoem(poems);
    const int imageIndex = useFavorite ? -1 : m_picker.pickImage(images);
    if ((!useFavorite && (poemIndex < 0 || imageIndex < 0)) || (useFavorite && playableFavorites.isEmpty())) {
        m_statusLabel->setText(QStringLiteral("随机抽取失败"));
        updateStatusUi();
        return;
    }
    const FavoritePair chosen = useFavorite
        ? playableFavorites.at(qMax(0, m_picker.pickIndex(playableFavorites.size(), -1)))
        : FavoritePair{poems.at(poemIndex), images.at(imageIndex)};
    // Picking happens here, not in the worker. Drawing from a copy of m_picker
    // left the original frozen on its seed, so every click replayed the same
    // number and, combined with the avoid-consecutive rule, alternated between
    // the same two images forever.
    m_picker.rememberPoem(chosen.poem.id);
    m_picker.rememberImage(chosen.image.path);
    startRenderedApply(chosen.poem, chosen.image);
}

void MainWindow::startRenderedApply(const Poem &poem, const ImageItem &image) {
    if (m_desktopBusy || m_previewBusy) return;
    m_desktopBusy = true;
    m_switchStartedGeneration = m_previewGeneration;
    m_statusLabel->setText(QStringLiteral("正在生成并设置桌面…"));
    const RenderSettings settings = m_config.render;
    const QSize target = targetWallpaperSize();
    const QString cache = m_store.wallpaperCachePath();
    QStringList protectedPaths;
    for (const QString &path : {m_config.runtime.lastSuccessfulWallpaper,
                                m_config.runtime.currentPreviewWallpaper,
                                m_previewPath}) {
        if (!path.isEmpty() && !protectedPaths.contains(path)) protectedPaths.append(path);
    }
    m_switchWatcher.setFuture(QtConcurrent::run([poem, image, settings, target, cache, protectedPaths] {
        WindowsWallpaperSetter setter;
        return WallpaperService::renderAndApply(poem, image, settings, target, cache, setter, protectedPaths);
    }));
    updateStatusUi();
}

void MainWindow::applyCurrentPreview() {
    if (m_desktopBusy || m_previewBusy || m_previewPending || !m_hasPreviewContent || m_previewPath.isEmpty()) return;
    startApply(m_previewPath, m_previewPoem, m_previewImage, ApplyKind::Preview);
}

void MainWindow::refreshPreview() {
    if (m_previewBusy || m_desktopBusy) return;
    ++m_previewGeneration;
    m_previewPending = true;
    m_previewApplied = false;
    m_config.runtime.currentPreviewApplied = false;
    persistConfig();
    m_previewDebounce->start();
    m_statusLabel->setText(QStringLiteral("准备更新预览…"));
    updateStatusUi();
}

void MainWindow::requestPreviewFor(const Poem &poem, const ImageItem &image) {
    m_pendingPreviewPoem = poem;
    m_pendingPreviewImage = image;
    m_hasPendingPreviewSelection = true;
    ++m_previewGeneration;
    m_previewPending = true;
    m_previewApplied = false;
    m_config.runtime.currentPreviewApplied = false;
    persistConfig();
    m_previewDebounce->start();
    m_statusLabel->setText(QStringLiteral("准备更新预览…"));
    updateStatusUi();
}

void MainWindow::schedulePreviewRefresh() {
    if (!m_hasPreviewContent && m_imageFiles->currentRow() < 0) return;
    ++m_previewGeneration;
    m_previewPending = true;
    m_previewApplied = false;
    m_config.runtime.currentPreviewApplied = false;
    persistConfig();
    m_previewApplied = false;
    m_previewDebounce->start();
    updateStatusUi();
}

void MainWindow::previewDebounceTimeout() { startPreviewRender(); }

void MainWindow::startPreviewRender() {
    if (m_previewBusy) return;
    Poem poem;
    ImageItem image;
    if (m_hasPendingPreviewSelection) {
        poem = m_pendingPreviewPoem;
        image = m_pendingPreviewImage;
    } else if (m_hasPreviewContent && !m_previewPoem.content.isEmpty() && !m_previewImage.path.isEmpty()) {
        poem = m_previewPoem;
        image = m_previewImage;
    } else {
        bool poemOk = false;
        bool imageOk = false;
        poem = selectedOrCurrentPoem(&poemOk);
        image = selectedImage(&imageOk);
        if (!poemOk || !imageOk) {
            m_previewPending = false;
            m_statusLabel->setText(QStringLiteral("请先添加图片并导入可用诗词"));
            updateStatusUi();
            return;
        }
    }
    m_previewPending = false;
    m_previewBusy = true;
    m_previewStartedGeneration = m_previewGeneration;
    const RenderSettings settings = m_config.render;
    const QSize target = targetWallpaperSize();
    const QString cache = m_store.wallpaperCachePath();
    m_statusLabel->setText(QStringLiteral("正在更新预览…"));
    m_previewWatcher.setFuture(QtConcurrent::run([poem, image, settings, target, cache] {
        return WallpaperService::renderPreview(poem, image, settings, target, cache);
    }));
    updateStatusUi();
}

void MainWindow::previewFinished() {
    m_previewBusy = false;
    const SwitchResult result = m_previewWatcher.result();
    if (m_previewStartedGeneration != m_previewGeneration) {
        m_previewPending = true;
        m_previewDebounce->start();
        updateStatusUi();
        return;
    }
    if (result.success) {
        setPreviewState(result, false);
        m_hasPendingPreviewSelection = false;
        m_statusLabel->setText(QStringLiteral("预览已更新，尚未应用到桌面"));
    } else {
        m_previewPending = false;
        m_statusLabel->setText(QStringLiteral("预览更新失败：%1").arg(result.message));
        appendLog(QStringLiteral("ERROR"), result.message);
    }
    updateAllUi();
}
void MainWindow::switchFinished() {
    m_desktopBusy = false;
    const SwitchResult result = m_switchWatcher.result();
    if (result.success) {
        setPreviewState(result, true);
        m_statusLabel->setText(result.message);
        appendLog(QStringLiteral("INFO"), result.message);
        if (m_switchStartedGeneration != m_previewGeneration) {
            m_previewApplied = false;
            schedulePreviewRefresh();
        }
    } else {
        m_statusLabel->setText(QStringLiteral("切换失败：%1").arg(result.message));
        appendLog(QStringLiteral("ERROR"), result.message);
    }
    updateAllUi();
    if (m_config.scheduleEnabled) updateScheduleTimer();
}

void MainWindow::startApply(const QString &path, const Poem &poem, const ImageItem &image, ApplyKind kind) {
    if (m_desktopBusy) return;
    m_desktopBusy = true;
    m_applyKind = kind;
    m_statusLabel->setText(kind == ApplyKind::Preview ? QStringLiteral("正在应用当前预览…") : QStringLiteral("正在将原始图片设置为桌面…"));
    const QString cache = m_store.wallpaperCachePath();
    QStringList protectedPaths;
    for (const QString &protectedPath : {m_config.runtime.lastSuccessfulWallpaper,
                                         m_config.runtime.currentPreviewWallpaper,
                                         m_previewPath}) {
        if (!protectedPath.isEmpty() && !protectedPaths.contains(protectedPath)) protectedPaths.append(protectedPath);
    }
    m_applyWatcher.setFuture(QtConcurrent::run([path, poem, image, cache, protectedPaths] {
        WindowsWallpaperSetter setter;
        return WallpaperService::applyExisting(path, poem, image, setter, cache, protectedPaths);
    }));
    updateStatusUi();
}

void MainWindow::applyFinished() {
    m_desktopBusy = false;
    const SwitchResult result = m_applyWatcher.result();
    if (result.success) {
        if (m_applyKind == ApplyKind::Preview) {
            m_previewApplied = true;
            m_config.runtime.currentPreviewWallpaper = m_previewPath;
            m_config.runtime.currentPreviewPoemId = m_previewPoem.id;
            m_config.runtime.currentPreviewImagePath = m_previewImage.path;
            m_config.runtime.currentPreviewApplied = true;
            m_config.runtime.lastSuccessfulWallpaper = m_previewPath;
            m_config.runtime.lastPoemId = m_previewPoem.id;
            m_config.runtime.lastImagePath = m_previewImage.path;
            m_config.runtime.lastSuccessTime = QDateTime::currentDateTime();
            rememberWallpaper(m_previewPoem, m_previewImage, m_previewPath);
            m_statusLabel->setText(QStringLiteral("已应用到桌面"));
        } else {
            m_previewApplied = false;
            m_config.runtime.currentPreviewApplied = false;
            m_statusLabel->setText(QStringLiteral("原始图片已设置为桌面（未生成诗词壁纸）"));
        }
        appendLog(QStringLiteral("INFO"), m_statusLabel->text());
        persistConfig();
    } else {
        m_statusLabel->setText(result.message);
        appendLog(QStringLiteral("ERROR"), result.message);
    }
    updateAllUi();
}

void MainWindow::setPreviewState(const SwitchResult &result, bool applied) {
    m_previewPath = result.outputPath;
    m_previewPoem = result.poem;
    m_previewImage = result.image;
    m_selectedImagePath = result.image.path;
    m_hasPreviewContent = !m_previewPath.isEmpty();
    m_previewApplied = applied;
    m_config.runtime.currentPreviewWallpaper = m_previewPath;
    m_config.runtime.currentPreviewPoemId = m_previewPoem.id;
    m_config.runtime.currentPreviewImagePath = m_previewImage.path;
    m_config.runtime.currentPreviewApplied = applied;
    if (applied) {
        m_config.runtime.lastSuccessfulWallpaper = m_previewPath;
        m_config.runtime.lastPoemId = m_previewPoem.id;
        m_config.runtime.lastImagePath = m_previewImage.path;
        m_config.runtime.lastSuccessTime = QDateTime::currentDateTime();
        rememberWallpaper(m_previewPoem, m_previewImage, m_previewPath);
    }
    persistConfig();
    updatePreview(m_previewPath);
}

void MainWindow::timerTimeout() { randomSwitchWallpaper(); }

void MainWindow::chooseTextColor() {
    const QColor current = m_config.render.contentColor.isValid() ? m_config.render.contentColor : m_config.render.textColor;
    const QColor color = QColorDialog::getColor(current, this, QStringLiteral("\u9009\u62e9\u6b63\u6587\u989c\u8272"), QColorDialog::ShowAlphaChannel);
    if (!color.isValid()) return;
    m_config.render.contentColor = color;
    m_config.render.textColor = color;
    updateSettingsUi();
    displaySettingsChanged();
}

void MainWindow::chooseTitleColor() {
    const QColor color = QColorDialog::getColor(m_config.render.titleColor, this, QStringLiteral("\u9009\u62e9\u6807\u9898\u989c\u8272"), QColorDialog::ShowAlphaChannel);
    if (!color.isValid()) return;
    m_config.render.titleColor = color;
    updateSettingsUi();
    displaySettingsChanged();
}

void MainWindow::chooseMetadataColor() {
    const QColor color = QColorDialog::getColor(m_config.render.metadataColor, this, QStringLiteral("\u9009\u62e9\u4f5c\u8005/\u671d\u4ee3\u989c\u8272"), QColorDialog::ShowAlphaChannel);
    if (!color.isValid()) return;
    m_config.render.metadataColor = color;
    updateSettingsUi();
    displaySettingsChanged();
}

void MainWindow::chooseDynastyColor() {
    const QColor color = QColorDialog::getColor(m_config.render.dynastyColor, this, QStringLiteral("选择朝代颜色"), QColorDialog::ShowAlphaChannel);
    if (!color.isValid()) return;
    m_config.render.dynastyColor = color;
    updateSettingsUi();
    displaySettingsChanged();
}

void MainWindow::typographyPresetChanged(int index) {
    if (index == 0) {
        m_config.render.titleScale = 1.50;
        m_config.render.authorScale = 0.72;
        m_config.render.dynastyScale = 0.68;
        m_config.render.titleWeight = 600;
        m_config.render.contentLineSpacing = 1.35;
        m_config.render.titleMetadataSpacing = 12;
        m_config.render.metadataContentSpacing = 24;
        m_config.render.metadataInlineSpacing = 10;
    } else if (index == 1) {
        m_config.render.titleScale = 1.35;
        m_config.render.authorScale = 0.78;
        m_config.render.dynastyScale = 0.74;
        m_config.render.titleWeight = 500;
        m_config.render.contentLineSpacing = 1.25;
        m_config.render.titleMetadataSpacing = 8;
        m_config.render.metadataContentSpacing = 16;
        m_config.render.metadataInlineSpacing = 8;
    }
    if (index != 2) {
        updateSettingsUi();
        displaySettingsChanged();
    }
}

void MainWindow::choosePanelColor() {
    const QColor color = QColorDialog::getColor(m_config.render.panelColor, this, QStringLiteral("\u9009\u62e9\u5e95\u677f\u989c\u8272"), QColorDialog::ShowAlphaChannel);
    if (!color.isValid()) return;
    m_config.render.panelColor = color;
    updateSettingsUi();
    displaySettingsChanged();
}

void MainWindow::displaySettingsChanged() {
    if (!m_fontCombo) return;
    m_config.render.fontFamily = m_fontCombo->currentFont().family();
    m_config.render.fontPointSize = m_fontSizeSpin->value();
    const QColor contentColor(m_textColorButton->text());
    const QColor titleColor(m_titleColorButton->text());
    const QColor metadataColor(m_metadataColorButton->text());
    const QColor panelColor(m_panelColorButton->text());
    if (contentColor.isValid()) {
        m_config.render.contentColor = contentColor;
        m_config.render.textColor = contentColor;
    }
    if (titleColor.isValid()) m_config.render.titleColor = titleColor;
    if (metadataColor.isValid()) m_config.render.metadataColor = metadataColor;
    const QColor dynastyColor(m_dynastyColorButton->text());
    if (dynastyColor.isValid()) m_config.render.dynastyColor = dynastyColor;
    if (panelColor.isValid()) m_config.render.panelColor = panelColor;
    m_config.render.panelEnabled = m_panelCheck->isChecked();
    m_config.render.shadowEnabled = m_shadowCheck->isChecked();
    m_config.render.relationTagEnabled = m_relationTagCheck->isChecked();
    m_config.render.pairSpacing = m_pairSpacingSpin->value();
    m_config.render.pairLayoutPanel = pairLayoutPanelFromString(m_pairLayoutPanelCombo->currentData().toString());
    m_config.render.anchor = anchorFromString(m_anchorCombo->currentData().toString());
    m_config.render.fitMode = fitModeFromString(m_fitCombo->currentData().toString());
    persistConfig();
    schedulePreviewRefresh();
}
void MainWindow::toggleSchedule(bool enabled) {
    m_config.scheduleEnabled = enabled;
    {
        const QSignalBlocker blocker(m_scheduleCheck);
        m_scheduleCheck->setChecked(enabled);
    }
    persistConfig();
    updateScheduleTimer();
    updateStatusUi();
}

void MainWindow::saveScheduleSettings() {
    m_config.intervalMinutes = m_intervalSpin->value();
    m_config.scheduleEnabled = m_scheduleCheck->isChecked();
    persistConfig();
    updateScheduleTimer();
    updateStatusUi();
}

bool MainWindow::addImageSource(const QString &path, bool isFile, bool rescan) {
    if (path.isEmpty()) return false;
    const QString normalized = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    if (!QFileInfo::exists(normalized)) return false;
    for (const ImageSource &source : m_config.imageSources) {
        if (source.isFile == isFile && normalizedPath(source.path) == normalizedPath(normalized)) {
            if (rescan) startImageScan();
            return true;
        }
    }
    m_config.imageSources.append(ImageSource{normalized, true, true, isFile});
    persistConfig();
    updateImageUi();
    if (rescan) startImageScan();
    return true;
}

bool MainWindow::addSampleImageSource(QString *error) {
    if (error) error->clear();
    const QString sampleDir = QDir(m_store.rootPath()).filePath(QStringLiteral("samples"));
    if (!QDir().mkpath(sampleDir)) {
        if (error) *error = QStringLiteral("无法创建示例图片目录");
        return false;
    }
    const QStringList names{
        QStringLiteral("ink-mountains.jpg"),
        QStringLiteral("moon-pines.jpg"),
        QStringLiteral("dusk-lake.jpg"),
    };
    for (const QString &name : names) {
        const QString target = QDir(sampleDir).filePath(name);
        const QFileInfo existing(target);
        if (existing.exists() && existing.size() > 0) continue;
        if (existing.exists()) QFile::remove(target);
        if (!QFile::copy(QStringLiteral(":/samples/%1").arg(name), target)) {
            if (error) *error = QStringLiteral("示例图片复制失败：%1").arg(name);
            return false;
        }
    }
    if (!addImageSource(sampleDir, false, false)) {
        if (error) *error = QStringLiteral("示例图片目录无法加入图片库");
        return false;
    }
    return true;
}

bool MainWindow::addCurrentWallpaperSource(QString *error) {
    if (error) error->clear();
    const QString current = currentDesktopWallpaperPath();
    if (current.isEmpty()) {
        if (error) *error = QStringLiteral("Windows 没有报告当前桌面壁纸");
        return false;
    }
    const QImage image(current);
    if (image.isNull()) {
        if (error) *error = QStringLiteral("当前桌面壁纸无法读取");
        return false;
    }
    const QString sampleDir = QDir(m_store.rootPath()).filePath(QStringLiteral("samples"));
    if (!QDir().mkpath(sampleDir)) {
        if (error) *error = QStringLiteral("无法创建应用数据目录");
        return false;
    }
    const QString target = QDir(sampleDir).filePath(QStringLiteral("current-wallpaper.jpg"));
    if (!image.save(target, "JPG", 92)) {
        if (error) *error = QStringLiteral("当前桌面壁纸复制失败");
        return false;
    }
    if (!addImageSource(target, true, false)) {
        if (error) *error = QStringLiteral("当前桌面壁纸无法加入图片库");
        return false;
    }
    return true;
}

void MainWindow::showOnboarding() {
    const bool firstRun = m_firstRun;
    QWizard wizard(this);
    wizard.setWindowTitle(QStringLiteral("首次使用引导"));
    wizard.setWizardStyle(QWizard::ModernStyle);
    wizard.setOption(QWizard::NoBackButtonOnStartPage, true);
    wizard.setButtonText(QWizard::NextButton, QStringLiteral("下一步"));
    wizard.setButtonText(QWizard::BackButton, QStringLiteral("上一步"));
    wizard.setButtonText(QWizard::FinishButton, QStringLiteral("开始使用"));
    wizard.setButtonText(QWizard::CancelButton, QStringLiteral("稍后再说"));
    wizard.addPage(new OnboardingIntroPage(&wizard));
    auto *imagePage = new OnboardingImagePage(&wizard);
    wizard.addPage(imagePage);
    if (wizard.exec() != QDialog::Accepted) {
        m_firstRun = false;
        return;
    }

    QStringList problems;
    bool ready = false;
    if (imagePage->wantsSamples()) {
        QString error;
        if (addSampleImageSource(&error)) ready = true;
        else problems << error;
    }
    if (imagePage->wantsCurrentWallpaper()) {
        QString error;
        if (addCurrentWallpaperSource(&error)) ready = true;
        else problems << error;
    }
    const QString folder = imagePage->folderPath();
    if (!folder.isEmpty()) {
        if (addImageSource(folder, false, false)) ready = true;
        else problems << QStringLiteral("文件夹无法访问：%1").arg(folder);
    }
    if (imagePage->wantsAutostart()) {
        toggleAutostart(true);
        if (!m_config.autostartEnabled) {
            problems << QStringLiteral("开机自启未能写入，可稍后在「定时与高级」中重试");
        }
    }

    m_firstRun = false;
    if (ready) {
        if (firstRun) m_pendingFirstWallpaper = true;
        startImageScan();
        if (firstRun) m_statusLabel->setText(QStringLiteral("正在准备第一张诗词壁纸…"));
    } else if (!ready) {
        m_statusLabel->setText(QStringLiteral("已跳过底图设置，可随时在「图片库」中添加图片"));
    }
    updateAllUi();
    if (!problems.isEmpty()) {
        appendLog(QStringLiteral("WARN"), problems.join(QStringLiteral("；")));
        m_statusLabel->setText(problems.join(QStringLiteral("；")));
    }
}

void MainWindow::toggleAutostart(bool enabled) {
    QString error;
    if (!WindowsAutoStart::setEnabled(enabled, &error)) {
        m_statusLabel->setText(QStringLiteral("开机自启设置失败：%1").arg(error));
        appendLog(QStringLiteral("ERROR"), m_statusLabel->text());
        const QSignalBlocker blocker(m_autostartCheck);
        m_autostartCheck->setChecked(m_config.autostartEnabled);
        return;
    }
    m_config.autostartEnabled = enabled;
    persistConfig();
    m_statusLabel->setText(enabled ? QStringLiteral("已开启开机自启，登录 Windows 后会静默驻留托盘")
                                   : QStringLiteral("已关闭开机自启"));
    appendLog(QStringLiteral("INFO"), m_statusLabel->text());
}

void MainWindow::exportPreviewImage() {
    if (!m_hasPreviewContent || !QFileInfo::exists(m_previewPath)) {
        m_statusLabel->setText(QStringLiteral("当前没有可导出的预览"));
        return;
    }
    QString directory = QStandardPaths::writableLocation(QStandardPaths::PicturesLocation);
    if (directory.isEmpty() || !QFileInfo(directory).isDir()) directory = QDir::homePath();
    const QString suggested = QDir(directory).filePath(QStringLiteral("诗词壁纸-%1.png")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd-HHmmss"))));
    QString target = QFileDialog::getSaveFileName(this, QStringLiteral("导出壁纸"), suggested,
        QStringLiteral("PNG 图片 (*.png);;JPEG 图片 (*.jpg *.jpeg)"));
    if (target.isEmpty()) return;
    const QString suffix = QFileInfo(target).suffix().toLower();
    if (suffix.isEmpty()) target += QStringLiteral(".png");
    else if (suffix != QStringLiteral("png") && suffix != QStringLiteral("jpg") && suffix != QStringLiteral("jpeg")) {
        m_statusLabel->setText(QStringLiteral("请选择 PNG 或 JPG 格式"));
        return;
    }
    QString error;
    if (!WallpaperService::exportImage(m_previewPath, target, &error)) {
        m_statusLabel->setText(QStringLiteral("导出失败：%1").arg(error));
        appendLog(QStringLiteral("ERROR"), m_statusLabel->text());
        return;
    }
    m_statusLabel->setText(QStringLiteral("已导出壁纸：%1").arg(QDir::toNativeSeparators(target)));
    appendLog(QStringLiteral("INFO"), m_statusLabel->text());
}

void MainWindow::copyPreviewImage() {
    if (!m_hasPreviewContent || !QFileInfo::exists(m_previewPath)) {
        m_statusLabel->setText(QStringLiteral("当前没有可复制的预览"));
        return;
    }
    QImage image = m_previewPixmap.isNull() ? QImage(m_previewPath) : m_previewPixmap.toImage();
    if (image.isNull()) {
        m_statusLabel->setText(QStringLiteral("预览图片无法读取"));
        return;
    }
    QApplication::clipboard()->setImage(image);
    m_statusLabel->setText(QStringLiteral("已复制当前壁纸到剪贴板"));
}

void MainWindow::clearCache() {
    QStringList protectedPaths;
    for (const QString &path : {m_config.runtime.lastSuccessfulWallpaper,
                                m_config.runtime.currentPreviewWallpaper,
                                m_previewPath}) {
        if (!path.isEmpty() && !protectedPaths.contains(path)) protectedPaths.append(path);
    }
    WallpaperService::cleanupCache(m_store.wallpaperCachePath(), protectedPaths, 0);
    m_statusLabel->setText(QStringLiteral("\u7f13\u5b58\u5df2\u6e05\u7406\uff0c\u5df2\u4fdd\u7559\u5f53\u524d\u684c\u9762\u58c1\u7eb8\u6587\u4ef6\u548c\u9884\u89c8\u6587\u4ef6"));
    appendLog(QStringLiteral("INFO"), m_statusLabel->text());
}

void MainWindow::openCacheDirectory() {
    QDir().mkpath(m_store.wallpaperCachePath());
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_store.wallpaperCachePath()));
}

void MainWindow::openLogDirectory() {
    QDir().mkpath(m_store.logPath());
    QDesktopServices::openUrl(QUrl::fromLocalFile(m_store.logPath()));
}

void MainWindow::resetDisplaySettings() {
    m_config.render = RenderSettings{};
    updateSettingsUi();
    displaySettingsChanged();
}

void MainWindow::showWindow() {
    showNormal();
    raise();
    activateWindow();
}

void MainWindow::quitFromTray() {
    allowQuit();
    qApp->quit();
}

void MainWindow::allowQuit() {
    m_allowQuit = true;
    if (m_timer) m_timer->stop();
    saveState();
}

void MainWindow::closeEvent(QCloseEvent *event) {
    if (!m_allowQuit && m_tray && m_tray->isVisible()) {
        hide();
        event->ignore();
        return;
    }
    saveState();
    event->accept();
}

// The layout sizes the label after the window's own resize event, and splitter
// drags never resize the window at all, so scaling follows the label instead.
bool MainWindow::eventFilter(QObject *watched, QEvent *event) {
    if (watched == m_previewLabel && event->type() == QEvent::Resize) rescalePreview();
    return QMainWindow::eventFilter(watched, event);
}

QSize MainWindow::targetWallpaperSize() const {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen || !screen->geometry().isValid()) return QSize(1920, 1080);
    // The full screen, not availableGeometry: the latter excludes the taskbar, so
    // composing to it produced e.g. 3840x2100 for a 3840x2160 panel and Windows
    // then cropped or letterboxed the result.
    const QRect geometry = screen->geometry();
    const qreal dpr = qMax<qreal>(1.0, screen->devicePixelRatio());
    return QSize(qMax(1, qRound(geometry.width() * dpr)), qMax(1, qRound(geometry.height() * dpr)));
}

QString MainWindow::colorButtonStyle(const QColor &color) const {
    return QStringLiteral("QPushButton { background-color: %1; color: %2; }")
        .arg(color.name(QColor::HexArgb), color.lightness() < 128 ? QStringLiteral("white") : QStringLiteral("black"));
}

QVector<Poem> MainWindow::enabledPoems() const {
    QVector<Poem> result;
    for (const Poem &poem : m_config.poems) if (poem.enabled && !poem.content.trimmed().isEmpty()) result.append(poem);
    return result;
}

Poem MainWindow::selectedOrCurrentPoem(bool *ok) const {
    if (ok) *ok = false;
    const int row = m_poemTable ? m_poemTable->currentRow() : -1;
    if (row >= 0 && row < m_config.poems.size() && !m_config.poems.at(row).content.trimmed().isEmpty()) {
        if (ok) *ok = true;
        return m_config.poems.at(row);
    }
    if (m_hasPreviewContent && !m_previewPoem.content.trimmed().isEmpty()) {
        if (ok) *ok = true;
        return m_previewPoem;
    }
    const QVector<Poem> poems = enabledPoems();
    if (!poems.isEmpty()) {
        if (ok) *ok = true;
        return poems.first();
    }
    return {};
}

ImageItem MainWindow::selectedImage(bool *ok) const {
    if (ok) *ok = false;
    if (m_selectedImagePath.isEmpty()) return {};
    for (const ImageItem &image : m_imageLibrary.validItems()) {
        if (image.path == m_selectedImagePath) {
            if (ok) *ok = true;
            return image;
        }
    }
    if (QFileInfo::exists(m_selectedImagePath)) {
        QImageReader reader(m_selectedImagePath);
        reader.setAutoTransform(true);
        if (reader.canRead()) {
            if (ok) *ok = true;
            const QFileInfo info(m_selectedImagePath);
            return ImageItem{m_selectedImagePath, info.size(), info.lastModified(), true, {}};
        }
    }
    return {};
}

void MainWindow::setTopStatus(const QString &text, const QColor &color) {
    m_desktopStateLabel->setText(QStringLiteral("● %1").arg(text));
    m_desktopStateLabel->setStyleSheet(QStringLiteral(
        "QLabel { padding: 5px 13px; border-radius: 13px; background: %1;"
        " color: #ffffff; font-size: 11pt; font-weight: 600; }").arg(color.name()));
}

void MainWindow::appendLog(const QString &level, const QString &message) {
    QDir().mkpath(m_store.logPath());
    const QString path = QDir(m_store.logPath()).filePath(QStringLiteral("app.log"));
    QFileInfo info(path);
    if (info.exists() && info.size() > 1024 * 1024) {
        QFile::remove(path + QStringLiteral(".1"));
        QFile::rename(path, path + QStringLiteral(".1"));
    }
    QFile file(path);
    if (!file.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream stream(&file);
    stream << QDateTime::currentDateTime().toString(Qt::ISODate) << " [" << level << "] " << message << Qt::endl;
}

} // namespace poetry

