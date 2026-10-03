#include "unit_conv.h"
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cmath>
#include <map>
#include <vector>

namespace {

std::wstring ToLower(const std::wstring& str) {
    std::wstring res = str;
    for (auto& c : res) c = towlower(c);
    return res;
}

std::wstring NormalizeUnit(const std::wstring& raw) {
    std::wstring u = ToLower(raw);
    // Length
    if (u == L"km" || u == L"kilometer" || u == L"kilometers") return L"km";
    if (u == L"m" || u == L"meter" || u == L"meters") return L"m";
    if (u == L"cm" || u == L"centimeter" || u == L"centimeters") return L"cm";
    if (u == L"mm" || u == L"millimeter" || u == L"millimeters") return L"mm";
    if (u == L"mi" || u == L"mile" || u == L"miles") return L"mi";
    if (u == L"ft" || u == L"foot" || u == L"feet") return L"ft";
    if (u == L"in" || u == L"inch" || u == L"inches") return L"in";
    if (u == L"yd" || u == L"yard" || u == L"yards") return L"yd";

    // Temperature
    if (u == L"c" || u == L"celsius") return L"c";
    if (u == L"f" || u == L"fahrenheit") return L"f";
    if (u == L"k" || u == L"kelvin") return L"k";

    // Weight
    if (u == L"kg" || u == L"kilogram" || u == L"kilograms") return L"kg";
    if (u == L"g" || u == L"gram" || u == L"grams") return L"g";
    if (u == L"mg" || u == L"milligram" || u == L"milligrams") return L"mg";
    if (u == L"lb" || u == L"lbs" || u == L"pound" || u == L"pounds") return L"lb";
    if (u == L"oz" || u == L"ounce" || u == L"ounces") return L"oz";

    // Digital Storage
    if (u == L"b" || u == L"byte" || u == L"bytes") return L"b";
    if (u == L"kb" || u == L"kilobyte" || u == L"kilobytes") return L"kb";
    if (u == L"mb" || u == L"megabyte" || u == L"megabytes") return L"mb";
    if (u == L"gb" || u == L"gigabyte" || u == L"gigabytes") return L"gb";
    if (u == L"tb" || u == L"terabyte" || u == L"terabytes") return L"tb";

    // Time
    if (u == L"ms" || u == L"millisecond" || u == L"milliseconds") return L"ms";
    if (u == L"s" || u == L"sec" || u == L"second" || u == L"seconds") return L"s";
    if (u == L"min" || u == L"minute" || u == L"minutes") return L"min";
    if (u == L"h" || u == L"hr" || u == L"hour" || u == L"hours") return L"h";
    if (u == L"d" || u == L"day" || u == L"days") return L"d";
    if (u == L"w" || u == L"wk" || u == L"week" || u == L"weeks") return L"w";
    if (u == L"y" || u == L"yr" || u == L"year" || u == L"years") return L"y";

    return u;
}

std::wstring FormatValue(double val) {
    if (std::isfinite(val) && std::floor(val) == val && std::fabs(val) < 1e12) {
        return std::to_wstring(static_cast<long long>(val));
    }
    std::wostringstream oss;
    oss << std::setprecision(6) << val;
    return oss.str();
}

} // namespace

UnitConvResult ConvertUnits(const std::wstring& rawInput) {
    UnitConvResult res;
    std::wstring input = rawInput;

    // Trim
    size_t start = input.find_first_not_of(L" \t");
    if (start == std::wstring::npos) return res;
    input = input.substr(start);

    // Split words
    std::wistringstream iss(input);
    std::vector<std::wstring> tokens;
    std::wstring t;
    while (iss >> t) tokens.push_back(t);

    if (tokens.size() < 2) return res;

    // Pattern 1: [100c] [in|to] [f]  (3 tokens)
    // Pattern 2: [100] [c] [in|to] [f] (4 tokens)
    double val = 0.0;
    std::wstring fromStr, toStr;

    if (tokens.size() >= 3 && (ToLower(tokens[1]) == L"in" || ToLower(tokens[1]) == L"to")) {
        // tokens[0] contains number + unit e.g. "100c"
        std::wstring first = tokens[0];
        size_t digitEnd = 0;
        bool hasDot = false;
        while (digitEnd < first.length() && (iswdigit(first[digitEnd]) || (first[digitEnd] == L'.' && !hasDot) || (digitEnd == 0 && (first[digitEnd] == L'+' || first[digitEnd] == L'-')))) {
            if (first[digitEnd] == L'.') hasDot = true;
            digitEnd++;
        }
        if (digitEnd == 0 || digitEnd >= first.length()) return res;
        val = _wtof(first.substr(0, digitEnd).c_str());
        fromStr = first.substr(digitEnd);
        toStr = tokens[2];
    } else if (tokens.size() >= 4 && (ToLower(tokens[2]) == L"in" || ToLower(tokens[2]) == L"to")) {
        // tokens[0]=100, tokens[1]=km, tokens[2]=in, tokens[3]=miles
        wchar_t* endPtr = nullptr;
        val = wcstod(tokens[0].c_str(), &endPtr);
        if (endPtr == tokens[0].c_str()) return res;
        fromStr = tokens[1];
        toStr = tokens[3];
    } else {
        return res;
    }

    std::wstring uFrom = NormalizeUnit(fromStr);
    std::wstring uTo = NormalizeUnit(toStr);

    if (uFrom.empty() || uTo.empty()) return res;

    double converted = 0.0;
    bool valid = false;

    // 1. Temperature
    if (uFrom == L"c" || uFrom == L"f" || uFrom == L"k") {
        if (uTo == L"c" || uTo == L"f" || uTo == L"k") {
            // Convert to Celsius first
            double cVal = val;
            if (uFrom == L"f") cVal = (val - 32.0) * 5.0 / 9.0;
            else if (uFrom == L"k") cVal = val - 273.15;

            // Convert Celsius to Target
            if (uTo == L"c") converted = cVal;
            else if (uTo == L"f") converted = (cVal * 9.0 / 5.0) + 32.0;
            else if (uTo == L"k") converted = cVal + 273.15;
            valid = true;
        }
    }

    // 2. Length (Base: Meter)
    static const std::map<std::wstring, double> lengthToMeter = {
        { L"mm", 0.001 }, { L"cm", 0.01 }, { L"m", 1.0 }, { L"km", 1000.0 },
        { L"in", 0.0254 }, { L"ft", 0.3048 }, { L"yd", 0.9144 }, { L"mi", 1609.344 }
    };
    if (!valid && lengthToMeter.count(uFrom) && lengthToMeter.count(uTo)) {
        double inMeters = val * lengthToMeter.at(uFrom);
        converted = inMeters / lengthToMeter.at(uTo);
        valid = true;
    }

    // 3. Weight (Base: Gram)
    static const std::map<std::wstring, double> weightToGram = {
        { L"mg", 0.001 }, { L"g", 1.0 }, { L"kg", 1000.0 },
        { L"oz", 28.349523 }, { L"lb", 453.59237 }
    };
    if (!valid && weightToGram.count(uFrom) && weightToGram.count(uTo)) {
        double inGrams = val * weightToGram.at(uFrom);
        converted = inGrams / weightToGram.at(uTo);
        valid = true;
    }

    // 4. Digital Storage (Base: Bytes)
    static const std::map<std::wstring, double> dataToBytes = {
        { L"b", 1.0 }, { L"kb", 1024.0 }, { L"mb", 1024.0 * 1024.0 },
        { L"gb", 1024.0 * 1024.0 * 1024.0 }, { L"tb", 1024.0 * 1024.0 * 1024.0 * 1024.0 }
    };
    if (!valid && dataToBytes.count(uFrom) && dataToBytes.count(uTo)) {
        double inBytes = val * dataToBytes.at(uFrom);
        converted = inBytes / dataToBytes.at(uTo);
        valid = true;
    }

    // 5. Time (Base: Seconds)
    static const std::map<std::wstring, double> timeToSec = {
        { L"ms", 0.001 }, { L"s", 1.0 }, { L"min", 60.0 }, { L"h", 3600.0 },
        { L"d", 86400.0 }, { L"w", 604800.0 }, { L"y", 31536000.0 }
    };
    if (!valid && timeToSec.count(uFrom) && timeToSec.count(uTo)) {
        double inSec = val * timeToSec.at(uFrom);
        converted = inSec / timeToSec.at(uTo);
        valid = true;
    }

    if (valid) {
        res.success = true;
        res.originalValue = val;
        res.convertedValue = converted;
        res.fromUnit = uFrom;
        res.toUnit = uTo;
        res.formatted = FormatValue(converted) + L" " + uTo;
    }

    return res;
}

bool LooksLikeUnitConversion(const std::wstring& input) {
    std::wstring lower = ToLower(input);
    return (lower.find(L" in ") != std::wstring::npos || lower.find(L" to ") != std::wstring::npos);
}
