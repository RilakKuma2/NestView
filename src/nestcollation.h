#pragma once
#include <QCollator>
#include <QString>

// Numeric QCollator mode is unavailable in some Linux C/POSIX locales.
// Compare digit runs explicitly, without integer overflow or a locale change.
class NestNaturalCollator {
public:
    NestNaturalCollator() { text.setCaseSensitivity(Qt::CaseInsensitive); }
    int compare(const QString &a, const QString &b) const {
        int i = 0, j = 0;
        auto digit = [](QChar c) { return c >= QLatin1Char('0') && c <= QLatin1Char('9'); };
        while (i < a.size() && j < b.size()) {
            if (digit(a[i]) && digit(b[j])) {
                int endA = i, endB = j;
                while (endA < a.size() && digit(a[endA])) ++endA;
                while (endB < b.size() && digit(b[endB])) ++endB;
                int significantA = i, significantB = j;
                while (significantA < endA && a[significantA] == QLatin1Char('0')) ++significantA;
                while (significantB < endB && b[significantB] == QLatin1Char('0')) ++significantB;
                if (endA - significantA != endB - significantB)
                    return endA - significantA < endB - significantB ? -1 : 1;
                const int result = QString::compare(a.mid(significantA, endA - significantA), b.mid(significantB, endB - significantB));
                if (result) return result;
                i = endA; j = endB;
            } else {
                int endA = i + 1, endB = j + 1;
                while (endA < a.size() && !digit(a[endA])) ++endA;
                while (endB < b.size() && !digit(b[endB])) ++endB;
                const int result = text.compare(a.mid(i, endA - i), b.mid(j, endB - j));
                if (result) return result;
                i = endA; j = endB;
            }
        }
        return i == a.size() ? (j == b.size() ? 0 : -1) : 1;
    }
private:
    QCollator text;
};
