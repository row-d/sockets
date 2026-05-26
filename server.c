
#if defined(_WIN32)
// si quiero usar windows.h, definir WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#pragma comment(lib, "Ws2_32.lib")
#endif

#if defined(__unix__)

#endif

#define DEFAULT_PORT "27015"

int main()
{

#if defined(_WIN32)

    struct addrinfo *result = NULL, *ptr = NULL;
    struct addrinfo hints = {
        .ai_family = AF_INET,
        .ai_socktype = SOCK_STREAM,
        .ai_protocol = IPPROTO_TCP,
        .ai_flags = AI_PASSIVE};

    int iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (0 != iResult)
        printf("error: %d\n", iResult);
#endif
    return 0;
}
