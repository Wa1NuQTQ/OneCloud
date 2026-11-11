#pragma once

#include "Blueprint/UserWidget.h"
#include "Misc/Optional.h"
#include "TimerManager.h"
#include "SacredLakeWidget.generated.h"

class UProgressBar;
class UWidgetSwitcher;
class UImage;
class UTextBlock;
class UForestSkillWidget;
enum class ESacredLakeResult : uint8;
enum class EOneCloudPrecipitation : uint8;

enum class ESacredLakePrompt : uint8
{
	None,
	Plant,
	Refill,
	Rain,
	Focus
};

// 规则层给出当前提示和世界坐标。TOptional 为空时省略该标记，零坐标仍可用作合法位置。
struct FSacredLakeGuidance
{
	ESacredLakePrompt Prompt = ESacredLakePrompt::None;
	TOptional<FVector> Plot;
	bool bPlanting = false;
	bool bPlantingValid = false;
	TOptional<FVector> RefillAnchor;
	TOptional<FVector> FocusCenter;
	// 聚雨范围按 UE 世界单位厘米保存，投影时才转换成 UI 坐标。
	float FocusRadiusCm = 0.f;
	TOptional<FVector> ArrivedFocusCenter;
	float ArrivedFocusRadiusCm = 0.f;
	TOptional<FVector> QueuedFocusCenter;
	float QueuedFocusRadiusCm = 0.f;
	struct FThreat
	{
		FVector Position;
		int32 Count = 0;
		bool bSiphoning = false;
		bool bMajor = false;
		bool operator==(const FThreat&) const = default;
	};
	// 每个岸向只保留最紧急的一个锚点；数量用于提示同侧不止一只敌人。
	TArray<FThreat, TInlineAllocator<4>> Threats;
	bool bArmoredThreat = false;
	bool bReticle = false;

	bool HasWorldAnchor() const
	{
		return Plot.IsSet() || RefillAnchor.IsSet() || FocusCenter.IsSet() || ArrivedFocusCenter.IsSet() || QueuedFocusCenter.IsSet() || !Threats.IsEmpty();
	}
};

// 规则层把每个槽位的状态推给 HUD。这里仅保存显示数据，不再次计算费用或冷却。
// 顺序固定为水柱、雪幕、冰雹、星坠、侧掠，与输入和施法数组一致。
struct FForestSkillHudState
{
	float CloudCost = 0.f;
	float CooldownRemainingSeconds = 0.f;
	float CooldownDurationSeconds = 0.f;
	bool bUnlocked = false;
	bool bReady = false;
	// 无目标与缺水是两种不同的失败原因，HUD 只使用规则已验证的结果。
	bool bHasValidTarget = true;
};

// 持有一份用于绘制的玩法状态，由规则主动推送。胜负、伤害和费用仍由规则决定。
UCLASS()
class ONECLOUD_API USacredLakeWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	// 两种地图共用湖水、结果和降雨状态。仅森林版需要云水、技能与场景标记。
	void Refresh(float Water, float Capacity, ESacredLakeResult Result, bool bExtracting, bool bRaining);
	void RefreshForest(float CloudWater, float CloudCapacity, int32 Seeds, float Pressure01, bool bBossPresent);
	void RefreshForestSkillDetails(TConstArrayView<FForestSkillHudState> Skills);
	void SetForestRainRequested(bool bRequested);
	void SetForestPrecipitation(EOneCloudPrecipitation Mode, bool bShowSwitchHint);
	void SetForestWeatherArrival(EOneCloudPrecipitation ArrivedMode, float ArrivalProgress01);
	void SetForestGrowth(float Growth01);
	void SetForestGuidance(const FSacredLakeGuidance& State);
	// 普通目标变化后短暂显示；紧急目标持续显示，操作反馈可暂时覆盖它。
	void SetForestObjective(const FText& Objective, bool bUrgent = false);
	void ShowForestFeedback(const FText& Message, bool bWarning = true);

protected:
	virtual void NativeDestruct() override;
	virtual int32 NativePaint(const FPaintArgs& Args, const FGeometry& AllottedGeometry,
		const FSlateRect& MyCullingRect, FSlateWindowElementList& OutDrawElements, int32 LayerId,
		const FWidgetStyle& InWidgetStyle, bool bParentEnabled) const override;

private:
	int32 PaintForestGuidance(const FGeometry& Geometry, FSlateWindowElementList& Draw, int32 Layer) const;
	void HideForestObjective();
	void ClearForestFeedback();
	void InvalidateForestPaint() const;
	void RefreshForestDesigner();
	void RefreshSkillDesigner();
	void RefreshContextLine();

	// WBP_SacredLakeHUD 的 Designer 负责固定布局；这些字段只接收玩法状态。
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> ForestPanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> PrototypePanel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> ForestLakeBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> ForestCloudBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> ForestGrowthBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> ForestPressureBar;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ForestLakeAmount;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ForestCloudAmount;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ForestSeedCount;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UTextBlock> ForestContext;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> ForestContextFrame;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> ForestLakeIcon;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> ForestPressureIcon;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> WeatherRain;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> WeatherSnow;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> WeatherHail;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> WeatherArrivedIcon;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UProgressBar> WeatherArrival;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidget> WeatherSwitchHint;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UWidgetSwitcher> ForestRainState;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UForestSkillWidget> WaterPillarSlot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UForestSkillWidget> SnowSlot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UForestSkillWidget> HailSlot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UForestSkillWidget> MeteorSlot;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UForestSkillWidget> SidestepSlot;

	// 旧原型地图仍可绑定下面这些控件；森林版使用上方的 Designer 面板。
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UProgressBar> WaterBar;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> SiphonMark;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> RainControls;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> RainState;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> ResultOverlay;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidgetSwitcher> ResultState;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> VictoryMark;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> DryMark;

	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UWidget> UnavailableMark;

	UPROPERTY(EditDefaultsOnly, Category = "Appearance")
	FLinearColor WaterColor = FLinearColor(0.25f, 0.76f, 0.73f);

	UPROPERTY(EditDefaultsOnly, Category = "Appearance")
	FLinearColor SiphonColor = FLinearColor(0.93f, 0.44f, 0.36f);

	// 原始水量用于费用判断与数字，归一化值用于资源条；HUD 不修改这些玩法资源。
	float LakeFraction = 0.f;
	float CloudFraction = 0.f;
	float LakeWaterAmount = 0.f;
	float LakeWaterCapacity = 0.f;
	float CloudWaterAmount = 0.f;
	float CloudWaterCapacity = 0.f;
	float GrowthFraction = 0.f;
	float WeatherArrivalProgress = 1.f;
	float BossPressure = 0.f;
	int32 SeedCount = 0;
	TArray<FForestSkillHudState> SkillDetails;
	FText ForestObjective;
	FText ForestFeedback;
	FTimerHandle ObjectiveTimer;
	FTimerHandle FeedbackTimer;
	FSacredLakeGuidance Guidance;
	ESacredLakeResult CurrentResult{};
	EOneCloudPrecipitation PrecipitationMode{};
	EOneCloudPrecipitation ArrivedPrecipitationMode{};
	bool bForestStatus = false;
	bool bSiphonActive = false;
	bool bRainActive = false;
	bool bRainRequested = false;
	bool bBossActive = false;
	bool bShowPrecipitationHint = false;
	bool bFeedbackWarning = false;
	bool bObjectiveUrgent = false;
	bool bObjectiveVisible = false;
};
