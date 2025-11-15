//
// Created by liu86 on 2025/11/15.
//

#pragma once
#include <QObject>
#include <QString>
#include <QFuture>
#include "Helper.h"

namespace media {
class Demuxer : public QObject {
    Q_OBJECT
public:
    explicit Demuxer(QObject* parent = nullptr);
    ~Demuxer() override;
    bool open(const QString& url);
    void close();

    auto asyncReadPacket() -> QFuture<AVPacketPtr>;
};

} // namespace media
