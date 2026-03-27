#pragma once
#include <QString>
#include <QStringList>

// ── VersionUtils ──────────────────────────────────────────────────────────────
// Утилиты для семантического сравнения версий формата "major.minor.patch"
// (поддерживается опциональный 4-й компонент: "major.minor.patch.hotfix").
// Использование: #include "versionutils.h" — заголовочный файл без .cpp.

class VersionUtils {
public:
    // Сравнивает две семантические версии.
    // Возвращает: -1 если v1 < v2,  0 если v1 == v2,  1 если v1 > v2.
    // Пустая строка или некорректный формат считается "0.0.0".
    [[nodiscard]] static int compare(const QString& v1, const QString& v2) {
        const QStringList p1 = v1.split('.');
        const QStringList p2 = v2.split('.');
        const int len = qMax(p1.size(), p2.size());
        for (int i = 0; i < len; ++i) {
            // Отсутствующие компоненты считаются нулём (0.4 == 0.4.0)
            bool ok1 = false, ok2 = false;
            const int a = (i < p1.size()) ? p1[i].toInt(&ok1) : 0;
            const int b = (i < p2.size()) ? p2[i].toInt(&ok2) : 0;
            // Некорректный компонент (не число) считается 0
            const int ca = ok1 ? a : 0;
            const int cb = ok2 ? b : 0;
            if (ca < cb) return -1;
            if (ca > cb) return  1;
        }
        return 0;
    }

    // Проверяет: v1 строго новее чем v2?
    [[nodiscard]] static bool isNewerThan(const QString& v1, const QString& v2) {
        return compare(v1, v2) > 0;
    }

    // Нормализует версию, заменяя пустую строку на безопасный дефолт "0.1.0"
    [[nodiscard]] static QString normalize(const QString& v) {
        return v.isEmpty() ? QStringLiteral("0.1.0") : v;
    }
};
