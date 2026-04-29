#include <windows.h>
#include <stdlib.h>
#include <stdio.h>

BOOL __stdcall tdo_mock_ReadFile(HANDLE hFile, LPVOID lpBuffer, DWORD nNumberOfBytesToRead, LPDWORD lpNumberOfBytesRead, LPOVERLAPPED lpOverlapped) {
    if (nNumberOfBytesToRead < 1) {
        fprintf(stderr, "Attempting to read zero bytes???\n");
        abort();
    }
    return ReadFile(hFile, lpBuffer, 1, lpNumberOfBytesRead, lpOverlapped);
}
