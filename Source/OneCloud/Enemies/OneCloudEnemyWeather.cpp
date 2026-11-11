#include "OneCloudEnemy.h"

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"

// 破甲期间雨伤倍率由 0.45 提高到 1.5。护甲表面仍用于落点选声和特效。
float AOneCloudEnemy::GetWeatherRainDamageMultiplier() const
{
	constexpr float ArmoredMultiplier = 0.45f;
	const UWorld* World = GetWorld();
	return bArmoredRainSurface && World && World->GetTimeSeconds() >= WeatherArmorBrokenUntil
		? ArmoredMultiplier
		: (bArmoredRainSurface ? 1.5f : 1.f);
}

void AOneCloudEnemy::BreakWeatherArmor(float DurationSeconds)
{
	// 记录结束时刻，新的冰雹命中可以延长破甲，较短的效果不会提前结束已有破甲。
	if (const UWorld* World = GetWorld(); IsAlive() && bArmoredRainSurface && World)
	{
		WeatherArmorBrokenUntil = FMath::Max(WeatherArmorBrokenUntil, World->GetTimeSeconds() + DurationSeconds);
	}
}

void AOneCloudEnemy::SetSnowSlowed(bool bSlowed)
{
	bSnowScreenSlowed = bSlowed;
	UpdateSnowSlow();
}

bool AOneCloudEnemy::UpdatePrecipitationChill(float ChillChange)
{
	// 普通降雪使用 0 到 1 的寒冷量。正值积累，负值用于离开覆盖后的恢复。
	const float NewChill = FMath::Clamp(PrecipitationChill + ChillChange, 0.f, 1.f);
	if (NewChill != PrecipitationChill)
	{
		PrecipitationChill = NewChill;
		UpdateSnowSlow();
	}
	return PrecipitationChill > 0.f;
}

void AOneCloudEnemy::UpdateSnowSlow()
{
	// 雪幕立即按满寒冷结算，普通降雪逐步积累。以原始速度为基准，避免多次减速相乘。
	const float Chill = bSnowScreenSlowed ? 1.f : PrecipitationChill;
	UCharacterMovementComponent* Movement = GetCharacterMovement();
	if (Chill > 0.f)
	{
		if (!WalkSpeedBeforeSnow.IsSet())
		{
			WalkSpeedBeforeSnow = Movement->MaxWalkSpeed;
		}
		Movement->MaxWalkSpeed = WalkSpeedBeforeSnow.GetValue() * FMath::Lerp(1.f, WeatherMinimumSpeedMultiplier, Chill);
	}
	else if (WalkSpeedBeforeSnow.IsSet())
	{
		Movement->MaxWalkSpeed = WalkSpeedBeforeSnow.GetValue();
		WalkSpeedBeforeSnow.Reset();
	}
}

void AOneCloudEnemy::SetWeatherMinimumSpeedMultiplier(float Multiplier)
{
	WeatherMinimumSpeedMultiplier = FMath::Clamp(Multiplier, 0.45f, 0.65f);
	UpdateSnowSlow();
}

void AOneCloudEnemy::ConfigureForestVariant(float Health, float SpeedCmPerSecond, bool bArmored)
{
	check(FMath::IsFinite(Health) && Health > 0.f);
	check(FMath::IsFinite(SpeedCmPerSecond) && SpeedCmPerSecond > 0.f);
	MaxHealth = CurrentHealth = Health;
	bArmoredRainSurface = bArmored;
	WalkSpeedBeforeSnow.Reset();
	GetCharacterMovement()->MaxWalkSpeed = SpeedCmPerSecond;
	UpdateSnowSlow();
	UpdateHealthBar();
}
