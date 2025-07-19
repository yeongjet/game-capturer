// example.cpp
// 演示如何使用DXGI捕获屏幕内容
// 需要链接dxgi.lib d3d11.lib d3dcompiler.lib

#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <stdio.h>

using Microsoft::WRL::ComPtr;

int main() {
    HRESULT hr;
    ComPtr<ID3D11Device> device;
    ComPtr<ID3D11DeviceContext> context;
    D3D_FEATURE_LEVEL featureLevel;
    hr = D3D11CreateDevice(
        nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0, nullptr, 0,
        D3D11_SDK_VERSION, &device, &featureLevel, &context);
    if (FAILED(hr)) {
        printf("Failed to create D3D11 device\n");
        return -1;
    }

    ComPtr<IDXGIDevice> dxgiDevice;
    device.As(&dxgiDevice);
    ComPtr<IDXGIAdapter> dxgiAdapter;
    dxgiDevice->GetAdapter(&dxgiAdapter);
    ComPtr<IDXGIFactory1> dxgiFactory;
    dxgiAdapter->GetParent(__uuidof(IDXGIFactory1), &dxgiFactory);
    ComPtr<IDXGIOutput> dxgiOutput;
    dxgiAdapter->EnumOutputs(0, &dxgiOutput);
    ComPtr<IDXGIOutput1> dxgiOutput1;
    dxgiOutput.As(&dxgiOutput1);

    ComPtr<IDXGIOutputDuplication> deskDupl;
    hr = dxgiOutput1->DuplicateOutput(device.Get(), &deskDupl);
    if (FAILED(hr)) {
        printf("DuplicateOutput failed: 0x%08X\n", hr);
        return -1;
    }

    DXGI_OUTDUPL_FRAME_INFO frameInfo;
    ComPtr<IDXGIResource> desktopResource;
    hr = deskDupl->AcquireNextFrame(1000, &frameInfo, &desktopResource);
    if (FAILED(hr)) {
        printf("AcquireNextFrame failed: 0x%08X\n", hr);
        return -1;
    }

    ComPtr<ID3D11Texture2D> acquiredDesktopImage;
    desktopResource.As(&acquiredDesktopImage);
    // 获取纹理描述
    D3D11_TEXTURE2D_DESC desc = {};
    acquiredDesktopImage->GetDesc(&desc);

    // 创建CPU可读的staging纹理
    D3D11_TEXTURE2D_DESC descStaging = desc;
    descStaging.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
    descStaging.Usage = D3D11_USAGE_STAGING;
    descStaging.BindFlags = 0;
    descStaging.MiscFlags = 0;
    ComPtr<ID3D11Texture2D> stagingTex;
    hr = device->CreateTexture2D(&descStaging, nullptr, &stagingTex);
    if (FAILED(hr)) {
        printf("CreateTexture2D (staging) failed: 0x%08X\n", hr);
        deskDupl->ReleaseFrame();
        return -1;
    }
    // 拷贝到staging纹理
    context->CopyResource(stagingTex.Get(), acquiredDesktopImage.Get());

    // 映射staging纹理，获取像素数据
    D3D11_MAPPED_SUBRESOURCE mapped;
    hr = context->Map(stagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (FAILED(hr)) {
        printf("Map failed: 0x%08X\n", hr);
        deskDupl->ReleaseFrame();
        return -1;
    }

    // 保存为BMP文件
    FILE *fp = fopen("output.bmp", "wb");
    if (!fp) {
        printf("无法创建output.bmp\n");
        context->Unmap(stagingTex.Get(), 0);
        deskDupl->ReleaseFrame();
        return -1;
    }
    // 写BMP头
    int width = desc.Width;
    int height = desc.Height;
    int rowPitch = width * 4;
    int imageSize = rowPitch * height;
    int fileSize = 54 + imageSize;
    unsigned char bmpFileHeader[14] = {
        'B','M', fileSize, fileSize>>8, fileSize>>16, fileSize>>24,
        0,0, 0,0, 54,0,0,0
    };
    unsigned char bmpInfoHeader[40] = {
        40,0,0,0, width, width>>8, width>>16, width>>24,
        height, height>>8, height>>16, height>>24,
        1,0, 32,0, 0,0,0,0,
        imageSize, imageSize>>8, imageSize>>16, imageSize>>24,
        0,0,0,0, 0,0,0,0, 0,0,0,0
    };
    fwrite(bmpFileHeader, 1, 14, fp);
    fwrite(bmpInfoHeader, 1, 40, fp);
    // BMP像素数据自下而上
    for (int y = height - 1; y >= 0; --y) {
        fwrite((BYTE*)mapped.pData + y * mapped.RowPitch, 1, rowPitch, fp);
    }
    fclose(fp);
    printf("已保存output.bmp\n");

    context->Unmap(stagingTex.Get(), 0);
    deskDupl->ReleaseFrame();
    return 0;
}
