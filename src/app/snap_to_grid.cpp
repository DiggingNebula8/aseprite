// Aseprite
// Copyright (C) 2019-2020  Igara Studio S.A.
// Copyright (C) 2001-2016  David Capello
//
// This program is distributed under the terms of
// the End-User License Agreement for Aseprite.

#ifdef HAVE_CONFIG_H
  #include "config.h"
#endif

#include "app/snap_to_grid.h"

#include "app/pref/preferences.h"
#include "gfx/point.h"
#include "gfx/rect.h"

#include <cmath>
#include <cstdlib>

namespace app {

// Helper function for isometric grid snapping using diamond projection.
// Based on Clint Bellanger's "Isometric Tiles Math":
//   https://clintbellanger.net/articles/isometric_math/
//
// Diamond projection formulas:
//   screen.x = (tile.x - tile.y) * (TILE_WIDTH / 2)
//   screen.y = (tile.x + tile.y) * (TILE_HEIGHT / 2)
//
// Inverse (screen to tile):
//   tile.x = (screen.x / halfW + screen.y / halfH) / 2
//   tile.y = (screen.y / halfH - screen.x / halfW) / 2
//
// Works with any width/height ratio:
//   - 2:1 ratio (e.g., 32x16) = standard isometric (~26.57°)
//   - Other ratios create dimetric projections
static gfx::Point snap_to_isometric_grid(const gfx::Rect& grid,
                                         const gfx::Point& point,
                                         const PreferSnapTo prefer)
{
  if (grid.isEmpty())
    return point;

  const double tileW = grid.w; // Diamond width
  const double tileH = grid.h; // Diamond height
  const double halfW = tileW / 2.0;
  const double halfH = tileH / 2.0;

  // Grid origin in screen coordinates
  const double originX = grid.x;
  const double originY = grid.y;

  // Convert screen point to tile coordinates (relative to origin)
  const double relX = point.x - originX;
  const double relY = point.y - originY;
  // Screen to tile transformation (inverse of diamond projection)
  const double tileXf = (relX / halfW + relY / halfH) / 2.0;
  const double tileYf = (relY / halfH - relX / halfW) / 2.0;

  // Calculate tile coordinates based on snap preference
  int tileX, tileY;
  switch (prefer) {
    case PreferSnapTo::ClosestGridVertex:
    default:
      // Round to nearest tile coordinate (diamond vertex)
      tileX = static_cast<int>(std::round(tileXf));
      tileY = static_cast<int>(std::round(tileYf));
      break;

    case PreferSnapTo::BoxOrigin:
    case PreferSnapTo::FloorGrid:
      // Floor to get the tile containing the point (top-left of bounding box)
      tileX = static_cast<int>(std::floor(tileXf));
      tileY = static_cast<int>(std::floor(tileYf));
      break;

    case PreferSnapTo::BoxEnd:
    case PreferSnapTo::CeilGrid:
      // Ceil to get the next tile boundary (bottom-right of bounding box)
      tileX = static_cast<int>(std::ceil(tileXf));
      tileY = static_cast<int>(std::ceil(tileYf));
      break;
  }

  // Convert back to screen coordinates (tile to screen transformation)
  const double snapX = originX + (tileX - tileY) * halfW;
  const double snapY = originY + (tileX + tileY) * halfH;

  gfx::Point bestSnap(static_cast<int>(std::round(snapX)), static_cast<int>(std::round(snapY)));

  // For ClosestGridVertex, also consider vertices on the nearest vertical
  // grid line as candidates. Vertical lines are at x = originX + k * halfW,
  // with vertices along them at y = originY + s * halfH for integer k, s.
  // This feature is disabled by default but can be enabled for future use.
  constexpr bool kSnapToVerticals = false;

  if (kSnapToVerticals && prefer == PreferSnapTo::ClosestGridVertex) {
    double bestDist = std::hypot(bestSnap.x - point.x, bestSnap.y - point.y);

    // Snap X to nearest vertical line.
    const double verticalIndex = relX / halfW;
    const int nearestVertIdx = static_cast<int>(std::round(verticalIndex));
    const double verticalX = originX + nearestVertIdx * halfW;

    // Snap Y to nearest vertex along that vertical.
    const double tileSumf = relY / halfH;
    const int nearestSum = static_cast<int>(std::round(tileSumf));
    const double verticalY = originY + nearestSum * halfH;

    gfx::Point vertSnap(static_cast<int>(std::round(verticalX)),
                        static_cast<int>(std::round(verticalY)));
    const double vertDist = std::hypot(double(vertSnap.x - point.x), double(vertSnap.y - point.y));

    if (vertDist < bestDist)
      bestSnap = vertSnap;
  }

  return bestSnap;
}

gfx::Point snap_to_grid(const gfx::Rect& grid,
                        const gfx::Point& point,
                        const PreferSnapTo prefer,
                        const gen::GridType gridType)
{
  // For isometric grid, use specialized snapping
  if (gridType == gen::GridType::ISOMETRIC) {
    return snap_to_isometric_grid(grid, point, prefer);
  }

  // Original rectangular grid snapping
  if (grid.isEmpty())
    return point;

  div_t d, dx, dy;
  dx = std::div(grid.x, grid.w);
  dy = std::div(grid.y, grid.h);

  gfx::Point newPoint(point.x - dx.rem, point.y - dy.rem);
  if (prefer != PreferSnapTo::ClosestGridVertex) {
    if (newPoint.x < 0)
      newPoint.x -= grid.w;
    if (newPoint.y < 0)
      newPoint.y -= grid.h;
  }

  switch (prefer) {
    case PreferSnapTo::ClosestGridVertex:
      d = std::div(newPoint.x, grid.w);
      newPoint.x = dx.rem + d.quot * grid.w + ((d.rem > grid.w / 2) ? grid.w : 0);

      d = std::div(newPoint.y, grid.h);
      newPoint.y = dy.rem + d.quot * grid.h + ((d.rem > grid.h / 2) ? grid.h : 0);
      break;

    case PreferSnapTo::BoxOrigin:
    case PreferSnapTo::FloorGrid:
      d = std::div(newPoint.x, grid.w);
      newPoint.x = dx.rem + d.quot * grid.w;

      d = std::div(newPoint.y, grid.h);
      newPoint.y = dy.rem + d.quot * grid.h;
      break;

    case PreferSnapTo::CeilGrid:
      d = std::div(newPoint.x, grid.w);
      newPoint.x = d.rem ? dx.rem + (d.quot + 1) * grid.w : newPoint.x;

      d = std::div(newPoint.y, grid.h);
      newPoint.y = d.rem ? dy.rem + (d.quot + 1) * grid.h : newPoint.y;
      break;

    case PreferSnapTo::BoxEnd:
      d = std::div(newPoint.x, grid.w);
      newPoint.x = dx.rem + (d.quot + 1) * grid.w;

      d = std::div(newPoint.y, grid.h);
      newPoint.y = dy.rem + (d.quot + 1) * grid.h;
      break;
  }

  return newPoint;
}

} // namespace app
