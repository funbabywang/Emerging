// emerging.cpp - Emerging语言编译器 (i686版本)
// 用法: i686-emerging.exe [--version] input.emg [output.asm]
// 生成汇编代码，需用nasm -f elf32编译

#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <map>
#include <memory>
#include <cctype>
#include <cstdlib>

using namespace std;

// ---------- 版本信息 ----------
const string VERSION = "i686 Emerging 编程语言版本1.0.0";

// ---------- 命令行解析 ----------
void printVersionAndExit() {
    cout << VERSION << endl;
    exit(0);
}

// ---------- 词法分析 ----------
enum TokenType {
    TOKEN_EOF, TOKEN_IDENT, TOKEN_NUMBER,
    TOKEN_INT, TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE, TOKEN_RETURN,
    TOKEN_ASSIGN, TOKEN_EQ, TOKEN_NE, TOKEN_LT, TOKEN_LE, TOKEN_GT, TOKEN_GE,
    TOKEN_PLUS, TOKEN_MINUS, TOKEN_MUL, TOKEN_DIV,
    TOKEN_LPAREN, TOKEN_RPAREN, TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_SEMICOLON, TOKEN_UNKNOWN
};

struct Token {
    TokenType type;
    string text;
    int intVal;
    Token(TokenType t = TOKEN_UNKNOWN, const string& txt = "", int v = 0)
        : type(t), text(txt), intVal(v) {
    }
};

class Lexer {
    istream& in;
    char cur;
    int line, col;
public:
    Lexer(istream& is) : in(is), cur(0), line(1), col(0) { nextChar(); }

    Token nextToken() {
        while (isspace(cur)) nextChar();
        if (cur == 0) return Token(TOKEN_EOF, "EOF");
        if (isalpha(cur) || cur == '_') return readIdent();
        if (isdigit(cur)) return readNumber();
        if (cur == '/') {
            nextChar();
            if (cur == '/') { skipLineComment(); return nextToken(); }
            if (cur == '*') { skipBlockComment(); return nextToken(); }
            return Token(TOKEN_DIV, "/");
        }
        char ch = cur;
        nextChar();
        switch (ch) {
        case '=':
            if (cur == '=') { nextChar(); return Token(TOKEN_EQ, "=="); }
            return Token(TOKEN_ASSIGN, "=");
        case '!':
            if (cur == '=') { nextChar(); return Token(TOKEN_NE, "!="); }
            return Token(TOKEN_UNKNOWN, "!");
        case '<':
            if (cur == '=') { nextChar(); return Token(TOKEN_LE, "<="); }
            return Token(TOKEN_LT, "<");
        case '>':
            if (cur == '=') { nextChar(); return Token(TOKEN_GE, ">="); }
            return Token(TOKEN_GT, ">");
        case '+': return Token(TOKEN_PLUS, "+");
        case '-': return Token(TOKEN_MINUS, "-");
        case '*': return Token(TOKEN_MUL, "*");
        case '(': return Token(TOKEN_LPAREN, "(");
        case ')': return Token(TOKEN_RPAREN, ")");
        case '{': return Token(TOKEN_LBRACE, "{");
        case '}': return Token(TOKEN_RBRACE, "}");
        case ';': return Token(TOKEN_SEMICOLON, ";");
        default: return Token(TOKEN_UNKNOWN, string(1, ch));
        }
    }

private:
    void nextChar() {
        if (in.get(cur)) {
            if (cur == '\n') { line++; col = 0; }
            else col++;
        }
        else cur = 0;
    }

    Token readIdent() {
        string ident;
        while (isalnum(cur) || cur == '_') {
            ident += cur;
            nextChar();
        }
        if (ident == "int") return Token(TOKEN_INT, ident);
        if (ident == "if") return Token(TOKEN_IF, ident);
        if (ident == "else") return Token(TOKEN_ELSE, ident);
        if (ident == "while") return Token(TOKEN_WHILE, ident);
        if (ident == "return") return Token(TOKEN_RETURN, ident);
        return Token(TOKEN_IDENT, ident);
    }

    Token readNumber() {
        string num;
        while (isdigit(cur)) {
            num += cur;
            nextChar();
        }
        int val = atoi(num.c_str());
        return Token(TOKEN_NUMBER, num, val);
    }

    void skipLineComment() {
        while (cur != '\n' && cur != 0) nextChar();
    }

    void skipBlockComment() {
        nextChar(); // 跳过 '*'
        while (true) {
            if (cur == 0) break;
            if (cur == '*') {
                nextChar();
                if (cur == '/') { nextChar(); break; }
            }
            else nextChar();
        }
    }
};

// ---------- 抽象语法树 ----------
struct Expr;
struct Stmt;
struct Function;

struct Expr {
    virtual ~Expr() {}
};

struct IntConst : Expr {
    int value;
    IntConst(int v) : value(v) {}
};

struct VarRef : Expr {
    string name;
    VarRef(const string& n) : name(n) {}
};

enum BinOp { BIN_ADD, BIN_SUB, BIN_MUL, BIN_DIV, BIN_LT, BIN_LE, BIN_GT, BIN_GE, BIN_EQ, BIN_NE };

// 自定义赋值操作码（不与枚举值冲突）
const BinOp BIN_ASSIGN = static_cast<BinOp>(100);

struct BinaryOp : Expr {
    BinOp op;
    unique_ptr<Expr> left, right;
    BinaryOp(BinOp o, unique_ptr<Expr> l, unique_ptr<Expr> r) : op(o), left(move(l)), right(move(r)) {}
};

struct Stmt {
    virtual ~Stmt() {}
};

struct AssignStmt : Stmt {
    string var;
    unique_ptr<Expr> rhs;
    AssignStmt(const string& v, unique_ptr<Expr> e) : var(v), rhs(move(e)) {}
};

struct IfStmt : Stmt {
    unique_ptr<Expr> cond;
    unique_ptr<Stmt> thenStmt;
    unique_ptr<Stmt> elseStmt;
    IfStmt(unique_ptr<Expr> c, unique_ptr<Stmt> t, unique_ptr<Stmt> e = nullptr)
        : cond(move(c)), thenStmt(move(t)), elseStmt(move(e)) {
    }
};

struct WhileStmt : Stmt {
    unique_ptr<Expr> cond;
    unique_ptr<Stmt> body;
    WhileStmt(unique_ptr<Expr> c, unique_ptr<Stmt> b) : cond(move(c)), body(move(b)) {}
};

struct ReturnStmt : Stmt {
    unique_ptr<Expr> expr;
    ReturnStmt(unique_ptr<Expr> e) : expr(move(e)) {}
};

struct BlockStmt : Stmt {
    vector<unique_ptr<Stmt>> stmts;
};

// 声明语句节点，用于处理变量声明
struct DeclStmt : Stmt {
    string var;
    DeclStmt(const string& v) : var(v) {}
};

struct Function {
    string name;
    vector<string> params; // 暂不支持参数
    unique_ptr<BlockStmt> body;
};

struct Program {
    vector<unique_ptr<Function>> functions;
};

// ---------- 符号表与作用域 ----------
struct Symbol {
    int offset; // 栈偏移（相对于ebp，负值）
    Symbol(int off = 0) : offset(off) {}
};

class Scope {
    vector<map<string, Symbol>> scopes;
    int stackSize; // 当前局部变量占用总字节数
public:
    Scope() : stackSize(0) { push(); }

    void push() { scopes.emplace_back(); }
    void pop() { scopes.pop_back(); }

    bool declare(const string& name) {
        if (scopes.back().count(name)) return false; // 重复定义
        stackSize += 4;
        scopes.back()[name] = Symbol(-stackSize);
        return true;
    }

    Symbol* lookup(const string& name) {
        for (auto it = scopes.rbegin(); it != scopes.rend(); ++it) {
            auto f = it->find(name);
            if (f != it->end()) return &f->second;
        }
        return nullptr;
    }

    int totalStackSize() const { return stackSize; }
};

// ---------- 语法分析 ----------
class Parser {
    Lexer& lex;
    Token curTok;
public:
    Parser(Lexer& l) : lex(l) { curTok = lex.nextToken(); }

    unique_ptr<Program> parse() {
        auto prog = make_unique<Program>();
        while (curTok.type != TOKEN_EOF) {
            if (curTok.type == TOKEN_INT) {
                prog->functions.push_back(parseFunction());
            }
            else {
                error("expected 'int' for function");
            }
        }
        return prog;
    }

private:
    void advance() { curTok = lex.nextToken(); }
    bool check(TokenType tt) { return curTok.type == tt; }
    bool match(TokenType tt) {
        if (check(tt)) { advance(); return true; }
        return false;
    }
    void expect(TokenType tt, const string& msg) {
        if (!match(tt)) error(msg);
    }

    [[noreturn]] void error(const string& msg) {
        cerr << "语法错误: " << msg << " 在 '" << curTok.text << "' 附近\n";
        exit(1);
    }

    // 函数解析：int name ( ) { ... }
    unique_ptr<Function> parseFunction() {
        expect(TOKEN_INT, "需要 'int'");
        if (curTok.type != TOKEN_IDENT) error("需要函数名");
        string name = curTok.text;
        advance();
        expect(TOKEN_LPAREN, "需要 '('");
        expect(TOKEN_RPAREN, "需要 ')'");
        expect(TOKEN_LBRACE, "需要 '{'");
        auto body = parseBlock();
        auto func = make_unique<Function>();
        func->name = name;
        func->body = move(body);
        return func;
    }

    // 块语句：{ ... }
    unique_ptr<BlockStmt> parseBlock() {
        auto block = make_unique<BlockStmt>();
        while (!check(TOKEN_RBRACE) && !check(TOKEN_EOF)) {
            block->stmts.push_back(parseStmt());
        }
        expect(TOKEN_RBRACE, "需要 '}'");
        return block;
    }

    // 语句解析
    unique_ptr<Stmt> parseStmt() {
        if (check(TOKEN_INT)) {
            advance(); // 跳过int
            if (curTok.type != TOKEN_IDENT) error("需要变量名");
            string var = curTok.text;
            advance();
            expect(TOKEN_SEMICOLON, "需要 ';' 在声明后");
            return make_unique<DeclStmt>(var);
        }
        if (check(TOKEN_IF)) return parseIf();
        if (check(TOKEN_WHILE)) return parseWhile();
        if (check(TOKEN_RETURN)) return parseReturn();
        if (check(TOKEN_LBRACE)) return parseBlock();
        return parseExpressionStmt();
    }

    unique_ptr<Stmt> parseIf() {
        advance(); // 'if'
        expect(TOKEN_LPAREN, "需要 '(' 在 if 后");
        auto cond = parseExpr();
        expect(TOKEN_RPAREN, "需要 ')' 在条件后");
        auto thenStmt = parseStmt();
        unique_ptr<Stmt> elseStmt;
        if (match(TOKEN_ELSE)) {
            elseStmt = parseStmt();
        }
        return make_unique<IfStmt>(move(cond), move(thenStmt), move(elseStmt));
    }

    unique_ptr<Stmt> parseWhile() {
        advance(); // 'while'
        expect(TOKEN_LPAREN, "需要 '(' 在 while 后");
        auto cond = parseExpr();
        expect(TOKEN_RPAREN, "需要 ')' 在条件后");
        auto body = parseStmt();
        return make_unique<WhileStmt>(move(cond), move(body));
    }

    unique_ptr<Stmt> parseReturn() {
        advance(); // 'return'
        auto expr = parseExpr();
        expect(TOKEN_SEMICOLON, "需要 ';' 在 return 后");
        return make_unique<ReturnStmt>(move(expr));
    }

    // 表达式语句：expr ;
    unique_ptr<Stmt> parseExpressionStmt() {
        auto expr = parseExpr();
        expect(TOKEN_SEMICOLON, "需要 ';'");
        // 表达式语句可能是赋值
        if (auto assign = dynamic_cast<BinaryOp*>(expr.get())) {
            if (assign->op == BIN_ASSIGN) {
                // 提取左值变量名
                if (auto leftVar = dynamic_cast<VarRef*>(assign->left.get())) {
                    string var = leftVar->name;
                    unique_ptr<Expr> rhs = move(assign->right);
                    auto stmt = make_unique<AssignStmt>(var, move(rhs));
                    return stmt;
                }
            }
        }
        // 其他表达式忽略（如函数调用，暂不支持）
        error("只有赋值表达式可以作为语句");
        return nullptr;
    }

    // 表达式解析（优先级处理）
    unique_ptr<Expr> parseExpr() { return parseAssignment(); }

    unique_ptr<Expr> parseAssignment() {
        auto left = parseEquality();
        if (match(TOKEN_ASSIGN)) {
            auto right = parseAssignment();
            // 构建赋值节点（作为二元运算，左值应为VarRef）
            if (auto var = dynamic_cast<VarRef*>(left.get())) {
                auto node = make_unique<BinaryOp>(BIN_ASSIGN, move(left), move(right));
                return node;
            }
            else {
                error("赋值左边必须是变量");
            }
        }
        return left;
    }

    unique_ptr<Expr> parseEquality() {
        auto left = parseRelational();
        while (true) {
            if (match(TOKEN_EQ)) {
                auto right = parseRelational();
                left = make_unique<BinaryOp>(BIN_EQ, move(left), move(right));
            }
            else if (match(TOKEN_NE)) {
                auto right = parseRelational();
                left = make_unique<BinaryOp>(BIN_NE, move(left), move(right));
            }
            else break;
        }
        return left;
    }

    unique_ptr<Expr> parseRelational() {
        auto left = parseAdditive();
        while (true) {
            if (match(TOKEN_LT)) {
                auto right = parseAdditive();
                left = make_unique<BinaryOp>(BIN_LT, move(left), move(right));
            }
            else if (match(TOKEN_LE)) {
                auto right = parseAdditive();
                left = make_unique<BinaryOp>(BIN_LE, move(left), move(right));
            }
            else if (match(TOKEN_GT)) {
                auto right = parseAdditive();
                left = make_unique<BinaryOp>(BIN_GT, move(left), move(right));
            }
            else if (match(TOKEN_GE)) {
                auto right = parseAdditive();
                left = make_unique<BinaryOp>(BIN_GE, move(left), move(right));
            }
            else break;
        }
        return left;
    }

    unique_ptr<Expr> parseAdditive() {
        auto left = parseMultiplicative();
        while (true) {
            if (match(TOKEN_PLUS)) {
                auto right = parseMultiplicative();
                left = make_unique<BinaryOp>(BIN_ADD, move(left), move(right));
            }
            else if (match(TOKEN_MINUS)) {
                auto right = parseMultiplicative();
                left = make_unique<BinaryOp>(BIN_SUB, move(left), move(right));
            }
            else break;
        }
        return left;
    }

    unique_ptr<Expr> parseMultiplicative() {
        auto left = parsePrimary();
        while (true) {
            if (match(TOKEN_MUL)) {
                auto right = parsePrimary();
                left = make_unique<BinaryOp>(BIN_MUL, move(left), move(right));
            }
            else if (match(TOKEN_DIV)) {
                auto right = parsePrimary();
                left = make_unique<BinaryOp>(BIN_DIV, move(left), move(right));
            }
            else break;
        }
        return left;
    }

    unique_ptr<Expr> parsePrimary() {
        if (match(TOKEN_NUMBER)) {
            return make_unique<IntConst>(curTok.intVal);
        }
        if (match(TOKEN_IDENT)) {
            return make_unique<VarRef>(curTok.text);
        }
        if (match(TOKEN_LPAREN)) {
            auto expr = parseExpr();
            expect(TOKEN_RPAREN, "需要 ')'");
            return expr;
        }
        error("需要基本表达式");
        return nullptr;
    }
};

// ---------- 代码生成 ----------
class CodeGenerator {
    ostream& out;
    Scope* globalScope; // 实际只需要函数内作用域，这里简化，为每个函数单独处理
public:
    CodeGenerator(ostream& os) : out(os), globalScope(nullptr) {}

    void generate(Program* prog) {
        out << "; Emerging语言生成的汇编 (NASM语法)\n";
        out << "section .text\n";
        out << "global _start\n\n";
        out << "_start:\n";
        out << "    call main\n";
        out << "    mov ebx, eax\n";
        out << "    mov eax, 1\n";
        out << "    int 0x80\n\n";

        for (auto& func : prog->functions) {
            generateFunction(func.get());
        }
    }

private:
    void generateFunction(Function* func) {
        out << func->name << ":\n";
        out << "    push ebp\n";
        out << "    mov ebp, esp\n";

        Scope localScope;
        globalScope = &localScope; // 用于变量查找
        // 第一遍：收集所有局部变量声明（通过遍历语句中的DeclStmt）
        int stackSize = collectDeclarations(func->body.get(), localScope);
        if (stackSize > 0) {
            out << "    sub esp, " << stackSize << "\n";
        }

        // 生成函数体语句
        generateBlock(func->body.get(), localScope);

        // 函数末尾隐含return 0？但标准要求有return，没有则返回0？
        // 假设用户一定有return
        out << "    leave\n";
        out << "    ret\n\n";
    }

    int collectDeclarations(Stmt* stmt, Scope& scope) {
        int size = 0;
        if (auto block = dynamic_cast<BlockStmt*>(stmt)) {
            scope.push();
            for (auto& s : block->stmts) {
                size += collectDeclarations(s.get(), scope);
            }
            scope.pop();
        }
        else if (auto decl = dynamic_cast<DeclStmt*>(stmt)) {
            // 声明语句
            scope.declare(decl->var);
            size += 4;
        }
        else if (auto ifs = dynamic_cast<IfStmt*>(stmt)) {
            size += collectDeclarations(ifs->thenStmt.get(), scope);
            if (ifs->elseStmt) size += collectDeclarations(ifs->elseStmt.get(), scope);
        }
        else if (auto whiles = dynamic_cast<WhileStmt*>(stmt)) {
            size += collectDeclarations(whiles->body.get(), scope);
        }
        // 注意：AssignStmt和ReturnStmt没有声明
        return size;
    }

    void generateBlock(BlockStmt* block, Scope& scope) {
        scope.push();
        for (auto& s : block->stmts) {
            generateStmt(s.get(), scope);
        }
        scope.pop();
    }

    void generateStmt(Stmt* stmt, Scope& scope) {
        if (auto assign = dynamic_cast<AssignStmt*>(stmt)) {
            generateAssign(assign, scope);
        }
        else if (auto ifs = dynamic_cast<IfStmt*>(stmt)) {
            generateIf(ifs, scope);
        }
        else if (auto whiles = dynamic_cast<WhileStmt*>(stmt)) {
            generateWhile(whiles, scope);
        }
        else if (auto ret = dynamic_cast<ReturnStmt*>(stmt)) {
            generateReturn(ret, scope);
        }
        else if (auto block = dynamic_cast<BlockStmt*>(stmt)) {
            generateBlock(block, scope);
        }
        else if (auto decl = dynamic_cast<DeclStmt*>(stmt)) {
            // 声明已在collect中处理，这里无代码生成
        }
    }

    void generateAssign(AssignStmt* assign, Scope& scope) {
        generateExpr(assign->rhs.get(), scope); // 结果在eax
        Symbol* sym = scope.lookup(assign->var);
        if (!sym) { cerr << "未定义的变量: " << assign->var << endl; exit(1); }
        out << "    mov [ebp" << showpos << sym->offset << noshowpos << "], eax\n";
    }

    void generateIf(IfStmt* ifs, Scope& scope) {
        static int labelCounter = 0;
        int id = labelCounter++;
        string labelElse = ".Lelse" + to_string(id);
        string labelEnd = ".Lend" + to_string(id);

        generateCondition(ifs->cond.get(), scope, labelElse); // 条件为假跳转到else
        generateStmt(ifs->thenStmt.get(), scope);
        out << "    jmp " << labelEnd << "\n";
        out << labelElse << ":\n";
        if (ifs->elseStmt) generateStmt(ifs->elseStmt.get(), scope);
        out << labelEnd << ":\n";
    }

    void generateWhile(WhileStmt* whiles, Scope& scope) {
        static int labelCounter = 0;
        int id = labelCounter++;
        string labelStart = ".Lstart" + to_string(id);
        string labelEnd = ".Lend" + to_string(id);

        out << labelStart << ":\n";
        generateCondition(whiles->cond.get(), scope, labelEnd);
        generateStmt(whiles->body.get(), scope);
        out << "    jmp " << labelStart << "\n";
        out << labelEnd << ":\n";
    }

    void generateReturn(ReturnStmt* ret, Scope& scope) {
        generateExpr(ret->expr.get(), scope); // 返回值在eax
        out << "    leave\n";
        out << "    ret\n";
    }

    // 条件生成：如果条件为假，跳转到label
    void generateCondition(Expr* cond, Scope& scope, const string& falseLabel) {
        // 简化：假设条件是比较表达式或常数
        if (auto bin = dynamic_cast<BinaryOp*>(cond)) {
            if (bin->op >= BIN_LT && bin->op <= BIN_NE) {
                generateExpr(bin->left.get(), scope); // 左值在eax
                out << "    push eax\n";
                generateExpr(bin->right.get(), scope); // 右值在eax
                out << "    pop ebx\n";
                out << "    cmp ebx, eax\n";
                const char* jcc = nullptr;
                switch (bin->op) {
                case BIN_LT: jcc = "jge"; break; // 如果左>=右，则跳（即不满足小于）
                case BIN_LE: jcc = "jg"; break;
                case BIN_GT: jcc = "jle"; break;
                case BIN_GE: jcc = "jl"; break;
                case BIN_EQ: jcc = "jne"; break;
                case BIN_NE: jcc = "je"; break;
                default: break;
                }
                out << "    " << jcc << " " << falseLabel << "\n";
                return;
            }
        }
        // 非比较表达式：将其值作为布尔，0为假，非0为真
        generateExpr(cond, scope);
        out << "    cmp eax, 0\n";
        out << "    je " << falseLabel << "\n";
    }

    void generateExpr(Expr* expr, Scope& scope) {
        if (auto num = dynamic_cast<IntConst*>(expr)) {
            out << "    mov eax, " << num->value << "\n";
        }
        else if (auto var = dynamic_cast<VarRef*>(expr)) {
            Symbol* sym = scope.lookup(var->name);
            if (!sym) { cerr << "未定义的变量: " << var->name << endl; exit(1); }
            out << "    mov eax, [ebp" << showpos << sym->offset << noshowpos << "]\n";
        }
        else if (auto bin = dynamic_cast<BinaryOp*>(expr)) {
            if (bin->op == BIN_ASSIGN) {
                // 赋值：右值生成，然后存入左变量，结果在eax
                generateExpr(bin->right.get(), scope);
                if (auto leftVar = dynamic_cast<VarRef*>(bin->left.get())) {
                    Symbol* sym = scope.lookup(leftVar->name);
                    if (!sym) { cerr << "未定义的变量: " << leftVar->name << endl; exit(1); }
                    out << "    mov [ebp" << showpos << sym->offset << noshowpos << "], eax\n";
                }
                else {
                    cerr << "无效的赋值目标\n"; exit(1);
                }
            }
            else {
                // 二元运算
                generateExpr(bin->left.get(), scope);
                out << "    push eax\n";
                generateExpr(bin->right.get(), scope);
                out << "    pop ebx\n";
                switch (bin->op) {
                case BIN_ADD: out << "    add eax, ebx\n"; break;
                case BIN_SUB: out << "    sub ebx, eax\n    mov eax, ebx\n"; break; // ebx - eax
                case BIN_MUL: out << "    imul eax, ebx\n"; break;
                case BIN_DIV: out << "    xchg eax, ebx\n    cdq\n    idiv ebx\n"; break; // ebx / eax -> eax
                default:
                    // 比较运算，返回布尔值0/1
                    out << "    cmp ebx, eax\n";
                    const char* setcc = nullptr;
                    switch (bin->op) {
                    case BIN_LT: setcc = "setl"; break;
                    case BIN_LE: setcc = "setle"; break;
                    case BIN_GT: setcc = "setg"; break;
                    case BIN_GE: setcc = "setge"; break;
                    case BIN_EQ: setcc = "sete"; break;
                    case BIN_NE: setcc = "setne"; break;
                    default: break;
                    }
                    out << "    xor eax, eax\n";
                    out << "    " << setcc << " al\n";
                    break;
                }
            }
        }
    }
};

int main(int argc, char* argv[]) {
    // 解析命令行参数
    if (argc >= 2 && string(argv[1]) == "--version") {
        printVersionAndExit();
    }
    if (argc < 2) {
        cerr << "用法: " << argv[0] << " [--version] <输入文件.emg> [输出文件.asm]\n";
        return 1;
    }
    string infile = argv[1];
    string outfile = (argc > 2) ? argv[2] : (infile + ".asm");

    ifstream in(infile);
    if (!in) {
        cerr << "无法打开输入文件: " << infile << endl;
        return 1;
    }

    Lexer lex(in);
    Parser parser(lex);
    auto prog = parser.parse();

    ofstream out(outfile);
    if (!out) {
        cerr << "无法打开输出文件: " << outfile << endl;
        return 1;
    }

    CodeGenerator cg(out);
    cg.generate(prog.get());

    cout << "汇编代码已写入 " << outfile << endl;
    cout << "现在执行: nasm -f elf32 " << outfile << " -o " << infile << ".o" << endl;
    return 0;
}