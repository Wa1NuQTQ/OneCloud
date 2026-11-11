#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "OneCloudGameMode.generated.h"

class UOneCloudComicWidget;

UCLASS()
class ONECLOUD_API AOneCloudGameMode : public AGameModeBase
{
	GENERATED_BODY()

public:
	// 由具体地图的 GameMode 蓝图选择。运行时使用该类创建漫画界面实例。
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Opening")
	TSubclassOf<UOneCloudComicWidget> OpeningComicClass;

	virtual void StartPlay() override;

private:
	void BeginLocalSession();
};
