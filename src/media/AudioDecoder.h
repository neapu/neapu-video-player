//
// Created by liu86 on 2025/11/15.
//

#pragma once
#include "Decoder.h"

namespace media {
class AudioDecoder : public Decoder {
    Q_OBJECT
public:
    explicit AudioDecoder(QObject* parent = nullptr);
    ~AudioDecoder() override = default;
};
} // namespace media
