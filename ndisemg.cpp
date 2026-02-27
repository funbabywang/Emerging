#include <iostream>
#include <fstream>
#include <vector>
#include <cstdint>
#include <cstring>
#include <iomanip>
#include <string>
#include <sstream>

using namespace std;

// 读取整个文件到内存
vector<uint8_t> readFile(const string& filename) {
    ifstream in(filename, ios::binary);
    if (!in) {
        cerr << "错误: 无法打开文件 " << filename << endl;
        return {};
    }
    return vector<uint8_t>((istreambuf_iterator<char>(in)), istreambuf_iterator<char>());
}

// 将整数转换为十六进制字符串（带0x前缀）
string to_hex(uint16_t val) {
    stringstream ss;
    ss << "0x" << hex << val;
    return ss.str();
}

// 将有符号8位整数转换为字符串（显示为 +/-数值）
string disp8_to_str(int8_t disp) {
    if (disp < 0)
        return "-" + to_string(-disp);
    else
        return "+" + to_string(disp);
}

// 反汇编一条指令，返回指令长度，并将助记符写入 mnemonic
int disassemble_inst(const uint8_t* code, size_t size, string& mnemonic) {
    if (size < 1) return 0;

    uint8_t op = code[0];
    mnemonic = "db 0x" + to_hex(op); // 默认未知指令

    // 单字节指令
    switch (op) {
    case 0x0E: mnemonic = "push cs"; return 1;
    case 0x1E: mnemonic = "push ds"; return 1;
    case 0x1F: mnemonic = "pop ds"; return 1;
    case 0x50: mnemonic = "push ax"; return 1;
    case 0x51: mnemonic = "push cx"; return 1;
    case 0x52: mnemonic = "push dx"; return 1;
    case 0x53: mnemonic = "push bx"; return 1;
    case 0x54: mnemonic = "push sp"; return 1;
    case 0x55: mnemonic = "push bp"; return 1;
    case 0x56: mnemonic = "push si"; return 1;
    case 0x57: mnemonic = "push di"; return 1;
    case 0x58: mnemonic = "pop ax"; return 1;
    case 0x59: mnemonic = "pop cx"; return 1;
    case 0x5A: mnemonic = "pop dx"; return 1;
    case 0x5B: mnemonic = "pop bx"; return 1;
    case 0x5C: mnemonic = "pop sp"; return 1;
    case 0x5D: mnemonic = "pop bp"; return 1;
    case 0x5E: mnemonic = "pop si"; return 1;
    case 0x5F: mnemonic = "pop di"; return 1;
    case 0x60: mnemonic = "pusha"; return 1;
    case 0x61: mnemonic = "popa"; return 1;
    case 0xC3: mnemonic = "ret"; return 1;
    case 0x41: mnemonic = "inc cx"; return 1;
        // 更多单字节可添加
    }

    // 多字节指令
    if (op == 0xB8) { // mov ax, imm16
        if (size < 3) return 0;
        uint16_t imm = *(uint16_t*)(code + 1);
        mnemonic = "mov ax, " + to_hex(imm);
        return 3;
    }
    else if (op == 0xB9) { // mov cx, imm16
        if (size < 3) return 0;
        uint16_t imm = *(uint16_t*)(code + 1);
        mnemonic = "mov cx, " + to_hex(imm);
        return 3;
    }
    else if (op == 0xBA) { // mov dx, imm16
        if (size < 3) return 0;
        uint16_t imm = *(uint16_t*)(code + 1);
        mnemonic = "mov dx, " + to_hex(imm);
        return 3;
    }
    else if (op == 0xBB) { // mov bx, imm16
        if (size < 3) return 0;
        uint16_t imm = *(uint16_t*)(code + 1);
        mnemonic = "mov bx, " + to_hex(imm);
        return 3;
    }
    else if (op == 0xB4) { // mov ah, imm8
        if (size < 2) return 0;
        uint8_t imm = code[1];
        mnemonic = "mov ah, " + to_hex(imm);
        return 2;
    }
    else if (op == 0xB2) { // mov dl, imm8
        if (size < 2) return 0;
        uint8_t imm = code[1];
        mnemonic = "mov dl, " + to_hex(imm);
        return 2;
    }
    else if (op == 0xA1) { // mov ax, [imm16]
        if (size < 3) return 0;
        uint16_t addr = *(uint16_t*)(code + 1);
        mnemonic = "mov ax, [" + to_hex(addr) + "]";
        return 3;
    }
    else if (op == 0xA3) { // mov [imm16], ax
        if (size < 3) return 0;
        uint16_t addr = *(uint16_t*)(code + 1);
        mnemonic = "mov [" + to_hex(addr) + "], ax";
        return 3;
    }
    else if (op == 0xE8) { // call rel16
        if (size < 3) return 0;
        int16_t disp = *(int16_t*)(code + 1);
        mnemonic = "call " + to_hex(disp);
        return 3;
    }
    else if (op == 0xE9) { // jmp rel16
        if (size < 3) return 0;
        int16_t disp = *(int16_t*)(code + 1);
        mnemonic = "jmp " + to_hex(disp);
        return 3;
    }
    else if (op == 0xEB) { // jmp short rel8
        if (size < 2) return 0;
        int8_t disp = (int8_t)code[1];
        mnemonic = "jmp short " + to_string((int)disp);
        return 2;
    }
    else if (op == 0x74) { // je rel8
        if (size < 2) return 0;
        int8_t disp = (int8_t)code[1];
        mnemonic = "je " + to_string((int)disp);
        return 2;
    }
    else if (op == 0x75) { // jne rel8
        if (size < 2) return 0;
        int8_t disp = (int8_t)code[1];
        mnemonic = "jne " + to_string((int)disp);
        return 2;
    }
    else if (op == 0x7C) { // jl rel8
        if (size < 2) return 0;
        int8_t disp = (int8_t)code[1];
        mnemonic = "jl " + to_string((int)disp);
        return 2;
    }
    else if (op == 0x7D) { // jge rel8
        if (size < 2) return 0;
        int8_t disp = (int8_t)code[1];
        mnemonic = "jge " + to_string((int)disp);
        return 2;
    }
    else if (op == 0xE2) { // loop rel8
        if (size < 2) return 0;
        int8_t disp = (int8_t)code[1];
        mnemonic = "loop " + to_string((int)disp);
        return 2;
    }
    else if (op == 0xCD) { // int imm8
        if (size < 2) return 0;
        uint8_t imm = code[1];
        mnemonic = "int " + to_hex(imm);
        return 2;
    }
    else if (op == 0x3D) { // cmp ax, imm16
        if (size < 3) return 0;
        uint16_t imm = *(uint16_t*)(code + 1);
        mnemonic = "cmp ax, " + to_hex(imm);
        return 3;
    }
    else if (op == 0x80 && code[1] == 0xC2) { // add dl, imm8
        if (size < 3) return 0;
        uint8_t imm = code[2];
        mnemonic = "add dl, " + to_hex(imm);
        return 3;
    }
    else if (op == 0xF7) {
        if (size < 2) return 0;
        uint8_t modrm = code[1];
        if (modrm == 0xF3) { // div bx
            mnemonic = "div bx";
            return 2;
        }
        else if (modrm == 0xD8) { // neg ax
            mnemonic = "neg ax";
            return 2;
        }
        else {
            // 未知F7指令
            mnemonic = "db 0xF7, 0x" + to_hex(modrm);
            return 2;
        }
    }
    else if (op == 0x8B) { // mov ax, [bp+disp]
        if (size < 3) return 0;
        uint8_t modrm = code[1];
        if (modrm == 0x46) { // mov ax, [bp+disp8]
            int8_t disp = (int8_t)code[2];
            mnemonic = "mov ax, [bp" + disp8_to_str(disp) + "]";
            return 3;
        }
        else if (modrm == 0x86) { // mov ax, [bp+disp16]
            if (size < 4) return 0;
            int16_t disp = *(int16_t*)(code + 2);
            mnemonic = "mov ax, [bp" + disp8_to_str(disp) + "]";
            return 4;
        }
        else {
            mnemonic = "db 0x8B, 0x" + to_hex(modrm);
            return 2;
        }
    }
    else if (op == 0x89) { // mov [bp+disp], ax
        if (size < 3) return 0;
        uint8_t modrm = code[1];
        if (modrm == 0x46) { // mov [bp+disp8], ax
            int8_t disp = (int8_t)code[2];
            mnemonic = "mov [bp" + disp8_to_str(disp) + "], ax";
            return 3;
        }
        else if (modrm == 0x86) { // mov [bp+disp16], ax
            if (size < 4) return 0;
            int16_t disp = *(int16_t*)(code + 2);
            mnemonic = "mov [bp" + disp8_to_str(disp) + "], ax";
            return 4;
        }
        else {
            mnemonic = "db 0x89, 0x" + to_hex(modrm);
            return 2;
        }
    }

    return 1; // 未知指令，按1字节处理
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        cerr << "用法: ndisemg.exe <Emerging可执行文件.exe>" << endl;
        return 1;
    }

    string filename = argv[1];
    vector<uint8_t> data = readFile(filename);
    if (data.empty()) return 1;

    // 检查MZ头部
    if (data.size() < 2 || data[0] != 'M' || data[1] != 'Z') {
        cerr << "错误: 不是有效的MZ可执行文件" << endl;
        return 1;
    }

    // 解析头部以获取代码起始偏移
    // 简单起见，假定头部大小为512字节（我们的链接器固定为512）
    size_t code_start = 512;
    if (data.size() > 2) {
        // 更精确地，从MZ头部读取 e_cparhdr * 16
        uint16_t e_cparhdr = *(uint16_t*)(&data[8]); // 偏移8字节
        code_start = e_cparhdr * 16;
    }

    if (code_start >= data.size()) {
        cerr << "错误: 代码段起始超出文件范围" << endl;
        return 1;
    }

    const uint8_t* code = data.data() + code_start;
    size_t code_size = data.size() - code_start;

    cout << "反汇编代码段 (起始偏移 0x" << hex << code_start << "):" << endl;

    size_t offset = 0;
    while (offset < code_size) {
        string mnemonic;
        int len = disassemble_inst(code + offset, code_size - offset, mnemonic);
        if (len <= 0) break;

        // 输出地址
        cout << hex << setw(8) << setfill('0') << offset << "  ";

        // 输出机器码
        for (int i = 0; i < len; i++) {
            cout << hex << setw(2) << setfill('0') << (int)code[offset + i] << " ";
        }
        // 对齐到12个字符宽度（最多显示6字节，不够补空格）
        for (int i = len; i < 6; i++) cout << "   ";
        cout << " " << mnemonic << endl;

        offset += len;
    }

    return 0;
}