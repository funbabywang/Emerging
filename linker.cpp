#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cstdint>
#include <cstring>
#include <cctype>
#include <algorithm>

using namespace std;

// ---------- PE 结构定义 ----------
#pragma pack(push, 1)
struct DOSHeader {
    uint16_t e_magic;      // 'MZ'
    uint16_t e_cblp;
    uint16_t e_cp;
    uint16_t e_crlc;
    uint16_t e_cparhdr;
    uint16_t e_minalloc;
    uint16_t e_maxalloc;
    uint16_t e_ss;
    uint16_t e_sp;
    uint16_t e_csum;
    uint16_t e_ip;
    uint16_t e_cs;
    uint16_t e_lfarlc;
    uint16_t e_ovno;
    uint16_t e_res[4];
    uint16_t e_oemid;
    uint16_t e_oeminfo;
    uint16_t e_res2[10];
    uint32_t e_lfanew;
};

struct PEHeader {
    uint32_t signature;    // 'PE\0\0'
    uint16_t machine;
    uint16_t numberOfSections;
    uint32_t timeDateStamp;
    uint32_t pointerToSymbolTable;
    uint32_t numberOfSymbols;
    uint16_t sizeOfOptionalHeader;
    uint16_t characteristics;
    // Optional Header (standard)
    uint16_t magic;
    uint8_t majorLinkerVersion;
    uint8_t minorLinkerVersion;
    uint32_t sizeOfCode;
    uint32_t sizeOfInitializedData;
    uint32_t sizeOfUninitializedData;
    uint32_t addressOfEntryPoint;
    uint32_t baseOfCode;
    uint32_t baseOfData;
    // Windows-specific
    uint32_t imageBase;
    uint32_t sectionAlignment;
    uint32_t fileAlignment;
    uint16_t majorOSVersion;
    uint16_t minorOSVersion;
    uint16_t majorImageVersion;
    uint16_t minorImageVersion;
    uint16_t majorSubsystemVersion;
    uint16_t minorSubsystemVersion;
    uint32_t win32VersionValue;
    uint32_t sizeOfImage;
    uint32_t sizeOfHeaders;
    uint32_t checkSum;
    uint16_t subsystem;
    uint16_t dllCharacteristics;
    uint32_t sizeOfStackReserve;
    uint32_t sizeOfStackCommit;
    uint32_t sizeOfHeapReserve;
    uint32_t sizeOfHeapCommit;
    uint32_t loaderFlags;
    uint32_t numberOfRvaAndSizes;
    // Data Directories
    uint32_t exportRVA;
    uint32_t exportSize;
    uint32_t importRVA;
    uint32_t importSize;
    uint32_t resourceRVA;
    uint32_t resourceSize;
    uint32_t exceptionRVA;
    uint32_t exceptionSize;
    uint32_t certRVA;
    uint32_t certSize;
    uint32_t relocRVA;
    uint32_t relocSize;
    uint32_t debugRVA;
    uint32_t debugSize;
    uint32_t archRVA;
    uint32_t archSize;
    uint32_t globalPtrRVA;
    uint32_t globalPtrSize;
    uint32_t tlsRVA;
    uint32_t tlsSize;
    uint32_t loadConfigRVA;
    uint32_t loadConfigSize;
    uint32_t boundImportRVA;
    uint32_t boundImportSize;
    uint32_t iatRVA;
    uint32_t iatSize;
    uint32_t delayImportRVA;
    uint32_t delayImportSize;
    uint32_t comRVA;
    uint32_t comSize;
    uint32_t reserved[6];
};

struct SectionHeader {
    char name[8];
    uint32_t virtualSize;
    uint32_t virtualAddress;
    uint32_t sizeOfRawData;
    uint32_t pointerToRawData;
    uint32_t pointerToRelocations;
    uint32_t pointerToLineNumbers;
    uint16_t numberOfRelocations;
    uint16_t numberOfLineNumbers;
    uint32_t characteristics;
};

struct ImportDescriptor {
    uint32_t originalFirstThunk; // RVA of INT
    uint32_t timeDateStamp;
    uint32_t forwarderChain;
    uint32_t nameRVA;            // RVA of DLL name
    uint32_t firstThunk;         // RVA of IAT (bound)
};

struct ImportByName {
    uint16_t hint;
    char name[1];
};
#pragma pack(pop)

// ---------- 汇编器 ----------
class Assembler {
    struct Label {
        string name;
        uint32_t address;
        bool defined;
        Label() : address(0), defined(false) {}
    };

    vector<string> lines;
    map<string, Label> labels;
    vector<uint8_t> code;
    vector<uint8_t> data;
    uint32_t codePos;
    uint32_t dataPos;
    bool inData;
    vector<string> externs;   // 外部符号列表
    map<string, uint32_t> importThunks; // 外部符号对应的IAT地址

    string cleanLine(const string& line) {
        size_t comment = line.find(';');
        string inst = line.substr(0, comment);
        size_t start = inst.find_first_not_of(" \t\r\n");
        if (start == string::npos) return "";
        size_t end = inst.find_last_not_of(" \t\r\n");
        return inst.substr(start, end - start + 1);
    }

    void pass1() {
        codePos = 0;
        dataPos = 0;
        inData = false;
        for (const string& line : lines) {
            string clean = cleanLine(line);
            if (clean.empty()) continue;

            if (clean == "section .text") {
                inData = false;
                continue;
            }
            if (clean == "section .data") {
                inData = true;
                continue;
            }
            if (clean.substr(0, 5) == "extern") {
                // 格式：extern _FunctionName
                string name = clean.substr(7);
                externs.push_back(name);
                continue;
            }
            if (clean.substr(0, 6) == "global") {
                // 忽略
                continue;
            }
            if (clean.substr(0, 4) == "bits") {
                continue;
            }

            size_t colon = clean.find(':');
            if (colon != string::npos) {
                string labelName = clean.substr(0, colon);
                size_t start = labelName.find_first_not_of(" \t");
                size_t end = labelName.find_last_not_of(" \t");
                if (start != string::npos && end != string::npos)
                    labelName = labelName.substr(start, end - start + 1);
                if (!labelName.empty()) {
                    Label& lbl = labels[labelName];
                    if (lbl.defined) {
                        cerr << "错误: 重复定义标签 " << labelName << endl;
                        exit(1);
                    }
                    lbl.defined = true;
                    lbl.address = inData ? dataPos : codePos;
                }
                continue;
            }

            if (inData) {
                // 数据定义：如 label dd 0 或 db '...'
                if (clean.find("dd") != string::npos) {
                    dataPos += 4;
                }
                else if (clean.find("db") != string::npos) {
                    // 简单处理字符串：如 str db 'hello',0
                    size_t quote1 = clean.find('\'');
                    size_t quote2 = clean.find('\'', quote1 + 1);
                    if (quote1 != string::npos && quote2 != string::npos) {
                        int len = quote2 - quote1 - 1;
                        dataPos += len + 1; // 加结尾0
                    }
                    else {
                        dataPos += 1; // 未知
                    }
                }
                else {
                    dataPos += 4; // 默认
                }
            }
            else {
                // 指令长度估算
                codePos += 5; // 粗略估计，pass2修正
            }
        }
    }

    void pass2() {
        code.clear();
        data.clear();
        codePos = 0;
        dataPos = 0;
        inData = false;
        for (const string& line : lines) {
            string clean = cleanLine(line);
            if (clean.empty()) continue;

            if (clean == "section .text") {
                inData = false;
                continue;
            }
            if (clean == "section .data") {
                inData = true;
                continue;
            }
            if (clean.substr(0, 5) == "extern") continue;
            if (clean.substr(0, 6) == "global") continue;
            if (clean.substr(0, 4) == "bits") continue;

            size_t colon = clean.find(':');
            if (colon != string::npos) continue;

            if (inData) {
                // 生成数据
                if (clean.find("dd") != string::npos) {
                    // 解析值
                    size_t eq = clean.find('=');
                    if (eq != string::npos) {
                        int val = stoi(clean.substr(eq + 1));
                        data.push_back(val & 0xFF);
                        data.push_back((val >> 8) & 0xFF);
                        data.push_back((val >> 16) & 0xFF);
                        data.push_back((val >> 24) & 0xFF);
                    }
                    else {
                        data.push_back(0); data.push_back(0); data.push_back(0); data.push_back(0);
                    }
                }
                else if (clean.find("db") != string::npos) {
                    size_t quote1 = clean.find('\'');
                    size_t quote2 = clean.find('\'', quote1 + 1);
                    if (quote1 != string::npos && quote2 != string::npos) {
                        string str = clean.substr(quote1 + 1, quote2 - quote1 - 1);
                        for (char c : str) data.push_back(c);
                        data.push_back(0);
                    }
                }
            }
            else {
                assembleInstruction(clean);
            }
        }
    }

    void assembleInstruction(const string& line) {
        istringstream iss(line);
        string opcode;
        iss >> opcode;
        transform(opcode.begin(), opcode.end(), opcode.begin(), ::tolower);

        if (opcode == "push") {
            string arg;
            iss >> arg;
            if (arg == "ebp") { code.push_back(0x55); codePos++; return; }
            if (arg == "eax") { code.push_back(0x50); codePos++; return; }
            if (arg == "ebx") { code.push_back(0x53); codePos++; return; }
            if (arg == "ecx") { code.push_back(0x51); codePos++; return; }
            if (arg == "edx") { code.push_back(0x52); codePos++; return; }
            if (arg == "esp") { code.push_back(0x54); codePos++; return; }
            // 立即数
            if (isdigit(arg[0]) || arg[0] == '-') {
                int val = stoi(arg);
                code.push_back(0x68);
                code.push_back(val & 0xFF);
                code.push_back((val >> 8) & 0xFF);
                code.push_back((val >> 16) & 0xFF);
                code.push_back((val >> 24) & 0xFF);
                codePos += 5;
                return;
            }
            // 可能是标签
            code.push_back(0x68);
            // 占位，后面修正
            code.push_back(0); code.push_back(0); code.push_back(0); code.push_back(0);
            codePos += 5;
            // 记录重定位
        }
        else if (opcode == "pop") {
            string arg;
            iss >> arg;
            if (arg == "ebp") { code.push_back(0x5D); codePos++; return; }
            if (arg == "eax") { code.push_back(0x58); codePos++; return; }
            if (arg == "ebx") { code.push_back(0x5B); codePos++; return; }
        }
        else if (opcode == "mov") {
            string dest, comma, src;
            iss >> dest >> comma >> src;
            if (comma != ",") {
                iss.clear();
                iss.str(line);
                iss >> opcode >> dest >> src;
            }
            // 简化：仅处理 mov eax, imm32
            if (dest == "eax" && src.find_first_of("0123456789-") != string::npos) {
                int val = stoi(src);
                code.push_back(0xB8);
                code.push_back(val & 0xFF);
                code.push_back((val >> 8) & 0xFF);
                code.push_back((val >> 16) & 0xFF);
                code.push_back((val >> 24) & 0xFF);
                codePos += 5;
                return;
            }
            // mov [ebp-4], eax
            if (dest.find("[ebp") != string::npos && src == "eax") {
                size_t start = dest.find('[') + 4;
                size_t end = dest.find(']', start);
                string offs = dest.substr(start, end - start);
                int offset = stoi(offs);
                code.push_back(0x89);
                code.push_back(0x45);
                code.push_back(offset & 0xFF);
                codePos += 3;
                return;
            }
            // mov eax, [ebp-4]
            if (dest == "eax" && src.find("[ebp") != string::npos) {
                size_t start = src.find('[') + 4;
                size_t end = src.find(']', start);
                string offs = src.substr(start, end - start);
                int offset = stoi(offs);
                code.push_back(0x8B);
                code.push_back(0x45);
                code.push_back(offset & 0xFF);
                codePos += 3;
                return;
            }
            // mov [_g_x], eax
            if (dest.find("[_g_") != string::npos && src == "eax") {
                // 全局变量地址需重定位
                code.push_back(0xA3);
                code.push_back(0); code.push_back(0); code.push_back(0); code.push_back(0);
                codePos += 5;
                return;
            }
        }
        else if (opcode == "call") {
            string target;
            iss >> target;
            if (target[0] == '_') {
                // 外部函数
                code.push_back(0xE8);
                code.push_back(0); code.push_back(0); code.push_back(0); code.push_back(0);
                codePos += 5;
                // 记录需要导入
                importThunks[target] = codePos - 4; // 占位地址
                return;
            }
            // 内部标签
            code.push_back(0xE8);
            code.push_back(0); code.push_back(0); code.push_back(0); code.push_back(0);
            codePos += 5;
        }
        else if (opcode == "add") {
            string dest, comma, src;
            iss >> dest >> comma >> src;
            if (dest == "esp" && src.find_first_of("0123456789") != string::npos) {
                int val = stoi(src);
                code.push_back(0x83);
                code.push_back(0xC4);
                code.push_back(val & 0xFF);
                codePos += 3;
                return;
            }
            // add eax, [esp]
            if (dest == "eax" && src == "[esp]") {
                code.push_back(0x03);
                code.push_back(0x04);
                code.push_back(0x24);
                codePos += 3;
                return;
            }
        }
        else if (opcode == "sub") {
            string dest, comma, src;
            iss >> dest >> comma >> src;
            if (dest == "esp" && src.find_first_of("0123456789") != string::npos) {
                int val = stoi(src);
                code.push_back(0x83);
                code.push_back(0xEC);
                code.push_back(val & 0xFF);
                codePos += 3;
                return;
            }
        }
        else if (opcode == "ret") {
            code.push_back(0xC3);
            codePos++;
        }
        else {
            cerr << "未知指令: " << opcode << " in " << line << endl;
            exit(1);
        }
    }

public:
    bool assemble(const string& asmFile, const string& exeFile) {
        ifstream in(asmFile);
        if (!in) {
            cerr << "无法打开汇编文件: " << asmFile << endl;
            return false;
        }
        string line;
        while (getline(in, line)) {
            lines.push_back(line);
        }
        in.close();

        pass1();
        pass2();

        // 构建 PE 文件
        ofstream out(exeFile, ios::binary);
        if (!out) {
            cerr << "无法创建输出文件: " << exeFile << endl;
            return false;
        }

        // DOS头
        DOSHeader dos = { 0 };
        dos.e_magic = 0x5A4D;
        dos.e_lfanew = 0x80; // PE头偏移

        out.write((char*)&dos, sizeof(dos));
        out.seekp(dos.e_lfanew);

        // PE头
        PEHeader pe = { 0 };
        pe.signature = 0x00004550;
        pe.machine = 0x014C; // i386
        pe.numberOfSections = 2; // .text 和 .data
        pe.sizeOfOptionalHeader = 0xE0;
        pe.characteristics = 0x0102; // 可执行、32位
        pe.magic = 0x010B;
        pe.majorLinkerVersion = 1;
        pe.minorLinkerVersion = 0;
        pe.sizeOfCode = code.size();
        pe.sizeOfInitializedData = data.size();
        pe.sizeOfUninitializedData = 0;
        pe.addressOfEntryPoint = 0x1000; // .text 默认RVA
        pe.baseOfCode = 0x1000;
        pe.baseOfData = 0x2000;
        pe.imageBase = 0x400000;
        pe.sectionAlignment = 0x1000;
        pe.fileAlignment = 0x200;
        pe.majorOSVersion = 4;
        pe.minorOSVersion = 0;
        pe.majorSubsystemVersion = 4;
        pe.minorSubsystemVersion = 0;
        pe.sizeOfImage = 0x4000; // 足够大
        pe.sizeOfHeaders = 0x200;
        pe.subsystem = 2; // GUI
        pe.dllCharacteristics = 0;
        pe.sizeOfStackReserve = 0x100000;
        pe.sizeOfStackCommit = 0x1000;
        pe.sizeOfHeapReserve = 0x100000;
        pe.sizeOfHeapCommit = 0x1000;
        pe.numberOfRvaAndSizes = 16;
        // 导入表
        pe.importRVA = 0x3000;
        pe.importSize = 0x1000;

        out.write((char*)&pe, sizeof(pe));

        // 节表
        SectionHeader text = { 0 };
        memcpy(text.name, ".text", 5);
        text.virtualSize = (code.size() + 0xFFF) & ~0xFFF;
        text.virtualAddress = 0x1000;
        text.sizeOfRawData = (code.size() + 0x1FF) & ~0x1FF;
        text.pointerToRawData = 0x200;
        text.characteristics = 0x60000020; // 代码、可执行、可读

        SectionHeader dataSec = { 0 };
        memcpy(dataSec.name, ".data", 5);
        dataSec.virtualSize = (data.size() + 0xFFF) & ~0xFFF;
        dataSec.virtualAddress = 0x2000;
        dataSec.sizeOfRawData = (data.size() + 0x1FF) & ~0x1FF;
        dataSec.pointerToRawData = 0x200 + ((code.size() + 0x1FF) & ~0x1FF);
        dataSec.characteristics = 0xC0000040; // 初始化数据、可读、可写

        out.write((char*)&text, sizeof(text));
        out.write((char*)&dataSec, sizeof(dataSec));

        // 写入代码
        out.seekp(text.pointerToRawData);
        out.write((char*)code.data(), code.size());
        // 写入数据
        out.seekp(dataSec.pointerToRawData);
        out.write((char*)data.data(), data.size());

        // 构建导入表
        if (!externs.empty()) {
            out.seekp(0x200 + text.sizeOfRawData + dataSec.sizeOfRawData);
            uint32_t importStart = out.tellp();

            // 导入描述符数组，以全0结束
            for (size_t i = 0; i < externs.size(); i++) {
                ImportDescriptor desc = { 0 };
                desc.nameRVA = 0x3000 + 0x100; // DLL名称RVA
                desc.firstThunk = 0x3000 + 0x200 + i * 4; // IAT RVA
                out.write((char*)&desc, sizeof(desc));
            }
            ImportDescriptor zero = { 0 };
            out.write((char*)&zero, sizeof(zero));

            // DLL名称 "user32.dll" 或 "kernel32.dll" (根据符号)
            uint32_t dllNamePos = out.tellp();
            string dllName = "user32.dll";
            out.write(dllName.c_str(), dllName.size() + 1);

            // IAT (导入地址表)
            uint32_t iatPos = out.tellp();
            for (size_t i = 0; i < externs.size(); i++) {
                uint32_t hintNameRVA = 0x3000 + 0x300 + i * 2; // 简单处理
                out.write((char*)&hintNameRVA, 4);
            }
            uint32_t zeroIAT = 0;
            out.write((char*)&zeroIAT, 4);

            // Hint-Name 表
            uint32_t hintNamePos = out.tellp();
            for (size_t i = 0; i < externs.size(); i++) {
                uint16_t hint = 0;
                out.write((char*)&hint, 2);
                string name = externs[i].substr(1); // 去掉下划线
                out.write(name.c_str(), name.size() + 1);
                // 对齐
                if (out.tellp() & 1) out.put(0);
            }
        }

        out.close();
        return true;
    }
};

int main(int argc, char* argv[]) {
    if (argc < 2) {
        cerr << "用法: linker.exe <输入.asm> [输出.exe]" << endl;
        return 1;
    }

    string asmFile = argv[1];
    string outFile;
    if (argc >= 3) {
        outFile = argv[2];
    }
    else {
        size_t dot = asmFile.find_last_of('.');
        if (dot == string::npos)
            outFile = asmFile + ".exe";
        else
            outFile = asmFile.substr(0, dot) + ".exe";
    }

    Assembler asmblr;
    if (asmblr.assemble(asmFile, outFile)) {
        cout << "链接成功，生成: " << outFile << endl;
        return 0;
    }
    else {
        return 1;
    }
}