#define _WIN32_WINNT 0x0600
#define STB_IMAGE_WRITE_IMPLEMENTATION

#include "client.h"
#include <d3d11.h>
#include <dxgi1_2.h>
#include <wrl/client.h>
#include <winsock2.h>
#include <ws2tcpip.h>

Client::Client(const struct sockaddr_in &local_addr)
    : local_addr(local_addr), adapter(nullptr), adapter_file(nullptr), cq(nullptr), listener(nullptr)
{
    NdOpenAdapter(
        IID_IND2Adapter,
        reinterpret_cast<const struct sockaddr *>(&local_addr),
        sizeof(local_addr),
        reinterpret_cast<void **>(&adapter));
    adapter->CreateOverlappedFile(&adapter_file);
    memset(&adapter_info, 0, sizeof(adapter_info));
    adapter_info.InfoVersion = ND_VERSION_2;
    ULONG adapterInfoSize = sizeof(adapter_info);
    adapter->Query(&adapter_info, &adapterInfoSize);
    adapter->CreateCompletionQueue(IID_IND2CompletionQueue, adapter_file, adapter_info.MaxCompletionQueueDepth, 0, 0, reinterpret_cast<VOID **>(&cq));
}

void Client::run(Window *windows, size_t count, sockaddr_in &remote_addr)
{
    OVERLAPPED overlapped = {0};
    for (size_t i = 0; i < count; ++i)
    {
        Window window = windows[i];
        IND2Connector *connector = nullptr;
        adapter->CreateConnector(IID_IND2Connector, adapter_file, reinterpret_cast<VOID **>(&connector));
        HRESULT hr = connector->Bind(
            reinterpret_cast<const sockaddr *>(&local_addr),
            sizeof(local_addr));
        if (hr == ND_PENDING)
        {
            hr = connector->GetOverlappedResult(&overlapped, TRUE);
        }
        IND2QueuePair *qp;
        DWORD queueDepth = min(adapter_info.MaxCompletionQueueDepth, adapter_info.MaxReceiveQueueDepth);
        adapter->CreateQueuePair(
            IID_IND2QueuePair,
            cq,
            cq,
            nullptr,
            queueDepth,
            queueDepth,
            1,
            1,
            0,
            reinterpret_cast<VOID **>(&qp));
        IND2MemoryRegion *frame_region;
        adapter->CreateMemoryRegion(
            IID_IND2MemoryRegion,
            adapter_file,
            reinterpret_cast<VOID **>(&frame_region));
        size_t buffer_size = window.pixel_count * 3;
        char *buffer = static_cast<char *>(HeapAlloc(GetProcessHeap(), 0, buffer_size));
        HRESULT hr2 = frame_region->Register(
            buffer,
            buffer_size,
            ND_MR_FLAG_ALLOW_LOCAL_WRITE,
            &overlapped);
        if (hr2 == ND_PENDING)
        {
            hr2 = frame_region->GetOverlappedResult(&overlapped, TRUE);
        }
        HRESULT hr3 = connector->Connect(
            qp,
            reinterpret_cast<const sockaddr *>(&remote_addr),
            sizeof(remote_addr),
            1,
            1,
            &window,
            sizeof(window),
            &overlapped);
        if (hr3 == ND_PENDING)
        {
            hr3 = connector->GetOverlappedResult(&overlapped, TRUE);
        }
        if (hr3 != ND_SUCCESS)
        {
            printf("Failed to connect: hr3 = 0x%08lX\n", hr3);
            exit(EXIT_FAILURE);
        }
        printf("connected\n");
        ULONG len = 0;
        connector->GetPrivateData(nullptr, &len);
        char *frame_region_info = static_cast<char *>(malloc(len));
        connector->GetPrivateData(frame_region_info, &len);
        struct RemoteFrameRegion *remote_frame_region = reinterpret_cast<struct RemoteFrameRegion *>(frame_region_info);
        printf("RemoteRegion info: address=%llu, token=%u\n",
               (unsigned long long)remote_frame_region->address,
               remote_frame_region->token);
        HRESULT hr4 = connector->CompleteConnect(&overlapped);
        if (hr4 == ND_PENDING)
        {
            hr4 = connector->GetOverlappedResult(&overlapped, TRUE);
        }
        for (;;)
        {
            write_frame(buffer, buffer_size, qp, frame_region, remote_frame_region);
        }
    }
}

void Client::write_frame(char *buffer, size_t buffer_size, IND2QueuePair *qp, IND2MemoryRegion *frame_region, RemoteFrameRegion *remote_frame_region)
{
    Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
    D3D11_TEXTURE2D_DESC desc = {};
    int cap_ret = capture_frame(nullptr, nullptr, tex, &desc);
    if (cap_ret != 0)
    {
        printf("capture_frame failed: %d\n", cap_ret);
        exit(EXIT_FAILURE);
    }
    Microsoft::WRL::ComPtr<ID3D11Device> device;
    tex->GetDevice(&device);
    Microsoft::WRL::ComPtr<ID3D11DeviceContext> context;
    device->GetImmediateContext(&context);
    D3D11_MAPPED_SUBRESOURCE mapped = {};
    HRESULT hr_map = context->Map(tex.Get(), 0, D3D11_MAP_READ, 0, &mapped);
    if (!SUCCEEDED(hr_map))
    {
        printf("context->Map failed\n");
        exit(EXIT_FAILURE);
    }
    int width = desc.Width;
    int height = desc.Height;
    for (int y = 0; y < height; ++y)
    {
        unsigned char *src = (unsigned char *)mapped.pData + y * mapped.RowPitch;
        unsigned char *dst = (unsigned char *)buffer + y * width * 3;
        for (int x = 0; x < width; ++x)
        {
            dst[x * 3 + 2] = src[x * 4 + 2];
            dst[x * 3 + 1] = src[x * 4 + 1];
            dst[x * 3 + 0] = src[x * 4 + 0];
        }
    }
    context->Unmap(tex.Get(), 0);
    ND2_SGE sge = {0};
    sge.Buffer = buffer;
    sge.BufferLength = (ULONG)(width * height * 3);
    sge.MemoryRegionToken = frame_region->GetLocalToken();
    LARGE_INTEGER freq, t1, t2;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t1);
    HRESULT hr_write = qp->Write(0, &sge, 1, remote_frame_region->address, remote_frame_region->token, 0);
    if (hr_write != ND_SUCCESS)
    {
        printf("qp->Write failed: 0x%08lX\n", hr_write);
    }
    wait();
    QueryPerformanceCounter(&t2);
    double elapsed_ms = (double)(t2.QuadPart - t1.QuadPart) * 1000.0 / freq.QuadPart;
    printf("Write: %.3f ms\n", elapsed_ms);
}

void Client::wait()
{
    for (;;)
    {
        ND2_RESULT ndRes;
        if (cq->GetResults(&ndRes, 1) == 1)
        {
            if (ND_SUCCESS != ndRes.Status)
            {
                printf("cq->GetResults failed: 0x%08lX\n", ndRes.Status);
                exit(EXIT_FAILURE);
            }
            break;
        }
    };
}