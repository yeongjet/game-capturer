#define _WIN32_WINNT 0x0600
#include "server.h"  

#include <winsock2.h>  
#include <ws2tcpip.h>  


Server::Server(const struct sockaddr_in& local_addr) {  
    HRESULT hr = NdOpenAdapter(  
        IID_IND2Adapter,  
        reinterpret_cast<const struct sockaddr*>(&local_addr),  
        sizeof(local_addr),  
        reinterpret_cast<void**>(&adapter)  
    );  
    adapter->CreateOverlappedFile(&adapter_file);  
    memset(&adapter_info, 0, sizeof(adapter_info));  
	adapter_info.InfoVersion = ND_VERSION_2;
	ULONG adapterInfoSize = sizeof(adapter_info);
    adapter->Query(&adapter_info, &adapterInfoSize);  
    adapter->CreateCompletionQueue(IID_IND2CompletionQueue, adapter_file, adapter_info.MaxCompletionQueueDepth, 0, 0, reinterpret_cast<VOID**>(&cq));  
    adapter->CreateListener(IID_IND2Listener, adapter_file, reinterpret_cast<VOID**>(&listener));  
    listener->Bind(reinterpret_cast<const sockaddr*>(&local_addr), sizeof(local_addr));  
    listener->Listen(0);  
    wchar_t ipString[INET_ADDRSTRLEN];  
    InetNtopW(AF_INET, &(local_addr.sin_addr), ipString, INET_ADDRSTRLEN);  
    wprintf(L"Server listening on %s:%d", ipString, ntohs(local_addr.sin_port));  
}

void Server::run() {
	OVERLAPPED overlapped = { 0 };
	for (;;) {
		IND2Connector* connector = nullptr;
		adapter->CreateConnector(IID_IND2Connector, adapter_file, reinterpret_cast<VOID**>(&connector));
		//OVERLAPPED overlapped = { 0 };
		HRESULT hr = listener->GetConnectionRequest(connector, &overlapped);
		if (hr == ND_PENDING)
		{
			hr = listener->GetOverlappedResult(&overlapped, TRUE);
		}
		printf("connected");
		ULONG len = 0;
		connector->GetPrivateData(nullptr, &len);
		char* window_info = static_cast<char*>(malloc(len));
		connector->GetPrivateData(window_info, &len);
		struct Window* window = reinterpret_cast<struct Window*>(window_info);
		printf("Window info: id=%d, title=%s, width=%d, height=%d, pixel_count=%zu\n",
			window->id,
			window->title,
			window->width,
			window->height,
			window->pixel_count);
		IND2MemoryRegion* frame_region;
		adapter->CreateMemoryRegion(
			IID_IND2MemoryRegion,
			adapter_file,
			reinterpret_cast<VOID**>(&frame_region)
		);
		size_t buffer_size = window->pixel_count * 3;
		char* buffer = static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, buffer_size));
		HRESULT hr2 = frame_region->Register(
			buffer,
			buffer_size,
			ND_MR_FLAG_ALLOW_REMOTE_WRITE,
			&overlapped
		);
		if (hr2 == ND_PENDING)
		{
			hr2 = frame_region->GetOverlappedResult(&overlapped, TRUE);
		}
		IND2QueuePair* qp;
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
			reinterpret_cast<VOID**>(&qp)
		);
		struct RemoteFrameRegion* remote_frame_region;
		remote_frame_region = reinterpret_cast<struct RemoteFrameRegion*>(buffer);
		remote_frame_region->address = reinterpret_cast<uint64_t>(buffer);
		remote_frame_region->token = frame_region->GetRemoteToken();
		HRESULT hr3 = connector->Accept(
			qp,
			1,
			1,
			remote_frame_region,
			sizeof(remote_frame_region),
			&overlapped
		);
		if (hr3 == ND_PENDING)
		{
			hr3 = connector->GetOverlappedResult(&overlapped, TRUE);
		}
	}
}