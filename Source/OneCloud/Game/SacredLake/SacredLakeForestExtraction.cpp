#include "SacredLakePrototype.h"

#include "AIController.h"
#include "Components/CapsuleComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Enemies/OneCloudEnemy.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Navigation/PathFollowingComponent.h"
#include "NavigationPath.h"
#include "NavigationSystem.h"

namespace
{
	constexpr double ShoreRetrySeconds = 0.5;
	constexpr double ExtractionWindupSeconds = 1.0;
	constexpr double InterruptProtectionSeconds = 2.0;
	constexpr double ShoreSeparationCm = 240.0;
	constexpr double ShoreRecheckCm = 5.0;
	constexpr int32 OrdinaryKillsPerSeed = 6;
	constexpr double BossChargeSeconds = 3.0;
	constexpr double BossRecoverySeconds = 3.0;
	constexpr int32 MaximumSpawnAttempts = 16;
	constexpr double SpawnRingHalfWidthCm = 300.0;

	// 从当前脚下接上导航组件剩余路点。涨水只会影响还没有走过的部分。
	TArray<FVector> GetRemainingShorePath(const AOneCloudEnemy& Enemy)
	{
		const AAIController* AI = Cast<AAIController>(Enemy.GetController());
		const UPathFollowingComponent* Following = AI ? AI->GetPathFollowingComponent() : nullptr;
		const FNavPathSharedPtr Path = Following ? Following->GetPath() : FNavPathSharedPtr();
		if (!Path.IsValid() || !Path->IsValid())
		{
			return {};
		}

		const TArray<FNavPathPoint>& Points = Path->GetPathPoints();
		const int32 NextPoint = static_cast<int32>(Following->GetNextPathIndex());
		if (!Points.IsValidIndex(NextPoint))
		{
			return {};
		}

		TArray<FVector> Remaining;
		Remaining.Reserve(Points.Num() - NextPoint + 1);
		Remaining.Add(Enemy.GetNavAgentLocation());
		for (int32 Index = NextPoint; Index < Points.Num(); ++Index)
		{
			Remaining.Add(*Path->GetPathPointLocation(Index));
		}
		return Remaining;
	}
}

// 生成、寻岸与抵达回调

// 挑选敌人蓝图，在整圈林带内随机出雾。先验干地、导航和当前岸线，再生成并登记。
bool ASacredLakePrototype::SpawnForestEnemy(EForestEnemyRank Rank)
{
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Navigation)
	{
		return false;
	}
	const double Elapsed = GetWorld()->GetTimeSeconds() - ForestStartedSeconds;
	EForestEnemyBehavior Behavior = EForestEnemyBehavior::Flesh;
	if (Rank == EForestEnemyRank::Boss)
	{
		Behavior = EForestEnemyBehavior::Armored;
	}
	else if (Rank == EForestEnemyRank::Elite)
	{
		Behavior = ForestEliteSpawnCount % 2 == 0 ? EForestEnemyBehavior::Flesh : EForestEnemyBehavior::Armored;
	}
	else if (Elapsed >= 180.0 && ForestSpawnCount % 4 == 3)
	{
		Behavior = EForestEnemyBehavior::Armored;
	}
	else if (Elapsed >= 60.0 && ForestSpawnCount % 3 == 1)
	{
		Behavior = EForestEnemyBehavior::Runner;
	}
	TSubclassOf<AOneCloudEnemy> SpawnClass = Behavior == EForestEnemyBehavior::Armored && ArmoredEnemyClass
		? ArmoredEnemyClass : EnemyClass;
	if (Rank == EForestEnemyRank::Boss && BossEnemyClass) { SpawnClass = BossEnemyClass; }

	AOneCloudEnemy* Defaults = SpawnClass.GetDefaultObject();
	if (!Defaults) { return false; }
	const UCapsuleComponent* Capsule = Defaults->GetCapsuleComponent();
	// CDO 未注册，世界缩放缓存仍可能是 1；出生会应用根组件的相对缩放。
	// 按实际出生半高放置胶囊，避免放大的首领嵌入地面后被碰撞检查拒绝。
	const float HalfHeight = Capsule->GetUnscaledCapsuleHalfHeight() *
		static_cast<float>(FMath::Abs(Capsule->GetRelativeScale3D().Z));
	AOneCloudEnemy* Enemy = nullptr;
	FVector ShoreGoal = FVector::ZeroVector;
	for (int32 Attempt = 0; Attempt < MaximumSpawnAttempts; ++Attempt)
	{
		const double Angle = ForestSpawnRandom.FRandRange(0.f, static_cast<float>(UE_TWO_PI));
		const FVector Radial(FMath::Cos(Angle), FMath::Sin(Angle), 0.0);
		const double Jitter = ForestSpawnRandom.FRandRange(-static_cast<float>(SpawnRingHalfWidthCm),
			static_cast<float>(SpawnRingHalfWidthCm));
		const FVector Candidate = WaterOrigin + FVector(Radial.X * ForestSpawnHalfExtentCm.X,
			Radial.Y * ForestSpawnHalfExtentCm.Y, 0.0) + Radial * Jitter;
		FVector Ground;
		FNavLocation Projected;
		if (!TraceGround(Candidate, Ground) || IsWet(Ground) ||
			!Navigation->ProjectPointToNavigation(Ground, Projected, FVector(180.0, 180.0, 250.0),
				&Defaults->GetNavAgentPropertiesRef()) ||
			!TraceGround(Projected.Location, Ground) || IsWet(Ground) ||
			!FindShoreGoal(*Defaults, Projected.Location, ShoreGoal))
		{
			continue;
		}
		// 不让碰撞修正把已验证的出生点推到水里或障碍后；受阻时换一个连续随机方向。
		FActorSpawnParameters Params;
		Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::DontSpawnIfColliding;
		Enemy = GetWorld()->SpawnActor<AOneCloudEnemy>(SpawnClass,
			Ground + FVector::UpVector * (HalfHeight + 4.f),
			FRotator(0.f, (WaterOrigin - Ground).Rotation().Yaw, 0.f), Params);
		if (IsValid(Enemy)) { break; }
	}
	if (!IsValid(Enemy))
	{
		return false;
	}

	FForestEnemy& State = ForestEnemies.AddDefaulted_GetRef();
	State.Enemy = Enemy;
	State.Rank = Rank;
	State.Behavior = Behavior;
	const bool bArmored = Behavior == EForestEnemyBehavior::Armored;
	float Health = 18.f;
	float Speed = 75.f;
	FName BehaviorTag(TEXT("ForestFlesh"));
	if (bArmored)
	{
		// 完整护甲可撑过部分岸路的背景雨；聚雹或破甲接雨缩短这个窗口。
		Health = 105.f;
		Speed = 55.f;
		BehaviorTag = FName(TEXT("ForestArmored"));
	}
	else if (Behavior == EForestEnemyBehavior::Runner)
	{
		// 疾行怪靠较短的暴露时间抵岸，雪的减速能让背景雨多打一段时间。
		Health = 85.f;
		Speed = 180.f;
		BehaviorTag = FName(TEXT("ForestRunner"));
	}
	if (Rank == EForestEnemyRank::Elite)
	{
		Health = bArmored ? 140.f : 250.f;
		Speed = 65.f;
		++ForestEliteSpawnCount;
	}
	else if (Rank == EForestEnemyRank::Boss)
	{
		Health = 1800.f;
		Speed = 55.f;
	}
	else { ++ForestSpawnCount; }
	Enemy->ConfigureForestVariant(Health, Speed, bArmored);
	Enemy->Tags.AddUnique(BehaviorTag);
	if (Rank == EForestEnemyRank::Boss)
	{
		ForestBoss = Enemy;
		Enemy->Tags.AddUnique(FName(TEXT("ForestBoss")));
	}
	else if (Rank == EForestEnemyRank::Elite)
	{
		Enemy->Tags.AddUnique(FName(TEXT("ForestElite")));
	}
	if (SiphonMaterial)
	{
		USplineMeshComponent* Stream = NewObject<USplineMeshComponent>(this, NAME_None, RF_Transient, Siphon.Get());
		AddInstanceComponent(Stream);
		Stream->SetupAttachment(GetRootComponent());
		Stream->SetVisibility(false);
		Stream->RegisterComponent();
		State.Stream = Stream;
	}
	Enemy->OnShoreReached.AddUObject(this, &ASacredLakePrototype::ForestShoreReached, Enemy);
	Enemy->OnShoreMoveFailed.AddUObject(this, &ASacredLakePrototype::ForestShoreFailed, Enemy);
	Enemy->OnDefeated.AddUObject(this, &ASacredLakePrototype::ForestEnemyDefeated, Enemy);
	RefreshForestEnemyState();
	if (Result == ESacredLakeResult::Defending && Enemy->IsAlive())
	{
		ReleaseForestShore(State);
		Enemy->CancelShoreMove();
		// 出生候选已验证岸位。先记录预留和移动状态，再提交可能同步抵达的请求。
		State.bHasReservedShore = true;
		State.bMoving = true;
		State.ReservedShore = ShoreGoal;
		State.LastShoreCheckHeightCm = WaterHeightCm;
		if (!Enemy->MoveToShore(ShoreGoal))
		{
			ForestShoreFailed(Enemy);
		}
	}
	return true;
}

// 释放旧岸位并取消旧移动，找到有效的新岸位后先预留，再交给 AIController 移动。
void ASacredLakePrototype::SeekForestShore(AOneCloudEnemy& Enemy)
{
	FForestEnemy* State = FindForestEnemy(&Enemy);
	if (Result != ESacredLakeResult::Defending || !State || !Enemy.IsAlive())
	{
		return;
	}
	ReleaseForestShore(*State);
	Enemy.CancelShoreMove();
	FVector Goal;
	if (FindShoreGoal(Enemy, Enemy.GetNavAgentLocation(), Goal))
	{
		State->bHasReservedShore = true;
		State->bMoving = true;
		State->ReservedShore = Goal;
		State->LastShoreCheckHeightCm = WaterHeightCm;
		// UE 判定已在目标处时可能同步发出抵达回调，所以必须先记录岸位状态。
		if (Enemy.MoveToShore(Goal))
		{
			return;
		}
	}
	ForestShoreFailed(&Enemy);
}

// 抵岸后以实际位置检查水线，进入抽水状态并记录第一次扣水时间。
void ASacredLakePrototype::ForestShoreReached(AOneCloudEnemy* Enemy)
{
	FForestEnemy* State = FindForestEnemy(Enemy);
	if (!State || !IsValid(Enemy) || !Enemy->IsAlive())
	{
		return;
	}
	if (State->bRetreating)
	{
		ForestShoreFailed(Enemy);
		return;
	}
	const FVector Position = Enemy->GetActorLocation();
	if (!FindNearbyWater(Position, State->WaterPoint))
	{
		ForestShoreFailed(Enemy);
		return;
	}
	State->bMoving = false;
	State->bSiphoning = true;
	State->LastShoreCheckLocation = Position;
	State->LastShoreCheckHeightCm = WaterHeightCm;
	State->bBossCharging = State->Rank == EForestEnemyRank::Boss;
	double Windup = ExtractionWindupSeconds;
	if (State->bBossCharging) { Windup = BossChargeSeconds; }
	else if (State->Behavior == EForestEnemyBehavior::Runner) { Windup = 0.5; }
	State->NextActionAtSeconds = GetWorld()->GetTimeSeconds() + Windup;
	if (USplineMeshComponent* Stream = State->Stream.Get())
	{
		PositionSiphon(*Stream, *Enemy, State->WaterPoint);
	}
	Enemy->PlayAttack();
	RefreshForestEnemyState();
}

// 等待短暂间隔再选岸，保留技能打断设置的更晚恢复时间。
void ASacredLakePrototype::ForestShoreFailed(AOneCloudEnemy* Enemy)
{
	if (FForestEnemy* State = FindForestEnemy(Enemy))
	{
		ReleaseForestShore(*State);
		State->NextActionAtSeconds = FMath::Max(State->NextActionAtSeconds,
			GetWorld()->GetTimeSeconds() + ShoreRetrySeconds);
		RefreshForestEnemyState();
	}
}

// 更新寻路并结算抽水

// 清理失效敌人，推进重试，并在水位变化足够大时检查目标和未走完的路段。
void ASacredLakePrototype::UpdateForestEnemies(double Now)
{
	for (int32 Index = ForestEnemies.Num() - 1; Index >= 0; --Index)
	{
		FForestEnemy& State = ForestEnemies[Index];
		AOneCloudEnemy* Enemy = State.Enemy.Get();
		if (!IsValid(Enemy) || !Enemy->IsAlive())
		{
			if (IsValid(Enemy))
			{
				Enemy->OnShoreReached.RemoveAll(this);
				Enemy->OnShoreMoveFailed.RemoveAll(this);
				Enemy->OnDefeated.RemoveAll(this);
			}
			if (USplineMeshComponent* Stream = State.Stream.Get())
			{
				Stream->DestroyComponent();
			}
			ForestEnemies.RemoveAt(Index);
		}
		else if (State.Rank == EForestEnemyRank::Boss && NextBossRepositionHealthFraction > 0.f &&
			Enemy->GetHealthFraction() <= NextBossRepositionHealthFraction && RepositionForestBoss(State))
		{
			NextBossRepositionHealthFraction -= 1.f / 3.f;
		}
		else if (!State.bMoving && !State.bSiphoning && Now >= State.NextActionAtSeconds)
		{
			SeekForestShore(*Enemy);
		}
		else if (State.bMoving &&
			FMath::Abs(WaterHeightCm - State.LastShoreCheckHeightCm) >= ShoreRecheckCm)
		{
			const bool bWaterRising = WaterHeightCm > State.LastShoreCheckHeightCm;
			State.LastShoreCheckHeightCm = WaterHeightCm;
			FVector WaterPoint;
			if (State.bHasReservedShore && !FindNearbyWater(State.ReservedShore, WaterPoint))
			{
				SeekForestShore(*Enemy);
			}
			// 目标仍干燥时，涨水也可能切断前往目标的剩余路段。
			else if (bWaterRising && !IsDryPath(GetRemainingShorePath(*Enemy)))
			{
				if (State.bRetreating)
				{
					Enemy->CancelShoreMove();
					ForestShoreFailed(Enemy);
				}
				else
				{
					SeekForestShore(*Enemy);
				}
			}
		}
	}
	UpdateForestExtraction(Now);
	RefreshForestEnemyState();
}

// 抽水按敌人等级定间隔和扣量。扣费前后都检查可达水点，干涸立刻结束防守。
void ASacredLakePrototype::UpdateForestExtraction(double Now)
{
	for (FForestEnemy& State : ForestEnemies)
	{
		AOneCloudEnemy* Enemy = State.Enemy.Get();
		if (!State.bSiphoning || !IsValid(Enemy))
		{
			continue;
		}
		const FVector Position = Enemy->GetActorLocation();
		const bool bDue = Now >= State.NextActionAtSeconds;
		const bool bMoved = !Position.Equals(State.LastShoreCheckLocation, ShoreRecheckCm);
		if (!bDue && !bMoved && FMath::Abs(WaterHeightCm - State.LastShoreCheckHeightCm) < ShoreRecheckCm)
		{
			continue;
		}
		FVector WaterPoint;
		if (!FindNearbyWater(Position, WaterPoint))
		{
			ForestShoreFailed(Enemy);
			continue;
		}
		if (bDue)
		{
			if (State.Rank == EForestEnemyRank::Boss && !State.bBossCharging)
			{
				State.bBossCharging = true;
				State.NextActionAtSeconds = Now + BossChargeSeconds;
				Enemy->PlayAttack();
				if (USplineMeshComponent* Stream = State.Stream.Get())
				{
					PositionSiphon(*Stream, *Enemy, WaterPoint);
				}
				continue;
			}
			float Amount = 4.f;
			double IntervalSeconds = 3.0;
			if (State.Behavior == EForestEnemyBehavior::Runner) { Amount = 3.f; IntervalSeconds = 2.5; }
			else if (State.Behavior == EForestEnemyBehavior::Armored) { Amount = 6.f; IntervalSeconds = 4.0; }
			if (State.Rank == EForestEnemyRank::Elite) { Amount = 6.f; IntervalSeconds = 4.0; }
			else if (State.Rank == EForestEnemyRank::Boss)
			{
				Amount = 18.f;
				IntervalSeconds = BossRecoverySeconds;
				State.bBossCharging = false;
				if (USplineMeshComponent* Stream = State.Stream.Get()) { Stream->SetVisibility(false); }
			}
			Resources.Extract(Amount);
			// 所有敌人共用抽水提示的间隔，多个扣水事件靠得很近时只播放一次。
			if (ForestSiphonCue && Now >= NextForestSiphonCueSeconds)
			{
				constexpr double MinimumCueSpacingSeconds = 2.5;
				NextForestSiphonCueSeconds = Now + MinimumCueSpacingSeconds;
				UGameplayStatics::PlaySoundAtLocation(this, ForestSiphonCue, WaterPoint,
					1.f, 1.f, 0.f, ForestSiphonCueAttenuation);
			}
			UpdateWater();
			if (Resources.LakeWater <= 0.f)
			{
				Finish(ESacredLakeResult::Dry);
				return;
			}
			State.NextActionAtSeconds = Now + IntervalSeconds;
			if (State.Rank != EForestEnemyRank::Boss) { Enemy->PlayAttack(); }
			if (!FindNearbyWater(Position, WaterPoint))
			{
				ForestShoreFailed(Enemy);
				continue;
			}
		}
		State.LastShoreCheckLocation = Position;
		State.LastShoreCheckHeightCm = WaterHeightCm;
		if (USplineMeshComponent* Stream = State.Stream.Get(); Stream && (State.Rank != EForestEnemyRank::Boss || State.bBossCharging) &&
			(bMoved || !WaterPoint.Equals(State.WaterPoint, 1.f)))
		{
			PositionSiphon(*Stream, *Enemy, WaterPoint);
		}
		State.WaterPoint = WaterPoint;
	}
}

// 技能打断与击败

// 必须处于可打断的抽水状态。水柱推退对重甲和首领无效。
bool ASacredLakePrototype::CanInterruptForestExtractor(const AOneCloudEnemy* Enemy, bool bPushBack) const
{
	if (Result != ESacredLakeResult::Defending || !IsValid(Enemy) || !Enemy->IsAlive()
		|| (bPushBack && Enemy->IsArmoredRainSurface()))
	{
		return false;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	return ForestEnemies.ContainsByPredicate([Enemy, Now, bPushBack](const FForestEnemy& State)
	{
		return State.Enemy.Get() == Enemy && State.bSiphoning && Now >= State.ProtectedUntilSeconds &&
			(State.Rank != EForestEnemyRank::Boss || State.bBossCharging) &&
			(!bPushBack || State.Rank != EForestEnemyRank::Boss);
	});
}

// 打断先释放岸位并启动保护时间。需要推退时，通过导航找到向外的干地再移动。
void ASacredLakePrototype::InterruptForestExtractor(AOneCloudEnemy* Enemy, bool bPushBack)
{
	FForestEnemy* State = FindForestEnemy(Enemy);
	if (!State || !CanInterruptForestExtractor(Enemy, bPushBack))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	ReleaseForestShore(*State);
	if (!bPushBack && Enemy->IsArmoredRainSurface() && State->Rank != EForestEnemyRank::Boss)
	{
		ShowForestFeedback(NSLOCTEXT("ForestFeedback", "ArmoredInterrupted", "已打断取水 · 重甲不会被推退"), false);
	}
	State->ProtectedUntilSeconds = Now + InterruptProtectionSeconds;
	State->NextActionAtSeconds = Now + 1.4;
	Enemy->CancelShoreMove();
	if (bPushBack)
	{
		const FVector Start = Enemy->GetNavAgentLocation();
		const FVector Back = Start + (Start - WaterOrigin).GetSafeNormal2D() * 330.f;
		FVector Ground;
		FNavLocation Retreat;
		UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
		if (Navigation && TraceGround(Back, Ground) &&
			Navigation->ProjectPointToNavigation(Ground, Retreat, FVector(80.0, 80.0, 180.0),
				&Enemy->GetNavAgentPropertiesRef()))
		{
			const UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(
				GetWorld(), Start, Retreat.Location, Enemy);
			if (Path && Path->IsValid() && !Path->IsPartial() && IsDryPath(Path->PathPoints))
			{
				State->bMoving = true;
				State->bRetreating = true;
				State->LastShoreCheckHeightCm = WaterHeightCm;
				if (!Enemy->MoveToShore(Retreat.Location))
				{
					ReleaseForestShore(*State);
				}
			}
		}
	}
	RefreshForestEnemyState();
	UpdateHud();
}

// 首领决定胜负，其他敌人按击败数量奖励种子。失效敌人由周期更新回收。
void ASacredLakePrototype::ForestEnemyDefeated(AOneCloudEnemy* Enemy)
{
	if (FForestEnemy* State = FindForestEnemy(Enemy))
	{
		ReleaseForestShore(*State);
		if (State->Rank == EForestEnemyRank::Boss)
		{
			Finish(Resources.LakeWater > 0.f ? ESacredLakeResult::Won : ESacredLakeResult::Dry);
			return;
		}
		if (State->Rank == EForestEnemyRank::Elite)
		{
			AwardForestSeed();
			ForestReliefEndsSeconds = GetWorld()->GetTimeSeconds() + 20.0;
			AwardForestBlessing();
		}
		else if (++OrdinaryKillsSinceSeed >= OrdinaryKillsPerSeed)
		{
			OrdinaryKillsSinceSeed = 0;
			AwardForestSeed();
		}
		RefreshForestEnemyState();
		UpdateHud();
	}
}

// 首领生命经过 2/3、1/3 时，沿可走干岸换位。失败时保留阈值，下次更新重试，不瞬移穿地形。
bool ASacredLakePrototype::RepositionForestBoss(FForestEnemy& State)
{
	AOneCloudEnemy* Enemy = State.Enemy.Get();
	UNavigationSystemV1* Navigation = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld());
	if (!Enemy || !Navigation) { return false; }
	const FVector Start = Enemy->GetNavAgentLocation();
	const FVector Radial = (Start - WaterOrigin).GetSafeNormal2D();
	const FVector Tangent(-Radial.Y, Radial.X, 0.0);
	for (const double Side : { 1.0, -1.0 })
	{
		const FVector Candidate = Start + Tangent * (1100.0 * Side) + Radial * 350.0;
		FVector Ground;
		FNavLocation Destination;
		if (!TraceGround(Candidate, Ground) || !Navigation->ProjectPointToNavigation(Ground, Destination,
			FVector(180.0, 180.0, 250.0), &Enemy->GetNavAgentPropertiesRef())) { continue; }
		const UNavigationPath* Path = Navigation->FindPathToLocationSynchronously(GetWorld(), Start, Destination.Location, Enemy);
		if (!Path || !Path->IsValid() || Path->IsPartial() || !IsDryPath(Path->PathPoints)) { continue; }
		ReleaseForestShore(State);
		Enemy->CancelShoreMove();
		State.bMoving = true;
		State.bRetreating = true;
		State.LastShoreCheckHeightCm = WaterHeightCm;
		State.NextActionAtSeconds = GetWorld()->GetTimeSeconds() + ShoreRetrySeconds;
		if (Enemy->MoveToShore(Destination.Location)) { return true; }
		ReleaseForestShore(State);
	}
	return false;
}

// 状态汇总与岸位预留

// 把多怪状态汇总成 HUD 当前关注的敌人和是否有人抽水，优先展示正在抽水的目标。
void ASacredLakePrototype::RefreshForestEnemyState()
{
	Extractor = nullptr;
	bExtracting = false;
	for (const FForestEnemy& State : ForestEnemies)
	{
		AOneCloudEnemy* Enemy = State.Enemy.Get();
		if (!IsValid(Enemy) || !Enemy->IsAlive())
		{
			continue;
		}
		if (!Extractor || (!bExtracting && State.bSiphoning))
		{
			Extractor = Enemy;
		}
		bExtracting |= State.bSiphoning;
	}
}

ASacredLakePrototype::FForestEnemy* ASacredLakePrototype::FindForestEnemy(const AOneCloudEnemy* Enemy)
{
	return ForestEnemies.FindByPredicate([Enemy](const FForestEnemy& State)
	{
		return State.Enemy.Get() == Enemy;
	});
}

// 按水平距离检查预留岸位，不等敌人实际抵达，防止多个敌人同时选中同一位置。
bool ASacredLakePrototype::IsShoreOccupied(const AOneCloudEnemy& Enemy, const FVector& Position) const
{
	return bUseForestRules && ForestEnemies.ContainsByPredicate([&Enemy, &Position](const FForestEnemy& State)
	{
		return State.bHasReservedShore && State.Enemy.IsValid() && State.Enemy.Get() != &Enemy
			&& FVector::DistSquared2D(State.ReservedShore, Position) < FMath::Square(ShoreSeparationCm);
	});
}

// 释放岸位、结束取水与退让，并隐藏该敌人的水流。
void ASacredLakePrototype::ReleaseForestShore(FForestEnemy& State)
{
	State.bSiphoning = false;
	State.bBossCharging = false;
	State.bMoving = false;
	State.bRetreating = false;
	State.bHasReservedShore = false;
	if (USplineMeshComponent* Stream = State.Stream.Get())
	{
		Stream->SetVisibility(false);
	}
}
