#include "OneCloudRain.h"

#include "Components/InstancedStaticMeshComponent.h"
#include "Enemies/OneCloudEnemy.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace
{
	constexpr float SnowRadiusCm = 1700.f;
	constexpr float SnowDurationSeconds = 6.f;
	constexpr float SnowUpdateIntervalSeconds = 0.25f;
	constexpr float ColumnRadiusCm = 470.f;
	constexpr float HailRadiusCm = 570.f;
	constexpr float HailDamage = 12.f;
	constexpr float MeteorRadiusCm = 1450.f;
	constexpr float MeteorWarningSeconds = 2.f;
	constexpr float MeteorDamage = 160.f;
	constexpr double ImpactHeightToleranceCm = 650.0;
	constexpr FLinearColor WaterColor(0.15f, 0.76f, 0.94f);
	constexpr FLinearColor SnowColor(0.73f, 0.89f, 1.f);
	constexpr FLinearColor HailColor(0.38f, 0.67f, 1.f);
	constexpr FLinearColor MeteorColor(1.f, 0.32f, 0.10f);

	using FWeatherTargets = TArray<AOneCloudEnemy*, TInlineAllocator<8>>;

	// 技能共用地面圆形范围和高度容差。普通降水的遮挡判定在 Coverage 文件。
	FWeatherTargets FindWeatherTargets(UWorld& World, FVector Center, float RadiusCm)
	{
		FWeatherTargets Targets;
		for (TActorIterator<AOneCloudEnemy> It(&World); It; ++It)
		{
			const FVector Position = It->GetActorLocation();
			if (It->IsAlive() && FMath::Abs(Position.Z - Center.Z) <= ImpactHeightToleranceCm &&
				FVector::DistSquared2D(Position, Center) <= FMath::Square(static_cast<double>(RadiusCm)))
			{
				Targets.Add(*It);
			}
		}
		return Targets;
	}
}

bool AOneCloudRain::HasWaterColumnTarget(FVector GroundTarget) const
{
	for (const AOneCloudEnemy* Enemy : FindWeatherTargets(*GetWorld(), GroundTarget, ColumnRadiusCm))
	{
		if (CanInterruptExtractor.IsBound() && CanInterruptExtractor.Execute(Enemy)) return true;
	}
	return false;
}

// 先选实际作用目标，再在它脚下播放水柱。重甲和首领只打断，不推离。
void AOneCloudRain::CastWaterColumn(FVector GroundTarget)
{
	if (!CanTargetWeather(GroundTarget))
	{
		return;
	}
	AOneCloudEnemy* Closest = nullptr;
	double ClosestDistanceSq = TNumericLimits<double>::Max();
	for (AOneCloudEnemy* Enemy : FindWeatherTargets(*GetWorld(), GroundTarget, ColumnRadiusCm))
	{
		if (!CanInterruptExtractor.IsBound() || !CanInterruptExtractor.Execute(Enemy))
		{
			continue;
		}
		const double DistanceSq = FVector::DistSquared2D(Enemy->GetActorLocation(), GroundTarget);
		if (DistanceSq < ClosestDistanceSq)
		{
			Closest = Enemy;
			ClosestDistanceSq = DistanceSq;
		}
	}
	if (Closest)
	{
		PlayWeatherPresentation(WaterColumnPresentation, Closest->GetActorLocation());
		// 事件第二个参数区分只中断和要求撤退，天气 Actor 不直接控制寻路。
		const bool bPushBack = CanPushBackExtractor.IsBound() && CanPushBackExtractor.Execute(Closest);
		OnWeatherInterrupt.Broadcast(Closest, bPushBack);
	}
}

// 雪幕以固定频率重新检查区域，敌人进出都会更新，不依赖粒子碰撞。
void AOneCloudRain::CastSnowScreen(FVector GroundTarget)
{
	if (!CanTargetWeather(GroundTarget))
	{
		return;
	}
	// 新雪幕替换旧雪幕，同时恢复旧区域里仍减速的敌人。
	EndSnowScreen();
	SnowCenter = GroundTarget;
	SnowEffect = PlayWeatherPresentation(SnowPresentation, GroundTarget);
	if (!SnowEffect.IsValid())
	{
		SnowMarker = ShowWeatherMarker(GroundTarget, SnowRadiusCm, SnowColor, 0.f);
	}
	UpdateSnowScreen();
	FTimerManager& Timers = GetWorldTimerManager();
	FTimerManagerTimerParameters Parameters;
	Parameters.bLoop = true;
	Parameters.bMaxOncePerFrame = true;
	Timers.SetTimer(SnowUpdateTimer, this, &AOneCloudRain::UpdateSnowScreen, SnowUpdateIntervalSeconds, Parameters);
	Timers.SetTimer(SnowEndTimer, this, &AOneCloudRain::EndSnowScreen, SnowDurationSeconds);
}

void AOneCloudRain::UpdateSnowScreen()
{
	TSet<TWeakObjectPtr<AOneCloudEnemy>> Inside;
	for (AOneCloudEnemy* Enemy : FindWeatherTargets(*GetWorld(), SnowCenter, SnowRadiusCm))
	{
		Enemy->SetWeatherMinimumSpeedMultiplier(MinimumSnowSpeed);
		Enemy->SetSnowSlowed(true);
		Inside.Add(Enemy);
	}
	for (const TWeakObjectPtr<AOneCloudEnemy>& Enemy : SlowedEnemies)
	{
		if (Enemy.IsValid() && !Inside.Contains(Enemy))
		{
			Enemy->SetSnowSlowed(false);
		}
	}
	// 留下这次命中的集合，供下次比较谁已离开。
	SlowedEnemies = MoveTemp(Inside);
}

// 到时、被替换或结束对局都走同一条清理路径。
void AOneCloudRain::EndSnowScreen()
{
	FTimerManager& Timers = GetWorldTimerManager();
	Timers.ClearTimer(SnowUpdateTimer);
	Timers.ClearTimer(SnowEndTimer);
	for (const TWeakObjectPtr<AOneCloudEnemy>& Enemy : SlowedEnemies)
	{
		if (Enemy.IsValid())
		{
			Enemy->SetSnowSlowed(false);
		}
	}
	SlowedEnemies.Reset();
	if (SnowEffect.IsValid())
	{
		SnowEffect->DestroyComponent();
	}
	SnowEffect.Reset();
	if (SnowMarker)
	{
		SnowMarker->DestroyComponent();
		SnowMarker = nullptr;
	}
}

// 冰雹先标记三处候选落点，再错峰结算覆盖范围内的落点。
void AOneCloudRain::CastHail(FVector GroundTarget)
{
	if (!CanTargetWeather(GroundTarget))
	{
		return;
	}
	FTimerManager& Timers = GetWorldTimerManager();
	for (FTimerHandle& Timer : HailTimers)
	{
		Timers.ClearTimer(Timer);
	}
	HailTimers.Reset();
	HailHits.Reset();
	// 每次施法重置命中集合，重叠的落点对同一敌人最多结算一次。
	const FVector Offsets[] = {FVector::ZeroVector, FVector(300.0, 190.0, 0.0), FVector(-240.0, -220.0, 0.0)};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Offsets); ++Index)
	{
		const FVector Target = GroundTarget + Offsets[Index];
		if (CanTargetWeather(Target))
		{
			const float Delay = 0.35f + 0.17f * Index;
			ShowWeatherMarker(Target, HailRadiusCm, HailColor, Delay);
			Timers.SetTimer(HailTimers.AddDefaulted_GetRef(),
				FTimerDelegate::CreateUObject(this, &AOneCloudRain::ResolveHailImpact, Target), Delay, false);
		}
	}
}

void AOneCloudRain::ResolveHailImpact(FVector GroundTarget)
{
	PlayWeatherPresentation(HailPresentation, GroundTarget);
	for (AOneCloudEnemy* Enemy : FindWeatherTargets(*GetWorld(), GroundTarget, HailRadiusCm))
	{
		bool bAlreadyHit = false;
		HailHits.Add(Enemy, &bAlreadyHit);
		if (!bAlreadyHit)
		{
			Enemy->BreakWeatherArmor(ArmorExposureSeconds);
			bHasCounteredArmor |= Enemy->IsArmoredRainSurface();
			// 冰雹破甲并中断取水，不要求关卡为目标重新找退路。
			OnWeatherInterrupt.Broadcast(Enemy, false);
			if (IsValid(Enemy))
			{
				const float ArmorBonus = Enemy->IsArmoredRainSurface() ? ArmoredHailMultiplier : 1.f;
				const float ChillBonus = Enemy->GetPrecipitationChill() >= 0.8f ? ChilledHailMultiplier : 1.f;
				UGameplayStatics::ApplyDamage(Enemy, HailDamage * ArmorBonus * ChillBonus,
					GetInstigatorController(), this, nullptr);
			}
		}
	}
}

// 星坠预警期间保留地面标记，延迟结束才查询当时仍在圈内的敌人。
void AOneCloudRain::CastMeteor(FVector GroundTarget)
{
	FTimerManager& Timers = GetWorldTimerManager();
	if (!CanTargetWeather(GroundTarget) || Timers.IsTimerActive(MeteorTimer))
	{
		return;
	}
	MeteorMarker = ShowWeatherMarker(GroundTarget, MeteorRadiusCm, MeteorColor, 0.f);
	PlayWeatherPresentation(MeteorWarningPresentation, GroundTarget);
	Timers.SetTimer(MeteorTimer,
		FTimerDelegate::CreateUObject(this, &AOneCloudRain::ResolveMeteorImpact, GroundTarget), MeteorWarningSeconds, false);
}

void AOneCloudRain::ResolveMeteorImpact(FVector GroundTarget)
{
	if (MeteorMarker)
	{
		MeteorMarker->DestroyComponent();
		MeteorMarker = nullptr;
	}
	PlayWeatherPresentation(MeteorImpactPresentation, GroundTarget);
	if (!MeteorImpactPresentation.Effect)
	{
		ShowWeatherMarker(GroundTarget, MeteorRadiusCm * 0.5f, MeteorColor, 0.65f);
	}
	for (AOneCloudEnemy* Enemy : FindWeatherTargets(*GetWorld(), GroundTarget, MeteorRadiusCm))
	{
		if (IsValid(Enemy))
		{
			UGameplayStatics::ApplyDamage(Enemy, MeteorDamage, GetInstigatorController(), this, nullptr);
		}
	}
}

// 同时清掉玩法计时器、状态和表现，避免对局结束后还有迟到的命中。
void AOneCloudRain::CancelWeatherSkills()
{
	EndSnowScreen();
	ClearRainFocus();
	FTimerManager& Timers = GetWorldTimerManager();
	Timers.ClearTimer(MeteorTimer);
	for (FTimerHandle& Timer : HailTimers)
	{
		Timers.ClearTimer(Timer);
	}
	HailTimers.Reset();
	HailHits.Reset();
	for (const TWeakObjectPtr<UNiagaraComponent>& Effect : WeatherEffects)
	{
		if (Effect.IsValid())
		{
			Effect->DestroyComponent();
		}
	}
	WeatherEffects.Reset();
	for (const TWeakObjectPtr<UInstancedStaticMeshComponent>& Marker : WeatherMarkers)
	{
		if (Marker.IsValid())
		{
			Marker->DestroyComponent();
		}
	}
	WeatherMarkers.Reset();
	MeteorMarker = nullptr;
}

// 共用表现。粒子组件可自动销毁，用弱引用登记便于提前结束对局时回收。
UNiagaraComponent* AOneCloudRain::PlayWeatherPresentation(const FWeatherPresentation& Presentation, FVector Position)
{
	UNiagaraComponent* Effect = nullptr;
	if (Presentation.Effect)
	{
		WeatherEffects.RemoveAllSwap([](const TWeakObjectPtr<UNiagaraComponent>& Effect) { return !Effect.IsValid(); });
		// 先设置缩放，再激活，防止第一帧仍按模板的小尺寸闪现。
		Effect = UNiagaraFunctionLibrary::SpawnSystemAttached(Presentation.Effect, GetRootComponent(),
			NAME_None, Position + Presentation.EffectOffset, FRotator::ZeroRotator,
			EAttachLocation::KeepWorldPosition, true, false);
		if (Effect)
		{
			Effect->SetWorldScale3D(Presentation.EffectScale);
			WeatherEffects.Add(Effect);
			Effect->Activate(true);
			if (Presentation.DurationSeconds > 0.f)
			{
				FTimerHandle Lifetime;
				GetWorldTimerManager().SetTimer(Lifetime,
					FTimerDelegate::CreateWeakLambda(this, [WeakEffect = TWeakObjectPtr<UNiagaraComponent>(Effect)]
					{
						if (WeakEffect.IsValid())
						{
							WeakEffect->Deactivate();
						}
					}), Presentation.DurationSeconds, false);
			}
		}
	}
	if (Presentation.Sound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, Presentation.Sound, Position,
			Presentation.SoundVolume, 1.f, 0.f,
			WeatherSkillAttenuation ? WeatherSkillAttenuation : ImpactAttenuation);
	}
	return Effect;
}

// 平面圆环表示精确作用半径。寿命为 0 时由调用者决定何时销毁。
UInstancedStaticMeshComponent* AOneCloudRain::ShowWeatherMarker(FVector Center, float RadiusCm,
	FLinearColor Color, float LifetimeSeconds)
{
	if (!WeatherMarkerRingMesh)
	{
		return nullptr;
	}
	WeatherMarkers.RemoveAllSwap([](const TWeakObjectPtr<UInstancedStaticMeshComponent>& Marker)
	{
		return !Marker.IsValid();
	});
	UInstancedStaticMeshComponent* Marker = NewObject<UInstancedStaticMeshComponent>(this, NAME_None, RF_Transient);
	AddInstanceComponent(Marker);
	Marker->SetStaticMesh(WeatherMarkerRingMesh);
	if (WeatherMarkerMaterial)
	{
		Marker->SetMaterial(0, WeatherMarkerMaterial);
	}
	Marker->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Marker->SetCanEverAffectNavigation(false);
	Marker->SetCastShadow(false);
	Marker->RegisterComponent();
	if (UMaterialInstanceDynamic* Material = Marker->CreateDynamicMaterialInstance(0))
	{
		Material->SetVectorParameterValue(TEXT("Color"), Color);
	}
	// 圆环是单个平面网格，材质裁出边界；不再沿圆创建二十个球体。
	Center.Z += 10.0;
	Marker->AddInstance(FTransform(FQuat::Identity, Center, FVector(RadiusCm / 50.0, RadiusCm / 50.0, 1.0)), true);
	WeatherMarkers.Add(Marker);
	if (LifetimeSeconds > 0.f)
	{
		FTimerHandle Timer;
		GetWorldTimerManager().SetTimer(Timer, FTimerDelegate::CreateWeakLambda(this,
			[WeakMarker = TWeakObjectPtr<UInstancedStaticMeshComponent>(Marker)]
			{
				if (WeakMarker.IsValid())
				{
					WeakMarker->DestroyComponent();
				}
			}), LifetimeSeconds, false);
	}
	return Marker;
}
