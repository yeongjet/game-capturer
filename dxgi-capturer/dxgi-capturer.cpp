#include "dxgi-capturer.h"
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>

using Microsoft::WRL::ComPtr;

int capture_frame(
    ComPtr<ID3D11Device> device,
    ComPtr<ID3D11DeviceContext> context,
    ComPtr<ID3D11Texture2D>& outTexture,
    D3D11_TEXTURE2D_DESC* outDesc
) {
    HRESULT hr;
    D3D_FEATURE_LEVEL featureLevel;
    // 如果未传入device/context则自动创建
    if (!device || !context) {
        hr = D3D11CreateDevice(
            nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
            D3D11_SDK_VERSION, &device, &featureLevel, &context);
        if (FAILED(hr)) {
            return -1;
        }
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    device.As(&dxgiDevice);
    ComPtr<IDXGIAdapter> dxgiAdapter;
    dxgiDevice->GetAdapter(&dxgiAdapter);
    ComPtr<IDXGIOutput> dxgiOutput;
    dxgiAdapter->EnumOutputs(0, &dxgiOutput);
    ComPtr<IDXGIOutput1> dxgiOutput1;
    dxgiOutput.As(&dxgiOutput1);

    ComPtr<IDXGIOutputDuplication> deskDupl;
    hr = dxgiOutput1->DuplicateOutput(device.Get(), &deskDupl);
    if (FAILED(hr)) {
        return -2;
    }

    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    ComPtr<IDXGIResource> desktopResource;
    hr = deskDupl->AcquireNextFrame(1000, &frameInfo, &desktopResource);
    if (FAILED(hr)) {
        return -3;
    }

    ComPtr<ID3D11Texture2D> acquiredDesktopImage;
    desktopResource.As(&acquiredDesktopImage);
    D3D11_TEXTURE2D_DESC desc = {};
    acquiredDesktopImage->GetDesc(&desc);
    if (outDesc) *outDesc = desc;

    D3D11_TEXTURE2D_DESC descStaging = desc;
    descStaging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    descStaging.Usage = D3D11_USAGE_STAGING;
    descStaging.BindFlags = 0;
    descStaging.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> stagingTex;
    hr = device->CreateTexture2D(&descStaging, nullptr, &stagingTex);
    if (FAILED(hr)) {
        deskDupl->ReleaseFrame();
        return -4;
    }
    context->CopyResource(stagingTex.Get(), acquiredDesktopImage.Get());

    // 可选：测试能否映射
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = context->Map(stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        deskDupl->ReleaseFrame();
        return -5;
    }
    context->Unmap(stagingTex.Get(), 0);
    deskDupl->ReleaseFrame();
    outTexture = stagingTex;
    return 0;
}
