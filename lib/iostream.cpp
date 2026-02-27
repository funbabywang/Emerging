// iostream.cpp - Emerging 流式 I/O（基于 stdio 函数）
#include <stdarg.h>  // 仅用于可变参数

// 声明 stdio 中实现的函数
extern "C" {
    int putchar(int);
    int getchar();
    int puts(const char*);
    int printf(const char*, ...);
}

// 定义 EOF
#define EOF (-1)

// 简单的整数转字符串（重复实现，但可独立）
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
    for (int j = 0; j < i / 2; j++) {
        char tmp = buf[j];
        buf[j] = buf[i - 1 - j];
        buf[i - 1 - j] = tmp;
    }
}

// 简单的字符串转整数（替代 atoi）
static int my_atoi(const char* s) {
    int result = 0;
    int sign = 1;
    while (*s == ' ') s++;
    if (*s == '-') {
        sign = -1;
        s++;
    }
    else if (*s == '+') {
        s++;
    }
    while (*s >= '0' && *s <= '9') {
        result = result * 10 + (*s - '0');
        s++;
    }
    return result * sign;
}

extern "C" {

    // 全局变量
    __declspec(dllexport) int cout = 1;
    __declspec(dllexport) int cin = 0;
    __declspec(dllexport) int endl = 10;

    __declspec(dllexport) int operator_put_int(int stream, int val) {
        if (stream == 1) { // cout
            char buffer[32];
            my_itoa(val, buffer);
            for (int i = 0; buffer[i]; i++) putchar(buffer[i]);
            return val;
        }
        return EOF;
    }

    __declspec(dllexport) int operator_put_str(int stream, const char* s) {
        if (stream == 1) { // cout
            return puts(s);
        }
        return EOF;
    }

    __declspec(dllexport) int operator_put_endl(int stream, int) {
        if (stream == 1) { // cout
            putchar('\n');
            return 1;
        }
        return EOF;
    }

    __declspec(dllexport) int operator_get_int(int stream, int* val) {
        if (stream == 0) { // cin
            char buffer[32];
            int i = 0;
            int c;
            while ((c = getchar()) != EOF && (c == ' ' || c == '\t' || c == '\n'));
            if (c == EOF) return EOF;
            buffer[i++] = (char)c;
            while ((c = getchar()) != EOF && c != ' ' && c != '\t' && c != '\n') {
                if (i < 31) buffer[i++] = (char)c;
            }
            buffer[i] = '\0';
            *val = my_atoi(buffer);
            return 1;
        }
        return EOF;
    }

    __declspec(dllexport) int operator_get_str(int stream, char* s) {
        if (stream == 0) { // cin
            int i = 0;
            int c;
            while ((c = getchar()) != EOF && (c == ' ' || c == '\t' || c == '\n'));
            if (c == EOF) return EOF;
            s[i++] = (char)c;
            while ((c = getchar()) != EOF && c != ' ' && c != '\t' && c != '\n') {
                s[i++] = (char)c;
            }
            s[i] = '\0';
            return 1;
        }
        return EOF;
    }

} // extern "C"