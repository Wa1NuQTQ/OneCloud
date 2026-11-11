#include "SacredLakePrototype.h"

#include "EnhancedInputComponent.h"
#include "Enemies/OneCloudEnemy.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Player/OneCloudFairy.h"
#include "UI/Hud/SacredLakeWidget.h"
#include "Weather/OneCloudRain.h"

namespace
{
	constexpr double AimRangeCm = 14000.0;
	constexpr double SkillRangeCm = 3500.0;
	constexpr double FocusToggleDistanceCm = 250.0;
	constexpr double TargetToleranceCm = 200.0;
	constexpr float PrecipitationHintPressure = 0.2f;

	// 消耗单位为云水点，冷却用世界时间秒。技能槽顺序与精灵蒙太奇和 HUD 一致。
	struct FForestSkillCost
	{
		float Water;
		float CooldownSeconds;
	};

	constexpr FForestSkillCost SkillCosts[] = {
		{6.f, 6.f},    // 水柱
		{10.f, 10.f},  // 雪幕
		{12.f, 12.f},  // 冰雹
		{30.f, 0.f}    // 星坠在首领存活时解锁，一局最多施放一次。
	};
	constexpr int32 MeteorSlot = UE_ARRAY_COUNT(SkillCosts) - 1;
}

// 按键绑定与输入入口

// Started 每次按下只调用一次。补水还需要在松开或动作取消时收尾。
void ASacredLakePrototype::BindForestInput(UEnhancedInputComponent& Input)
{
	struct FBinding
	{
		UInputAction* Action;
		void (ASacredLakePrototype::*Handler)();
	};
	const FBinding Bindings[] = {
		{PlantAction, &ASacredLakePrototype::PlantFromInput},
		{RefillAction, &ASacredLakePrototype::StartRefillFromInput},
		{FocusAction, &ASacredLakePrototype::ToggleForestFocus},
		{WaterColumnAction, &ASacredLakePrototype::WaterColumnFromInput},
		{SnowAction, &ASacredLakePrototype::SnowFromInput},
		{HailAction, &ASacredLakePrototype::HailFromInput},
		{MeteorAction, &ASacredLakePrototype::MeteorFromInput},
		{SidestepAction, &ASacredLakePrototype::SidestepFromInput}
	};
	for (const FBinding& Binding : Bindings)
	{
		if (Binding.Action)
		{
			Input.BindAction(Binding.Action, ETriggerEvent::Started, this, Binding.Handler);
		}
	}
	if (RefillAction)
	{
		Input.BindAction(RefillAction, ETriggerEvent::Completed, this, &ASacredLakePrototype::StopRefillFromInput);
		Input.BindAction(RefillAction, ETriggerEvent::Canceled, this, &ASacredLakePrototype::StopRefillFromInput);
	}
}

// 森林输入与目标提示共用守湖中、未暂停的条件，开场漫画期间不会结算玩家动作。
bool ASacredLakePrototype::CanActInForest() const
{
	return bUseForestRules && Result == ESacredLakeResult::Defending
		&& !UGameplayStatics::IsGamePaused(this);
}

void ASacredLakePrototype::PlantFromInput()
{
	if (!CanActInForest()) { return; }
	if (bForestPlantingPreview)
	{
		CancelForestPlanting();
		return;
	}
	bForestPlantingPreview = true;
	ShowForestFeedback(NSLOCTEXT("ForestFeedback", "ChoosePlantGround", "瞄准岛上地面 · 左键种植，瞄准已种树可培育；右键或 E 取消"), false);
	UpdateForestGuidance();
}

void ASacredLakePrototype::CancelForestPlanting()
{
	if (!bForestPlantingPreview) { return; }
	bForestPlantingPreview = false;
	UpdateForestGuidance();
}

void ASacredLakePrototype::StartRefillFromInput()
{
	SetRefillingCloud(true);
}

void ASacredLakePrototype::StopRefillFromInput()
{
	SetRefillingCloud(false);
}

void ASacredLakePrototype::SidestepFromInput()
{
	if (CanActInForest())
	{
		if (AOneCloudFairy* Fairy = Cast<AOneCloudFairy>(UGameplayStatics::GetPlayerPawn(this, 0)))
		{
			if (!Fairy->TrySidestep())
			{
				ShowForestFeedback(NSLOCTEXT("ForestFeedback", "SidestepCooling", "侧掠尚在冷却"));
			}
		}
	}
}

void ASacredLakePrototype::WaterColumnFromInput()
{
	RequestForestSkill(0);
}

void ASacredLakePrototype::SnowFromInput()
{
	RequestForestSkill(1);
}

void ASacredLakePrototype::HailFromInput()
{
	RequestForestSkill(2);
}

void ASacredLakePrototype::MeteorFromInput()
{
	RequestForestSkill(MeteorSlot);
}

// 相机瞄准与聚雨

// 种植只读取真正的地形交点，树冠、水面和装饰物不应把落点抬到半空。
// 障碍、岛界和最高水位由 CanPlantAtLocation 统一校验。
bool ASacredLakePrototype::ResolvePlantingAim(FVector& Target) const
{
	const APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0);
	if (!CanActInForest() || !Player || !Player->GetPawn()) { return false; }
	FVector ViewPosition;
	FRotator ViewRotation;
	Player->GetPlayerViewPoint(ViewPosition, ViewRotation);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestPlantingAim), true, Player->GetPawn());
	const FVector End = ViewPosition + ViewRotation.Vector() * AimRangeCm;
	const bool bHit = IsValid(GroundActor)
		? GroundActor->ActorLineTraceSingle(Hit, ViewPosition, End, ECC_Visibility, Params)
		: GetWorld()->LineTraceSingleByChannel(Hit, ViewPosition, End, ECC_Visibility, Params);
	if (!bHit) { return false; }
	Target = Hit.ImpactPoint;
	return !Target.ContainsNaN();
}

// 从当前相机发出可见性射线，用命中的水平位置重新取地面或水面，再确认目标在天气范围内。
bool ASacredLakePrototype::ResolveForestAim(FVector& Target) const
{
	const APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0);
	if (!CanActInForest() || !IsValid(Rain) || !Player || !Player->GetPawn())
	{
		return false;
	}
	FVector ViewPosition;
	FRotator ViewRotation;
	Player->GetPlayerViewPoint(ViewPosition, ViewRotation);
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestAim), true, Player->GetPawn());
	if (!GetWorld()->LineTraceSingleByChannel(Hit, ViewPosition, ViewPosition + ViewRotation.Vector() * AimRangeCm,
		ECC_Visibility, Params) || !TraceGround(Hit.ImpactPoint, Target))
	{
		return false;
	}
	if (IsWet(Target))
	{
		Target.Z = WaterHeightCm;
	}
	return Rain->CanTargetWeather(Target);
}

// 再次瞄准已有聚雨点可关闭聚雨。兼顾玩家点的位置与云边缘实际允许的聚雨圆心。
void ASacredLakePrototype::ToggleForestFocus()
{
	if (bForestPlantingPreview)
	{
		FVector PlantGround;
		if (!ResolvePlantingAim(PlantGround))
		{
			ShowForestFeedback(NSLOCTEXT("ForestFeedback", "AimPlantGround", "把画面中央对准湖心岛地面"));
		}
		else if (TryPlantAtLocation(PlantGround))
		{
			CancelForestPlanting();
		}
		return;
	}
	FVector Target;
	if (!ResolveForestAim(Target))
	{
		if (CanActInForest())
		{
			ShowForestFeedback(NSLOCTEXT("ForestFeedback", "AimCloudGround", "用画面中央瞄准云下地面"));
		}
		return;
	}
	const double ToggleDistanceSquared = FMath::Square(FocusToggleDistanceCm);
	if (Rain->HasRainFocus() && (FVector::DistSquared2D(LastFocusRequest, Target) <= ToggleDistanceSquared ||
		FVector::DistSquared2D(Rain->GetRainFocusLocation(), Target) <= ToggleDistanceSquared))
	{
		Rain->ClearRainFocus();
		ShowForestFeedback(NSLOCTEXT("ForestFeedback", "FocusCleared", "已恢复均匀降水"), false);
	}
	else if (Rain->SetRainFocus(Target))
	{
		// 靠近云边缘时，真正显示的聚雨圆心可能向内移动。
		LastFocusRequest = Target;
		ShowForestFeedback(NSLOCTEXT("ForestFeedback", "FocusPlaced", "整片云仍在下雨 · 圈内加强，再次点击该处恢复均匀降雨"), false);
	}
}

// 施法请求、通知提交和校验

// 输入时先检查能否施法，保存技能槽和落点，并为本次请求分配编号。此时尚未扣费。
bool ASacredLakePrototype::RequestForestSkill(int32 SkillSlot)
{
	if (!CanActInForest())
	{
		return false;
	}
	CancelForestPlanting();
	SettleForestResources(GetWorld()->GetTimeSeconds());
	const FText BlockReason = GetForestSkillBlockReason(SkillSlot);
	if (!BlockReason.IsEmpty())
	{
		ShowForestFeedback(BlockReason);
		return false;
	}
	FVector Target;
	AOneCloudFairy* Fairy = Cast<AOneCloudFairy>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (!IsValid(Fairy) || !ResolveForestAim(Target))
	{
		ShowForestFeedback(NSLOCTEXT("ForestFeedback", "SkillTargetBlocked", "瞄准云下地面 · 目标太远或被遮挡"));
		return false;
	}
	if (SkillSlot == 0 && !Rain->HasWaterColumnTarget(Target))
	{
		ShowForestFeedback(NSLOCTEXT("ForestFeedback", "ColumnNeedsExtractor", "水柱需要瞄准正在取水或蓄力的敌人"));
		return false;
	}
	if (!CanCommitForestSkill(SkillSlot, Target))
	{
		ShowForestFeedback(NSLOCTEXT("ForestFeedback", "SkillOutOfReach", "靠近目标 · 技能范围为精灵周围 35 米"));
		return false;
	}
	PendingSkillSlot = SkillSlot;
	PendingSkillTarget = Target;
	const uint32 CastId = ++ForestCastId;
	UpdateHud();
	if (!Fairy->BeginSkillCast(SkillSlot,
		FSimpleDelegate::CreateUObject(this, &ASacredLakePrototype::CommitForestSkill, CastId),
		FSimpleDelegate::CreateUObject(this, &ASacredLakePrototype::CancelForestSkill, CastId)))
	{
		// 角色没有可播放的施法蒙太奇时，仍走同一套提交和扣费逻辑。
		CommitForestSkill(CastId);
	}
	return true;
}

// 收到动画释放通知后复核落点与资源。先清除待提交槽，保证重复通知只能扣费一次。
void ASacredLakePrototype::CommitForestSkill(uint32 CastId)
{
	if (CastId != ForestCastId || PendingSkillSlot == INDEX_NONE)
	{
		return;
	}
	const int32 Slot = PendingSkillSlot;
	PendingSkillSlot = INDEX_NONE;
	SettleForestResources(GetWorld()->GetTimeSeconds());
	if (!CanCommitForestSkill(Slot, PendingSkillTarget) || !TrySpendCloudWater(SkillCosts[Slot].Water))
	{
		ShowForestFeedback(NSLOCTEXT("ForestFeedback", "SkillCanceled", "施法未释放 · 目标被遮挡或云水不足，本次未扣水"));
		UpdateHud();
		return;
	}

	SkillReadyAtSeconds[Slot] = GetWorld()->GetTimeSeconds() + SkillCosts[Slot].CooldownSeconds *
		(Slot == 2 ? GetForestHailCooldownMultiplier() : 1.f);
	switch (Slot)
	{
	case 0:
		Rain->CastWaterColumn(PendingSkillTarget);
		break;
	case 1:
		Rain->CastSnowScreen(PendingSkillTarget);
		break;
	case 2:
		Rain->CastHail(PendingSkillTarget);
		break;
	case MeteorSlot:
		bMeteorUsed = true;
		Rain->CastMeteor(PendingSkillTarget);
		break;
	}
	UpdateHud();
}

// 只取消本次施法编号对应的请求，避免旧动画结束事件取消后来的技能。
void ASacredLakePrototype::CancelForestSkill(uint32 CastId)
{
	if (CastId == ForestCastId)
	{
		PendingSkillSlot = INDEX_NONE;
		UpdateHud();
	}
}

// 技能开始和释放共用同一条件。以当前相机重新检查保存落点的距离和视线。
bool ASacredLakePrototype::CanCommitForestSkill(int32 Slot, const FVector& Target) const
{
	if (!CanSpendOnForestSkill(Slot) || !IsValid(Rain) || !Rain->CanTargetWeather(Target))
	{
		return false;
	}

	const APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0);
	if (!Player || !Player->GetPawn())
	{
		return false;
	}
	if (FVector::DistSquared(Player->GetPawn()->GetActorLocation(), Target) > FMath::Square(SkillRangeCm) ||
		(Slot == 0 && !Rain->HasWaterColumnTarget(Target)))
	{
		return false;
	}
	FVector ViewPosition;
	FRotator ViewRotation;
	Player->GetPlayerViewPoint(ViewPosition, ViewRotation);
	if (FVector::DistSquared(ViewPosition, Target) > FMath::Square(AimRangeCm))
	{
		return false;
	}
	FHitResult Hit;
	FCollisionQueryParams Params(SCENE_QUERY_STAT(ForestCastSight), true, Player->GetPawn());
	return !GetWorld()->LineTraceSingleByChannel(Hit, ViewPosition, Target + FVector::UpVector * 25.f,
		ECC_Visibility, Params) ||
		FVector::DistSquared2D(Hit.ImpactPoint, Target) <= FMath::Square(TargetToleranceCm);
}

bool ASacredLakePrototype::IsForestSkillUnlocked(int32 Slot) const
{
	return Slot >= 0 && Slot < UE_ARRAY_COUNT(SkillCosts) && (Slot != MeteorSlot || IsForestBossActive());
}

// 检查解锁、冷却、云水和单局使用限制。槽位解锁检查也保护冷却数组索引。
bool ASacredLakePrototype::CanSpendOnForestSkill(int32 Slot) const
{
	return CanActInForest() && IsForestSkillUnlocked(Slot)
		&& GetWorld()->GetTimeSeconds() >= SkillReadyAtSeconds[Slot] && Resources.CloudWater >= SkillCosts[Slot].Water
		&& (Slot != MeteorSlot || !bMeteorUsed);
}

// 失败原因与实际提交使用相同的解锁、费用和时间字段，界面不另算一套规则。
FText ASacredLakePrototype::GetForestSkillBlockReason(int32 Slot) const
{
	if (Slot < 0 || Slot >= UE_ARRAY_COUNT(SkillCosts))
	{
		return NSLOCTEXT("ForestFeedback", "InvalidSkill", "技能不可用");
	}
	if (!IsForestSkillUnlocked(Slot))
	{
		return NSLOCTEXT("ForestFeedback", "MeteorLocked", "星坠在首领出现时解锁");
	}
	if (Slot == MeteorSlot && bMeteorUsed)
	{
		return NSLOCTEXT("ForestFeedback", "MeteorUsed", "本局星坠已使用");
	}
	const AOneCloudFairy* Fairy = Cast<AOneCloudFairy>(UGameplayStatics::GetPlayerPawn(this, 0));
	if (PendingSkillSlot != INDEX_NONE || (Fairy && Fairy->IsSkillCastAnimating()))
	{
		return NSLOCTEXT("ForestFeedback", "AlreadyCasting", "正在施法 · 等待当前动作完成");
	}
	const float Remaining = static_cast<float>(SkillReadyAtSeconds[Slot] - GetWorld()->GetTimeSeconds());
	if (Remaining > 0.f)
	{
		return FText::Format(NSLOCTEXT("ForestFeedback", "SkillCooling", "技能冷却中 · 还需 {0} 秒"),
			FText::AsNumber(FMath::CeilToInt(Remaining)));
	}
	if (Resources.CloudWater < SkillCosts[Slot].Water)
	{
		return FText::Format(NSLOCTEXT("ForestFeedback", "SkillWaterLow", "云水不足 · 需要 {0} 点，靠近仪式石按住 F 补云"),
			FText::AsNumber(FMath::RoundToInt(SkillCosts[Slot].Water)));
	}
	return FText::GetEmpty();
}

// 技能图标与场景提示

// 每个槽位只推送一份显示状态；解锁、费用、冷却和目标有效性均使用现有施法规则。
void ASacredLakePrototype::UpdateForestSkillHud() const
{
	const AOneCloudFairy* Fairy = Cast<AOneCloudFairy>(UGameplayStatics::GetPlayerPawn(this, 0));
	const bool bCanStartSkill = IsValid(Fairy) && PendingSkillSlot == INDEX_NONE && !Fairy->IsSkillCastAnimating();
	FForestSkillHudState SkillStates[5];
	FVector AimTarget;
	const bool bHasAimTarget = ResolveForestAim(AimTarget);
	for (int32 Slot = 0; Slot < UE_ARRAY_COUNT(SkillCosts); ++Slot)
	{
		FForestSkillHudState& State = SkillStates[Slot];
		State.CloudCost = SkillCosts[Slot].Water;
		State.CooldownDurationSeconds = SkillCosts[Slot].CooldownSeconds * (Slot == 2 ? GetForestHailCooldownMultiplier() : 1.f);
		State.CooldownRemainingSeconds = static_cast<float>(FMath::Max(0.0,
			SkillReadyAtSeconds[Slot] - GetWorld()->GetTimeSeconds()));
		// 星坠用完后显示锁定状态，不能把单局次数耗尽误画成瞄准失败。
		State.bUnlocked = IsForestSkillUnlocked(Slot) && (Slot != MeteorSlot || !bMeteorUsed);
		State.bHasValidTarget = bHasAimTarget && CanCommitForestSkill(Slot, AimTarget);
		State.bReady = State.bUnlocked && bCanStartSkill && CanSpendOnForestSkill(Slot) && State.bHasValidTarget;
	}
	SkillStates[4].bUnlocked = true;
	SkillStates[4].bReady = CanActInForest() && IsValid(Fairy) && Fairy->IsSidestepReady();
	SkillStates[4].CooldownDurationSeconds = 2.f;
	SkillStates[4].CooldownRemainingSeconds = IsValid(Fairy) ? Fairy->GetSidestepCooldownRemainingSeconds() : 0.f;
	Hud->RefreshForestSkillDetails(SkillStates);
	Hud->SetForestRainRequested(bForestRainRequested);
	UpdateForestObjective();
	if (IsValid(Rain))
	{
		const bool bShowSwitchHint = CanActInForest() && GetForestPressure01() < PrecipitationHintPressure;
		Hud->SetForestPrecipitation(Rain->GetPrecipitation(), bShowSwitchHint);
		Hud->SetForestWeatherArrival(Rain->GetArrivedPrecipitation(), Rain->GetWeatherArrivalProgress());
	}
}
