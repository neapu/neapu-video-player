//
// Created by liu86 on 2025/10/28.
//

#pragma once
#include <QMainWindow>
#include <memory>
#include "../media/Player.h"
#include "ControlWidget.h"

namespace view {
class VideoRenderer;
class AudioRenderer;
class MainWindow final : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow();
    ~MainWindow() override;

private:
    void createMenus();
    void createLayout();

private slots:
    void onOpenFile();
    void onAudioPlayingStateChanged(bool playing);

private:
    VideoRenderer* m_videoRenderer{nullptr};
    AudioRenderer* m_audioRenderer{nullptr};
    ControlWidget* m_controlWidget{nullptr};
    std::unique_ptr<media::Player> m_player{nullptr};
    bool m_isDecodeOver{false};
};

} // namespace view
