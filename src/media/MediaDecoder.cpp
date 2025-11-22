//
// Created by liu86 on 2025/11/18.
//

#include "MediaDecoder.h"
#include "MediaDecoderImpl.h"

namespace media {
auto MediaDecoder::createMediaDecoder(const CreateParam& param) -> std::expected<std::unique_ptr<MediaDecoder>, std::string>
{
    try {
        auto decoder = std::make_unique<MediaDecoderImpl>(param);
        return std::expected<std::unique_ptr<MediaDecoder>, std::string>(std::move(decoder));
    } catch (const std::exception& e) {
        return std::unexpected(std::string("Failed to create MediaDecoder: ") + e.what());
    }
}

} // namespace media