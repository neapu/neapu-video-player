//
// Created by liu86 on 2025/10/28.
//

#include "MainWindow.h"
#include <QMenuBar>
#include "AudioRenderer.h"
#include "VideoRenderer.h"
#include "logger.h"

#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include "ControlWidget.h"

namespace view {
MainWindow::MainWindow()
{
    m_settings = new QSettings("NeapuVideoPlayer.ini", QSettings::IniFormat, this);

    // 坑点，在macos中必须先创建VideoRenderer，再执行resize和创建菜单等操作
    // 否则会导致QRhi初始化失败，现象为报错：QRhiWidget: No QRhi，并且窗口无法显示
    createLayout();

    createMenus();

    resize(800, 600);
    setMinimumSize(800, 600);

    connect(m_playerController, &PlayerController::fileNameChanged, this, [this](const QString& fileName) {
        if (fileName.isEmpty()) {
            setWindowTitle("Neapu Video Player");
        } else {
            setWindowTitle(QString("Neapu Video Player - %1").arg(fileName));
        }
    });
}
MainWindow::~MainWindow()
{
    NEAPU_FUNC_TRACE;
}
void MainWindow::createMenus()
{
    auto* fineManu = menuBar()->addMenu(tr("&File"));
    auto* openAction = fineManu->addAction(tr("&Open"));
    auto* exitAction = fineManu->addAction(tr("E&xit"));

    connect(openAction, &QAction::triggered, [this]() {
        m_playerController->onOpen();
    });
    connect(exitAction, &QAction::triggered, [this]() { close(); });
}
void MainWindow::createLayout()
{
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* layout = new QVBoxLayout(centralWidget);
    m_videoRenderer = new VideoRenderer(centralWidget);
    m_playerController = new PlayerController(m_videoRenderer, this);
    layout->addWidget(m_videoRenderer, 1);
    m_controlWidget = new ControlWidget(m_playerController, centralWidget);
    m_controlWidget->setFixedHeight(80);
    layout->addWidget(m_controlWidget, 0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
}
} // namespace view