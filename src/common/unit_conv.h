#pragma once
#include <string>

struct UnitConvResult {
    bool success = false;
    double originalValue = 0.0;
    double convertedValue = 0.0;
    std::wstring fromUnit;
    std::wstring toUnit;
    std::wstring formatted;
};

// Tries to parse and evaluate unit conversions like:
// "100c in f", "10 km to miles", "500mb in gb", "150 lbs to kg", "24 hours in days"
UnitConvResult ConvertUnits(const std::wstring& input);

bool LooksLikeUnitConversion(const std::wstring& input);
