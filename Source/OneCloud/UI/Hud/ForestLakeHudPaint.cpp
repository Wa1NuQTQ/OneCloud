#include "SacredLakeWidget.h"

#include "Game/SacredLake/SacredLakePrototype.h"

// 固定资源条、技能槽和情境行均由 WBP_SacredLakeHUD 的 Designer 绘制。
// 世界圆环和岸向标记必须随相机投影，才留在原生绘制中。
int32 USacredLakeWidget::NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
	const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
	const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const
{
	const int32 BaseLayer = Super::NativePaint(Args, AllottedGeometry, MyCullingRect, OutDrawElements,
		LayerId, InWidgetStyle, bParentEnabled);
	return bForestStatus && CurrentResult == ESacredLakeResult::Defending
		? PaintForestGuidance(AllottedGeometry, OutDrawElements, BaseLayer) : BaseLayer;
}
