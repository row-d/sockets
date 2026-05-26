
#if defined(_WIN32)
// si quiero usar windows.h, definir WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "Ws2_32.lib")
#endif

int main()
{
#if defined(_WIN32)
        SOCKET ClientSocket;
#endif
}