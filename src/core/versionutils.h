#pragma once
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

// ── VersionUtils ──────────────────────────────────────────────────────────────
// Header-only. Сравнивает семантические версии "major.minor.patch[.hotfix]".

class VersionUtils {
public:
    // Возвращает: -1 если v1 < v2,  0 если v1 == v2,  1 если v1 > v2.
    [[nodiscard]] static int compare(const std::string& v1, const std::string& v2) {
        const auto parts = [](const std::string& v) {
            std::vector<int> result;
            std::istringstream ss(v);
            std::string tok;
            while (std::getline(ss, tok, '.')) {
                try { result.push_back(std::stoi(tok)); }
                catch (...) { result.push_back(0); }
            }
            return result;
        };
        const auto p1 = parts(v1);
        const auto p2 = parts(v2);
        const std::size_t len = std::max(p1.size(), p2.size());
        for (std::size_t i = 0; i < len; ++i) {
            const int a = (i < p1.size()) ? p1[i] : 0;
            const int b = (i < p2.size()) ? p2[i] : 0;
            if (a < b) return -1;
            if (a > b) return  1;
        }
        return 0;
    }

    [[nodiscard]] static bool isNewerThan(const std::string& v1, const std::string& v2) {
        return compare(v1, v2) > 0;
    }

    [[nodiscard]] static std::string normalize(const std::string& v) {
        return v.empty() ? "0.1.0" : v;
    }
};
