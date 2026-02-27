#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <algorithm>

using namespace std;

#define EMERGING_VERSION "1.0-win32"

// ---------- 工具函数 ----------
string getBaseName(const string& path) {
    size_t start = path.find_last_of("/\\");
    size_t end = path.find_last_of('.');
    if (start == string::npos) start = 0; else start++;
    if (end == string::npos || end < start) end = path.length();
    return path.substr(start, end - start);
}

// ---------- 词法分析 ----------
enum TokenType {
    TOK_EOF, TOK_IDENT, TOK_NUMBER,
    TOK_INT, TOK_PRINT, TOK_RETURN,
    TOK_ASSIGN, TOK_SEMICOLON, TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE, TOK_PLUS, TOK_MINUS,
    TOK_MUL, TOK_DIV, TOK_COMMA, TOK_EXTERN,
    TOK_STRING                    // 字符串字面量（用于API调用）
};

struct Token {
    TokenType type;
    string text;
    int value;
    string str;                   // 字符串内容
};

class Lexer {
    istream& in;
    char ch;
    Token token;
    void nextChar() { if (in.get(ch)) {} else ch = EOF; }
public:
    Lexer(istream& is) : in(is), ch(' ') { nextChar(); }

    Token nextToken() {
        while (isspace(ch)) nextChar();
        if (ch == EOF) return token = { TOK_EOF, "", 0 };

        if (isalpha(ch) || ch == '_') {
            string id;
            while (isalnum(ch) || ch == '_') {
                id += ch;
                nextChar();
            }
            if (id == "int") return token = { TOK_INT, id, 0 };
            if (id == "print") return token = { TOK_PRINT, id, 0 };
            if (id == "return") return token = { TOK_RETURN, id, 0 };
            if (id == "extern") return token = { TOK_EXTERN, id, 0 };
            return token = { TOK_IDENT, id, 0 };
        }

        if (isdigit(ch)) {
            int val = 0;
            while (isdigit(ch)) {
                val = val * 10 + (ch - '0');
                nextChar();
            }
            return token = { TOK_NUMBER, "", val };
        }

        if (ch == '"') {
            nextChar(); // 跳过 "
            string str;
            while (ch != '"' && ch != EOF) {
                str += ch;
                nextChar();
            }
            if (ch == '"') nextChar();
            token = { TOK_STRING, "", 0 };
            token.str = str;
            return token;
        }

        char c = ch;
        nextChar();
        switch (c) {
        case '=': return token = { TOK_ASSIGN, "=", 0 };
        case ';': return token = { TOK_SEMICOLON, ";", 0 };
        case '(': return token = { TOK_LPAREN, "(", 0 };
        case ')': return token = { TOK_RPAREN, ")", 0 };
        case '{': return token = { TOK_LBRACE, "{", 0 };
        case '}': return token = { TOK_RBRACE, "}", 0 };
        case '+': return token = { TOK_PLUS, "+", 0 };
        case '-': return token = { TOK_MINUS, "-", 0 };
        case '*': return token = { TOK_MUL, "*", 0 };
        case '/': return token = { TOK_DIV, "/", 0 };
        case ',': return token = { TOK_COMMA, ",", 0 };
        default: cerr << "未知字符: " << c << endl; exit(1);
        }
    }

    Token current() const { return token; }
    void advance() { nextToken(); }
    bool check(TokenType t) const { return token.type == t; }
    bool match(TokenType t) { if (check(t)) { advance(); return true; } return false; }
    void expect(TokenType t, const string& msg) {
        if (!check(t)) { cerr << "语法错误: 期望 " << msg << endl; exit(1); }
    }
};

// ---------- 符号表 ----------
struct Symbol {
    string name;
    bool isGlobal;
    bool isExtern;      // 是否是外部函数
    int offset;          // 局部变量偏移
};

class SymbolTable {
    map<string, Symbol> locals;
    vector<string> globals;
    vector<string> externs;   // 外部函数名
public:
    void addGlobal(const string& name) {
        globals.push_back(name);
    }

    void addExtern(const string& name) {
        externs.push_back(name);
    }

    void beginFunction() { locals.clear(); }

    void addLocal(const string& name, int offset) {
        locals[name] = { name, false, false, offset };
    }

    Symbol* lookup(const string& name) {
        auto it = locals.find(name);
        if (it != locals.end()) return &it->second;
        for (size_t i = 0; i < globals.size(); ++i) {
            if (globals[i] == name) {
                static Symbol gsym;
                gsym = { name, true, false, (int)i * 4 };
                return &gsym;
            }
        }
        for (size_t i = 0; i < externs.size(); ++i) {
            if (externs[i] == name) {
                static Symbol esym;
                esym = { name, true, true, 0 };
                return &esym;
            }
        }
        return nullptr;
    }

    const vector<string>& getGlobals() const { return globals; }
    const vector<string>& getExterns() const { return externs; }
};

// ---------- 代码生成器（32位模式） ----------
class CodeGen {
    ofstream& out;
    int localCount;
    string currentFunc;
    vector<string> stringLiterals;   // 收集字符串常量
    map<string, string> stringLabels; // 字符串内容 -> 标签名
public:
    CodeGen(ofstream& os) : out(os), localCount(0) {}

    string newStringLabel() {
        static int n = 0;
        return "str" + to_string(n++);
    }

    void prolog() {
        out << "; 由Emerging编译器生成 v" << EMERGING_VERSION << "\n";
        out << "bits 32\n";
        out << "section .text\n\n";
        out << "extern _ExitProcess@4\n";
        out << "extern _MessageBoxA@16\n";  // 常用API
        out << "\n";
        out << "global _start\n";
        out << "_start:\n";
        out << "    call main\n";
        out << "    push eax\n";
        out << "    call _ExitProcess@4\n\n";
    }

    void beginFunction(const string& name) {
        currentFunc = name;
        localCount = 0;
        out << name << ":\n";
        out << "    push ebp\n";
        out << "    mov ebp, esp\n";
        out << "    sub esp, " << localCount * 4 << "\n"; // 稍后修正
    }

    void addLocal(const string& name) {
        localCount++;
        // 空间分配在函数结束时调整
    }

    void endFunction() {
        // 调整栈空间
        out << ".return_" << currentFunc << ":\n";
        out << "    mov esp, ebp\n";
        out << "    pop ebp\n";
        out << "    ret\n\n";
    }

    void emit(const string& code) { out << code << "\n"; }

    void emitString(const string& str) {
        string label;
        auto it = stringLabels.find(str);
        if (it == stringLabels.end()) {
            label = newStringLabel();
            stringLabels[str] = label;
        }
        else {
            label = it->second;
        }
        out << "    push " << label << "\n";  // 压入字符串地址
    }

    void emitDataSection(const vector<string>& globals, const vector<string>& externs) {
        out << "\nsection .data\n";
        for (const string& g : globals) {
            out << "_g_" << g << " dd 0\n";
        }
        // 输出字符串常量
        for (auto& p : stringLabels) {
            out << p.second << " db '" << p.first << "', 0\n";
        }
        out << "\n";

        // 输出导入表（由链接器处理，这里仅作标记）
        if (!externs.empty()) {
            out << "; 导入符号:\n";
            for (const string& e : externs) {
                out << "; extern " << e << "\n";
            }
        }
    }
};

// ---------- 语法分析器 ----------
class Parser {
    Lexer& lex;
    SymbolTable syms;
    CodeGen& cg;
    string currentFunction;
    int localCounter;
public:
    Parser(Lexer& l, CodeGen& gen) : lex(l), cg(gen), localCounter(0) {}

    void parseProgram() {
        cg.prolog();
        while (!lex.check(TOK_EOF)) {
            if (lex.check(TOK_EXTERN)) {
                parseExtern();
            }
            else if (lex.check(TOK_INT)) {
                lex.advance(); // 'int'
                if (lex.check(TOK_IDENT)) {
                    string name = lex.current().text;
                    lex.advance();
                    if (lex.check(TOK_LPAREN)) {
                        parseFunction(name);
                    }
                    else if (lex.check(TOK_SEMICOLON)) {
                        syms.addGlobal(name);
                        lex.advance(); // ';'
                    }
                    else {
                        cerr << "语法错误: 函数或变量定义后缺少 ; 或 (\n";
                        exit(1);
                    }
                }
                else {
                    cerr << "语法错误: 期望标识符\n";
                    exit(1);
                }
            }
            else {
                cerr << "语法错误: 只支持int声明或extern\n";
                exit(1);
            }
        }
        cg.emitDataSection(syms.getGlobals(), syms.getExterns());
    }

    void parseExtern() {
        lex.advance(); // 'extern'
        lex.expect(TOK_INT, "'int'");
        lex.advance(); // 'int'
        lex.expect(TOK_IDENT, "函数名");
        string name = lex.current().text;
        lex.advance();
        lex.expect(TOK_LPAREN, "'('");
        lex.advance();
        // 解析参数（简化，忽略）
        while (!lex.check(TOK_RPAREN)) {
            if (lex.check(TOK_INT)) lex.advance();
            if (lex.check(TOK_IDENT)) lex.advance();
            if (lex.check(TOK_COMMA)) lex.advance();
        }
        lex.expect(TOK_RPAREN, "')'");
        lex.advance();
        lex.expect(TOK_SEMICOLON, "';'");
        lex.advance();
        syms.addExtern(name);
    }

    void parseFunction(const string& name) {
        currentFunction = name;
        syms.beginFunction();
        localCounter = 0;
        lex.expect(TOK_LPAREN, "'('");
        lex.advance(); // '('
        lex.expect(TOK_RPAREN, "')'");
        lex.advance(); // ')'
        lex.expect(TOK_LBRACE, "'{'");
        lex.advance(); // '{'

        cg.beginFunction(name);

        while (!lex.check(TOK_RBRACE) && !lex.check(TOK_EOF)) {
            parseStatement();
        }

        lex.expect(TOK_RBRACE, "'}'");
        lex.advance(); // '}'

        cg.endFunction();
    }

    void parseStatement() {
        if (lex.check(TOK_INT)) {
            lex.advance(); // 'int'
            lex.expect(TOK_IDENT, "变量名");
            string varName = lex.current().text;
            lex.advance();
            int offset = -4 - 4 * localCounter++;
            syms.addLocal(varName, offset);
            // 可选初始化
            if (lex.check(TOK_ASSIGN)) {
                lex.advance(); // '='
                parseExpression();
                Symbol* s = syms.lookup(varName);
                cg.emit("    mov [ebp" + to_string(s->offset) + "], eax");
            }
            lex.expect(TOK_SEMICOLON, "';'");
            lex.advance(); // ';'
        }
        else if (lex.check(TOK_IDENT)) {
            string varName = lex.current().text;
            lex.advance(); // ident
            Symbol* s = syms.lookup(varName);
            if (!s) { cerr << "未定义变量: " << varName << endl; exit(1); }
            if (s->isExtern) {
                // 函数调用
                lex.expect(TOK_LPAREN, "'('");
                lex.advance(); // '('
                // 处理参数（逆序压栈）
                vector<int> args;
                while (!lex.check(TOK_RPAREN)) {
                    parseExpression(); // 结果在 eax
                    cg.emit("    push eax");
                    if (lex.check(TOK_COMMA)) lex.advance();
                }
                lex.expect(TOK_RPAREN, "')'");
                lex.advance(); // ')'
                lex.expect(TOK_SEMICOLON, "';'");
                lex.advance(); // ';'
                cg.emit("    call _" + varName);
                cg.emit("    add esp, " + to_string(args.size() * 4));
            }
            else {
                // 赋值
                lex.expect(TOK_ASSIGN, "'='");
                lex.advance(); // '='
                parseExpression();
                lex.expect(TOK_SEMICOLON, "';'");
                lex.advance(); // ';'
                if (s->isGlobal) {
                    cg.emit("    mov [_g_" + varName + "], eax");
                }
                else {
                    cg.emit("    mov [ebp" + to_string(s->offset) + "], eax");
                }
            }
        }
        else if (lex.check(TOK_PRINT)) {
            lex.advance(); // print
            lex.expect(TOK_LPAREN, "'('");
            lex.advance(); // '('
            parseExpression(); // 结果在 eax
            lex.expect(TOK_RPAREN, "')'");
            lex.advance(); // ')'
            lex.expect(TOK_SEMICOLON, "';'");
            lex.advance(); // ';'
            // 生成对 printf 的调用（需有 extern int printf(...)）
            cg.emit("    push eax");
            cg.emit("    push format"); // 需定义格式字符串
            cg.emit("    call _printf");
            cg.emit("    add esp, 8");
        }
        else if (lex.check(TOK_RETURN)) {
            lex.advance(); // return
            parseExpression();
            lex.expect(TOK_SEMICOLON, "';'");
            lex.advance(); // ';'
            cg.emit("    jmp .return_" + currentFunction);
        }
        else if (lex.check(TOK_STRING)) {
            // 字符串作为表达式处理
            string str = lex.current().str;
            lex.advance();
            cg.emitString(str);
        }
        else {
            cerr << "语法错误: 未知语句" << endl;
            exit(1);
        }
    }

    void parseExpression() {
        parseTerm();
        while (lex.check(TOK_PLUS) || lex.check(TOK_MINUS)) {
            Token op = lex.current();
            lex.advance();
            parseTerm();
            if (op.type == TOK_PLUS) {
                cg.emit("    add eax, [esp]");
                cg.emit("    add esp, 4");
            }
            else {
                cg.emit("    mov ebx, [esp]");
                cg.emit("    add esp, 4");
                cg.emit("    sub ebx, eax");
                cg.emit("    mov eax, ebx");
            }
        }
    }

    void parseTerm() {
        parseFactor();
        while (lex.check(TOK_MUL) || lex.check(TOK_DIV)) {
            Token op = lex.current();
            lex.advance();
            cg.emit("    push eax");
            parseFactor();
            cg.emit("    pop ebx");
            if (op.type == TOK_MUL) {
                cg.emit("    imul eax, ebx");
            }
            else {
                cg.emit("    cdq");
                cg.emit("    idiv ebx");
            }
        }
    }

    void parseFactor() {
        if (lex.check(TOK_NUMBER)) {
            int val = lex.current().value;
            cg.emit("    mov eax, " + to_string(val));
            lex.advance();
        }
        else if (lex.check(TOK_IDENT)) {
            string name = lex.current().text;
            lex.advance();
            Symbol* s = syms.lookup(name);
            if (!s) { cerr << "未定义变量: " << name << endl; exit(1); }
            if (s->isGlobal) {
                cg.emit("    mov eax, [_g_" + name + "]");
            }
            else {
                cg.emit("    mov eax, [ebp" + to_string(s->offset) + "]");
            }
        }
        else if (lex.check(TOK_STRING)) {
            // 字符串直接量：返回其在数据段中的标签
            string str = lex.current().str;
            lex.advance();
            cg.emitString(str);
        }
        else if (lex.check(TOK_LPAREN)) {
            lex.advance(); // '('
            parseExpression();
            lex.expect(TOK_RPAREN, "')'");
            lex.advance(); // ')'
        }
        else {
            cerr << "语法错误: 期望因子" << endl;
            exit(1);
        }
    }
};

// ---------- 主函数 ----------
int main(int argc, char* argv[]) {
    string srcFile;
    string outFile;
    bool versionOnly = false;

    for (int i = 1; i < argc; ++i) {
        string arg = argv[i];
        if (arg == "--version") {
            cout << "Emerging compiler version " << EMERGING_VERSION << endl;
            versionOnly = true;
            break;
        }
        else if (arg == "-o") {
            if (i + 1 < argc) {
                outFile = argv[++i];
            }
            else {
                cerr << "错误: -o 选项缺少参数" << endl;
                return 1;
            }
        }
        else if (arg[0] == '-') {
            cerr << "未知选项: " << arg << endl;
            return 1;
        }
        else {
            if (!srcFile.empty()) {
                cerr << "错误: 只能指定一个源文件" << endl;
                return 1;
            }
            srcFile = arg;
        }
    }

    if (versionOnly) return 0;

    if (srcFile.empty()) {
        cerr << "用法: emerging.exe [--version] [-o output.exe] <源文件.emg>" << endl;
        return 1;
    }

    ifstream in(srcFile);
    if (!in) {
        cerr << "无法打开源文件: " << srcFile << endl;
        return 1;
    }

    string baseName = getBaseName(srcFile);
    string asmFile = baseName + ".asm";
    if (outFile.empty()) {
        outFile = baseName + ".exe";
    }

    ofstream out(asmFile);
    if (!out) {
        cerr << "无法创建汇编文件: " << asmFile << endl;
        return 1;
    }

    Lexer lex(in);
    CodeGen cg(out);
    Parser parser(lex, cg);
    parser.parseProgram();

    out.close();
    cout << "汇编代码已生成: " << asmFile << endl;

    // 调用 linker.exe 进行汇编和链接
    string linkerCmd = "linker.exe " + asmFile + " " + outFile;
    cout << "执行: " << linkerCmd << endl;
    int ret = system(linkerCmd.c_str());
    if (ret != 0) {
        cerr << "链接失败" << endl;
        return 1;
    }

    cout << "编译成功，生成可执行文件: " << outFile << endl;
    return 0;
}