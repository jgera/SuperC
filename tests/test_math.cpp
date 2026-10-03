#include <iostream>
#include <cassert>
#include <cmath>
#include "../src/common/math_eval.h"
#include "../src/common/unit_conv.h"

int main() {
    std::wcout << L"Running SuperC Test Suite..." << std::endl;

    // 1. Math Tests
    auto testMath = [](const std::wstring& expr, double expected) {
        MathResult res = EvaluateMath(expr);
        if (!res.success) {
            std::wcout << L"[FAIL] Expr: " << expr << L" Error: " << res.error << std::endl;
            return false;
        }
        if (std::abs(res.value - expected) > 1e-6) {
            std::wcout << L"[FAIL] Expr: " << expr << L" Expected: " << expected << L" Got: " << res.value << std::endl;
            return false;
        }
        std::wcout << L"[PASS Math] " << expr << L" = " << res.formatted << std::endl;
        return true;
    };

    assert(testMath(L"5+7", 12.0));
    assert(testMath(L"100 - 35.5", 64.5));
    assert(testMath(L"12 * 4", 48.0));
    assert(testMath(L"100 / 4", 25.0));
    assert(testMath(L"(10 + 2) * 3", 36.0));
    assert(testMath(L"2^10", 1024.0));
    assert(testMath(L"sqrt(144)", 12.0));

    // 2. Unit Conversion Tests
    auto testUnit = [](const std::wstring& query, double expected) {
        UnitConvResult res = ConvertUnits(query);
        if (!res.success) {
            std::wcout << L"[FAIL Unit] Query: " << query << std::endl;
            return false;
        }
        if (std::abs(res.convertedValue - expected) > 1e-3) {
            std::wcout << L"[FAIL Unit] Query: " << query << L" Expected: " << expected << L" Got: " << res.convertedValue << std::endl;
            return false;
        }
        std::wcout << L"[PASS Unit] " << query << L" = " << res.formatted << std::endl;
        return true;
    };

    assert(testUnit(L"100c in f", 212.0));
    assert(testUnit(L"32f in c", 0.0));
    assert(testUnit(L"10km in m", 10000.0));
    assert(testUnit(L"1024mb in gb", 1.0));
    // 3. Ctrl+Backspace Word-Left Deletion Tests
    auto testWordDelete = [](const std::wstring& input, int caret, const std::wstring& expectedText, int expectedCaret) {
        int pos = caret;
        while (pos > 0 && iswspace(input[pos - 1])) {
            pos--;
        }
        if (pos > 0) {
            bool isAlphaNum = iswalnum(input[pos - 1]) || input[pos - 1] == L'_';
            if (isAlphaNum) {
                while (pos > 0 && (iswalnum(input[pos - 1]) || input[pos - 1] == L'_')) {
                    pos--;
                }
            } else {
                while (pos > 0 && !iswalnum(input[pos - 1]) && input[pos - 1] != L'_' && !iswspace(input[pos - 1])) {
                    pos--;
                }
            }
        }
        std::wstring result = input.substr(0, pos) + input.substr(caret);
        if (result != expectedText || pos != expectedCaret) {
            std::wcout << L"[FAIL WordDelete] Input: \"" << input << L"\" Expected: \"" << expectedText << L"\" (pos " << expectedCaret << L") Got: \"" << result << L"\" (pos " << pos << L")" << std::endl;
            return false;
        }
        std::wcout << L"[PASS WordDelete] \"" << input << L"\" -> \"" << result << L"\"" << std::endl;
        return true;
    };

    assert(testWordDelete(L"notepad calc", 12, L"notepad ", 8));
    assert(testWordDelete(L"notepad ", 8, L"", 0));
    assert(testWordDelete(L"hello   world", 13, L"hello   ", 8));
    assert(testWordDelete(L"hello   world   ", 16, L"hello   ", 8));
    assert(testWordDelete(L"5 + 12", 6, L"5 + ", 4));
    assert(testWordDelete(L"5 + ", 4, L"5 ", 2));
    assert(testWordDelete(L"5 ", 2, L"", 0));
    assert(testWordDelete(L"", 0, L"", 0));
    assert(testWordDelete(L"   ", 3, L"", 0));
    assert(testWordDelete(L"c:\\path\\file", 12, L"c:\\path\\", 8));

    std::wcout << L"All SuperC Tests PASSED successfully!" << std::endl;
    return 0;
}
