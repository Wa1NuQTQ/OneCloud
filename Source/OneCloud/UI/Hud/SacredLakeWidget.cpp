#include "SacredLakeWidget.h"

#include "Components/ProgressBar.h"
#include "Components/WidgetSwitcher.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "Game/SacredLake/SacredLakePrototype.h"
#include "Widgets/SWidget.h"
#include "ForestLakeHudInk.h"
#include "ForestSkillWidget.h"
#include "Weather/OneCloudRain.h"

namespace
{
	constexpr float ObjectiveDisplaySeconds = 6.f;
	constexpr float FeedbackDisplaySeconds = 2.8f;
}

// 共用状态先入缓存，原型地图再同步到 Designer 控件。
void USacredLakeWidget::Refresh(float Water, float Capacity, ESacredLakeResult Result, bool bExtracting, bool bRaining)
{
	LakeWaterAmount = Water;
	LakeWaterCapacity = Capacity;
	LakeFraction = Capacity > 0.f ? FMath::Clamp(Water / Capacity, 0.f, 1.f) : 0.f;
	CurrentResult = Result;
	bSiphonActive = bExtracting;
	bRainActive = bRaining;
	if (bForestStatus)
	{
		RefreshForestDesigner();
		return;
	}
	if (!WaterBar || !SiphonMark || !RainControls || !RainState || !ResultOverlay || !ResultState)
	{
		return;
	}

	WaterBar->SetPercent(LakeFraction);
	WaterBar->SetFillColorAndOpacity(bExtracting ? SiphonColor : WaterColor);
	SiphonMark->SetVisibility(bExtracting ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const bool bDefending = Result == ESacredLakeResult::Defending;
	RainControls->SetVisibility(bDefending ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	ResultOverlay->SetVisibility(bDefending ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);

	if (bDefending)
	{
		// Designer 的第 0 项为待机云，第 1 项为降雨云。
		RainState->SetActiveWidgetIndex(bRaining ? 1 : 0);
		return;
	}

	switch (Result)
	{
	case ESacredLakeResult::Won:
		ResultState->SetActiveWidget(VictoryMark);
		break;
	case ESacredLakeResult::Dry:
		ResultState->SetActiveWidget(DryMark);
		break;
	case ESacredLakeResult::Unavailable:
		ResultState->SetActiveWidget(UnavailableMark);
		break;
	case ESacredLakeResult::Defending:
		break;
	}
}

// 固定面板保存在 Designer；原生绘制只留下依赖镜头投影的世界锚点。
void USacredLakeWidget::RefreshForest(float CloudWater, float CloudCapacity, int32 Seeds, float Pressure01, bool bBossPresent)
{
	if (!bForestStatus)
	{
		// 旧原型的装饰与控件一起隐藏，避免底板残留在森林资源条后方。
		if (PrototypePanel) { PrototypePanel->SetVisibility(ESlateVisibility::Collapsed); }
		for (UWidget* Widget : {static_cast<UWidget*>(WaterBar.Get()), SiphonMark.Get(), RainControls.Get(), ResultOverlay.Get()})
		{
			if (Widget)
			{
				Widget->SetVisibility(ESlateVisibility::Collapsed);
			}
		}
		SetVisibility(ESlateVisibility::HitTestInvisible);
		bForestStatus = true;
	}
	CloudWaterAmount = CloudWater;
	CloudWaterCapacity = CloudCapacity;
	CloudFraction = CloudCapacity > 0.f ? FMath::Clamp(CloudWater / CloudCapacity, 0.f, 1.f) : 0.f;
	SeedCount = FMath::Max(0, Seeds);
	BossPressure = FMath::Clamp(Pressure01, 0.f, 1.f);
	bBossActive = bBossPresent;
	RefreshForestDesigner();
	RefreshSkillDesigner();
	// 新数值只影响画面，标记 Paint 无效即可让 Slate 更新，无需重新布局。
	InvalidateForestPaint();
}

void USacredLakeWidget::RefreshForestSkillDetails(TConstArrayView<FForestSkillHudState> Skills)
{
	// 五个固定槽位的少量显示数据。冷却只由规则推送，不在绘制阶段扣时间。
	SkillDetails.Reset(5);
	for (const FForestSkillHudState& State : Skills.Left(FMath::Min(5, Skills.Num())))
	{
		SkillDetails.Add(State);
	}
	RefreshSkillDesigner();
	InvalidateForestPaint();
}

void USacredLakeWidget::SetForestRainRequested(bool bRequested)
{
	if (bRainRequested == bRequested)
	{
		return;
	}
	bRainRequested = bRequested;
	RefreshForestDesigner();
}

void USacredLakeWidget::SetForestPrecipitation(EOneCloudPrecipitation Mode, bool bShowSwitchHint)
{
	if (PrecipitationMode == Mode && bShowPrecipitationHint == bShowSwitchHint)
	{
		return;
	}
	PrecipitationMode = Mode;
	bShowPrecipitationHint = bShowSwitchHint;
	RefreshForestDesigner();
	InvalidateForestPaint();
}

void USacredLakeWidget::SetForestWeatherArrival(EOneCloudPrecipitation ArrivedMode, float ArrivalProgress01)
{
	ArrivedPrecipitationMode = ArrivedMode;
	WeatherArrivalProgress = FMath::Clamp(ArrivalProgress01, 0.f, 1.f);
	RefreshForestDesigner();
}

void USacredLakeWidget::SetForestGrowth(float Growth01)
{
	GrowthFraction = FMath::Clamp(Growth01, 0.f, 1.f);
	if (ForestGrowthBar)
	{
		ForestGrowthBar->SetPercent(GrowthFraction);
	}
}

void USacredLakeWidget::SetForestGuidance(const FSacredLakeGuidance& State)
{
	if (Guidance.Prompt == State.Prompt && Guidance.Plot == State.Plot &&
		Guidance.bPlanting == State.bPlanting && Guidance.bPlantingValid == State.bPlantingValid && Guidance.RefillAnchor == State.RefillAnchor &&
		Guidance.FocusCenter == State.FocusCenter &&
		Guidance.FocusRadiusCm == State.FocusRadiusCm && Guidance.ArrivedFocusCenter == State.ArrivedFocusCenter &&
		Guidance.ArrivedFocusRadiusCm == State.ArrivedFocusRadiusCm && Guidance.Threats == State.Threats &&
		Guidance.QueuedFocusCenter == State.QueuedFocusCenter && Guidance.QueuedFocusRadiusCm == State.QueuedFocusRadiusCm &&
		Guidance.bArmoredThreat == State.bArmoredThreat &&
		Guidance.bReticle == State.bReticle)
	{
		return;
	}
	const bool bWasWorldAnchored = Guidance.HasWorldAnchor();
	Guidance = State;
	RefreshContextLine();
	// 世界坐标未变时屏幕位置仍会随相机变化，有世界标记时要求 Slate 持续重绘。
	const bool bWorldAnchored = Guidance.HasWorldAnchor();
	if (bWorldAnchored != bWasWorldAnchored)
	{
		ForceVolatile(bWorldAnchored);
	}
	InvalidateForestPaint();
}

void USacredLakeWidget::SetForestObjective(const FText& Objective, bool bUrgent)
{
	if (ForestObjective.EqualTo(Objective) && bObjectiveUrgent == bUrgent)
	{
		return;
	}
	ForestObjective = Objective;
	bObjectiveUrgent = bUrgent;
	bObjectiveVisible = !Objective.IsEmpty();
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ObjectiveTimer);
		if (bObjectiveVisible && !bObjectiveUrgent)
		{
			// 只在目标变化时重新计时；规则反复推送同一步骤不会让教学常驻。
			World->GetTimerManager().SetTimer(ObjectiveTimer, this, &USacredLakeWidget::HideForestObjective,
				ObjectiveDisplaySeconds, false);
		}
	}
	RefreshContextLine();
	InvalidateForestPaint();
}

void USacredLakeWidget::HideForestObjective()
{
	bObjectiveVisible = false;
	RefreshContextLine();
	InvalidateForestPaint();
}

void USacredLakeWidget::ShowForestFeedback(const FText& Message, bool bWarning)
{
	ForestFeedback = Message;
	bFeedbackWarning = bWarning;
	RefreshContextLine();
	if (UWorld* World = GetWorld())
	{
		// 游戏暂停时保留当前提示；恢复后继续计时，避免玩家在菜单中错过反馈。
		World->GetTimerManager().SetTimer(FeedbackTimer, this, &USacredLakeWidget::ClearForestFeedback,
			FeedbackDisplaySeconds, false);
	}
	InvalidateForestPaint();
}

void USacredLakeWidget::ClearForestFeedback()
{
	ForestFeedback = FText::GetEmpty();
	RefreshContextLine();
	InvalidateForestPaint();
}

void USacredLakeWidget::RefreshForestDesigner()
{
	using namespace OneCloud::ForestHud;
	if (!ForestPanel)
	{
		return;
	}
	ForestPanel->SetVisibility(bForestStatus && CurrentResult == ESacredLakeResult::Defending
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	const FLinearColor LakeInk = bSiphonActive ? SiphonColor : WaterColor;
	if (ForestLakeBar) { ForestLakeBar->SetPercent(LakeFraction); ForestLakeBar->SetFillColorAndOpacity(LakeInk); }
	if (ForestLakeIcon) { ForestLakeIcon->SetColorAndOpacity(LakeInk); }
	if (ForestCloudBar) { ForestCloudBar->SetPercent(CloudFraction); }
	const FText AmountFormat = NSLOCTEXT("ForestHUD", "WaterAmount", "{0} / {1}");
	if (ForestLakeAmount)
	{
		ForestLakeAmount->SetText(FText::Format(AmountFormat,
			FText::AsNumber(FMath::RoundToInt(LakeWaterAmount)), FText::AsNumber(FMath::RoundToInt(LakeWaterCapacity))));
	}
	if (ForestCloudAmount)
	{
		ForestCloudAmount->SetText(FText::Format(AmountFormat,
			FText::AsNumber(FMath::RoundToInt(CloudWaterAmount)), FText::AsNumber(FMath::RoundToInt(CloudWaterCapacity))));
	}
	if (ForestGrowthBar) { ForestGrowthBar->SetPercent(GrowthFraction); }
	if (ForestPressureBar)
	{
		ForestPressureBar->SetPercent(BossPressure);
		ForestPressureBar->SetFillColorAndOpacity(bBossActive ? SiphonColor : SeedColor);
	}
	if (ForestPressureIcon) { ForestPressureIcon->SetColorAndOpacity(bBossActive ? SiphonColor : SeedColor); }
	if (ForestSeedCount) { ForestSeedCount->SetText(FText::AsNumber(SeedCount)); }
	UImage* WeatherIcons[] = {WeatherRain, WeatherSnow, WeatherHail};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(WeatherIcons); ++Index)
	{
		if (WeatherIcons[Index])
		{
			const bool bSelected = static_cast<int32>(PrecipitationMode) == Index;
			WeatherIcons[Index]->SetColorAndOpacity(bSelected
				? CloudColor : MistColor.CopyWithNewOpacity(0.35f));
			// 尺寸也表达选中状态，避免只凭颜色区分当前选择的天气。
			WeatherIcons[Index]->SetRenderScale(FVector2D(bSelected ? 1.12f : 0.9f));
		}
	}
	// 请求与实际降雨分开：缺水后的蓄水等待不应看起来像玩家主动收雨。
	if (ForestRainState)
	{
		int32 StateIndex = 0;
		if (bRainActive) { StateIndex = 1; }
		else if (bRainRequested) { StateIndex = 2; }
		ForestRainState->SetActiveWidgetIndex(StateIndex);
	}
	if (WeatherSwitchHint) { WeatherSwitchHint->SetVisibility(bShowPrecipitationHint
		? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed); }
	if (WeatherArrival) { WeatherArrival->SetPercent(WeatherArrivalProgress); }
	const int32 ArrivedIndex = static_cast<int32>(ArrivedPrecipitationMode);
	if (WeatherArrivedIcon && ArrivedIndex >= 0 && ArrivedIndex < UE_ARRAY_COUNT(WeatherIcons) && WeatherIcons[ArrivedIndex])
	{
		WeatherArrivedIcon->SetBrush(WeatherIcons[ArrivedIndex]->GetBrush());
		WeatherArrivedIcon->SetColorAndOpacity(CloudColor);
	}
	RefreshContextLine();
}

void USacredLakeWidget::RefreshSkillDesigner()
{
	UForestSkillWidget* Slots[] = {WaterPillarSlot, SnowSlot, HailSlot, MeteorSlot, SidestepSlot};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Slots); ++Index)
	{
		if (Slots[Index] && SkillDetails.IsValidIndex(Index))
		{
			Slots[Index]->Refresh(SkillDetails[Index], CloudWaterAmount);
		}
	}
}

void USacredLakeWidget::RefreshContextLine()
{
	if (!ForestContext || !ForestContextFrame) { return; }
	// 操作必须得到即时回应。短反馈结束后恢复尚未解决的紧急目标，始终只显示一行。
	FText Message;
	bool bWarning = false;
	if (!ForestFeedback.IsEmpty())
	{
		Message = ForestFeedback;
		bWarning = bFeedbackWarning;
	}
	else if (bObjectiveVisible)
	{
		Message = ForestObjective;
		bWarning = bObjectiveUrgent;
	}
	ForestContext->SetText(Message);
	ForestContext->SetColorAndOpacity(FSlateColor(bWarning
		? SiphonColor : OneCloud::ForestHud::CloudColor));
	ForestContextFrame->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void USacredLakeWidget::NativeDestruct()
{
	if (UWorld* World = GetWorld())
	{
		World->GetTimerManager().ClearTimer(ObjectiveTimer);
		World->GetTimerManager().ClearTimer(FeedbackTimer);
	}
	Super::NativeDestruct();
}

void USacredLakeWidget::InvalidateForestPaint() const
{
	if (const TSharedPtr<SWidget> SlateWidget = GetCachedWidget())
	{
		SlateWidget->Invalidate(EInvalidateWidgetReason::Paint);
	}
}
