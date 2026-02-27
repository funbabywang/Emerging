#include <iostream>
#include <sstream>
#include <string>
#include <map>
#include <cctype>
#include <vector>
#include <algorithm>
#include <stdexcept>

using namespace std;

// ---------- 词法分析 ----------
enum TokenType {
    TOK_EOF, TOK_IDENT, TOK_NUMBER,
    TOK_INT, TOK_PRINT, TOK_RETURN,
    TOK_ASSIGN, TOK_SEMICOLON, TOK_LPAREN, TOK_RPAREN,
    TOK_LBRACE, TOK_RBRACE, TOK_PLUS, TOK_MINUS,
    TOK_MUL, TOK_DIV, TOK_COMMA
};

struct Token {
    TokenType type;
    string text;
    int value;
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

        if (isalpha(ch)) {
            string id;
            while (isalnum(ch) || ch == '_') {
                id += ch;
                nextChar();
            }
            if (id == "int") return token = { TOK_INT, id, 0 };
            if (id == "print") return token = { TOK_PRINT, id, 0 };
            if (id == "return") return token = { TOK_RETURN, id, 0 };
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
        default: throw runtime_error(string("未知字符: ") + c);
        }
    }

    Token current() const { return token; }
    void advance() { nextToken(); }
    bool check(TokenType t) const { return token.type == t; }
    bool match(TokenType t) { if (check(t)) { advance(); return true; } return false; }
    void expect(TokenType t, const string& msg) {
        if (!check(t)) throw runtime_error("语法错误: 期望 " + msg);
    }
};

// ---------- 符号表 ----------
map<string, int> variables;

// ---------- 表达式求值 ----------
int parseExpression(Lexer& lex);

int parseFactor(Lexer& lex) {
    if (lex.check(TOK_NUMBER)) {
        int val = lex.current().value;
        lex.advance();
        return val;
    }
    else if (lex.check(TOK_IDENT)) {
        string name = lex.current().text;
        lex.advance();
        auto it = variables.find(name);
        if (it == variables.end())
            throw runtime_error("未定义变量 '" + name + "'");
        return it->second;
    }
    else if (lex.check(TOK_LPAREN)) {
        lex.advance(); // '('
        int val = parseExpression(lex);
        lex.expect(TOK_RPAREN, "')'");
        lex.advance(); // ')'
        return val;
    }
    else {
        throw runtime_error("语法错误: 期望因子");
    }
}

int parseTerm(Lexer& lex) {
    int left = parseFactor(lex);
    while (lex.check(TOK_MUL) || lex.check(TOK_DIV)) {
        Token op = lex.current();
        lex.advance();
        int right = parseFactor(lex);
        if (op.type == TOK_MUL) left *= right;
        else if (op.type == TOK_DIV) {
            if (right == 0) throw runtime_error("除零");
            left /= right;
        }
    }
    return left;
}

int parseExpression(Lexer& lex) {
    int left = parseTerm(lex);
    while (lex.check(TOK_PLUS) || lex.check(TOK_MINUS)) {
        Token op = lex.current();
        lex.advance();
        int right = parseTerm(lex);
        if (op.type == TOK_PLUS) left += right;
        else left -= right;
    }
    return left;
}

// ---------- 语句执行 ----------
void parseStatement(Lexer& lex) {
    if (lex.check(TOK_INT)) {
        lex.advance();
        lex.expect(TOK_IDENT, "变量名");
        string varName = lex.current().text;
        lex.advance();
        if (lex.check(TOK_ASSIGN)) {
            lex.advance(); // '='
            int val = parseExpression(lex);
            variables[varName] = val;
        }
        else {
            variables[varName] = 0; // 默认初始化
        }
        lex.expect(TOK_SEMICOLON, "';'");
        lex.advance();
    }
    else if (lex.check(TOK_IDENT)) {
        string varName = lex.current().text;
        lex.advance();
        lex.expect(TOK_ASSIGN, "'='");
        lex.advance();
        int val = parseExpression(lex);
        lex.expect(TOK_SEMICOLON, "';'");
        lex.advance();
        variables[varName] = val;
    }
    else if (lex.check(TOK_PRINT)) {
        lex.advance();
        lex.expect(TOK_LPAREN, "'('");
        lex.advance();
        int val = parseExpression(lex);
        lex.expect(TOK_RPAREN, "')'");
        lex.advance();
        lex.expect(TOK_SEMICOLON, "';'");
        lex.advance();
        cout << val << endl;
    }
    else if (lex.check(TOK_RETURN)) {
        lex.advance();
        int val = parseExpression(lex);
        lex.expect(TOK_SEMICOLON, "';'");
        lex.advance();
        cout << "返回值: " << val << endl;
    }
    else {
        throw runtime_error("未知语句");
    }
}

// ---------- 帮助 ----------
void printHelp() {
    cout << "EmergingShell 命令:\n";
    cout << "  .exit     退出\n";
    cout << "  .vars     显示所有变量\n";
    cout << "  .help     显示此帮助\n";
    cout << "支持语句: int var [= expr]; var = expr; print(expr); return expr;\n";
}

// ---------- 主交互循环 ----------
int main() {
    cout << "EmergingShell v1.1 (Emerging Language REPL)\n";
    cout << "输入 .help 获取帮助，.exit 退出。\n";

    string line;
    while (true) {
        cout << "> ";
        if (!getline(cin, line)) break; // 输入流关闭则退出
        if (line.empty()) continue;

        // 去掉行首尾空白
        size_t start = line.find_first_not_of(" \t\r\n");
        if (start == string::npos) continue;
        size_t end = line.find_last_not_of(" \t\r\n");
        string trimmed = line.substr(start, end - start + 1);

        // 处理内置命令
        if (trimmed[0] == '.') {
            if (trimmed == ".exit") break;
            else if (trimmed == ".vars") {
                if (variables.empty()) cout << "无变量\n";
                else for (const auto& p : variables) cout << p.first << " = " << p.second << "\n";
                continue;
            }
            else if (trimmed == ".help") { printHelp(); continue; }
            else { cout << "未知命令: " << trimmed << "\n"; continue; }
        }

        // 解析并执行语句
        istringstream iss(trimmed);
        Lexer lex(iss);
        try {
            parseStatement(lex);
            if (!lex.check(TOK_EOF))
                cout << "警告: 输入包含多余内容，已忽略。\n";
        }
        catch (const exception& e) {
            cout << "错误: " << e.what() << endl;
        }
    }

    return 0;
}