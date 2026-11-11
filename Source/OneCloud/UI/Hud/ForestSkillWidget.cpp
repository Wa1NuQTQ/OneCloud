#include "ForestSkillWidget.h"

#include "Components/Image.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "ForestLakeHudInk.h"
#include "SacredLakeWidget.h"

void UForestSkillWidget::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (SkillIcon && SkillTexture) { SkillIcon->SetBrushFromTexture(SkillTexture); }
	if (KeyLabel) { KeyLabel->SetText(KeyGlyph); }
}

void UForestSkillWidget::Refresh(const FForestSkillHudState& State, float AvailableCloudWater)
{
	using namespace OneCloud::ForestHud;
	const bool bCooling = State.CooldownRemainingSeconds > 0.f;
	const bool bMissingWater = State.bUnlocked && !bCooling && AvailableCloudWater + KINDA_SMALL_NUMBER < State.CloudCost;
	const FLinearColor Color = State.bReady ? CloudColor : MistColor;
	SkillIcon->SetColorAndOpacity(Color.CopyWithNewOpacity(State.bUnlocked ? 1.f : 0.3f));
	LockIcon->SetVisibility(State.bUnlocked ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	MissingWaterIcon->SetVisibility(bMissingWater ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const bool bTargetBlocked = State.bUnlocked && !bCooling && !bMissingWater && !State.bHasValidTarget;
	TargetBlockedIcon->SetVisibility(bTargetBlocked ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	CooldownBar->SetVisibility(State.bUnlocked && bCooling ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const float Remaining01 = State.CooldownDurationSeconds > 0.f
		? FMath::Clamp(State.CooldownRemainingSeconds / State.CooldownDurationSeconds, 0.f, 1.f) : 0.f;
	CooldownBar->SetPercent(1.f - Remaining01);
	CostLabel->SetText(State.bUnlocked && State.CloudCost > 0.f
		? FText::AsNumber(FMath::RoundToInt(State.CloudCost)) : FText::GetEmpty());
	if (CostWater) { CostWater->SetVisibility(State.bUnlocked && State.CloudCost > 0.f
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	CostLabel->SetColorAndOpacity(FSlateColor(bMissingWater ? FLinearColor(0.98f, 0.63f, 0.4f) : MistColor));
}
