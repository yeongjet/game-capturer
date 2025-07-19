#pragma once
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

// 返回0表示成功，非0表示失败
// 参数：device/context可选传入（可为nullptr，内部自动创建），outDesc返回捕获的纹理描述
// 返回的outTexture为CPU可读的staging纹理，调用者负责释放
int capture_frame(
    Microsoft::WRL::ComPtr<ID3D11Device> device,
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context,
    Microsoft::WRL::ComPtr<ID3D11Texture2D>& outTexture,
    D3D11_TEXTURE2D_DESC* outDesc = nullptr
);
