// Uninstall.cpp - Emerging 卸载程序（无临时文件版）
// 编译: cl /GS- /Os Uninstall.cpp /link /SUBSYSTEM:WINDOWS user32.lib shell32.lib

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shellapi.h>
#include <stdio.h>

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    // 获取当前可执行文件路径和目录
    char exePath[MAX_PATH];
    char installDir[MAX_PATH];
    DWORD len = GetModuleFileNameA(NULL, exePath, MAX_PATH);
    if (len == 0) return 1;
    strcpy_s(installDir, exePath);
    char* lastSlash = strrchr(installDir, '\\');
    if (lastSlash) *lastSlash = '\0';

    // 检查管理员权限
    BOOL isAdmin = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD size = sizeof(elevation);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, size, &size)) {
            isAdmin = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }

    if (!isAdmin) {
        // 以管理员权限重新启动
        SHELLEXECUTEINFOA sei = { sizeof(sei) };
        sei.lpVerb = "runas";
        sei.lpFile = exePath;
        sei.nShow = SW_SHOW;
        if (ShellExecuteExA(&sei)) {
            return 0; // 提权后退出当前进程
        }
        else {
            MessageBoxA(NULL, "需要管理员权限才能卸载。", "Emerging 卸载", MB_ICONERROR);
            return 1;
        }
    }

    // 确认卸载
    int result = MessageBoxA(NULL,
        "确定要卸载 Emerging 及其所有组件吗？",
        "Emerging 卸载",
        MB_YESNO | MB_ICONQUESTION);
    if (result != IDYES) return 0;

    // 构建删除命令：使用 ping 延迟约 2 秒，然后删除目录
    char cmdLine[1024];
    // ping 127.0.0.1 -n 3 大约延迟 2 秒
    sprintf_s(cmdLine, "cmd.exe /c ping 127.0.0.1 -n 3 > nul && rmdir /s /q \"%s\"", installDir);

    STARTUPINFOA si = { sizeof(si) };
    PROCESS_INFORMATION pi;
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE; // 隐藏 cmd 窗口

    if (CreateProcessA(NULL, cmdLine, NULL, NULL, FALSE, CREATE_NO_WINDOW, NULL, NULL, &si, &pi)) {
        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
    }
    else {
        MessageBoxA(NULL, "无法启动卸载进程。", "错误", MB_ICONERROR);
        return 1;
    }

    // 退出当前进程，让子进程删除目录
    return 0;
}