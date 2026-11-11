#include "OneCloudRain.h"

#include "Components/PointLightComponent.h"
#include "Engine/PointLight.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

namespace
{
	constexpr float LightningMinIntervalSeconds = 9.f;
	constexpr float LightningMaxIntervalSeconds = 17.f;
	constexpr float LightningDurationSeconds = 0.16f;
	constexpr double SpeedOfSoundCmPerSecond = 34300.0;
}

// 一次性计时器每次重新抽取间隔。只有仍在发射降水时才安排下一次。
void AOneCloudRain::ScheduleLightning()
{
	if (bIsRaining && (IsValid(LightningLight) || ThunderSound))
	{
		GetWorldTimerManager().SetTimer(LightningTimer, this, &AOneCloudRain::FlashLightning,
			FMath::FRandRange(LightningMinIntervalSeconds, LightningMaxIntervalSeconds));
	}
}

void AOneCloudRain::FlashLightning()
{
	if (!bIsRaining)
	{
		return;
	}
	FVector Position = GetActorLocation();
	FTimerManager& Timers = GetWorldTimerManager();
	if (IsValid(LightningLight) && LightningLight->PointLightComponent)
	{
		Position = LightningLight->GetActorLocation();
		LightningLight->PointLightComponent->SetVisibility(true);
		Timers.SetTimer(LightningEndTimer, this, &AOneCloudRain::HideLightning, LightningDurationSeconds);
	}
	if (ThunderSound)
	{
		const APlayerController* Player = GetWorld()->GetFirstPlayerController();
		if (Player && Player->IsLocalController())
		{
			FVector Listener;
			FVector Forward;
			FVector Right;
			Player->GetAudioListenerPosition(Listener, Forward, Right);
			// UE 距离单位为厘米。用距离除以声速得到光到声的延迟，并保留最短间隔。
			const float DelaySeconds = static_cast<float>(FMath::Max(0.10,
				FVector::Distance(Position, Listener) / SpeedOfSoundCmPerSecond));
			Timers.SetTimer(ThunderTimer,
				FTimerDelegate::CreateUObject(this, &AOneCloudRain::PlayThunder, Position), DelaySeconds, false);
		}
	}
	ScheduleLightning();
}

void AOneCloudRain::HideLightning()
{
	if (IsValid(LightningLight) && LightningLight->PointLightComponent)
	{
		LightningLight->PointLightComponent->SetVisibility(false);
	}
}

// 使用闪光时保存的世界位置播雷声，衰减以该位置到当前监听器的距离计算。
void AOneCloudRain::PlayThunder(FVector Position)
{
	if (ThunderSound)
	{
		UGameplayStatics::PlaySoundAtLocation(this, ThunderSound, Position, 0.55f, 1.f, 0.f, ThunderAttenuation);
	}
}
