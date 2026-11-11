#include "SacredLakePrototype.h"

#include "Components/AudioComponent.h"
#include "Components/SplineMeshComponent.h"
#include "Enemies/OneCloudEnemy.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "Kismet/GameplayStatics.h"
#include "Player/OneCloudFairy.h"
#include "TimerManager.h"
#include "Weather/OneCloudRain.h"

namespace
{
	constexpr float ForestUpdateInterval = 0.2f;
	constexpr float ForestGuidanceInterval = 0.05f;
	constexpr double SpawnRetrySeconds = 1.0;
	constexpr double BossSpawnObstructionTimeoutSeconds = 12.0;
	constexpr double EliteIntervalSeconds = 150.0;
	constexpr double FirstOrdinarySpawnSeconds = 40.0;
	constexpr float ResumeRainSeconds = 5.f;

	struct FForestSpawnStage
	{
		double BeginsAtSeconds;
		double IntervalSeconds;
		int32 BatchSize;
		int32 OrdinaryLimit;
	};

	constexpr FForestSpawnStage SpawnStages[] = {
		{0.0, 10.0, 1, 3},
		{120.0, 7.0, 1, 6},
		{360.0, 5.0, 2, 12},
		{660.0, 4.0, 2, 18}
	};

	const FForestSpawnStage& GetSpawnStage(double Elapsed)
	{
		for (int32 Index = UE_ARRAY_COUNT(SpawnStages) - 1; Index > 0; --Index)
		{
			if (Elapsed >= SpawnStages[Index].BeginsAtSeconds) { return SpawnStages[Index]; }
		}
		return SpawnStages[0];
	}
}

// 开局、周期更新和收尾

// 以世界时间为基准建立森林局。结算、HUD 提示和降水耗尽各有自己的触发时机。
void ASacredLakePrototype::BeginForest()
{
	if (!IsValid(CloudRefillAnchor) || !FMath::IsFinite(CloudRefillRadiusCm) || CloudRefillRadiusCm <= 0.f)
	{
		UE_LOG(LogTemp, Error, TEXT("%s needs a forest cloud refill anchor and a positive refill radius."), *GetName());
		Finish(ESacredLakeResult::Unavailable);
		return;
	}
	if (!FMath::IsFinite(ForestSpawnHalfExtentCm.X) || !FMath::IsFinite(ForestSpawnHalfExtentCm.Y) ||
		ForestSpawnHalfExtentCm.X <= WaterFootprint.X || ForestSpawnHalfExtentCm.Y <= WaterFootprint.Y)
	{
		UE_LOG(LogTemp, Error, TEXT("%s needs a finite forest spawn ring outside the lake footprint."), *GetName());
		Finish(ESacredLakeResult::Unavailable);
		return;
	}

	ForestPlots.SetNum(FSacredLakeResources::MaxPlantingPlots);
	bForestPlantingPreview = false;
	// 无论哪个 Actor 先 BeginPlay，降雨自己的初始设置都与森林默认开雨保持一致。
	Rain->bStartRaining = true;
	Rain->SetRaining(false);
	RefreshForestGrowth();
	ForestStartedSeconds = ForestLastUpdateSeconds = GetWorld()->GetTimeSeconds();
	const float FirstSpawnDelay = static_cast<float>(FirstOrdinarySpawnSeconds);
	NextForestOrdinarySpawnSeconds = ForestStartedSeconds + FirstSpawnDelay;
	NextForestEliteSpawnSeconds = ForestStartedSeconds + FMath::Max(FirstSpawnDelay + 1.f, ForestFirstEliteDelaySeconds);
	ForestBossArrivesSeconds = ForestStartedSeconds + FMath::Max(FirstSpawnDelay + 1.f, ForestBossDelaySeconds);
	NextForestBossSpawnSeconds = ForestBossArrivesSeconds;
	ForestSpawnRandom.Initialize(ForestSpawnSeed != 0 ? ForestSpawnSeed : FMath::Rand());
	Rain->OnRainEmissionChanged.AddUObject(this, &ASacredLakePrototype::ForestRainEmissionChanged);
	Rain->OnWeatherInterrupt.AddUObject(this, &ASacredLakePrototype::InterruptForestExtractor);
	Rain->CanPushBackExtractor.BindUObject(this, &ASacredLakePrototype::CanInterruptForestExtractor, true);
	Rain->CanInterruptExtractor.BindUObject(this, &ASacredLakePrototype::CanInterruptForestExtractor, false);
	// 默认整片云持续下雨；Q 仍可手动收雨，缺水时沿原预算等待恢复后续发。
	bForestRainRequested = true;
	RefreshForestRainBudget();
	// 结算使用真实经过的世界时间；卡顿后无需在同一帧补跑多次相同状态。
	FTimerManagerTimerParameters RulesTimerParameters;
	RulesTimerParameters.bLoop = true;
	RulesTimerParameters.bMaxOncePerFrame = true;
	GetWorldTimerManager().SetTimer(ForestTimer, this, &ASacredLakePrototype::UpdateForest,
		ForestUpdateInterval, RulesTimerParameters);
	if (Hud)
	{
		// 频繁更新跟随视角的提示，掉帧时每帧也只执行一次提示刷新。
		FTimerManagerTimerParameters GuidanceTimerParameters;
		GuidanceTimerParameters.bLoop = true;
		GuidanceTimerParameters.bMaxOncePerFrame = true;
		GetWorldTimerManager().SetTimer(ForestGuidanceTimer, this, &ASacredLakePrototype::UpdateForestGuidance,
			ForestGuidanceInterval, GuidanceTimerParameters);
		UpdateForestGuidance();
	}
	StartForestMusic(0.f);
	UpdateHud();
}

// 统一清理森林的计时器、委托、抽水样条和技能。增加施法编号可让迟到的通知失效。
void ASacredLakePrototype::EndForest()
{
	FadeForestMusic();
	GetWorldTimerManager().ClearTimer(ForestTimer);
	GetWorldTimerManager().ClearTimer(ForestGuidanceTimer);
	GetWorldTimerManager().ClearTimer(ForestRainBudgetTimer);
	ForestPhase = EForestLakePhase::Finished;
	bForestPlantingPreview = false;
	bForestRainEmitting = false;
	bForestRainRequested = false;
	bForestRainWaitingForWater = false;
	PendingBlessingChoices.Reset();
	PendingBlessingRewards = 0;
	OnBlessingChoiceChanged.Broadcast();
	StopCloudRefillHold();
	UpdateForestGuidance();
	PendingSkillSlot = INDEX_NONE;
	++ForestCastId;
	if (AOneCloudFairy* Fairy = Cast<AOneCloudFairy>(UGameplayStatics::GetPlayerPawn(this, 0)))
	{
		Fairy->CancelSkillCast();
	}
	if (IsValid(Rain))
	{
		Rain->OnRainEmissionChanged.RemoveAll(this);
		Rain->OnWeatherInterrupt.RemoveAll(this);
		if (Rain->CanPushBackExtractor.IsBoundToObject(this))
		{
			Rain->CanPushBackExtractor.Unbind();
		}
		if (Rain->CanInterruptExtractor.IsBoundToObject(this))
		{
			Rain->CanInterruptExtractor.Unbind();
		}
		Rain->CancelWeatherSkills();
	}
	for (FForestEnemy& State : ForestEnemies)
	{
		if (AOneCloudEnemy* Enemy = State.Enemy.Get())
		{
			Enemy->OnShoreReached.RemoveAll(this);
			Enemy->OnShoreMoveFailed.RemoveAll(this);
			Enemy->OnDefeated.RemoveAll(this);
			Enemy->CancelShoreMove();
		}
		if (USplineMeshComponent* Stream = State.Stream.Get())
		{
			Stream->DestroyComponent();
		}
	}
	ForestEnemies.Reset();
	ForestBoss.Reset();
	RefreshForestEnemyState();
}

// 每 0.2 秒推进一局。先结清水量并更新成长，再让敌人按新的水位重新判断岸线。
void ASacredLakePrototype::UpdateForest()
{
	if (Result != ESacredLakeResult::Defending || !IsValid(Rain))
	{
		return;
	}
	const double Now = GetWorld()->GetTimeSeconds();
	UpdateCloudRefillProximity();
	SettleForestResources(Now);
	UpdateForestPlots(Now);
	RefreshForestRainBudget();
	UpdateForestEnemies(Now);
	if (Result == ESacredLakeResult::Defending)
	{
		UpdateForestSpawns(Now);
		UpdateHud();
	}
}

// 连续的水量结算与精确停雨

// 结算上次记录到 Now 的时间段。输入、发射状态或补水范围变动时也会调用，以免跨状态算账。
void ASacredLakePrototype::SettleForestResources(double Now)
{
	const float Seconds = static_cast<float>(FMath::Max(0.0, Now - ForestLastUpdateSeconds));
	if (Seconds <= 0.f)
	{
		return;
	}
	ForestLastUpdateSeconds = Now;
	const float PreviousLake = Resources.LakeWater;
	Resources.Advance(Seconds, bForestRainEmitting, bCloudRefillInRange);
	if (!bHasRefilledForestCloud && Resources.LastEmergencyTransfer > 0.f)
	{
		bHasRefilledForestCloud = true;
		ShowForestFeedback(NSLOCTEXT("ForestFeedback", "RefillLearned", "补云成功 · 湖水转入云中，松开 F 停止"), false);
	}
	if (Resources.LakeWater != PreviousLake)
	{
		UpdateWater();
	}
}

// 按降水 Actor 的实际发射状态计费，粒子在空中飞行的尾段由天气层自己处理。
void ASacredLakePrototype::ForestRainEmissionChanged(bool bEmitting)
{
	// 到此为止的时间段仍使用旧发射状态，算完再切到新状态。
	SettleForestResources(GetWorld()->GetTimeSeconds());
	bForestRainEmitting = bEmitting;
	RefreshForestRainBudget();
}

// 保存用户意图，不用暂时缺水造成的实际发射停止反转 Q 的含义。
void ASacredLakePrototype::SetForestRainRequested(bool bRequested)
{
	if (!CanActInForest() || !IsValid(Rain)) { return; }
	SettleForestResources(GetWorld()->GetTimeSeconds());
	bForestRainRequested = bRequested;
	if (!bRequested) { bForestRainWaitingForWater = false; }
	RefreshForestRainBudget();
	UpdateHud();
}

// 另设耗尽计时，空云时停止产生新粒子，已有雨滴仍完整落地发声。
// 恢复必须攒够五秒用量，避免每获得极少水量就一帧开、一帧关。
void ASacredLakePrototype::RefreshForestRainBudget()
{
	FTimerManager& Timers = GetWorldTimerManager();
	if (!IsValid(Rain) || Result != ESacredLakeResult::Defending) { return; }
	if (!bForestRainRequested)
	{
		Timers.ClearTimer(ForestRainBudgetTimer);
		if (Rain->IsRaining()) { Rain->SetRaining(false); }
		return;
	}
	if (Resources.CloudWater <= UE_KINDA_SMALL_NUMBER)
	{
		bForestRainWaitingForWater = true;
		Timers.ClearTimer(ForestRainBudgetTimer);
		if (Rain->IsRaining()) { Rain->SetRaining(false); }
		return;
	}
	if (!bForestRainEmitting)
	{
		if (bForestRainWaitingForWater && Resources.CloudWater < ResumeRainSeconds * Resources.RainCostPerSecond)
		{
			return;
		}
		bForestRainWaitingForWater = false;
		Rain->SetRaining(true);
		if (!Rain->IsRaining()) { return; }
		if (!bHasStartedForestRain)
		{
			bHasStartedForestRain = true;
			ShowForestFeedback(NSLOCTEXT("ForestFeedback", "RainStarted", "降水已开启 · 云水持续消耗，雨滴随后抵达地面"), false);
		}
	}
	// 忽略未来可能补进来的水，采用最早耗尽时刻；触发后按真实收支复核。
	const float SecondsUntilDry = Resources.CloudWater / Resources.RainCostPerSecond;
	const float SecondsRemaining = Timers.GetTimerRemaining(ForestRainBudgetTimer);
	if (SecondsRemaining < 0.f || SecondsUntilDry + UE_KINDA_SMALL_NUMBER < SecondsRemaining)
	{
		Timers.SetTimer(ForestRainBudgetTimer, this, &ASacredLakePrototype::ForestRainBudgetExpired,
			SecondsUntilDry, false);
	}
}

void ASacredLakePrototype::ForestRainBudgetExpired()
{
	GetWorldTimerManager().ClearTimer(ForestRainBudgetTimer);
	SettleForestResources(GetWorld()->GetTimeSeconds());
	RefreshForestRainBudget();
	UpdateHud();
}

// 连续出怪

// 连续压力按四个时间段增加；精英死后的二十秒只放缓普通生成，不清场、不暂停。
void ASacredLakePrototype::UpdateForestSpawns(double Now)
{
	const double Elapsed = Now - ForestStartedSeconds;
	ForestPressure01 = static_cast<float>(FMath::Clamp(Elapsed /
		(ForestBossArrivesSeconds - ForestStartedSeconds), 0.0, 1.0));
	if (Now >= NextForestBossSpawnSeconds && !IsForestBossActive())
	{
		if (SpawnForestEnemy(EForestEnemyRank::Boss))
		{
			BossSpawnBlockedSinceSeconds = -1.0;
		}
		else
		{
			if (BossSpawnBlockedSinceSeconds < 0.0) { BossSpawnBlockedSinceSeconds = Now; }
			else if (Now - BossSpawnBlockedSinceSeconds >= BossSpawnObstructionTimeoutSeconds)
			{
				UE_LOG(LogTemp, Error, TEXT("%s could not find a dry, reachable position on the forest spawn ring for the boss."), *GetName());
				Finish(ESacredLakeResult::Unavailable);
				return;
			}
			NextForestBossSpawnSeconds = Now + SpawnRetrySeconds;
		}
	}

	int32 EliteCount = 0;
	int32 OrdinaryCount = 0;
	for (const FForestEnemy& State : ForestEnemies)
	{
		if (State.Enemy.IsValid() && State.Enemy->IsAlive())
		{
			OrdinaryCount += State.Rank == EForestEnemyRank::Ordinary ? 1 : 0;
			EliteCount += State.Rank == EForestEnemyRank::Elite ? 1 : 0;
		}
	}
	const bool bBossActive = IsForestBossActive();
	const FForestSpawnStage& Stage = GetSpawnStage(Elapsed);
	const int32 StageLimit = bBossActive ? 24 : FMath::Min(Stage.OrdinaryLimit, FMath::Max(1, ForestOrdinaryEnemyLimit));
	// 首领期间只补普通怪，已有精英保留；不能把被阻止生成的精英当作已获奖励。
	if (!bBossActive && Now >= NextForestEliteSpawnSeconds && EliteCount < 2)
	{
		const bool bSpawned = SpawnForestEnemy(EForestEnemyRank::Elite);
		NextForestEliteSpawnSeconds = Now + (bSpawned ? EliteIntervalSeconds : SpawnRetrySeconds);
	}
	if (Now < NextForestOrdinarySpawnSeconds || OrdinaryCount >= StageLimit) { return; }

	double Interval = Stage.IntervalSeconds;
	if (Now < ForestReliefEndsSeconds) { Interval *= 2.0; }
	bool bAnySpawned = false;
	for (int32 Index = 0; Index < Stage.BatchSize && OrdinaryCount < StageLimit; ++Index)
	{
		if (SpawnForestEnemy(EForestEnemyRank::Ordinary)) { bAnySpawned = true; ++OrdinaryCount; }
	}
	NextForestOrdinarySpawnSeconds = Now + (bAnySpawned ? Interval : SpawnRetrySeconds);
	if (bAnySpawned && ForestPhase == EForestLakePhase::Preparing)
	{
		ForestPhase = EForestLakePhase::Assault;
		FadeForestMusic();
	}
}

bool ASacredLakePrototype::IsForestBossActive() const
{
	const AOneCloudEnemy* Boss = ForestBoss.Get();
	return Boss && Boss->IsAlive();
}

// 开局、祝福选择与结局音乐

// 使用 2D 播放通道。指定起播秒数可从同一音频的不同位置进入。
void ASacredLakePrototype::StartForestMusic(float StartTimeSeconds, bool bPlayWhenPaused)
{
	if (!ForestInterludeMusic || GetNetMode() == NM_DedicatedServer)
	{
		return;
	}
	// 替换时停掉旧通道，避免暂停中的世界音乐在恢复后与祝福音乐叠播。
	if (IsValid(ForestMusic)) { ForestMusic->Stop(); }
	ForestMusic = UGameplayStatics::CreateSound2D(this, ForestInterludeMusic);
	if (ForestMusic)
	{
		ForestMusic->bIsUISound = bPlayWhenPaused;
		ForestMusic->FadeIn(0.8f, bPlayWhenPaused ? 0.10f : 0.16f, StartTimeSeconds);
	}
}

void ASacredLakePrototype::FadeForestMusic()
{
	if (IsValid(ForestMusic))
	{
		ForestMusic->FadeOut(0.6f, 0.f);
	}
	ForestMusic = nullptr;
}
