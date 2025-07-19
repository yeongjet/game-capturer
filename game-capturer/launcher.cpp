#include <iostream>
#include "server.h"
#include "client.h"

int main(int argc, TCHAR *argv[])
{
	bool is_server = false;
	bool is_client = false;
	struct sockaddr_in remote_addr = {0};
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	for (int i = 1; i < argc; i++)
	{
		TCHAR *arg = argv[i];
		if ((_tcscmp(arg, _T("-s")) == 0) || (_tcscmp(arg, _T("-S")) == 0))
		{
			is_server = true;
		}
		else if ((_tcscmp(arg, _T("-c")) == 0) || (_tcscmp(arg, _T("-C")) == 0))
		{
			is_client = true;
		}
	}
	int len = sizeof(remote_addr);
	WSAStringToAddress(argv[argc - 1], AF_INET, nullptr,
					   reinterpret_cast<struct sockaddr *>(&remote_addr), &len);
	NdStartup();
	if (is_server)
	{
		Server server(remote_addr);
		server.run();
	}
	else
	{
		struct sockaddr_in local_addr;
		SIZE_T len = sizeof(local_addr);
		HRESULT hr = NdResolveAddress((const struct sockaddr *)&remote_addr,
									  sizeof(remote_addr), (struct sockaddr *)&local_addr, &len);
		// 创建一个 Window 数组，假设有 1 个窗口，实际可根据需要调整
		Window windows[1] = {
			{
				1,			   // id
				"test window", // title
				3840,		   // width
				2160,		   // height
				3840 * 2160	   // pixel_count
			}};
		size_t window_count = 1;
		Client client(local_addr);
		client.run(windows, window_count, remote_addr);
	}
	NdCleanup();
	_fcloseall();
	WSACleanup();
	return 0;
}
