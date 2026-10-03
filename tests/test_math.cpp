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
    assert(testUnit(L"24h in d", 1.0));

    std::wcout << L"All SuperC Tests PASSED successfully!" << std::endl;
    return 0;
}
