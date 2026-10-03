#include <iostream>
#include <cassert>
#include "../src/math_eval.h"

int main() {
    std::wcout << L"Running Math Evaluator Unit Tests..." << std::endl;

    auto test = [](const std::wstring& expr, double expected) {
        MathResult res = EvaluateMath(expr);
        if (!res.success) {
            std::wcout << L"[FAIL] Expr: " << expr << L" Error: " << res.error << std::endl;
            return false;
        }
        if (std::abs(res.value - expected) > 1e-6) {
            std::wcout << L"[FAIL] Expr: " << expr << L" Expected: " << expected << L" Got: " << res.value << std::endl;
            return false;
        }
        std::wcout << L"[PASS] " << expr << L" = " << res.formatted << std::endl;
        return true;
    };

    assert(test(L"5+7", 12.0));
    assert(test(L"100 - 35.5", 64.5));
    assert(test(L"12 * 4", 48.0));
    assert(test(L"100 / 4", 25.0));
    assert(test(L"(10 + 2) * 3", 36.0));
    assert(test(L"2^10", 1024.0));
    assert(test(L"sqrt(144)", 12.0));
    assert(test(L"abs(-42)", 42.0));
    assert(test(L"round(3.7)", 4.0));
    assert(test(L"0x10 + 5", 21.0));
    assert(test(L"100 * 15%", 15.0));
    assert(test(L"50 + 10%", 50.1)); // 50 + 0.1

    assert(LooksLikeMath(L"5+7") == true);
    assert(LooksLikeMath(L"100*1.18") == true);
    assert(LooksLikeMath(L"sqrt(25)") == true);
    assert(LooksLikeMath(L"ipconfig") == false);
    assert(LooksLikeMath(L"ping 8.8.8.8") == false);
    assert(LooksLikeMath(L"git status") == false);

    std::wcout << L"All tests PASSED successfully!" << std::endl;
    return 0;
}
