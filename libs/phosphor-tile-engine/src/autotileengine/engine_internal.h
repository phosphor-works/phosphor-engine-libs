// SPDX-FileCopyrightText: 2026 Phosphor Works Team
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QObject>
#include <QRect>
#include <QVector>

#include <algorithm>
#include <PhosphorEngine/NavigationContext.h>
#include <PhosphorEngine/PerScreenKeys.h>
#include "tileenginelogging.h"

namespace PhosphorTileEngine {

using NavigationContext = PhosphorEngine::NavigationContext;
using TilingStateKey = PhosphorEngine::TilingStateKey;
namespace PerScreenKeys = PhosphorEngine::PerScreenKeys;

template<typename T>
T* checkedCast(QObject* obj, const char* context)
{
    if (!obj)
        return nullptr;
    auto* concrete = qobject_cast<T*>(obj);
    if (!concrete) {
        qCWarning(PhosphorTileEngine::lcTileEngine) << context << ": QObject is not the expected type — skipping";
    }
    return concrete;
}

/// Whether the first @p tileCount zones are one rect repeated, which is how a
/// monocle stack looks (the monocle algorithm, or a degenerate fillArea
/// fallback on a tiny screen). A single window is ordinary tiling, so it
/// takes at least two. The count is clamped to the zone vector, because a
/// stale vector can be shorter than the tiled count.
inline bool isMonocleLayout(const QVector<QRect>& zones, int tileCount)
{
    const int count = std::min(tileCount, static_cast<int>(zones.size()));
    return count >= 2 && std::all_of(zones.begin() + 1, zones.begin() + count, [&](const QRect& z) {
               return z == zones[0];
           });
}

} // namespace PhosphorTileEngine
