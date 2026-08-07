
#include "main_window.h"

#include "../platform/windows_wallpaper_setter.h"

#include <QtConcurrent/QtConcurrentRun>
#include <QAbstractItemView>
#include <QApplication>
#include <QCheckBox>
#include <QColorDialog>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QHeaderView>
#include <QImageReader>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMessageBox>
#include <QPushButton>
#include <QResizeEvent>
#include <QScreen>
#include <QSignalBlocker>
#include <QSizePolicy>
#include <QSpinBox>
#include <QSplitter>
#include <QStyle>
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

namespace poetry {

namespace {
QWidget *makeTabPage(QTabWidget *tabs, const QString &title) {
    auto *page = new QWidget(tabs);
    page->setObjectName(title + QStringLiteral("Page"));
    tabs->addTab(page, title);
    return page;
}
QString imageFilter() { return QStringLiteral("图片 (*.png *.jpg *.jpeg *.bmp *.webp)"); }
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent), m_picker(static_cast<quint32>(QDateTime::currentMSecsSinceEpoch() & 0xffffffffu)) {
    setWindowTitle(QStringLiteral("诗词壁纸"));
    setMinimumSize(980, 680);
    resize(1280, 900);
    setupUi();
    setupTray();
    loadState();
    startImageScan();
    updateScheduleTimer();
    updateAllUi();
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
    // Readability baseline: 12 pt UI text, 36 px primary actions, and 8 px
    // spacing rhythm. These values remain legible on 100% DPI and scale with
    // the platform font on high-DPI screens.
    QFont uiFont = font();
    uiFont.setPointSize(12);
    setFont(uiFont);
    setStyleSheet(QStringLiteral(R"(
        QMainWindow, QWidget { color: #243244; }
        QGroupBox { border: 1px solid #d9e1ea; border-radius: 10px; margin-top: 12px; padding: 18px 12px 12px; font-size: 13pt; font-weight: 600; background: #ffffff; }
        QGroupBox::title { subcontrol-origin: margin; left: 14px; padding: 0 6px; color: #1c3854; }
        QPushButton, QToolButton { min-height: 36px; padding: 6px 14px; border: 1px solid #c7d3df; border-radius: 7px; background: #f8fafc; font-size: 12pt; }
        QPushButton:hover, QToolButton:hover { background: #eaf3fb; border-color: #78a9d2; }
        QPushButton:disabled { color: #9aa8b5; background: #eef2f5; }
        QComboBox, QFontComboBox, QSpinBox, QLineEdit { min-height: 34px; padding: 3px 8px; font-size: 12pt; }
        QListWidget, QTableWidget, QTextEdit { font-size: 12pt; }
        QListWidget::item { min-height: 32px; padding: 4px; }
        QTableWidget { gridline-color: #e2e8f0; alternate-background-color: #f7fafc; }
        QHeaderView::section { min-height: 34px; padding: 4px 8px; font-size: 11pt; font-weight: 600; background: #edf3f8; }
        QTabBar::tab { min-height: 38px; padding: 6px 18px; font-size: 12pt; }
        QLabel { font-size: 12pt; }
    )"));
    auto *root = new QVBoxLayout(central);
    root->setContentsMargins(18, 16, 18, 16);
    root->setSpacing(14);

    auto *topBar = new QHBoxLayout();
    auto *title = new QLabel(QStringLiteral("诗词壁纸"), central);
    title->setObjectName(QStringLiteral("appTitleLabel"));
    title->setStyleSheet(QStringLiteral("font-size:22pt;font-weight:700;color:#17324d;"));
    m_desktopStateLabel = new QLabel(QStringLiteral("尚未生成预览"), central);
    m_desktopStateLabel->setObjectName(QStringLiteral("desktopStateLabel"));
    m_countsLabel = new QLabel(central);
    m_countsLabel->setObjectName(QStringLiteral("resourceSummaryLabel"));
    m_scheduleSummaryLabel = new QLabel(central);
    m_scheduleSummaryLabel->setObjectName(QStringLiteral("scheduleSummaryLabel"));
    auto *moreButton = new QToolButton(central);
    moreButton->setObjectName(QStringLiteral("moreMenuButton"));
    moreButton->setText(QStringLiteral("更多 ▾"));
    moreButton->setPopupMode(QToolButton::InstantPopup);
    auto *moreMenu = new QMenu(moreButton);
    auto *openCacheAction = moreMenu->addAction(QStringLiteral("打开缓存目录"));
    auto *openLogAction = moreMenu->addAction(QStringLiteral("打开日志目录"));
    auto *clearCacheAction = moreMenu->addAction(QStringLiteral("清理缓存"));
    moreMenu->addSeparator();
    auto *quitAction = moreMenu->addAction(QStringLiteral("退出"));
    moreButton->setMenu(moreMenu);
    connect(openCacheAction, &QAction::triggered, this, &MainWindow::openCacheDirectory);
    connect(openLogAction, &QAction::triggered, this, &MainWindow::openLogDirectory);
    connect(clearCacheAction, &QAction::triggered, this, &MainWindow::clearCache);
    connect(quitAction, &QAction::triggered, this, &MainWindow::quitFromTray);
    topBar->addWidget(title);
    topBar->addSpacing(14);
    topBar->addWidget(m_desktopStateLabel);
    topBar->addStretch();
    topBar->addWidget(m_countsLabel);
    topBar->addSpacing(14);
    topBar->addWidget(m_scheduleSummaryLabel);
    topBar->addSpacing(8);
    topBar->addWidget(moreButton);
    root->addLayout(topBar);

    auto *mainSplitter = new QSplitter(Qt::Horizontal, central);
    mainSplitter->setObjectName(QStringLiteral("mainContentSplitter"));
    mainSplitter->setChildrenCollapsible(false);
    auto *previewGroup = new QGroupBox(QStringLiteral("壁纸预览"), mainSplitter);
    previewGroup->setMinimumWidth(560);
    auto *previewLayout = new QVBoxLayout(previewGroup);
    m_previewLabel = new QLabel(QStringLiteral("还没有可用预览\n请先添加图片目录并导入诗词库"), previewGroup);
    m_previewLabel->setObjectName(QStringLiteral("previewLabel"));
    m_previewLabel->setAlignment(Qt::AlignCenter);
    m_previewLabel->setMinimumSize(640, 400);
    m_previewLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_previewLabel->setStyleSheet(QStringLiteral("background:#202020;color:#bdbdbd;border:1px solid #505050;"));
    previewLayout->addWidget(m_previewLabel, 1);
    m_previewHintLabel = new QLabel(QStringLiteral("修改显示设置后会实时更新预览，不会自动修改桌面。"), previewGroup);
    m_previewHintLabel->setObjectName(QStringLiteral("previewHintLabel"));
    m_previewHintLabel->setWordWrap(true);
    previewLayout->addWidget(m_previewHintLabel);

    auto *quickGroup = new QGroupBox(QStringLiteral("快速操作"), mainSplitter);
    quickGroup->setMinimumWidth(270);
    auto *quickLayout = new QVBoxLayout(quickGroup);
    m_switchButton = new QPushButton(QStringLiteral("随机换一张壁纸"), quickGroup);
    m_switchButton->setObjectName(QStringLiteral("switchWallpaperButton"));
    m_switchButton->setToolTip(QStringLiteral("\u968f\u673a\u9009\u62e9\u4e00\u5f20\u56fe\u7247\u548c\u4e00\u9996\u542f\u7528\u8bd7\u8bcd\uff0c\u751f\u6210\u5e76\u8bbe\u7f6e\u684c\u9762\u58c1\u7eb8"));
    m_switchButton->setMinimumHeight(48);
    m_switchButton->setStyleSheet(QStringLiteral("font-weight:700;font-size:14pt;background:#256b9f;color:white;border:0;"));
    m_applyPreviewButton = new QPushButton(QStringLiteral("应用当前预览"), quickGroup);
    m_applyPreviewButton->setObjectName(QStringLiteral("applyPreviewButton"));
    m_applyPreviewButton->setToolTip(QStringLiteral("\u628a\u5f53\u524d\u9884\u89c8\u76f4\u63a5\u5e94\u7528\u5230 Windows \u684c\u9762\uff0c\u4e0d\u91cd\u65b0\u968f\u673a\u5185\u5bb9"));
    m_refreshPreviewButton = new QPushButton(QStringLiteral("重新预览"), quickGroup);
    m_refreshPreviewButton->setObjectName(QStringLiteral("refreshPreviewButton"));
    m_pauseButton = new QPushButton(QStringLiteral("开启定时"), quickGroup);
    m_pauseButton->setObjectName(QStringLiteral("pauseScheduleButton"));
    m_pauseButton->setToolTip(QStringLiteral("\u5feb\u901f\u5f00\u542f\u6216\u505c\u6b62\u5b9a\u65f6\u5207\u6362"));
    quickLayout->addWidget(m_switchButton);
    quickLayout->addWidget(m_applyPreviewButton);
    quickLayout->addWidget(m_refreshPreviewButton);
    quickLayout->addWidget(m_pauseButton);
    quickLayout->addSpacing(12);
    auto *summaryTitle = new QLabel(QStringLiteral("当前预览内容"), quickGroup);
    summaryTitle->setStyleSheet(QStringLiteral("font-weight:600;"));
    quickLayout->addWidget(summaryTitle);
    m_previewContentLabel = new QLabel(QStringLiteral("暂无"), quickGroup);
    m_previewContentLabel->setObjectName(QStringLiteral("previewContentLabel"));
    m_previewContentLabel->setWordWrap(true);
    m_previewContentLabel->setMinimumHeight(90);
    quickLayout->addWidget(m_previewContentLabel);
    m_statusLabel = new QLabel(QStringLiteral("准备就绪"), quickGroup);
    m_statusLabel->setObjectName(QStringLiteral("statusLabel"));
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet(QStringLiteral("color:#555;"));
    quickLayout->addWidget(m_statusLabel);
    quickLayout->addStretch();
    mainSplitter->addWidget(previewGroup);
    mainSplitter->addWidget(quickGroup);
    mainSplitter->setStretchFactor(0, 7);
    mainSplitter->setStretchFactor(1, 3);
    root->addWidget(mainSplitter, 3);

    m_tabs = new QTabWidget(central);
    m_tabs->setObjectName(QStringLiteral("resourceSettingsTabs"));
    m_tabs->setDocumentMode(true);
    m_tabs->setUsesScrollButtons(true);
    m_tabs->setMinimumHeight(300);
    root->addWidget(m_tabs, 2);
    setCentralWidget(central);

    auto *imagePage = makeTabPage(m_tabs, QStringLiteral("图片库"));
    auto *imageRoot = new QVBoxLayout(imagePage);
    auto *imageToolbar = new QHBoxLayout();
    m_selectImageButton = new QPushButton(QStringLiteral("选择图片…"), imagePage);
    m_selectImageButton->setObjectName(QStringLiteral("selectImageButton"));
    m_addImageDirectoryButton = new QPushButton(QStringLiteral("添加图片目录"), imagePage);
    m_addImageDirectoryButton->setObjectName(QStringLiteral("addImageDirectoryButton"));
    auto *removeDirectoryButton = new QPushButton(QStringLiteral("移除目录"), imagePage);
    removeDirectoryButton->setObjectName(QStringLiteral("removeImageDirectoryButton"));
    auto *rescanButton = new QPushButton(QStringLiteral("重新扫描"), imagePage);
    rescanButton->setObjectName(QStringLiteral("rescanImagesButton"));
    imageToolbar->addWidget(m_selectImageButton);
    imageToolbar->addWidget(m_addImageDirectoryButton);
    imageToolbar->addWidget(removeDirectoryButton);
    imageToolbar->addWidget(rescanButton);
    imageToolbar->addStretch();
    imageRoot->addLayout(imageToolbar);
    auto *imageSplitter = new QSplitter(Qt::Horizontal, imagePage);
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
    fileLayout->addWidget(m_imageFiles);
    imageSplitter->addWidget(directoryGroup);
    imageSplitter->addWidget(fileGroup);
    imageSplitter->setStretchFactor(0, 1);
    imageSplitter->setStretchFactor(1, 2);
    imageRoot->addWidget(imageSplitter, 1);
    m_selectedImageInfoLabel = new QLabel(QStringLiteral("请选择一张图片"), imagePage);
    m_selectedImageInfoLabel->setObjectName(QStringLiteral("selectedImageInfoLabel"));
    m_selectedImageInfoLabel->setWordWrap(true);
    imageRoot->addWidget(m_selectedImageInfoLabel);
    auto *imageActionRow = new QHBoxLayout();
    m_useSelectedImagePreviewButton = new QPushButton(QStringLiteral("使用选中图片预览"), imagePage);
    m_useSelectedImagePreviewButton->setObjectName(QStringLiteral("useSelectedImagePreviewButton"));
    m_setSelectedImageButton = new QPushButton(QStringLiteral("将选中图片设为壁纸"), imagePage);
    m_setSelectedImageButton->setObjectName(QStringLiteral("setSelectedImageButton"));
    imageActionRow->addWidget(m_useSelectedImagePreviewButton);
    imageActionRow->addWidget(m_setSelectedImageButton);
    imageActionRow->addStretch();
    imageRoot->addLayout(imageActionRow);
    m_imageScanLabel = new QLabel(imagePage);
    m_imageScanLabel->setObjectName(QStringLiteral("imageScanStatusLabel"));
    imageRoot->addWidget(m_imageScanLabel);
    auto *poemPage = makeTabPage(m_tabs, QStringLiteral("诗词库"));
    auto *poemRoot = new QVBoxLayout(poemPage);
    auto *poemToolbar = new QHBoxLayout();
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
    poemToolbar->addWidget(addPoemButton);
    poemToolbar->addWidget(editPoemButton);
    poemToolbar->addWidget(importButton);
    poemToolbar->addWidget(togglePoemButton);
    poemToolbar->addWidget(deletePoemButton);
    poemToolbar->addWidget(usePoemButton);
    poemToolbar->addStretch();
    poemRoot->addLayout(poemToolbar);
    m_poemTable = new QTableWidget(poemPage);
    m_poemTable->setObjectName(QStringLiteral("poemTable"));
    m_poemTable->setColumnCount(4);
    m_poemTable->setHorizontalHeaderLabels({QStringLiteral("启用"), QStringLiteral("标题"), QStringLiteral("作者"), QStringLiteral("正文摘要")});
    m_poemTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_poemTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_poemTable->verticalHeader()->setDefaultSectionSize(42);
    m_poemTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_poemTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    poemRoot->addWidget(m_poemTable, 1);
    m_poemCountLabel = new QLabel(poemPage);
    m_poemCountLabel->setObjectName(QStringLiteral("poemCountLabel"));
    poemRoot->addWidget(m_poemCountLabel);

    auto *displayPage = makeTabPage(m_tabs, QStringLiteral("显示设置"));
    auto *displayRoot = new QVBoxLayout(displayPage);
    auto *displayForm = new QFormLayout();
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
    displayForm->addRow(QStringLiteral("\u5b57\u4f53\u65cf"), m_fontCombo);
    displayForm->addRow(QStringLiteral("\u6b63\u6587\u57fa\u51c6\u5b57\u53f7"), m_fontSizeSpin);
    displayForm->addRow(QStringLiteral("\u6392\u7248\u9884\u8bbe"), m_typographyPreset);
    displayForm->addRow(QStringLiteral("\u6807\u9898\u989c\u8272"), m_titleColorButton);
    displayForm->addRow(QStringLiteral("\u4f5c\u8005/\u671d\u4ee3\u989c\u8272"), m_metadataColorButton);
    displayForm->addRow(QStringLiteral("\u6b63\u6587\u989c\u8272"), m_textColorButton);
    displayForm->addRow(QStringLiteral("\u6587\u5b57\u4f4d\u7f6e"), m_anchorCombo);
    displayForm->addRow(QStringLiteral("\u56fe\u7247\u9002\u914d"), m_fitCombo);
    auto *panelRow = new QHBoxLayout();
    panelRow->addWidget(m_panelCheck);
    panelRow->addWidget(m_panelColorButton);
    panelRow->addStretch();
    displayForm->addRow(QStringLiteral("文字样式"), panelRow);
    displayForm->addRow(QString(), m_shadowCheck);
    displayRoot->addLayout(displayForm);
    auto *displayHint = new QLabel(QStringLiteral("修改字体、字号、颜色、位置或适配方式后，150–300 ms 内更新左侧预览，不会自动修改桌面。"), displayPage);
    displayHint->setWordWrap(true);
    displayRoot->addWidget(displayHint);
    auto *resetDisplayButton = new QPushButton(QStringLiteral("恢复默认显示设置"), displayPage);
    resetDisplayButton->setObjectName(QStringLiteral("resetDisplaySettingsButton"));
    displayRoot->addWidget(resetDisplayButton, 0, Qt::AlignLeft);
    displayRoot->addStretch();

    auto *schedulePage = makeTabPage(m_tabs, QStringLiteral("定时与高级"));
    auto *scheduleRoot = new QVBoxLayout(schedulePage);
    m_scheduleCheck = new QCheckBox(QStringLiteral("开启定时切换"), schedulePage);
    m_scheduleCheck->setObjectName(QStringLiteral("scheduleCheckBox"));
    m_intervalSpin = new QSpinBox(schedulePage);
    m_intervalSpin->setObjectName(QStringLiteral("intervalSpinBox"));
    m_intervalSpin->setRange(1, 1440);
    m_intervalSpin->setSuffix(QStringLiteral(" 分钟"));
    auto *scheduleForm = new QFormLayout();
    scheduleForm->addRow(QString(), m_scheduleCheck);
    scheduleForm->addRow(QStringLiteral("切换间隔"), m_intervalSpin);
    m_scheduleNextLabel = new QLabel(schedulePage);
    m_scheduleNextLabel->setObjectName(QStringLiteral("scheduleNextLabel"));
    scheduleForm->addRow(QStringLiteral("下次切换"), m_scheduleNextLabel);
    scheduleRoot->addLayout(scheduleForm);
    auto *scheduleHint = new QLabel(QStringLiteral("程序重启后按启动时间重新计时，不补偿错过的切换。"), schedulePage);
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
    connect(m_textColorButton, &QPushButton::clicked, this, &MainWindow::chooseTextColor);
    connect(m_titleColorButton, &QPushButton::clicked, this, &MainWindow::chooseTitleColor);
    connect(m_metadataColorButton, &QPushButton::clicked, this, &MainWindow::chooseMetadataColor);
    connect(m_panelColorButton, &QPushButton::clicked, this, &MainWindow::choosePanelColor);
    connect(m_typographyPreset, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::typographyPresetChanged);
    connect(m_fontCombo, &QFontComboBox::currentFontChanged, this, &MainWindow::displaySettingsChanged);
    connect(m_fontSizeSpin, &QSpinBox::valueChanged, this, &MainWindow::displaySettingsChanged);
    connect(m_panelCheck, &QCheckBox::toggled, this, &MainWindow::displaySettingsChanged);
    connect(m_shadowCheck, &QCheckBox::toggled, this, &MainWindow::displaySettingsChanged);
    connect(m_anchorCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::displaySettingsChanged);
    connect(m_fitCombo, qOverload<int>(&QComboBox::currentIndexChanged), this, &MainWindow::displaySettingsChanged);
    connect(resetDisplayButton, &QPushButton::clicked, this, &MainWindow::resetDisplaySettings);
    connect(m_scheduleCheck, &QCheckBox::toggled, this, &MainWindow::toggleSchedule);
    connect(m_intervalSpin, &QSpinBox::valueChanged, this, &MainWindow::saveScheduleSettings);
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
    m_tray->setIcon(style()->standardIcon(QStyle::SP_ComputerIcon));
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
    QStringList warnings;
    m_store.load(m_config, &warnings);
    if (!hadConfigFile) {
        m_config.poems = PoetryLibrary::defaultPoems();
        persistConfig();
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
    updateSettingsUi();
    updatePreviewSummary();
    updateStatusUi();
}

void MainWindow::updateImageUi() {
    m_ignoreImageChanges = true;
    m_imageDirectories->clear();
    for (const ImageSource &source : m_config.imageSources) {
        const QString label = source.isFile
            ? QStringLiteral("?? ? %1").arg(QFileInfo(source.path).fileName())
            : QStringLiteral("?? ? %1").arg(source.path);
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
        auto *item = new QListWidgetItem(QFileInfo(image.path).fileName(), m_imageFiles);
        item->setData(Qt::UserRole, image.path);
        item->setToolTip(image.path);
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
        m_poemTable->setItem(row, 1, new QTableWidgetItem(poem.title));
        m_poemTable->setItem(row, 2, new QTableWidgetItem(poem.author));
        QString content = poem.content;
        content.replace(QStringLiteral("\n"), QStringLiteral(" / "));
        m_poemTable->setItem(row, 3, new QTableWidgetItem(content));
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
    m_panelColorButton->setText(m_config.render.panelColor.name(QColor::HexArgb));
    m_panelColorButton->setStyleSheet(colorButtonStyle(m_config.render.panelColor));
    m_panelCheck->setChecked(m_config.render.panelEnabled);
    m_shadowCheck->setChecked(m_config.render.shadowEnabled);
    m_anchorCombo->setCurrentIndex(m_anchorCombo->findData(anchorToString(m_config.render.anchor)));
    m_fitCombo->setCurrentIndex(m_fitCombo->findData(fitModeToString(m_config.render.fitMode)));
    const QSignalBlocker b12(m_scheduleCheck);
    const QSignalBlocker b13(m_intervalSpin);
    m_scheduleCheck->setChecked(m_config.scheduleEnabled);
    m_intervalSpin->setValue(m_config.intervalMinutes);
}
void MainWindow::updateStatusUi() {
    const int validImages = m_imageLibrary.validItems().size();
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
    m_pauseButton->setText(m_config.scheduleEnabled ? QStringLiteral("暂停定时") : QStringLiteral("开启定时"));
    m_scheduleSummaryLabel->setText(m_config.scheduleEnabled ? QStringLiteral("定时已开启") : QStringLiteral("定时已关闭"));
}

void MainWindow::updatePreview(const QString &path) {
    if (path.isEmpty() || !QFileInfo::exists(path)) {
        m_previewLabel->setPixmap(QPixmap());
        m_previewLabel->setText(QStringLiteral("还没有可用预览\n请先添加图片目录并导入诗词库"));
        return;
    }
    const QPixmap pixmap(path);
    if (pixmap.isNull()) return;
    m_previewLabel->setText(QString());
    m_previewLabel->setPixmap(pixmap.scaled(m_previewLabel->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_previewLabel->setToolTip(path);
}

void MainWindow::updatePreviewSummary() {
    if (!m_hasPreviewContent) {
        m_previewContentLabel->setText(QStringLiteral("暂无当前预览"));
        return;
    }
    const QString poemTitle = m_previewPoem.title.isEmpty() ? m_previewPoem.content.left(20) : m_previewPoem.title;
    const QString author = m_previewPoem.author.isEmpty() ? QString() : QStringLiteral("\n作者：%1").arg(m_previewPoem.author);
    const QString imageName = m_previewImage.path.isEmpty() ? QStringLiteral("未知") : QFileInfo(m_previewImage.path).fileName();
    m_previewContentLabel->setText(QStringLiteral("诗词：%1%2\n图片：%3").arg(poemTitle, author, imageName));
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

void MainWindow::startImageScan() {
    if (m_scanWatcher.isRunning()) return;
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
    m_selectedImageInfoLabel->setText(QStringLiteral("当前选中图片：%1\n%2 × %3 · %4 KB\n使用选中图片预览不会自动修改桌面；设置原图会直接修改桌面。")
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
    dialog.setWindowTitle(creating ? QStringLiteral("\u65b0\u589e\u8bd7\u8bcd") : QStringLiteral("\u7f16\u8f91\u8bd7\u8bcd"));
    dialog.setMinimumSize(560, 440);
    auto *layout = new QVBoxLayout(&dialog);
    auto *form = new QFormLayout();
    auto *titleEdit = new QLineEdit(poem.title, &dialog);
    auto *authorEdit = new QLineEdit(poem.author, &dialog);
    auto *dynastyEdit = new QLineEdit(poem.dynasty, &dialog);
    auto *contentEdit = new QTextEdit(poem.content, &dialog);
    contentEdit->setMinimumHeight(190);
    contentEdit->setPlaceholderText(QStringLiteral("\u6b63\u6587\u652f\u6301\u6362\u884c\uff0c\u6bcf\u884c\u4f1a\u6309\u58c1\u7eb8\u5b89\u5168\u533a\u57df\u81ea\u52a8\u6362\u884c\u3002"));
    form->addRow(QStringLiteral("\u6807\u9898"), titleEdit);
    form->addRow(QStringLiteral("\u4f5c\u8005"), authorEdit);
    form->addRow(QStringLiteral("\u671d\u4ee3"), dynastyEdit);
    form->addRow(QStringLiteral("\u6b63\u6587"), contentEdit);
    layout->addLayout(form);
    auto *hint = new QLabel(QStringLiteral("\u5efa\u8bae\u586b\u5199\u6807\u9898\u3001\u4f5c\u8005\u548c\u671d\u4ee3\uff1b\u6b63\u6587\u4e0d\u80fd\u4e3a\u7a7a\u3002\u6807\u9898\u3001\u4f5c\u8005\uff0f\u671d\u4ee3\u3001\u6b63\u6587\u4f1a\u4f7f\u7528\u4e0d\u540c\u5b57\u53f7\u548c\u989c\u8272\u3002"), &dialog);
    hint->setWordWrap(true);
    hint->setStyleSheet(QStringLiteral("color:#5b6b7b;"));
    layout->addWidget(hint);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    buttons->button(QDialogButtonBox::Ok)->setText(QStringLiteral("\u4fdd\u5b58"));
    buttons->button(QDialogButtonBox::Cancel)->setText(QStringLiteral("\u53d6\u6d88"));
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::accepted, &dialog, [&dialog, contentEdit] {
        if (contentEdit->toPlainText().trimmed().isEmpty()) {
            QMessageBox::warning(&dialog, QStringLiteral("\u65e0\u6cd5\u4fdd\u5b58"), QStringLiteral("\u8bd7\u8bcd\u6b63\u6587\u4e0d\u80fd\u4e3a\u7a7a\u3002"));
            return;
        }
        dialog.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    if (dialog.exec() != QDialog::Accepted) return false;
    poem.title = titleEdit->text().trimmed();
    poem.author = authorEdit->text().trimmed();
    poem.dynasty = dynastyEdit->text().trimmed();
    poem.content = contentEdit->toPlainText().trimmed();
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
    const QVector<ImageItem> images = m_imageLibrary.validItems();
    if (poems.isEmpty() || images.isEmpty()) {
        m_statusLabel->setText(QStringLiteral("缺少可用图片或诗词，无法随机换一张壁纸"));
        updateStatusUi();
        return;
    }
    m_desktopBusy = true;
    m_switchStartedGeneration = m_previewGeneration;
    m_statusLabel->setText(QStringLiteral("正在随机生成并设置桌面…"));
    const RenderSettings settings = m_config.render;
    const QSize target = targetWallpaperSize();
    const QString cache = m_store.wallpaperCachePath();
    const RandomPicker picker = m_picker;
    QStringList protectedPaths;
    for (const QString &path : {m_config.runtime.lastSuccessfulWallpaper,
                                m_config.runtime.currentPreviewWallpaper,
                                m_previewPath}) {
        if (!path.isEmpty() && !protectedPaths.contains(path)) protectedPaths.append(path);
    }
    m_switchWatcher.setFuture(QtConcurrent::run([poems, images, settings, target, cache, picker, protectedPaths] {
        WindowsWallpaperSetter setter;
        return WallpaperService::run(poems, images, settings, target, cache, picker, setter, protectedPaths);
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
        m_picker.rememberPoem(result.poemId);
        m_picker.rememberImage(result.imagePath);
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
    if (panelColor.isValid()) m_config.render.panelColor = panelColor;
    m_config.render.panelEnabled = m_panelCheck->isChecked();
    m_config.render.shadowEnabled = m_shadowCheck->isChecked();
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

void MainWindow::resizeEvent(QResizeEvent *event) {
    QMainWindow::resizeEvent(event);
    if (!m_previewPath.isEmpty()) updatePreview(m_previewPath);
}

QSize MainWindow::targetWallpaperSize() const {
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen) return QSize(1920, 1080);
    QRect workArea = screen->availableGeometry();
    if (!workArea.isValid()) workArea = screen->geometry();
    if (!workArea.isValid()) return QSize(1920, 1080);
    const qreal dpr = qMax<qreal>(1.0, screen->devicePixelRatio());
    return QSize(qMax(1, qRound(workArea.width() * dpr)), qMax(1, qRound(workArea.height() * dpr)));
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
    m_desktopStateLabel->setText(text);
    m_desktopStateLabel->setStyleSheet(QStringLiteral("padding:4px 10px;border-radius:4px;background:%1;color:white;font-weight:600;").arg(color.name()));
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

