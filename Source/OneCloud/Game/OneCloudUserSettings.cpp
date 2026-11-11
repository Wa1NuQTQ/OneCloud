#include "OneCloudUserSettings.h"

void UOneCloudUserSettings::ValidateSettings()
{
	Super::ValidateSettings();
	MasterVolume = FMath::IsFinite(MasterVolume) ? FMath::Clamp(MasterVolume, 0.f, 1.f) : 0.8f;
	MusicVolume = FMath::IsFinite(MusicVolume) ? FMath::Clamp(MusicVolume, 0.f, 1.f) : 0.65f;
	EffectsVolume = FMath::IsFinite(EffectsVolume) ? FMath::Clamp(EffectsVolume, 0.f, 1.f) : 0.9f;
}
