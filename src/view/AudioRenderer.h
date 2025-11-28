//
// Created by liu86 on 2025/10/28.
//

#pragma once
#include <QObject>
#include "../media/Frame.h"
#include <atomic>

typedef struct ma_device ma_device;

namespace view {
class AudioRenderer:public QObject {
    Q_OBJECT
public:
    explicit AudioRenderer(QObject* parent = nullptr);
    ~AudioRenderer() override = default;

    bool start(int sampleRate, int channels);
    void stop();

    void seek(int serial);

    bool seeking() const { return m_seeking.load(); }

signals:
    void playingStateChanged(bool playing);
    void playingPts(int64_t);
    void eof();

private:
    void maDataCallback(ma_device* pDevice, void* pOutput, const void* pInput, uint32_t frameCount);
    void updateCurrentData(int64_t thresholdUs);
    void setPlaying(bool playing);

private:
    std::atomic_bool m_playing{false};

    ma_device* m_device{nullptr};

    media::FramePtr m_currentData{nullptr};
    media::FramePtr m_nextData{nullptr};
    size_t m_offset{0};

    std::atomic<int64_t> m_startTimeUs{0};
    std::atomic_int m_serial{0};
    std::atomic_bool m_seeking{false};
    std::atomic_bool m_running{false};
};

} // namespace view
