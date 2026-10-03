#include "math_eval.h"
#include <cmath>
#include <cctype>
#include <sstream>
#include <iomanip>
#include <vector>
#include <algorithm>

namespace {

struct Token {
    enum Type {
        Number,
        Plus,
        Minus,
        Multiply,
        Divide,
        Modulo,
        Power,
        LParen,
        RParen,
        Comma,
        Identifier,
        EndOfInput,
        Error
    } type;
    double number_value = 0.0;
    std::wstring str_value;
};

class Lexer {
public:
    explicit Lexer(const std::wstring& src) : input(src), pos(0) {}

    Token NextToken() {
        SkipWhitespace();
        if (pos >= input.length()) {
            return { Token::EndOfInput, 0.0, L"" };
        }

        wchar_t c = input[pos];

        // Hex number: 0x...
        if (c == L'0' && pos + 1 < input.length() && (input[pos + 1] == L'x' || input[pos + 1] == L'X')) {
            size_t start = pos;
            pos += 2;
            while (pos < input.length() && iswxdigit(input[pos])) {
                pos++;
            }
            std::wstring hexStr = input.substr(start, pos - start);
            wchar_t* endPtr = nullptr;
            unsigned __int64 val = _wcstoui64(hexStr.c_str(), &endPtr, 16);
            return { Token::Number, static_cast<double>(val), hexStr };
        }

        // Decimal number
        if (iswdigit(c) || (c == L'.' && pos + 1 < input.length() && iswdigit(input[pos + 1]))) {
            size_t start = pos;
            bool hasDot = (c == L'.');
            pos++;
            while (pos < input.length()) {
                if (input[pos] == L'.') {
                    if (hasDot) break;
                    hasDot = true;
                    pos++;
                } else if (iswdigit(input[pos])) {
                    pos++;
                } else {
                    break;
                }
            }

            // Check percentage suffix like 15%
            bool isPercent = false;
            if (pos < input.length() && input[pos] == L'%') {
                // If it's % followed by an operand or end of token, check if it's modulo or percentage
                // e.g. "15%" vs "15 % 4"
                // If followed by space and digit, could be modulo; if followed by operator or end, percentage
                size_t nextPos = pos + 1;
                while (nextPos < input.length() && iswspace(input[nextPos])) nextPos++;
                if (nextPos >= input.length() || input[nextPos] == L'+' || input[nextPos] == L'-' || 
                    input[nextPos] == L'*' || input[nextPos] == L'/' || input[nextPos] == L')') {
                    isPercent = true;
                    pos++;
                }
            }

            std::wstring numStr = input.substr(start, isPercent ? (pos - start - 1) : (pos - start));
            double val = _wtof(numStr.c_str());
            if (isPercent) val /= 100.0;
            return { Token::Number, val, numStr };
        }

        // Identifiers (functions and constants like sqrt, pi, sin, e)
        if (iswalpha(c) || c == L'_') {
            size_t start = pos;
            while (pos < input.length() && (iswalnum(input[pos]) || input[pos] == L'_')) {
                pos++;
            }
            std::wstring id = input.substr(start, pos - start);
            std::transform(id.begin(), id.end(), id.begin(), ::towlower);
            return { Token::Identifier, 0.0, id };
        }

        pos++;
        switch (c) {
            case L'+': return { Token::Plus, 0.0, L"+" };
            case L'-': return { Token::Minus, 0.0, L"-" };
            case L'*': return { Token::Multiply, 0.0, L"*" };
            case L'/': return { Token::Divide, 0.0, L"/" };
            case L'%': return { Token::Modulo, 0.0, L"%" };
            case L'^': return { Token::Power, 0.0, L"^" };
            case L'(': return { Token::LParen, 0.0, L"(" };
            case L')': return { Token::RParen, 0.0, L")" };
            case L',': return { Token::Comma, 0.0, L"," };
            default:   return { Token::Error, 0.0, std::wstring(1, c) };
        }
    }

private:
    std::wstring input;
    size_t pos;

    void SkipWhitespace() {
        while (pos < input.length() && iswspace(input[pos])) {
            pos++;
        }
    }
};

class Parser {
public:
    explicit Parser(Lexer& lexer) : lexer(lexer), hasError(false) {
        Advance();
    }

    bool Parse(double& outResult, std::wstring& outError) {
        outResult = ParseExpression();
        if (hasError) {
            outError = errorMessage;
            return false;
        }
        if (currentToken.type != Token::EndOfInput) {
            outError = L"Unexpected token: " + currentToken.str_value;
            return false;
        }
        return true;
    }

private:
    Lexer& lexer;
    Token currentToken;
    bool hasError;
    std::wstring errorMessage;

    void Advance() {
        currentToken = lexer.NextToken();
    }

    void Error(const std::wstring& msg) {
        if (!hasError) {
            hasError = true;
            errorMessage = msg;
        }
    }

    // Expression := Term { ('+' | '-') Term }
    double ParseExpression() {
        double left = ParseTerm();
        while (currentToken.type == Token::Plus || currentToken.type == Token::Minus) {
            Token::Type op = currentToken.type;
            Advance();
            double right = ParseTerm();
            if (op == Token::Plus) left += right;
            else left -= right;
        }
        return left;
    }

    // Term := Power { ('*' | '/' | '%') Power }
    double ParseTerm() {
        double left = ParsePower();
        while (currentToken.type == Token::Multiply || currentToken.type == Token::Divide || currentToken.type == Token::Modulo) {
            Token::Type op = currentToken.type;
            Advance();
            double right = ParsePower();
            if (op == Token::Multiply) {
                left *= right;
            } else if (op == Token::Divide) {
                if (right == 0.0) {
                    Error(L"Division by zero");
                    return 0.0;
                }
                left /= right;
            } else if (op == Token::Modulo) {
                if (right == 0.0) {
                    Error(L"Modulo by zero");
                    return 0.0;
                }
                left = std::fmod(left, right);
            }
        }
        return left;
    }

    // Power := Factor [ '^' Power ]
    double ParsePower() {
        double left = ParseFactor();
        if (currentToken.type == Token::Power) {
            Advance();
            double right = ParsePower(); // right-associative
            left = std::pow(left, right);
        }
        return left;
    }

    // Factor := ('+' | '-') Factor | Number | '(' Expression ')' | Identifier | Function
    double ParseFactor() {
        if (currentToken.type == Token::Plus) {
            Advance();
            return ParseFactor();
        }
        if (currentToken.type == Token::Minus) {
            Advance();
            return -ParseFactor();
        }
        if (currentToken.type == Token::Number) {
            double val = currentToken.number_value;
            Advance();
            return val;
        }
        if (currentToken.type == Token::LParen) {
            Advance();
            double val = ParseExpression();
            if (currentToken.type != Token::RParen) {
                Error(L"Expected closing parenthesis ')'");
                return 0.0;
            }
            Advance();
            return val;
        }
        if (currentToken.type == Token::Identifier) {
            std::wstring name = currentToken.str_value;
            Advance();

            // Constants
            if (name == L"pi") return 3.14159265358979323846;
            if (name == L"e") return 2.71828182845904523536;

            // Functions must be followed by '('
            if (currentToken.type != Token::LParen) {
                Error(L"Unknown identifier: " + name);
                return 0.0;
            }
            Advance(); // consume '('
            double arg1 = ParseExpression();
            double arg2 = 0.0;
            if (currentToken.type == Token::Comma) {
                Advance();
                arg2 = ParseExpression();
            }
            if (currentToken.type != Token::RParen) {
                Error(L"Expected ')' after function arguments");
                return 0.0;
            }
            Advance(); // consume ')'

            if (name == L"sqrt") {
                if (arg1 < 0.0) { Error(L"Square root of negative number"); return 0.0; }
                return std::sqrt(arg1);
            } else if (name == L"cbrt") {
                return std::cbrt(arg1);
            } else if (name == L"abs") {
                return std::fabs(arg1);
            } else if (name == L"round") {
                return std::round(arg1);
            } else if (name == L"floor") {
                return std::floor(arg1);
            } else if (name == L"ceil") {
                return std::ceil(arg1);
            } else if (name == L"sin") {
                return std::sin(arg1);
            } else if (name == L"cos") {
                return std::cos(arg1);
            } else if (name == L"tan") {
                return std::tan(arg1);
            } else if (name == L"log" || name == L"log10") {
                if (arg1 <= 0.0) { Error(L"Logarithm of non-positive number"); return 0.0; }
                return std::log10(arg1);
            } else if (name == L"ln") {
                if (arg1 <= 0.0) { Error(L"Logarithm of non-positive number"); return 0.0; }
                return std::log(arg1);
            } else if (name == L"pow") {
                return std::pow(arg1, arg2);
            } else {
                Error(L"Unknown function: " + name);
                return 0.0;
            }
        }

        Error(L"Unexpected syntax at: " + currentToken.str_value);
        return 0.0;
    }
};

std::wstring FormatNumber(double val) {
    // If value is integer within representable range
    if (std::isfinite(val) && std::floor(val) == val && std::fabs(val) < 1e15) {
        long long intVal = static_cast<long long>(val);
        return std::to_wstring(intVal);
    }
    std::wostringstream oss;
    oss << std::setprecision(10) << val;
    std::wstring s = oss.str();
    return s;
}

} // namespace

MathResult EvaluateMath(const std::wstring& expr) {
    MathResult res;
    res.success = false;
    res.value = 0.0;

    std::wstring clean = expr;
    // Strip leading '=' or 'calc ' or 'm ' if present
    if (!clean.empty() && clean[0] == L'=') {
        clean = clean.substr(1);
    } else if (clean.rfind(L"calc ", 0) == 0) {
        clean = clean.substr(5);
    } else if (clean.rfind(L"m ", 0) == 0) {
        clean = clean.substr(2);
    }

    // Trim leading whitespace
    size_t start = clean.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) {
        res.error = L"Empty expression";
        return res;
    }
    clean = clean.substr(start);

    Lexer lexer(clean);
    Parser parser(lexer);

    double val = 0.0;
    std::wstring err;
    if (parser.Parse(val, err)) {
        res.success = true;
        res.value = val;
        res.formatted = FormatNumber(val);
    } else {
        res.success = false;
        res.error = err;
    }

    return res;
}

bool LooksLikeMath(const std::wstring& rawExpr) {
    std::wstring expr = rawExpr;
    // Trim
    size_t start = expr.find_first_not_of(L" \t\r\n");
    if (start == std::wstring::npos) return false;
    expr = expr.substr(start);

    // Explicit math prefix
    if (expr[0] == L'=' || expr.rfind(L"calc ", 0) == 0 || expr.rfind(L"m ", 0) == 0) {
        return true;
    }

    // Check if it contains math operators
    bool hasOperator = false;
    for (wchar_t c : expr) {
        if (c == L'+' || c == L'*' || c == L'/' || c == L'^' || c == L'%' || c == L'(' || c == L')') {
            hasOperator = true;
            break;
        }
    }

    // Also recognize function prefixes like sqrt(...), sin(...), etc.
    bool startsWithFunc = false;
    const wchar_t* funcs[] = { L"sqrt(", L"abs(", L"round(", L"floor(", L"ceil(", L"sin(", L"cos(", L"tan(", L"log(", L"ln(", L"pow(" };
    for (const wchar_t* f : funcs) {
        if (expr.rfind(f, 0) == 0) {
            startsWithFunc = true;
            break;
        }
    }

    // If it starts with a digit or minus or parenthesis or function, AND has operator or func:
    if (startsWithFunc || (hasOperator && (iswdigit(expr[0]) || expr[0] == L'(' || expr[0] == L'+' || expr[0] == L'-' || expr[0] == L'.'))) {
        // Test if it actually evaluates successfully
        MathResult testRes = EvaluateMath(expr);
        return testRes.success;
    }

    return false;
}
