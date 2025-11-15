//
// Created by liu86 on 2025/11/15.
//

#pragma once
#include <QRhiWidget>

namespace view {

class VideoRenderer:public QRhiWidget {
    Q_OBJECT
public:
    explicit VideoRenderer(QWidget* parent = nullptr);
    ~VideoRenderer() override = default;

    void initialize(QRhiCommandBuffer* cb) override;
    void render(QRhiCommandBuffer* cb) override;
};

} // namespace view
