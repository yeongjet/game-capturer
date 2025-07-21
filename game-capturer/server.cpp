#define _WIN32_WINNT 0x0600
#include "server.h"
#include <winsock2.h>
#include <ws2tcpip.h>
#include <algorithm>
#include <thread>
#include <memory>

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
		char *channel_info = static_cast<char *>(malloc(len));
		connector->GetPrivateData(channel_info, &len);
		struct Channel *channel = reinterpret_cast<struct Channel *>(channel_info);
		IND2MemoryRegion *frame_region;
		adapter->CreateMemoryRegion(
			IID_IND2MemoryRegion,
			adapter_file,
			reinterpret_cast<VOID **>(&frame_region));
		size_t buffer_size = channel->window.width * channel->window.height * 3;
		// auto buffer = std::shared_ptr<char[]>(static_cast<char *>(HeapAlloc(GetProcessHeap(), 0, buffer_size)), [](char* p){ if(p) HeapFree(GetProcessHeap(), 0, p); });
		// 创建共享内存
		HANDLE hMapFile = CreateFileMappingW(
			INVALID_HANDLE_VALUE,
			NULL,
			PAGE_READWRITE,
			0,
			(DWORD)buffer_size,
			L"game capturer");
		if (hMapFile == NULL)
		{
			printf("CreateFileMapping failed: %lu\n", GetLastError());
			exit(EXIT_FAILURE);
		}
		char *raw_buffer = (char *)MapViewOfFile(hMapFile, FILE_MAP_ALL_ACCESS, 0, 0, buffer_size);
		if (!raw_buffer)
		{
			printf("MapViewOfFile failed: %lu\n", GetLastError());
			CloseHandle(hMapFile);
			exit(EXIT_FAILURE);
		}
		// 用 shared_ptr 管理，自动释放
		auto buffer = std::shared_ptr<char[]>(raw_buffer, [hMapFile](char *p)
											  { if(p) UnmapViewOfFile(p); CloseHandle(hMapFile); });
		HRESULT hr2 = frame_region->Register(
			buffer.get(),
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
		HRESULT hr3 = connector->Accept(
			qp,
			1,
			1,
			nullptr,
			0,
			&overlapped);
		if (hr3 == ND_PENDING)
		{
			hr3 = connector->GetOverlappedResult(&overlapped, TRUE);
		}
		std::thread([this, buffer, qp, token = frame_region->GetLocalToken(), channel]()
					{
			while (true) {
				this->read_frame(buffer, qp, token, channel);
			} })
			.detach();
		// create_window(buffer, channel->window.width, channel->window.height);
		while (true)
		{
		}
	}
}

LRESULT CALLBACK BufferDisplayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProcW(hwnd, msg, wParam, lParam);
}

// void Server::create_window(std::shared_ptr<char[]> buffer, int width, int height)
// {
//    WNDCLASSW wc = {0};
//    wc.lpfnWndProc = BufferDisplayWndProc;
//    wc.hInstance = GetModuleHandleW(NULL);
//    wc.lpszClassName = L"BufferDisplayWindow";
//    RegisterClassW(&wc);
// 	// 计算窗口外部尺寸，使客户区正好为 width x height
// 	RECT rc = {0, 0, width, height};
// 	AdjustWindowRectEx(&rc, WS_OVERLAPPEDWINDOW, FALSE, 0);
// 	int win_width = rc.right - rc.left;
// 	int win_height = rc.bottom - rc.top;
// 	HWND hwnd = CreateWindowExW(0, L"BufferDisplayWindow", L"Frame", WS_OVERLAPPEDWINDOW,
// 								CW_USEDEFAULT, CW_USEDEFAULT, win_width, win_height, NULL, NULL, GetModuleHandleW(NULL), NULL);
// 	ShowWindow(hwnd, SW_SHOW);
// 	HDC hdc = GetDC(hwnd);
// 	BITMAPINFO bmi = {0};
// 	bmi.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
// 	bmi.bmiHeader.biWidth = width;
// 	bmi.bmiHeader.biHeight = -height; // 负数表示正向
// 	bmi.bmiHeader.biPlanes = 1;
// 	bmi.bmiHeader.biBitCount = 24;
// 	bmi.bmiHeader.biCompression = BI_RGB;
// 	MSG msg;
// 	while (true)
// 	{
// 		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
// 		{
// 			if (msg.message == WM_QUIT)
// 				return;
// 			if (msg.message == WM_KEYDOWN && msg.wParam == VK_ESCAPE)
// 				return;
// 			TranslateMessage(&msg);
// 			DispatchMessage(&msg);
// 		}
// 		// 直接以原始大小显示
// 		StretchDIBits(hdc, 0, 0, width, height, 0, 0, width, height, buffer.get(), &bmi, DIB_RGB_COLORS, SRCCOPY);
// 	}
// 	ReleaseDC(hwnd, hdc);
// 	DestroyWindow(hwnd);
// }

void Server::read_frame(std::shared_ptr<char[]> buffer, IND2QueuePair *qp, UINT32 local_token, Channel *channel)
{
	int width = channel->window.width;
	int height = channel->window.height;
	uint64_t remote_address = channel->remote_address;
	uint32_t remote_token = channel->remote_token;
	ND2_SGE sge = {0};
	sge.Buffer = buffer.get();
	sge.BufferLength = (ULONG)(width * height * 3);
	sge.MemoryRegionToken = local_token;
	LARGE_INTEGER freq, t1, t2;
	QueryPerformanceFrequency(&freq);
	QueryPerformanceCounter(&t1);
	HRESULT hr = qp->Read(0, &sge, 1, remote_address, remote_token, 0);
	if (hr != ND_SUCCESS)
	{
		printf("qp->Read failed: 0x%08lX\n", hr);
	}
	wait();
	QueryPerformanceCounter(&t2);
	double ms = (t2.QuadPart - t1.QuadPart) * 1000.0 / freq.QuadPart;
	printf("read_frame elapsed time: %.3f ms\n", ms);
}

void Server::wait()
{
	for (;;)
	{
		ND2_RESULT res;
		if (cq->GetResults(&res, 1) == 1)
		{
			if (ND_SUCCESS != res.Status)
			{
				printf("cq->GetResults failed: 0x%08lX\n", res.Status);
				exit(EXIT_FAILURE);
			}
			break;
		}
	};
}