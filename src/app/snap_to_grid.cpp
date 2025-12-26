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

#include <cstdlib>
#include <cmath>

namespace app {

// Helper function for isometric grid snapping
// Grid pattern: vertical lines at dx spacing, diagonals connecting them
// Snap to diamond pattern vertices where all lines intersect
static gfx::Point snap_to_isometric_grid(const gfx::Rect& grid, const gfx::Point& point,
                                          const PreferSnapTo prefer)
{
  if (grid.isEmpty())
    return point;

  const double tileW = grid.w;
  const double tileH = grid.h;
  const double dx = tileW / 2.0;  // Vertical line spacing
  const double dy = tileH;        // Diagonal step height

  // Find the base grid position
  double baseX = grid.x;
  double baseY = grid.y;

  // For isometric grid with verticals at dx spacing:
  // Intersection points form a diamond pattern where:
  // - Row 0: x = baseX, baseX + tileW, baseX + 2*tileW, ... (even columns)
  // - Row 1: x = baseX + dx, baseX + dx + tileW, ... (odd columns)
  // Pattern alternates each row

  // Find the closest row
  double relY = point.y - baseY;
  int row = static_cast<int>(std::round(relY / dy));
  double snapY = baseY + row * dy;

  // Calculate X position based on row parity (alternating pattern)
  double relX = point.x - baseX;
  bool oddRow = (row % 2) != 0;
  
  // Offset for odd rows
  double xOffset = oddRow ? dx : 0.0;
  
  // Find closest column (using tileW spacing within each row type)
  int col = static_cast<int>(std::round((relX - xOffset) / tileW));
  double snapX = baseX + xOffset + col * tileW;

  // Also check the adjacent row's closest point
  int altRow = (relY / dy - row > 0) ? row + 1 : row - 1;
  double altSnapY = baseY + altRow * dy;
  bool altOddRow = (altRow % 2) != 0;
  double altXOffset = altOddRow ? dx : 0.0;
  int altCol = static_cast<int>(std::round((relX - altXOffset) / tileW));
  double altSnapX = baseX + altXOffset + altCol * tileW;

  // Return the closest point
  double dist1 = std::hypot(snapX - point.x, snapY - point.y);
  double dist2 = std::hypot(altSnapX - point.x, altSnapY - point.y);

  if (dist2 < dist1) {
    return gfx::Point(static_cast<int>(altSnapX), static_cast<int>(altSnapY));
  }
  return gfx::Point(static_cast<int>(snapX), static_cast<int>(snapY));
}

gfx::Point snap_to_grid(const gfx::Rect& grid, const gfx::Point& point,
                        const PreferSnapTo prefer, const gen::GridType gridType)
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
