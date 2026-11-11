#include "OneCloudRain.h"

#include "RainImpactSurface.h"
#include "Sound/SoundCue.h"
#include "Sound/SoundNodeRandom.h"
#include "Sound/SoundNodeWavePlayer.h"
#include "Sound/SoundWave.h"

USoundBase* AOneCloudRain::SelectWeatherImpactSound(ERainImpactSurface Surface, bool bSnow, bool bHail)
{
	const int32 Index = static_cast<int32>(Surface);
	const TArray<TObjectPtr<USoundBase>>* Sounds = bSnow ? &SnowImpactSounds : (bHail ? &HailImpactSounds : nullptr);
	if (Sounds && Sounds->IsValidIndex(Index) && (*Sounds)[Index]) return (*Sounds)[Index];
	return SelectImpactSound(Surface);
}

// 命中表面决定音色。泥土与肉身缺少专用音色时沿用沙地，未知表面不发声。
USoundBase* AOneCloudRain::SelectImpactSound(ERainImpactSurface Surface)
{
	switch (Surface)
	{
	case ERainImpactSurface::Sand:
		return SelectSandSound();
	case ERainImpactSurface::Flesh:
		return FleshImpactSound ? FleshImpactSound.Get() : SelectSandSound();
	case ERainImpactSurface::Soil:
		return SoilImpactSound ? SoilImpactSound.Get() : SelectSandSound();
	case ERainImpactSurface::Water:
		return WaterImpactSound.Get();
	case ERainImpactSurface::Leaf:
		return LeafImpactSound.Get();
	case ERainImpactSurface::Metal:
		return MetalImpactSound.Get();
	default:
		return nullptr;
	}
}

// 沙地常见，安全缓存命中时直接从波形列表抽取，避免反复遍历 Sound Cue。
USoundBase* AOneCloudRain::SelectSandSound()
{
	if (SandImpactSound != CachedSandPreset || CachedSandSounds.Num() < 2)
	{
		return SandImpactSound;
	}
	if (AvailableSandIndices.IsEmpty())
	{
		for (int32 Index = 0; Index < CachedSandSounds.Num(); ++Index)
		{
			// 新一轮仍排除上一轮最后一项，避免两轮交界连续重复。
			if (Index != LastSandIndex)
			{
				AvailableSandIndices.Add(Index);
			}
		}
	}
	const int32 Slot = FMath::RandHelper(AvailableSandIndices.Num());
	LastSandIndex = AvailableSandIndices[Slot];
	AvailableSandIndices.RemoveAtSwap(Slot, 1, EAllowShrinking::No);
	return CachedSandSounds[LastSandIndex];
}

void AOneCloudRain::CacheSandSounds()
{
	CachedSandSounds.Reset();
	AvailableSandIndices.Reset();
	LastSandIndex = INDEX_NONE;
	CachedSandPreset = SandImpactSound;
	USoundCue* Cue = Cast<USoundCue>(SandImpactSound);
	USoundNodeRandom* Random = Cue ? Cast<USoundNodeRandom>(Cue->FirstNode) : nullptr;
	// 只展开等权、无放回、没有额外处理的随机 Cue。
	// 含音量、并发或循环设置的资产继续交给原 Cue 播放。
	// 声音类别可以存在，但下方要求 Wave 与 Cue 完全一致，音量混音仍由引擎类别处理。
	if (!Random || !Random->bRandomizeWithoutReplacement || !ImpactAttenuation ||
		Cue->VolumeMultiplier != 1.f || Cue->PitchMultiplier != 1.f ||
		Cue->bOverrideConcurrency || !Cue->ConcurrencySet.IsEmpty() ||
		Random->PreselectAtLevelLoad != 0 || Random->Weights.Num() != Random->ChildNodes.Num())
	{
		return;
	}
	for (int32 Index = 0; Index < Random->ChildNodes.Num(); ++Index)
	{
		USoundNodeWavePlayer* Player = Cast<USoundNodeWavePlayer>(Random->ChildNodes[Index]);
		USoundWave* Wave = Player ? Player->GetSoundWave() : nullptr;
		if (!Wave || Player->bLooping || Wave->IsLooping() || Random->Weights[Index] != 1.f ||
			Wave->SoundClassObject != Cue->SoundClassObject || Wave->Priority != Cue->Priority ||
			Wave->bOverrideConcurrency || !Wave->ConcurrencySet.IsEmpty())
		{
			CachedSandSounds.Reset();
			return;
		}
		CachedSandSounds.Add(Wave);
	}
}
