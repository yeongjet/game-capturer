#include <stdio.h>
#include <tchar.h>

#include <stdio.h>
#include <tchar.h>
#include <process.h>
#include <new>
#include <sal.h>
#include <initguid.h>
#include <ndsupport.h>
#include <ndstatus.h>
#include <memory>

#include "channel.h"

class Server
{
protected:
    IND2Adapter *adapter;
    HANDLE adapter_file;
    ND2_ADAPTER_INFO adapter_info = {0};
    IND2CompletionQueue *cq;
    IND2Listener *listener;

public:
    Server(const struct sockaddr_in &v4Src);
    void run();
    void create_window(std::shared_ptr<char[]> buffer, int width, int height);
    void read_frame(std::shared_ptr<char[]> buffer, IND2QueuePair *qp, UINT32 local_token, Channel *channel);
    void wait();
    ~Server()
    {
        // if (m_pBuf != nullptr)
        //{
        //     HeapFree(GetProcessHeap(), 0, m_pBuf);
        // }

        // if (m_pTmpBuf != nullptr)
        //{
        //     free(m_pTmpBuf);
        // }
    }

protected:
};