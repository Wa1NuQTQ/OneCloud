#include "SacredLakePrototype.h"

#include "Enemies/OneCloudEnemy.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UI/Hud/SacredLakeWidget.h"
#include "Weather/OneCloudRain.h"

namespace
{
	constexpr double SpiderMarkerHeightCm = 85.0;
	constexpr float RefillHintCloudFraction = 0.5f;
}

// 把树位、聚雨、敌人和补水目标写入 HUD 提示。操作提示按当前情境择一显示。
void ASacredLakePrototype::UpdateForestGuidance() const
{
	if (!Hud)
	{
		return;
	}

	FSacredLakeGuidance Guidance;
	if (CanActInForest())
	{
		Guidance.bPlanting = bForestPlantingPreview;
		FVector PlotGround;
		if (bForestPlantingPreview && ResolvePlantingAim(PlotGround))
		{
			Guidance.Plot = PlotGround;
			FVector ValidGround;
			Guidance.bPlantingValid = CanPlantAtLocation(PlotGround, ValidGround);
			if (Guidance.bPlantingValid) { Guidance.Plot = ValidGround; }
		}

		Guidance.bReticle = bForestPlantingPreview || (FocusAction && IsValid(Rain));
		FVector AimPoint;
		FVector Focus;
		if (!bForestPlantingPreview && Guidance.bReticle && ResolveForestAim(AimPoint)
			&& Rain->ResolveRainFocus(AimPoint, Focus, Guidance.FocusRadiusCm))
		{
			Guidance.FocusCenter = Focus;
		}
		if (IsValid(Rain) && Rain->HasArrivedRainFocus())
		{
			Guidance.ArrivedFocusCenter = Rain->GetArrivedRainFocusLocation();
			Guidance.ArrivedFocusRadiusCm = Rain->GetArrivedRainFocusRadiusCm();
		}
		if (IsValid(Rain) && Rain->HasRainFocus() && Rain->GetWeatherArrivalProgress() < 1.f)
		{
			Guidance.QueuedFocusCenter = Rain->GetRainFocusLocation();
			Guidance.QueuedFocusRadiusCm = Rain->RainFocusRadiusCm;
		}

		const bool bHasThreat = IsValid(Extractor) && Extractor->IsAlive();
		// 四岸分别选一个最紧急的锚点，避免屏外另一侧的抽水被单一目标遮住。
		FSacredLakeGuidance::FThreat Sides[4]{};
		int32 Priorities[4] = {-1, -1, -1, -1};
		const FVector LakeCenter = IsValid(WaterSurface) ? WaterSurface->GetActorLocation() : GetActorLocation();
		for (const FForestEnemy& State : ForestEnemies)
		{
			const AOneCloudEnemy* Enemy = State.Enemy.Get();
			if (!IsValid(Enemy) || !Enemy->IsAlive()) { continue; }
			const FVector Position = Enemy->GetActorLocation();
			const FVector Offset = Position - LakeCenter;
			const int32 Side = FMath::Abs(Offset.X) >= FMath::Abs(Offset.Y)
				? (Offset.X >= 0.0 ? 0 : 1) : (Offset.Y >= 0.0 ? 2 : 3);
			FSacredLakeGuidance::FThreat& Threat = Sides[Side];
			++Threat.Count;
			const int32 Priority = (State.bSiphoning ? 10 : 0) + static_cast<int32>(State.Rank);
			if (Priority > Priorities[Side])
			{
				Priorities[Side] = Priority;
				Threat.Position = Position + FVector::UpVector * SpiderMarkerHeightCm;
				Threat.bMajor = State.Rank != EForestEnemyRank::Ordinary;
			}
			Threat.bSiphoning |= State.bSiphoning;
		}
		for (const FSacredLakeGuidance::FThreat& Threat : Sides)
		{
			if (Threat.Count > 0) { Guidance.Threats.Add(Threat); }
		}
		const bool bThreatInReach = bHasThreat && IsValid(Rain) && Rain->CanTargetWeather(Extractor->GetActorLocation());
		Guidance.bArmoredThreat = PrecipitationAction && IsValid(Rain) &&
			ForestEnemies.ContainsByPredicate([this](const FForestEnemy& State)
			{
				const AOneCloudEnemy* Enemy = State.Enemy.Get();
				return Enemy && Enemy->IsAlive() && Enemy->IsArmoredRainSurface() &&
					Rain->CanTargetWeather(Enemy->GetActorLocation());
			});
		const bool bCanRefill = RefillAction && Resources.CanRefillCloud() && IsValid(CloudRefillAnchor);
		const bool bNearRefill = bCanRefill && IsNearCloudRefillAnchor();
		if (bCanRefill && (bNearRefill || Resources.CloudWater <= GetCloudCapacity() * RefillHintCloudFraction))
		{
			Guidance.RefillAnchor = CloudRefillAnchor->GetActorLocation();
		}
		if (bForestPlantingPreview)
		{
			Guidance.Prompt = ESacredLakePrompt::Plant;
		}
		else if (bNearRefill && !bCloudRefillHeld)
		{
			Guidance.Prompt = ESacredLakePrompt::Refill;
		}
		else if (bThreatInReach && RainAction && !bHasStartedForestRain && !Rain->IsRaining() && Resources.CloudWater > 0.f)
		{
			Guidance.Prompt = ESacredLakePrompt::Rain;
		}
		else if (bThreatInReach && FocusAction && !Rain->HasEffectiveFocusHit() && Rain->IsRaining())
		{
			Guidance.Prompt = ESacredLakePrompt::Focus;
		}
	}
	Hud->SetForestGuidance(Guidance);
}

// 首次教学只看已经成功的行为，不锁按键、不暂停战斗；紧急状态优先于教学。
void ASacredLakePrototype::UpdateForestObjective() const
{
	if (!Hud)
	{
		return;
	}
	FText Objective;
	bool bUrgent = false;
	if (CanActInForest())
	{
		const bool bHasPlanted = ForestPlots.ContainsByPredicate([](const FForestPlot& Plot) { return Plot.bPlanted; });
		const bool bHasThreat = IsValid(Extractor) && Extractor->IsAlive();
		const bool bHasArmoredThreat = ForestEnemies.ContainsByPredicate([](const FForestEnemy& State)
		{
			const AOneCloudEnemy* Enemy = State.Enemy.Get();
			return Enemy && Enemy->IsAlive() && Enemy->IsArmoredRainSurface();
		});
		// 早期缺水时先保留一次种植费用；一直照着补云提示按 F 会把湖水维持在 10，无法投资回水。
		const bool bReserveForEarlyTree = Resources.Growth < 6 && Resources.Seeds > 0 && Resources.LakeWater <= 20.f &&
			ForestPlots.ContainsByPredicate([](const FForestPlot& Plot)
			{
				return Plot.Level < FSacredLakeResources::MaxTreeLevel && (!Plot.bPlanted || Plot.bMature);
			});
		const APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0);
		int32 ViewWidth = 0;
		int32 ViewHeight = 0;
		if (Player) { Player->GetViewportSize(ViewWidth, ViewHeight); }
		const bool bOffscreenSiphon = Player && ViewWidth > 0 && ViewHeight > 0 &&
			ForestEnemies.ContainsByPredicate([Player, ViewWidth, ViewHeight](const FForestEnemy& State)
			{
				const AOneCloudEnemy* Enemy = State.Enemy.Get();
				if (!State.bSiphoning || !IsValid(Enemy) || !Enemy->IsAlive()) { return false; }
				FVector2D Screen;
				return !Player->ProjectWorldLocationToScreen(Enemy->GetActorLocation(), Screen, true) ||
					Screen.X < 0.0 || Screen.Y < 0.0 || Screen.X > ViewWidth || Screen.Y > ViewHeight;
			});
		if (Resources.LakeWater <= 20.f && bExtracting)
		{
			bUrgent = true;
			Objective = NSLOCTEXT("ForestObjective", "LakeCritical", "圣湖告急 · 1 / 3 打断取水");
		}
		else if (ForestEnemies.ContainsByPredicate([](const FForestEnemy& State) { return State.bBossCharging; }))
		{
			bUrgent = true;
			Objective = NSLOCTEXT("ForestObjective", "BossCharging", "首领正在蓄力 · 1 / 3 打断");
		}
		else if (bOffscreenSiphon)
		{
			bUrgent = true;
			Objective = NSLOCTEXT("ForestObjective", "OffscreenSiphon", "身后正在取水 · 转向紫色标记");
		}
		else if (bForestPlantingPreview)
		{
			Objective = NSLOCTEXT("ForestObjective", "PlantingPreview", "左键落种 / 培育 · 右键取消");
		}
		else if (bReserveForEarlyTree)
		{
			bUrgent = true;
			Objective = Resources.CanPlant()
				? NSLOCTEXT("ForestObjective", "InvestBeforeRefill", "先 E 种树，留足 5 点湖水")
				: NSLOCTEXT("ForestObjective", "SaveForTree", "暂缓补云 · 留水种树");
		}
		else if (bCloudRefillHeld)
		{
			Objective = NSLOCTEXT("ForestObjective", "Refilling", "湖水 → 云水 · 松开 F 停止");
		}
		else if (Resources.CloudWater <= 10.f)
		{
			bUrgent = true;
			Objective = Resources.CanRefillCloud()
				? NSLOCTEXT("ForestObjective", "CloudCritical", "云水不足 · 仪式石旁按住 F")
				: NSLOCTEXT("ForestObjective", "LakeCannotRefill", "湖水不足 · 等待树木回水");
		}
		else if (IsForestBossActive())
		{
			Objective = NSLOCTEXT("ForestObjective", "Boss", "守住圣湖，击退首领 · 4 星坠");
		}
		else if (bHasThreat && !bHasStartedForestRain && !Rain->IsRaining())
		{
			Objective = NSLOCTEXT("ForestObjective", "FirstRain", "窃水者来袭 · Q 恢复降雨");
		}
		else if (!bHasPlanted && Resources.CanPlant())
		{
			Objective = NSLOCTEXT("ForestObjective", "FirstTree", "E 种树 → 左键落种 · 增强回水与雨量");
		}
		else if (Rain->IsRaining() && Rain->GetWeatherArrivalProgress() < 1.f && !Rain->HasEffectiveRainHit() && bHasThreat)
		{
			Objective = NSLOCTEXT("ForestObjective", "RainArriving", "雨尚在途中 · 抵达后生效");
		}
		else if (Resources.CloudWater <= 22.f && !bHasRefilledForestCloud && Resources.CanRefillCloud())
		{
			Objective = NSLOCTEXT("ForestObjective", "FirstRefill", "仪式石旁按住 F · 湖水补入云");
		}
		else if (bHasArmoredThreat && !Rain->HasCounteredArmor())
		{
			Objective = NSLOCTEXT("ForestObjective", "FirstCounter", "重甲来袭 · Tab 选雹 / 3 破甲");
		}
		else if (Rain->IsRaining() && !Rain->HasEffectiveFocusHit())
		{
			Objective = NSLOCTEXT("ForestObjective", "FirstFocus", "左键聚雨 · 加强一侧，整片仍下雨");
		}
		else if (Resources.MatureTrees == 0 && bHasPlanted)
		{
			Objective = NSLOCTEXT("ForestObjective", "Growing", "幼苗正在扎根 · 空格升高观察");
		}
		else if (Resources.Seeds > 0 && ForestPlots.ContainsByPredicate([](const FForestPlot& Plot)
			{ return Plot.Level < FSacredLakeResources::MaxTreeLevel && (!Plot.bPlanted || Plot.bMature); }))
		{
			Objective = NSLOCTEXT("ForestObjective", "MoreTrees", "E 种树 / 培育 · 六棵，各三级");
		}
		else
		{
			// 已完成的教学不常驻；抽水时只留一条紧急行动，其他时间让声音和画面说话。
			Objective = bExtracting ? NSLOCTEXT("ForestObjective", "Draining", "岸边正在取水 · 朝紫色标记聚雨") : FText::GetEmpty();
		}
	}
	Hud->SetForestObjective(Objective, bUrgent);
}

// 文案只由玩法事件推送；HUD 自己处理显示时长，不再重复判断费用或胜负。
void ASacredLakePrototype::ShowForestFeedback(const FText& Message, bool bWarning) const
{
	if (bUseForestRules && Hud)
	{
		Hud->ShowForestFeedback(Message, bWarning);
	}
}
