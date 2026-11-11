#pragma once

#include "CoreTypes.h"

// C++ 落点分类。根据物理材质或敌人护甲得到，再交给声音和特效共用。
enum class ERainImpactSurface : uint8
{
	Unknown, Sand, Water, Leaf, Metal, Flesh, Soil, Count
};
