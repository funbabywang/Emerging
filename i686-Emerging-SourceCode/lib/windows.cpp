// windows.cpp - Windows API 封装（不包含 windows.h，手动声明 API）

// 手动声明 Windows API 函数 (__stdcall)
extern "C" {
    int __stdcall MessageBoxA(void* hWnd, const char* lpText, const char* lpCaption, unsigned int uType);
    void* __stdcall GetStdHandle(unsigned int nStdHandle);
    int __stdcall WriteConsoleA(void* hConsoleOutput, const char* lpBuffer, unsigned int nNumberOfCharsToWrite, unsigned int* lpNumberOfCharsWritten, void* lpReserved);
    int __stdcall ReadConsoleA(void* hConsoleInput, char* lpBuffer, unsigned int nNumberOfCharsToRead, unsigned int* lpNumberOfCharsRead, void* lpReserved);
    void __stdcall ExitProcess(unsigned int uExitCode);
    void* __stdcall GetModuleHandleA(const char* lpModuleName);
    void* __stdcall CreateFileA(const char* lpFileName, unsigned int dwDesiredAccess, unsigned int dwShareMode, void* lpSecurityAttributes, unsigned int dwCreationDisposition, unsigned int dwFlagsAndAttributes, void* hTemplateFile);
    int __stdcall CloseHandle(void* hObject);
    int __stdcall ReadFile(void* hFile, char* lpBuffer, unsigned int nNumberOfBytesToRead, unsigned int* lpNumberOfBytesRead, void* lpOverlapped);
    int __stdcall WriteFile(void* hFile, const char* lpBuffer, unsigned int nNumberOfBytesToWrite, unsigned int* lpNumberOfBytesWritten, void* lpOverlapped);
}

// 内部函数（加前缀 Emg），供 .def 映射到标准名称
extern "C" {

    __declspec(dllexport) int EmgMessageBoxA(int hWnd, const char* lpText, const char* lpCaption, int uType) {
        return MessageBoxA((void*)hWnd, lpText, lpCaption, (unsigned int)uType);
    }

    __declspec(dllexport) int EmgGetStdHandle(int nStdHandle) {
        return (int)GetStdHandle((unsigned int)nStdHandle);
    }

    __declspec(dllexport) int EmgWriteConsoleA(int hConsoleOutput, const char* lpBuffer, int nNumberOfCharsToWrite, int* lpNumberOfCharsWritten, int lpReserved) {
        unsigned int written;
        int result = WriteConsoleA((void*)hConsoleOutput, lpBuffer, (unsigned int)nNumberOfCharsToWrite, &written, (void*)lpReserved);
        if (lpNumberOfCharsWritten) *lpNumberOfCharsWritten = (int)written;
        return result;
    }

    __declspec(dllexport) int EmgReadConsoleA(int hConsoleInput, char* lpBuffer, int nNumberOfCharsToRead, int* lpNumberOfCharsRead, int lpReserved) {
        unsigned int read;
        int result = ReadConsoleA((void*)hConsoleInput, lpBuffer, (unsigned int)nNumberOfCharsToRead, &read, (void*)lpReserved);
        if (lpNumberOfCharsRead) *lpNumberOfCharsRead = (int)read;
        return result;
    }

    __declspec(dllexport) void EmgExitProcess(int uExitCode) {
        ExitProcess((unsigned int)uExitCode);
    }

    __declspec(dllexport) int EmgGetModuleHandleA(const char* lpModuleName) {
        return (int)GetModuleHandleA(lpModuleName);
    }

    __declspec(dllexport) int EmgCreateFileA(const char* lpFileName, int dwDesiredAccess, int dwShareMode, int lpSecurityAttributes, int dwCreationDisposition, int dwFlagsAndAttributes, int hTemplateFile) {
        return (int)CreateFileA(lpFileName, (unsigned int)dwDesiredAccess, (unsigned int)dwShareMode, (void*)lpSecurityAttributes, (unsigned int)dwCreationDisposition, (unsigned int)dwFlagsAndAttributes, (void*)hTemplateFile);
    }

    __declspec(dllexport) int EmgCloseHandle(int hObject) {
        return CloseHandle((void*)hObject);
    }

    __declspec(dllexport) int EmgReadFile(int hFile, char* lpBuffer, int nNumberOfBytesToRead, int* lpNumberOfBytesRead, int lpOverlapped) {
        unsigned int read;
        int result = ReadFile((void*)hFile, lpBuffer, (unsigned int)nNumberOfBytesToRead, &read, (void*)lpOverlapped);
        if (lpNumberOfBytesRead) *lpNumberOfBytesRead = (int)read;
        return result;
    }

    __declspec(dllexport) int EmgWriteFile(int hFile, const char* lpBuffer, int nNumberOfBytesToWrite, int* lpNumberOfBytesWritten, int lpOverlapped) {
        unsigned int written;
        int result = WriteFile((void*)hFile, lpBuffer, (unsigned int)nNumberOfBytesToWrite, &written, (void*)lpOverlapped);
        if (lpNumberOfBytesWritten) *lpNumberOfBytesWritten = (int)written;
        return result;
    }

} // extern "C"