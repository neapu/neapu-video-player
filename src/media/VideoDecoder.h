//
// Created by liu86 on 2025/11/15.
//

#pragma once
#include "Decoder.h"

namespace media {

class VideoDecoder : public Decoder {
    Q_OBJECT
public:
    explicit VideoDecoder(QObject* parent = nullptr);
    ~VideoDecoder() override = default;
};

} // namespace media
