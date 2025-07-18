// screen-capturer.cpp : This file contains the 'main' function. Program execution begins and ends there.
//

#include <iostream>
#include "server.h"

int main(int argc, TCHAR* argv[])
{
	bool is_server = false;
	bool is_client = false;
	struct sockaddr_in v4Server = { 0 };
	WSADATA wsaData;
	WSAStartup(MAKEWORD(2, 2), &wsaData);

	for (int i = 1; i < argc; i++)
	{
		TCHAR* arg = argv[i];
		if ((_tcscmp(arg, _T("-s")) == 0) || (_tcscmp(arg, _T("-S")) == 0))
		{
			is_server = true;
		}
		else if ((_tcscmp(arg, _T("-c")) == 0) || (_tcscmp(arg, _T("-C")) == 0))
		{
			is_client = true;
		}
	}
	int len = sizeof(v4Server);
	WSAStringToAddress(argv[argc - 1], AF_INET, nullptr,
		reinterpret_cast<struct sockaddr*>(&v4Server), &len);
	NdStartup();
	if (is_server)
	{
		Server server(v4Server);
		server.run();
	}
	else
	{
		//struct sockaddr_in v4Src;
		//SIZE_T len = sizeof(v4Src);
		//HRESULT hr = NdResolveAddress((const struct sockaddr*)&v4Server,
		//    sizeof(v4Server), (struct sockaddr*)&v4Src, &len);

		//Client client;
		//client.RunTest(v4Src, v4Server, 0, 0);
	}
	NdCleanup();
	_fcloseall();
	WSACleanup();
	return 0;
}
