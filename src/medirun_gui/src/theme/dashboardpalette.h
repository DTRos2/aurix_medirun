#ifndef DASHBOARDPALETTE_H
#define DASHBOARDPALETTE_H

#include <QColor>

namespace DashboardPalette {

inline const QColor CardBg{QStringLiteral("#0b1622")};
inline const QColor PanelBg{QStringLiteral("#0d1f30")};
inline const QColor HeaderInnerBg{QStringLiteral("#0d1a29")};

inline const QColor BorderMuted{QStringLiteral("#1d3348")};
inline const QColor BorderDivider{QStringLiteral("#16293c")};
inline const QColor BorderGuide{QStringLiteral("#2a4a66")};

inline const QColor TextPrimary{QStringLiteral("#e8eef5")};
inline const QColor TextSecondary{QStringLiteral("#a8bdd2")};
inline const QColor TextTertiary{QStringLiteral("#7d93aa")};
inline const QColor TextMuted{QStringLiteral("#4a6b88")};

inline const QColor Cyan{QStringLiteral("#39c5e0")};
inline const QColor CyanOn{QStringLiteral("#062633")};
inline const QColor CyanBg{QStringLiteral("#12283c")};
inline const QColor CyanBorder{QStringLiteral("#2a5f7d")};

inline const QColor Green{QStringLiteral("#4ade80")};
inline const QColor GreenOn{QStringLiteral("#062f18")};
inline const QColor GreenBg{QStringLiteral("#0f2e22")};
inline const QColor GreenBorder{QStringLiteral("#1d5c3f")};

inline const QColor Amber{QStringLiteral("#fbbf24")};
inline const QColor AmberOn{QStringLiteral("#332508")};
inline const QColor AmberBg{QStringLiteral("#332508")};
inline const QColor AmberBorder{QStringLiteral("#6b4e12")};

inline const QColor Red{QStringLiteral("#f87171")};
inline const QColor RedOn{QStringLiteral("#3d1220")};
inline const QColor RedBg{QStringLiteral("#3d1220")};
inline const QColor RedBorder{QStringLiteral("#7c2438")};

// Robot marker "ready/idle" fill — distinct from TextMuted's usage as stroke-only tone.
inline const QColor RobotReady{QStringLiteral("#3a5570")};

} // namespace DashboardPalette

#endif // DASHBOARDPALETTE_H
