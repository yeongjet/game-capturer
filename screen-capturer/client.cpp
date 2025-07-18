#define _WIN32_WINNT 0x0600
#include "client.h"

#include <winsock2.h>
#include <ws2tcpip.h>

Client::Client(const struct sockaddr_in &local_addr)
    : local_addr(local_addr), adapter(nullptr), adapter_file(nullptr), cq(nullptr), listener(nullptr), adapter_info()
{
    HRESULT hr = NdOpenAdapter(
        IID_IND2Adapter,
        reinterpret_cast<const struct sockaddr *>(&local_addr),
        sizeof(local_addr),
        reinterpret_cast<void **>(&adapter));
    adapter->CreateOverlappedFile(&adapter_file);
    ULONG size = sizeof(adapter_info);
    memset(&adapter_info, 0, size);
    adapter->Query(&adapter_info, &size);
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
            ND_MR_FLAG_ALLOW_REMOTE_WRITE,
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
        if (hr == ND_PENDING)
        {
            hr = connector->GetOverlappedResult(&overlapped, TRUE);
        }
        printf("connected");
        ULONG len = 0;
        connector->GetPrivateData(nullptr, &len);
        char *frame_region_info = static_cast<char *>(malloc(len));
        connector->GetPrivateData(frame_region_info, &len);
        struct RemoteFrameRegion *remote_frame_region = reinterpret_cast<struct RemoteFrameRegion *>(frame_region_info);
        // 打印 remote_region 内容
        printf("RemoteRegion info: address=%llu, token=%u\n",
               (unsigned long long)remote_frame_region->address,
               remote_frame_region->token);
    }
}