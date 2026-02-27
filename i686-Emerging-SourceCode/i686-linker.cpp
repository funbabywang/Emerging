// linker.cpp - 简单的ELF32链接器，将.o链接为静态可执行文件
// 用法: i686-linker.exe <输入.o> <输出.exe>

#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <cstdint>
#include <algorithm>
#include <map>
#include <string>

using namespace std;

// ELF32 数据结构 (来自 elf.h)
typedef uint32_t Elf32_Addr;
typedef uint16_t Elf32_Half;
typedef uint32_t Elf32_Off;
typedef int32_t  Elf32_Sword;
typedef uint32_t Elf32_Word;

#define EI_NIDENT 16

struct Elf32_Ehdr {
    unsigned char e_ident[EI_NIDENT];
    Elf32_Half    e_type;
    Elf32_Half    e_machine;
    Elf32_Word    e_version;
    Elf32_Addr    e_entry;
    Elf32_Off     e_phoff;
    Elf32_Off     e_shoff;
    Elf32_Word    e_flags;
    Elf32_Half    e_ehsize;
    Elf32_Half    e_phentsize;
    Elf32_Half    e_phnum;
    Elf32_Half    e_shentsize;
    Elf32_Half    e_shnum;
    Elf32_Half    e_shstrndx;
};

struct Elf32_Shdr {
    Elf32_Word sh_name;
    Elf32_Word sh_type;
    Elf32_Word sh_flags;
    Elf32_Addr sh_addr;
    Elf32_Off  sh_offset;
    Elf32_Word sh_size;
    Elf32_Word sh_link;
    Elf32_Word sh_info;
    Elf32_Word sh_addralign;
    Elf32_Word sh_entsize;
};

struct Elf32_Sym {
    Elf32_Word st_name;
    Elf32_Addr st_value;
    Elf32_Word st_size;
    unsigned char st_info;
    unsigned char st_other;
    Elf32_Half st_shndx;
};

struct Elf32_Rel {
    Elf32_Addr r_offset;
    Elf32_Word r_info;
};

#define ELF32_R_SYM(i) ((i)>>8)
#define ELF32_R_TYPE(i) ((unsigned char)(i))
#define ELF32_R_INFO(s,t) (((s)<<8)+(unsigned char)(t))

// 节类型
#define SHT_NULL        0
#define SHT_PROGBITS    1
#define SHT_SYMTAB      2
#define SHT_STRTAB      3
#define SHT_REL         9

// 节标志
#define SHF_WRITE       0x1
#define SHF_ALLOC       0x2
#define SHF_EXECINSTR   0x4

// 符号绑定
#define ELF32_ST_BIND(i) ((i)>>4)
#define ELF32_ST_TYPE(i) ((i)&0xf)
#define STB_LOCAL       0
#define STB_GLOBAL      1

// 特殊节索引
#define SHN_UNDEF       0

// 重定位类型
#define R_386_32        1
#define R_386_PC32      2

// 程序头
struct Elf32_Phdr {
    Elf32_Word p_type;
    Elf32_Off  p_offset;
    Elf32_Addr p_vaddr;
    Elf32_Addr p_paddr;
    Elf32_Word p_filesz;
    Elf32_Word p_memsz;
    Elf32_Word p_flags;
    Elf32_Word p_align;
};

#define PT_LOAD         1
#define PF_X            1
#define PF_W            2
#define PF_R            4

// 简单链接器：仅支持一个输入文件，处理.text和.data节，生成静态可执行
class Linker {
    ifstream& in;
    ofstream& out;
    vector<char> fileData;

    Elf32_Ehdr inEhdr;
    vector<Elf32_Shdr> inShdrs;
    vector<char> shstrtab;
    vector<char> strtab;
    vector<Elf32_Sym> symtab;
    vector<Elf32_Rel> relText;  // .text的重定位
    vector<Elf32_Rel> relData;  // .data的重定位

    // 输出节信息
    struct OutSection {
        Elf32_Word type;
        Elf32_Word flags;
        Elf32_Addr addr;
        Elf32_Word size;
        vector<char> data;
        OutSection(Elf32_Word t, Elf32_Word f) : type(t), flags(f), addr(0), size(0) {}
    };
    vector<OutSection> outSections;
    Elf32_Addr entryPoint;

    // 符号解析结果
    map<string, Elf32_Addr> globalSymbols;

public:
    Linker(ifstream& i, ofstream& o) : in(i), out(o), entryPoint(0) {}

    bool link() {
        if (!readInput()) return false;
        if (!parseSections()) return false;
        if (!resolveSymbols()) return false;
        if (!applyRelocations()) return false;
        if (!buildOutput()) return false;
        return true;
    }

private:
    bool readInput() {
        in.seekg(0, ios::end);
        size_t size = in.tellg();
        in.seekg(0, ios::beg);
        fileData.resize(size);
        in.read(fileData.data(), size);
        return !in.fail();
    }

    bool parseSections() {
        if (fileData.size() < sizeof(Elf32_Ehdr)) {
            cerr << "无效的ELF文件: 太小\n";
            return false;
        }
        memcpy(&inEhdr, fileData.data(), sizeof(Elf32_Ehdr));
        if (memcmp(inEhdr.e_ident, "\177ELF\1\1\1", 7) != 0) {
            cerr << "不是有效的ELF32文件\n";
            return false;
        }

        // 读取节头表
        size_t shoff = inEhdr.e_shoff;
        size_t shnum = inEhdr.e_shnum;
        size_t shentsize = inEhdr.e_shentsize;
        if (shoff + shnum * shentsize > fileData.size()) {
            cerr << "节头表超出文件范围\n";
            return false;
        }
        inShdrs.resize(shnum);
        for (size_t i = 0; i < shnum; i++) {
            memcpy(&inShdrs[i], fileData.data() + shoff + i * shentsize, shentsize);
        }

        // 读取节名字符串表
        size_t shstrndx = inEhdr.e_shstrndx;
        if (shstrndx >= shnum) {
            cerr << "无效的节名字符串索引\n";
            return false;
        }
        Elf32_Shdr& shstrShdr = inShdrs[shstrndx];
        if (shstrShdr.sh_type != SHT_STRTAB) {
            cerr << "节名字符串表类型错误\n";
            return false;
        }
        shstrtab.resize(shstrShdr.sh_size);
        memcpy(shstrtab.data(), fileData.data() + shstrShdr.sh_offset, shstrShdr.sh_size);

        // 查找符号表和字符串表
        int symtabIdx = -1, strtabIdx = -1;
        for (size_t i = 0; i < shnum; i++) {
            const char* name = shstrtab.data() + inShdrs[i].sh_name;
            if (inShdrs[i].sh_type == SHT_SYMTAB) {
                symtabIdx = i;
            }
            else if (inShdrs[i].sh_type == SHT_STRTAB && strcmp(name, ".strtab") == 0) {
                strtabIdx = i;
            }
        }
        if (symtabIdx == -1 || strtabIdx == -1) {
            cerr << "未找到符号表或字符串表\n";
            return false;
        }

        // 读取符号表
        Elf32_Shdr& symShdr = inShdrs[symtabIdx];
        size_t symCount = symShdr.sh_size / sizeof(Elf32_Sym);
        symtab.resize(symCount);
        memcpy(symtab.data(), fileData.data() + symShdr.sh_offset, symShdr.sh_size);

        // 读取字符串表
        Elf32_Shdr& strShdr = inShdrs[strtabIdx];
        strtab.resize(strShdr.sh_size);
        memcpy(strtab.data(), fileData.data() + strShdr.sh_offset, strShdr.sh_size);

        // 收集重定位节（.rel.text, .rel.data）
        for (size_t i = 0; i < shnum; i++) {
            if (inShdrs[i].sh_type == SHT_REL) {
                const char* name = shstrtab.data() + inShdrs[i].sh_name;
                if (strstr(name, ".text") != nullptr) {
                    size_t relCount = inShdrs[i].sh_size / sizeof(Elf32_Rel);
                    relText.resize(relCount);
                    memcpy(relText.data(), fileData.data() + inShdrs[i].sh_offset, inShdrs[i].sh_size);
                }
                else if (strstr(name, ".data") != nullptr) {
                    size_t relCount = inShdrs[i].sh_size / sizeof(Elf32_Rel);
                    relData.resize(relCount);
                    memcpy(relData.data(), fileData.data() + inShdrs[i].sh_offset, inShdrs[i].sh_size);
                }
            }
        }

        return true;
    }

    bool resolveSymbols() {
        // 收集全局符号地址（相对于所在节的偏移）
        // 我们只处理 .text 和 .data
        Elf32_Addr textAddr = 0x08048000 + sizeof(Elf32_Ehdr) + sizeof(Elf32_Phdr); // 简单布局
        Elf32_Addr dataAddr = textAddr; // 将在确定.text大小后更新

        // 先确定各个节在输出中的虚拟地址
        for (auto& shdr : inShdrs) {
            if (shdr.sh_flags & SHF_ALLOC) {
                const char* name = shstrtab.data() + shdr.sh_name;
                if (strcmp(name, ".text") == 0) {
                    textAddr = 0x08048000 + sizeof(Elf32_Ehdr) + sizeof(Elf32_Phdr); // 起始
                    // 暂存
                }
                else if (strcmp(name, ".data") == 0) {
                    dataAddr = textAddr; // 稍后加上.text大小
                }
            }
        }

        // 计算.text大小
        Elf32_Word textSize = 0;
        for (auto& shdr : inShdrs) {
            if (shdr.sh_flags & SHF_ALLOC) {
                const char* name = shstrtab.data() + shdr.sh_name;
                if (strcmp(name, ".text") == 0) {
                    textSize = shdr.sh_size;
                    break;
                }
            }
        }
        dataAddr = textAddr + textSize;

        // 遍历符号表，记录全局符号地址
        for (auto& sym : symtab) {
            unsigned char bind = ELF32_ST_BIND(sym.st_info);
            if (bind == STB_GLOBAL) {
                const char* symName = strtab.data() + sym.st_name;
                if (sym.st_shndx != SHN_UNDEF) {
                    // 获取符号所在节的基址
                    Elf32_Addr base = 0;
                    const char* secName = "";
                    if (sym.st_shndx < inShdrs.size()) {
                        secName = shstrtab.data() + inShdrs[sym.st_shndx].sh_name;
                    }
                    if (strcmp(secName, ".text") == 0) {
                        base = textAddr;
                    }
                    else if (strcmp(secName, ".data") == 0) {
                        base = dataAddr;
                    }
                    else {
                        // 忽略其他节
                        continue;
                    }
                    globalSymbols[symName] = base + sym.st_value;
                    if (strcmp(symName, "_start") == 0) {
                        entryPoint = base + sym.st_value;
                    }
                }
            }
        }

        if (entryPoint == 0) {
            cerr << "未找到入口点 _start\n";
            return false;
        }
        return true;
    }

    bool applyRelocations() {
        // 我们需要知道各节在输出中的地址，以及各节数据
        // 先确定各节在输出中的虚拟地址和偏移
        Elf32_Addr textAddr = 0x08048000 + sizeof(Elf32_Ehdr) + sizeof(Elf32_Phdr); // 起始
        Elf32_Addr dataAddr = textAddr; // 暂未知
        Elf32_Word textSize = 0, dataSize = 0;

        for (auto& shdr : inShdrs) {
            const char* name = shstrtab.data() + shdr.sh_name;
            if (shdr.sh_flags & SHF_ALLOC) {
                if (strcmp(name, ".text") == 0) {
                    textSize = shdr.sh_size;
                }
                else if (strcmp(name, ".data") == 0) {
                    dataSize = shdr.sh_size;
                }
            }
        }
        dataAddr = textAddr + textSize;

        // 读取.text和.data数据
        vector<char> textData, dataData;
        for (auto& shdr : inShdrs) {
            const char* name = shstrtab.data() + shdr.sh_name;
            if (shdr.sh_flags & SHF_ALLOC) {
                if (strcmp(name, ".text") == 0) {
                    textData.resize(shdr.sh_size);
                    memcpy(textData.data(), fileData.data() + shdr.sh_offset, shdr.sh_size);
                }
                else if (strcmp(name, ".data") == 0) {
                    dataData.resize(shdr.sh_size);
                    memcpy(dataData.data(), fileData.data() + shdr.sh_offset, shdr.sh_size);
                }
            }
        }

        // 应用.text重定位
        for (auto& rel : relText) {
            Elf32_Word symIdx = ELF32_R_SYM(rel.r_info);
            Elf32_Word type = ELF32_R_TYPE(rel.r_info);
            if (symIdx >= symtab.size()) continue;
            Elf32_Sym& sym = symtab[symIdx];
            const char* symName = strtab.data() + sym.st_name;
            auto it = globalSymbols.find(symName);
            if (it == globalSymbols.end()) {
                cerr << "未解析的符号: " << symName << endl;
                return false;
            }
            Elf32_Addr symVal = it->second;
            Elf32_Addr place = textAddr + rel.r_offset; // 重定位位置在输出中的地址
            int32_t* patch = reinterpret_cast<int32_t*>(textData.data() + rel.r_offset);
            if (type == R_386_32) {
                *patch += symVal;
            }
            else if (type == R_386_PC32) {
                *patch += symVal - place;
            }
            else {
                cerr << "不支持的重定位类型: " << type << endl;
                return false;
            }
        }

        // 应用.data重定位（类似）
        for (auto& rel : relData) {
            Elf32_Word symIdx = ELF32_R_SYM(rel.r_info);
            Elf32_Word type = ELF32_R_TYPE(rel.r_info);
            if (symIdx >= symtab.size()) continue;
            Elf32_Sym& sym = symtab[symIdx];
            const char* symName = strtab.data() + sym.st_name;
            auto it = globalSymbols.find(symName);
            if (it == globalSymbols.end()) {
                cerr << "未解析的符号: " << symName << endl;
                return false;
            }
            Elf32_Addr symVal = it->second;
            Elf32_Addr place = dataAddr + rel.r_offset;
            int32_t* patch = reinterpret_cast<int32_t*>(dataData.data() + rel.r_offset);
            if (type == R_386_32) {
                *patch += symVal;
            }
            else if (type == R_386_PC32) {
                *patch += symVal - place;
            }
            else {
                cerr << "不支持的重定位类型: " << type << endl;
                return false;
            }
        }

        // 保存处理后的节数据
        outSections.emplace_back(SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR);
        outSections.back().addr = textAddr;
        outSections.back().size = textSize;
        outSections.back().data.swap(textData);

        outSections.emplace_back(SHT_PROGBITS, SHF_ALLOC | SHF_WRITE);
        outSections.back().addr = dataAddr;
        outSections.back().size = dataSize;
        outSections.back().data.swap(dataData);

        return true;
    }

    bool buildOutput() {
        // 构建输出ELF可执行文件
        // 计算程序头数量
        Elf32_Word phnum = outSections.size(); // 每个可加载节一个段
        // ELF头 + 程序头表 + 节数据
        Elf32_Off e_ehsize = sizeof(Elf32_Ehdr);
        Elf32_Off e_phentsize = sizeof(Elf32_Phdr);
        Elf32_Off e_phoff = e_ehsize;
        Elf32_Off e_shoff = 0; // 不输出节头表
        Elf32_Off dataOffset = e_ehsize + phnum * e_phentsize;

        // 写入ELF头
        Elf32_Ehdr ehdr;
        memset(&ehdr, 0, sizeof(ehdr));
        memcpy(ehdr.e_ident, "\177ELF\1\1\1", 7);
        ehdr.e_ident[EI_NIDENT - 1] = 0;
        ehdr.e_type = 2; // ET_EXEC
        ehdr.e_machine = 3; // EM_386
        ehdr.e_version = 1;
        ehdr.e_entry = entryPoint;
        ehdr.e_phoff = e_phoff;
        ehdr.e_shoff = 0;
        ehdr.e_flags = 0;
        ehdr.e_ehsize = e_ehsize;
        ehdr.e_phentsize = e_phentsize;
        ehdr.e_phnum = phnum;
        ehdr.e_shentsize = 0;
        ehdr.e_shnum = 0;
        ehdr.e_shstrndx = 0;
        out.write(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));

        // 写入程序头表
        Elf32_Off offset = dataOffset;
        for (auto& sec : outSections) {
            Elf32_Phdr phdr;
            memset(&phdr, 0, sizeof(phdr));
            phdr.p_type = PT_LOAD;
            phdr.p_offset = offset;
            phdr.p_vaddr = sec.addr;
            phdr.p_paddr = sec.addr;
            phdr.p_filesz = sec.size;
            phdr.p_memsz = sec.size;
            phdr.p_flags = (sec.flags & SHF_EXECINSTR) ? (PF_R | PF_X) : (PF_R | PF_W);
            phdr.p_align = 0x1000; // 4K对齐
            out.write(reinterpret_cast<char*>(&phdr), sizeof(phdr));
            offset += sec.size;
        }

        // 写入节数据
        for (auto& sec : outSections) {
            out.write(sec.data.data(), sec.size);
        }

        return out.good();
    }
};

int main(int argc, char* argv[]) {
    if (argc != 3) {
        cerr << "用法: " << argv[0] << " <输入.o> <输出.exe>\n";
        return 1;
    }
    string infile = argv[1];
    string outfile = argv[2];

    ifstream in(infile, ios::binary);
    if (!in) {
        cerr << "无法打开输入文件: " << infile << endl;
        return 1;
    }

    ofstream out(outfile, ios::binary);
    if (!out) {
        cerr << "无法打开输出文件: " << outfile << endl;
        return 1;
    }

    Linker linker(in, out);
    if (!linker.link()) {
        cerr << "链接失败\n";
        return 1;
    }

    cout << "链接成功，生成 " << outfile << endl;
    return 0;
}