#pragma once

#include "CellPositions.h"
#include <array>

class AdjacentCellFinder
{
public:
	AdjacentCellFinder(const CellPositions::positions_list& positions, int dimensions, int marker_size);

	std::array<int, 4> find(int index);

	int dimensions() const;
	int marker_size() const;

protected:
	int in_row_with_margin(int index) const;

	int right(int index) const;
	int left(int index) const;
	int bottom(int index) const;
	int top(int index) const;

protected:
	const CellPositions::positions_list& _positions;
	int _dimensions;
	int _markerSize;
	int _firstMid;
	int _firstBottom;
};
