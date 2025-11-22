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
    resize(800, 600);
    setMinimumSize(800, 600);

    m_settings = new QSettings("NeapuVideoPlayer.ini", QSettings::IniFormat, this);
    m_player = new Player(this);
    m_audioRenderer = new AudioRenderer([this]() {
        return m_player->getAudioFrame();
    }, this);


    createMenus();

    createLayout();

    m_player->setVideoRenderer(m_videoRenderer);
    m_player->setAudioRenderer(m_audioRenderer);

    connect(m_videoRenderer, &VideoRenderer::initialized, [this]() {
        m_player->setD3D11Device(m_videoRenderer->getD3D11Device());
    });
}
MainWindow::~MainWindow()
{
    NEAPU_FUNC_TRACE;
    m_player->close();
}
void MainWindow::createMenus()
{
    auto* fineManu = menuBar()->addMenu(tr("&File"));
    auto* openAction = fineManu->addAction(tr("&Open"));
    auto* exitAction = fineManu->addAction(tr("E&xit"));

    connect(openAction, &QAction::triggered, [this]() {
        m_player->open();
        m_player->play();
    });
    connect(exitAction, &QAction::triggered, [this]() { close(); });
}
void MainWindow::createLayout()
{
    auto* centralWidget = new QWidget(this);
    setCentralWidget(centralWidget);

    auto* layout = new QVBoxLayout(centralWidget);
    m_videoRenderer = new VideoRenderer(centralWidget);
    layout->addWidget(m_videoRenderer, 1);
    m_controlWidget = new ControlWidget(centralWidget);
    m_controlWidget->setFixedHeight(80);
    layout->addWidget(m_controlWidget, 0);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
}
} // namespace view