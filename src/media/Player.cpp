//
// Created by liu86 on 2025/11/24.
//

#include "Player.h"
#include "PlayerImpl.h"

namespace media {
Player& Player::instance()
{
    static PlayerImpl instance;
    return instance;
}
} // namespace media