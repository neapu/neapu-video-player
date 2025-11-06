#pragma once
#include <string>
#include <functional>

#ifdef _WIN32
typedef struct ID3D11Device ID3D11Device;
typedef struct ID3D11DeviceContext ID3D11DeviceContext;
#endif

namespace media {
using DecodeOverCallback =  std::function<void()>;
struct OpenMediaParams {
    std::string url;
    DecodeOverCallback decodeOverCallback{nullptr};
    bool enableHWAccel{false};
    bool copyBackRender{true};
#ifdef _WIN32
    ID3D11Device* d3d11Device{nullptr};
    ID3D11DeviceContext* d3d11DeviceContext{nullptr};
#endif
};
struct Rational {
    int num{0};
    int den{1};
};
}