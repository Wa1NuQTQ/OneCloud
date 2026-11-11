#include "OneCloudRain.h"

#include "Enemies/OneCloudEnemy.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Kismet/GameplayStatics.h"
#include "NiagaraComponent.h"
#include "NiagaraSystem.h"

namespace
{
	constexpr float RainUpdateIntervalSeconds = 0.25f;
	constexpr float MaximumLocalShare = 0.4f;
	constexpr float MaximumLocalDensity = 2.4f;
	constexpr float ChillGainPerSnowSecond = 0.5f;
	constexpr float ChillRecoveryPerSecond = 0.25f;

	bool IsInsideCoverage(FVector Position, FVector Origin, FVector2D HalfExtent)
	{
		return FMath::Abs(Position.X - Origin.X) <= HalfExtent.X &&
			FMath::Abs(Position.Y - Origin.Y) <= HalfExtent.Y && Position.Z < Origin.Z;
	}
}

// 选择局部天气；背景雨不切换。旧粒子继续模拟，不会被重置或重复生成。
void AOneCloudRain::CyclePrecipitation()
{
	SetPrecipitation(static_cast<EOneCloudPrecipitation>((static_cast<uint8>(Precipitation) + 1) % 3));
}

void AOneCloudRain::SetPrecipitation(EOneCloudPrecipitation Mode)
{
	if (Mode == Precipitation || static_cast<uint8>(Mode) > 2) return;
	Precipitation = Mode;
	UpdatePrecipitationEmission();
	if (bIsRaining) QueueRainPhase();
}

// 保留老接口供蓝图读取成长；实际发射和伤害只采用同一份出水预算。
void AOneCloudRain::SetForestRainIntensity(float Intensity)
{
	if (FMath::IsFinite(Intensity)) ForestRainIntensity = FMath::Clamp(Intensity, 1.f, 4.f);
}

void AOneCloudRain::SetForestRainBudget(float WaterRate)
{
	if (!FMath::IsFinite(WaterRate) || WaterRate <= 0.f || ForestWaterRate == WaterRate) return;
	ForestWaterRate = WaterRate;
	UpdateNiagaraCoverage();
	if (bIsRaining) QueueRainPhase();
}

void AOneCloudRain::SetForestWeatherModifiers(float FleshRain, float ChillGain, float MinimumSpeed,
	float ChilledHail, float ArmoredHail, float ArmorBreakSeconds)
{
	FleshRainMultiplier = FleshRain;
	ChillGainMultiplier = ChillGain;
	MinimumSnowSpeed = MinimumSpeed;
	ChilledHailMultiplier = ChilledHail;
	ArmoredHailMultiplier = ArmoredHail;
	ArmorExposureSeconds = ArmorBreakSeconds;
}

// 分配同一份水。圈内密度上限限制实际分流比例，多余水量留给背景雨。
float AOneCloudRain::CalculateLocalShare(float RadiusCm) const
{
	const double Area = 4.0 * RainCoverageHalfExtentCm.X * RainCoverageHalfExtentCm.Y;
	const double LocalArea = UE_PI * FMath::Square(static_cast<double>(RadiusCm));
	if (Area <= LocalArea || LocalArea <= 0.0) return 0.f;
	return static_cast<float>(FMath::Min(static_cast<double>(MaximumLocalShare),
		(MaximumLocalDensity - 1.0) / (Area / LocalArea - 1.0)));
}

float AOneCloudRain::GetLocalWeatherShare() const
{
	const float Radius = static_cast<float>(FMath::Min3(static_cast<double>(RainFocusRadiusCm),
		RainCoverageHalfExtentCm.X, RainCoverageHalfExtentCm.Y));
	return bRainFocused ? CalculateLocalShare(Radius) : 0.f;
}
float AOneCloudRain::GetBackgroundRainShare() const { return 1.f - GetLocalWeatherShare(); }

float AOneCloudRain::GetWeatherArrivalProgress() const
{
	if (PendingRain.IsEmpty() || !GetWorld() || RainFlightSeconds <= 0.f) return 1.f;
	return FMath::Clamp(1.f - static_cast<float>(PendingRain.Last().ArrivesAt - GetWorld()->GetTimeSeconds()) /
		RainFlightSeconds, 0.f, 1.f);
}

bool AOneCloudRain::SetRainFocus(FVector GroundTarget)
{
	float Radius = 0.f;
	if (!ResolveRainFocus(GroundTarget, RainFocusLocation, Radius)) return false;
	bRainFocused = true;
	UpdatePrecipitationEmission();
	if (bIsRaining) QueueRainPhase();
	return true;
}

void AOneCloudRain::ClearRainFocus()
{
	if (!bRainFocused) return;
	bRainFocused = false;
	UpdatePrecipitationEmission();
	if (bIsRaining) QueueRainPhase();
}

bool AOneCloudRain::ResolveRainFocus(FVector GroundTarget, FVector& Focus, float& RadiusCm) const
{
	if (!CanTargetWeather(GroundTarget)) return false;
	const FVector Origin = GetActorLocation();
	const double Radius = FMath::Min3(static_cast<double>(RainFocusRadiusCm),
		RainCoverageHalfExtentCm.X, RainCoverageHalfExtentCm.Y);
	if (!FMath::IsFinite(Radius) || Radius <= 0.0) return false;
	Focus = GroundTarget;
	Focus.X = FMath::Clamp(GroundTarget.X, Origin.X - RainCoverageHalfExtentCm.X + Radius,
		Origin.X + RainCoverageHalfExtentCm.X - Radius);
	Focus.Y = FMath::Clamp(GroundTarget.Y, Origin.Y - RainCoverageHalfExtentCm.Y + Radius,
		Origin.Y + RainCoverageHalfExtentCm.Y - Radius);
	RadiusCm = static_cast<float>(Radius);
	return true;
}

bool AOneCloudRain::CanTargetWeather(FVector GroundTarget) const
{
	return !GroundTarget.ContainsNaN() && IsInsideCoverage(GroundTarget, GetActorLocation(), RainCoverageHalfExtentCm);
}

// 记录发射快照，随雨滴到达地面。战斗读取快照，而非提前使用已切换的UI状态。
void AOneCloudRain::QueueRainPhase()
{
	UWorld* World = GetWorld();
	if (!bUseWeatherRainDamage || !World || !FMath::IsFinite(RainFlightSeconds)) return;
	FRainPhase Phase;
	Phase.ArrivesAt = World->GetTimeSeconds() + FMath::Max(0.f, RainFlightSeconds);
	Phase.Focus = RainFocusLocation;
	Phase.Precipitation = Precipitation;
	Phase.Intensity = ForestWaterRate;
	Phase.FocusRadiusCm = static_cast<float>(FMath::Min3(static_cast<double>(RainFocusRadiusCm),
		RainCoverageHalfExtentCm.X, RainCoverageHalfExtentCm.Y));
	Phase.LocalShare = GetLocalWeatherShare();
	Phase.bEmitting = bIsRaining;
	Phase.bFocused = bRainFocused;
	PendingRain.Add(Phase);
	FTimerManager& Timers = GetWorldTimerManager();
	if (!Timers.IsTimerActive(WeatherRainTimer))
	{
		LastRainUpdate = World->GetTimeSeconds();
		FTimerManagerTimerParameters Parameters;
		Parameters.bLoop = true;
		Parameters.bMaxOncePerFrame = true;
		Timers.SetTimer(WeatherRainTimer, this, &AOneCloudRain::ResolveWeatherRain,
			RainUpdateIntervalSeconds, Parameters);
	}
}

void AOneCloudRain::ResolveWeatherRain()
{
	UWorld* World = GetWorld();
	const double Now = World->GetTimeSeconds();
	const double Start = LastRainUpdate;
	LastRainUpdate = Now;
	int32 ArrivedCount = 0;
	while (ArrivedCount < PendingRain.Num() && PendingRain[ArrivedCount].ArrivesAt <= Now) ++ArrivedCount;
	bool bActive = ArrivedRain.bEmitting;
	for (int32 Index = 0; Index < ArrivedCount; ++Index) bActive |= PendingRain[Index].bEmitting;
	const FVector Origin = GetActorLocation();
	const double Area = 4.0 * RainCoverageHalfExtentCm.X * RainCoverageHalfExtentCm.Y;
	if (bActive || bHasChilledEnemies)
	{
		bHasChilledEnemies = false;
		const FCollisionQueryParams Params(SCENE_QUERY_STAT(WeatherRainCover), false, this);
		for (TActorIterator<AOneCloudEnemy> It(World); It; ++It)
		{
			AOneCloudEnemy* Enemy = *It;
			if (!Enemy->IsAlive()) continue;
			const FVector Target = Enemy->GetActorLocation();
			double RainUnits = 0.0, SnowUnits = 0.0, HailUnits = 0.0, SnowSeconds = 0.0;
			double PhaseStart = Start;
			const bool bInside = IsInsideCoverage(Target, Origin, RainCoverageHalfExtentCm);
			for (int32 Index = 0; bInside && Index <= ArrivedCount; ++Index)
			{
				const FRainPhase& Phase = Index == 0 ? ArrivedRain : PendingRain[Index - 1];
				const double PhaseEnd = Index < ArrivedCount ? FMath::Max(PhaseStart, PendingRain[Index].ArrivesAt) : Now;
				const double Seconds = FMath::Max(0.0, PhaseEnd - PhaseStart);
				if (Phase.bEmitting)
				{
					RainUnits += Seconds * Phase.Intensity * (1.0 - Phase.LocalShare);
					if (Phase.bFocused && Phase.FocusRadiusCm > 0.f &&
						FVector::DistSquared2D(Target, Phase.Focus) <= FMath::Square(static_cast<double>(Phase.FocusRadiusCm)))
					{
						const double Local = Seconds * Phase.Intensity * Phase.LocalShare * Area /
							(UE_PI * FMath::Square(static_cast<double>(Phase.FocusRadiusCm)));
						if (Phase.Precipitation == EOneCloudPrecipitation::Rain) RainUnits += Local;
						else if (Phase.Precipitation == EOneCloudPrecipitation::Hail) HailUnits += Local;
						else { SnowUnits += Local; SnowSeconds += Seconds; }
					}
				}
				PhaseStart = PhaseEnd;
			}
			FHitResult Cover;
			const bool bExposed = RainUnits + SnowUnits + HailUnits > 0.0 && World->LineTraceSingleByChannel(
				Cover, FVector(Target.X, Target.Y, Origin.Z), Target, ECC_Visibility, Params) && Cover.GetActor() == Enemy;
			Enemy->SetWeatherMinimumSpeedMultiplier(MinimumSnowSpeed);
			const double ChillGain = bExposed ? SnowUnits * ChillGainPerSnowSecond * ChillGainMultiplier : 0.0;
			const double Recovery = FMath::Max(0.0, Now - Start - (bExposed ? SnowSeconds : 0.0));
			bHasChilledEnemies |= Enemy->UpdatePrecipitationChill(static_cast<float>(ChillGain - Recovery * ChillRecoveryPerSecond));
			if (bExposed)
			{
				const bool bArmored = Enemy->IsArmoredRainSurface();
				const float RainMultiplier = Enemy->GetWeatherRainDamageMultiplier() * (bArmored ? 1.f : FleshRainMultiplier);
				const float HailMultiplier = (bArmored ? 1.15f * ArmoredHailMultiplier : 0.62f) *
					(Enemy->GetPrecipitationChill() >= 0.8f ? ChilledHailMultiplier : 1.f);
				const float Damage = static_cast<float>(RainUnits * RainMultiplier +
					SnowUnits * (bArmored ? 0.2 : 0.3) + HailUnits * HailMultiplier) * WeatherRainDamagePerSecond;
				bHasEffectiveRainHit |= RainUnits > 0.0;
				bHasEffectiveFocusHit |= SnowUnits + HailUnits > 0.0 ||
					(ArrivedRain.bFocused && FVector::DistSquared2D(Target, ArrivedRain.Focus) <=
						FMath::Square(static_cast<double>(ArrivedRain.FocusRadiusCm)));
				bHasCounteredArmor |= bArmored && HailUnits > 0.0;
				if (Damage > 0.f) UGameplayStatics::ApplyDamage(Enemy, Damage, GetInstigatorController(), this, nullptr);
			}
		}
	}
	if (ArrivedCount > 0)
	{
		ArrivedRain = PendingRain[ArrivedCount - 1];
		PendingRain.RemoveAt(0, ArrivedCount, EAllowShrinking::No);
	}
	if (!ArrivedRain.bEmitting && PendingRain.IsEmpty() && !bHasChilledEnemies)
		GetWorldTimerManager().ClearTimer(WeatherRainTimer);
}

void AOneCloudRain::ClearPrecipitationSnow()
{
	if (UWorld* World = GetWorld(); World && bHasChilledEnemies)
	{
		for (TActorIterator<AOneCloudEnemy> It(World); It; ++It) It->UpdatePrecipitationChill(-1.f);
		bHasChilledEnemies = false;
	}
}

// 稳定状态最多两股发射。基础雨与局部天气沿用同一套归一化分配。
void AOneCloudRain::UpdateNiagaraCoverage()
{
	const float LocalShare = GetLocalWeatherShare();
	const UNiagaraSystem* RainSystem = NiagaraComponent->GetAsset();
	if (!RainSystem) return;
	const FNiagaraVariable SpawnParameter(FNiagaraTypeDefinition::GetFloatDef(), TEXT("User.ForestSpawnRate"));
	const auto& RainParameters = RainSystem->GetExposedParameters();
	const float BaseRate = RainParameters.IndexOf(SpawnParameter) != INDEX_NONE
		? RainParameters.GetParameterValue<float>(SpawnParameter) : 1500.f;
	const auto Configure = [&](UNiagaraComponent* Component, bool bLocal, EOneCloudPrecipitation Mode)
	{
		if (!Component || !Component->GetAsset()) return;
		FVector Focus = RainFocusLocation;
		Focus.Z = GetActorLocation().Z;
		Component->SetVariablePosition(TEXT("User.RainFocusCenter"), Focus);
		Component->SetVariableFloat(TEXT("User.RainFocusRadiusCm"), RainFocusRadiusCm);
		Component->SetVariableFloat(TEXT("User.RainFocusWeight"), bLocal ? 1.f : 0.f);
		Component->SetVariableVec2(TEXT("User.RainCoverageHalfExtentCm"), RainCoverageHalfExtentCm);
		Component->SetVariableInt(TEXT("User.PrecipitationMode"), static_cast<int32>(Mode));
		Component->SetVariableFloat(TEXT("User.RainIntensity"), 1.f);
		const bool bEmitting = bIsRaining && (!bLocal || (bRainFocused && Precipitation == Mode));
		Component->SetVariableFloat(TEXT("User.ForestSpawnRate"), bEmitting ? BaseRate * ForestWaterRate *
			(bLocal ? LocalShare : 1.f - LocalShare) : 0.f);
	};
	Configure(NiagaraComponent, false, EOneCloudPrecipitation::Rain);
	Configure(LocalRainComponent, true, EOneCloudPrecipitation::Rain);
	Configure(SnowPrecipitationComponent, true, EOneCloudPrecipitation::Snow);
	Configure(HailPrecipitationComponent, true, EOneCloudPrecipitation::Hail);
}

void AOneCloudRain::UpdatePrecipitationEmission()
{
	UpdateNiagaraCoverage();
	const auto SetEmission = [this](UNiagaraComponent* Component, bool bEnabled)
	{
		if (!Component) return;
		if (bEnabled && !Component->IsActive()) Component->Activate(false);
		// 森林降水用零出生率收雨，保持模拟存量粒子。
		// Deactivate 后尾雨期间 IsActive 仍可为 true，重新 Activate 又可能重置 GPU 模拟。
		else if (!bUseWeatherRainDamage && !bEnabled && Component->IsActive()) Component->Deactivate();
	};
	SetEmission(NiagaraComponent, bIsRaining);
	SetEmission(LocalRainComponent, bIsRaining && bRainFocused && Precipitation == EOneCloudPrecipitation::Rain);
	SetEmission(SnowPrecipitationComponent, bIsRaining && bRainFocused && Precipitation == EOneCloudPrecipitation::Snow);
	SetEmission(HailPrecipitationComponent, bIsRaining && bRainFocused && Precipitation == EOneCloudPrecipitation::Hail);
}

UNiagaraComponent* AOneCloudRain::CreatePrecipitationComponent(UNiagaraSystem* System)
{
	if (!System) return nullptr;
	UNiagaraComponent* Component = NewObject<UNiagaraComponent>(this, NAME_None, RF_Transient);
	AddInstanceComponent(Component);
	Component->SetAutoActivate(false);
	Component->SetupAttachment(NiagaraComponent);
	Component->SetAsset(System);
	Component->RegisterComponent();
	Component->SetVariableObject(TEXT("User.ImpactHandler"), this);
	return Component;
}
