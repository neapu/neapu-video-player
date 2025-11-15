//
// Created by liu86 on 2025/11/15.
//

#pragma once
#include <QObject>

namespace media {

class Decoder : public QObject {
    Q_OBJECT
public:
    explicit Decoder(QObject* parent = nullptr);
    ~Decoder() override = default;
};

} // namespace media
