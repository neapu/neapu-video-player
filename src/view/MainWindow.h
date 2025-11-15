//
// Created by liu86 on 2025/10/28.
//

#pragma once
#include <QMainWindow>
#include <memory>
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

private:
    VideoRenderer* m_videoRenderer{nullptr};
    AudioRenderer* m_audioRenderer{nullptr};
    ControlWidget* m_controlWidget{nullptr};
    bool m_isDecodeOver{false};
};

} // namespace view
