#pragma once

#include "GameFramework/GameUserSettings.h"
#include "OneCloudUserSettings.generated.h"

// 复用引擎的 GameUserSettings 保存本机偏好，不把音量写进地图或玩法存档。
UCLASS(Config = GameUserSettings)
class ONECLOUD_API UOneCloudUserSettings : public UGameUserSettings
{
	GENERATED_BODY()

public:
	UPROPERTY(Config)
	float MasterVolume = 0.8f;

	UPROPERTY(Config)
	float MusicVolume = 0.65f;

	UPROPERTY(Config)
	float EffectsVolume = 0.9f;

	virtual void ValidateSettings() override;
};
