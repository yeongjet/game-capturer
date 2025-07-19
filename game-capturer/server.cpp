#define _WIN32_WINNT 0x0600
#include "server.h"
#include <winsock2.h>
#include <ws2tcpip.h>

// 循环保存buffer为图片


Server::Server(const struct sockaddr_in &local_addr)
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
	adapter->CreateListener(IID_IND2Listener, adapter_file, reinterpret_cast<VOID **>(&listener));
	listener->Bind(reinterpret_cast<const sockaddr *>(&local_addr), sizeof(local_addr));
	listener->Listen(0);
	wchar_t ipString[INET_ADDRSTRLEN];
	InetNtopW(AF_INET, &(local_addr.sin_addr), ipString, INET_ADDRSTRLEN);
	wprintf(L"Server listening on %s:%d\n", ipString, ntohs(local_addr.sin_port));
}

void Server::run()
{
	OVERLAPPED overlapped = {0};
	for (;;)
	{
		IND2Connector *connector = nullptr;
		adapter->CreateConnector(IID_IND2Connector, adapter_file, reinterpret_cast<VOID **>(&connector));
		// OVERLAPPED overlapped = { 0 };
		HRESULT hr = listener->GetConnectionRequest(connector, &overlapped);
		if (hr == ND_PENDING)
		{
			hr = listener->GetOverlappedResult(&overlapped, TRUE);
		}
		printf("connected\n");
		ULONG len = 0;
		connector->GetPrivateData(nullptr, &len);
		char *window_info = static_cast<char *>(malloc(len));
		connector->GetPrivateData(window_info, &len);
		struct Window *window = reinterpret_cast<struct Window *>(window_info);
		printf("window: id=%d, title=%s, width=%d, height=%d, pixel_count=%zu\n",
			   window->id,
			   window->title.c_str(),
			   window->width,
			   window->height,
			   window->pixel_count);
		IND2MemoryRegion *frame_region;
		adapter->CreateMemoryRegion(
			IID_IND2MemoryRegion,
			adapter_file,
			reinterpret_cast<VOID **>(&frame_region));
		size_t buffer_size = window->pixel_count * 3;
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
		struct RemoteFrameRegion *remote_frame_region;
		remote_frame_region = reinterpret_cast<struct RemoteFrameRegion *>(buffer);
		remote_frame_region->address = reinterpret_cast<uint64_t>(buffer);
		remote_frame_region->token = frame_region->GetRemoteToken();
		// 打印 remote_frame_region 内容
		printf("RemoteFrameRegion: address=%llu, token=%u\n",
			(unsigned long long)remote_frame_region->address,
			remote_frame_region->token);
		HRESULT hr3 = connector->Accept(
			qp,
			1,
			1,
			remote_frame_region,
			sizeof(*remote_frame_region),
			&overlapped);
		if (hr3 == ND_PENDING)
		{
			hr3 = connector->GetOverlappedResult(&overlapped, TRUE);
		}
		save_frame(buffer, window->width, window->height);
	}
}

// 循环保存buffer为图片
void Server::save_frame(char* buffer, int width, int height) {
	int rowPitch = width * 3;
	while (true) {
		FILE *fp = fopen("output_server.bmp", "wb");
		if (!fp) {
			printf("无法创建output_server.bmp\n");
			break;
		}
		int fileSize = 54 + rowPitch * height;
		unsigned char bmpFileHeader[14] = {
			'B','M', fileSize, fileSize>>8, fileSize>>16, fileSize>>24,
			0,0, 0,0, 54,0,0,0
		};
		unsigned char bmpInfoHeader[40] = {
			40,0,0,0, width, width>>8, width>>16, width>>24,
			height, height>>8, height>>16, height>>24,
			1,0, 24,0, 0,0,0,0,
			rowPitch*height, (rowPitch*height)>>8, (rowPitch*height)>>16, (rowPitch*height)>>24,
			0,0,0,0, 0,0,0,0, 0,0,0,0
		};
		fwrite(bmpFileHeader, 1, 14, fp);
		fwrite(bmpInfoHeader, 1, 40, fp);
		for (int y = height - 1; y >= 0; --y) {
			fwrite((unsigned char*)buffer + y * width * 3, 1, rowPitch, fp);
		}
		fclose(fp);
		printf("Frame saved to output_server.bmp\n");
		Sleep(1000); // 约30fps
	}
}
