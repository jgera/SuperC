#pragma once
#include <string>

struct MathResult {
    bool success;
    double value;
    std::wstring formatted;
    std::wstring error;
};

// Evaluates a mathematical expression string (e.g., "5+7", "(12.5*4)/2", "2^10", "sqrt(144)")
// Returns success = false if invalid or not a math expression.
MathResult EvaluateMath(const std::wstring& expr);

// Checks if the string begins with characteristics typical of a math query or explicit math prefix
bool LooksLikeMath(const std::wstring& expr);
