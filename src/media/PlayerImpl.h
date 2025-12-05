//
// Created by liu86 on 2025/11/25.
//

#pragma once
#include "Player.h"
#include "Demuxer.h"
#include "VideoDecoder.h"
#include "AudioDecoder.h"

namespace media {

class PlayerImpl : public Player {
public:
    PlayerImpl() = default;
    ~PlayerImpl() override = default;

    FramePtr getVideoFrame() override;
    FramePtr getAudioFrame() override;

    bool open(const OpenParam& param) override;
    void close() override;

    void seek(double seconds) override;

    bool isOpened() const override;

    bool hasVideo() const override;
    bool hasAudio() const override;

    double fps() const override;
    int sampleRate() const override;
    int channelCount() const override;

    double durationSeconds() const override;

    void play() override;
    void pause() override;

    bool isPlaying() const override { return m_playing.load(); }
    
    int64_t lastPlayPtsUs() const override { return m_lastPlayPtsUs.load(); }

#ifdef __linux__
    void* vaDisplay() const override;
#endif
private:
    void createVideoDecoder();
    void createAudioDecoder();

private:
    OpenParam m_param;
    std::unique_ptr<Demuxer> m_demuxer;
    std::unique_ptr<AudioDecoder> m_audioDecoder;
    std::unique_ptr<VideoDecoder> m_videoDecoder;

    std::atomic_int m_serial{0};
    std::atomic<int64_t> m_startTimeUs{0};
    std::atomic<int64_t> m_lastPlayPtsUs{0};
    std::atomic_bool m_playing{false};
    bool m_videoSeeking{false};
    bool m_audioSeeking{false};
    std::mutex m_seekMutex;
    std::atomic_bool m_videoEof{false};
    std::atomic_bool m_audioEof{false};

    FramePtr m_nextVideoFrame;
    FramePtr m_nextAudioFrame;
};

} // namespace media
