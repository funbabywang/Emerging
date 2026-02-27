// stdio.cpp - Emerging 标准 I/O 函数（基于 Windows API，无系统头文件）
#include <stdarg.h>  // 唯一包含的头文件，用于可变参数

// 手动声明所需的 Windows API
extern "C" {
    void* __stdcall GetStdHandle(unsigned int nStdHandle);
    int __stdcall WriteConsoleA(void* hConsoleOutput, const char* lpBuffer, unsigned int nNumberOfCharsToWrite, unsigned int* lpNumberOfCharsWritten, void* lpReserved);
    int __stdcall ReadConsoleA(void* hConsoleInput, char* lpBuffer, unsigned int nNumberOfCharsToRead, unsigned int* lpNumberOfCharsRead, void* lpReserved);
    int __stdcall WriteFile(void* hFile, const char* lpBuffer, unsigned int nNumberOfBytesToWrite, unsigned int* lpNumberOfBytesWritten, void* lpOverlapped);
    int __stdcall ReadFile(void* hFile, char* lpBuffer, unsigned int nNumberOfBytesToRead, unsigned int* lpNumberOfBytesRead, void* lpOverlapped);
    void* __stdcall CreateFileA(const char* lpFileName, unsigned int dwDesiredAccess, unsigned int dwShareMode, void* lpSecurityAttributes, unsigned int dwCreationDisposition, unsigned int dwFlagsAndAttributes, void* hTemplateFile);
    int __stdcall CloseHandle(void* hObject);
}

// 定义 EOF 常量
#define EOF (-1)

// 标准输入输出句柄缓存
static void* hStdOut = 0;
static void* hStdIn = 0;

static void* GetStdOut() {
    if (!hStdOut) hStdOut = GetStdHandle((unsigned int)-11);  // STD_OUTPUT_HANDLE
    return hStdOut;
}

static void* GetStdIn() {
    if (!hStdIn) hStdIn = GetStdHandle((unsigned int)-10);    // STD_INPUT_HANDLE
    return hStdIn;
}

// 简单的字符串长度函数（避免依赖 strlen）
static int my_strlen(const char* s) {
    int len = 0;
    while (s[len]) len++;
    return len;
}

// 简单的整数转字符串（用于 printf 模拟）
static void my_itoa(int val, char* buf) {
    int i = 0;
    int is_negative = 0;
    if (val < 0) {
        is_negative = 1;
        val = -val;
    }
    do {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    } while (val > 0);
    if (is_negative) buf[i++] = '-';
    buf[i] = '\0';
    // 反转字符串
    for (int j = 0; j < i / 2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = tmp;
    }
}

extern "C" {

    // int putchar(int c)
    __declspec(dllexport) int putchar(int c) {
        char ch = (char)c;
        unsigned int written;
        if (WriteConsoleA(GetStdOut(), &ch, 1, &written, 0)) {
            return c;
        }
        return EOF;
    }

    // int getchar()
    __declspec(dllexport) int getchar() {
        char ch;
        unsigned int read;
        if (ReadConsoleA(GetStdIn(), &ch, 1, &read, 0)) {
            return (int)ch;
        }
        return EOF;
    }

    // int puts(const char* s)
    __declspec(dllexport) int puts(const char* s) {
        unsigned int written;
        int len = my_strlen(s);
        if (WriteConsoleA(GetStdOut(), s, len, &written, 0)) {
            WriteConsoleA(GetStdOut(), "\r\n", 2, &written, 0);
            return 1;
        }
        return EOF;
    }

    // int printf(const char* format, ...)
    __declspec(dllexport) int printf(const char* format, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, format);

        // 非常简单的格式化：仅支持 %d 和 %s
        int buf_idx = 0;
        for (int i = 0; format[i] && buf_idx < 1023; i++) {
            if (format[i] == '%') {
                i++;
                if (format[i] == 'd') {
                    int val = va_arg(args, int);
                    char num[32];
                    my_itoa(val, num);
                    for (int j = 0; num[j] && buf_idx < 1023; j++) {
                        buffer[buf_idx++] = num[j];
                    }
                }
                else if (format[i] == 's') {
                    const char* str = va_arg(args, const char*);
                    for (int j = 0; str[j] && buf_idx < 1023; j++) {
                        buffer[buf_idx++] = str[j];
                    }
                }
                else {
                    buffer[buf_idx++] = '%';
                    if (format[i]) buffer[buf_idx++] = format[i];
                }
            }
            else {
                buffer[buf_idx++] = format[i];
            }
        }
        buffer[buf_idx] = '\0';
        va_end(args);

        unsigned int written;
        if (WriteConsoleA(GetStdOut(), buffer, buf_idx, &written, 0)) {
            return buf_idx;
        }
        return EOF;
    }

    // int fopen(const char* filename, const char* mode)
    __declspec(dllexport) int fopen(const char* filename, const char* mode) {
        unsigned int access = 0, creation = 0;
        if (mode[0] == 'r') {
            access = 0x80000000;  // GENERIC_READ
            creation = 3;          // OPEN_EXISTING
        }
        else if (mode[0] == 'w') {
            access = 0x40000000;   // GENERIC_WRITE
            creation = 2;          // CREATE_ALWAYS
        }
        else if (mode[0] == 'a') {
            access = 0x40000000;   // GENERIC_WRITE
            creation = 4;          // OPEN_ALWAYS
        }
        else {
            return 0;
        }
        void* hFile = CreateFileA(filename, access, 1, 0, creation, 0x80, 0);  // FILE_ATTRIBUTE_NORMAL = 0x80
        return (int)hFile;
    }

    // int fclose(int fp)
    __declspec(dllexport) int fclose(int fp) {
        return CloseHandle((void*)fp) ? 0 : EOF;
    }

    // int fprintf(int fp, const char* format, ...)
    __declspec(dllexport) int fprintf(int fp, const char* format, ...) {
        char buffer[1024];
        va_list args;
        va_start(args, format);
        // 同样的简单格式化
        int buf_idx = 0;
        for (int i = 0; format[i] && buf_idx < 1023; i++) {
            if (format[i] == '%') {
                i++;
                if (format[i] == 'd') {
                    int val = va_arg(args, int);
                    char num[32];
                    my_itoa(val, num);
                    for (int j = 0; num[j] && buf_idx < 1023; j++) {
                        buffer[buf_idx++] = num[j];
                    }
                }
                else if (format[i] == 's') {
                    const char* str = va_arg(args, const char*);
                    for (int j = 0; str[j] && buf_idx < 1023; j++) {
                        buffer[buf_idx++] = str[j];
                    }
                }
                else {
                    buffer[buf_idx++] = '%';
                    if (format[i]) buffer[buf_idx++] = format[i];
                }
            }
            else {
                buffer[buf_idx++] = format[i];
            }
        }
        buffer[buf_idx] = '\0';
        va_end(args);

        unsigned int written;
        if (WriteFile((void*)fp, buffer, buf_idx, &written, 0)) {
            return buf_idx;
        }
        return EOF;
    }

    // int fgetc(int fp)
    __declspec(dllexport) int fgetc(int fp) {
        char ch;
        unsigned int read;
        if (ReadFile((void*)fp, &ch, 1, &read, 0) && read == 1) {
            return (int)ch;
        }
        return EOF;
    }

    // int fputc(int c, int fp)
    __declspec(dllexport) int fputc(int c, int fp) {
        char ch = (char)c;
        unsigned int written;
        if (WriteFile((void*)fp, &ch, 1, &written, 0) && written == 1) {
            return c;
        }
        return EOF;
    }

} // extern "C"