#include "OneCloudGameMode.h"

#include "GameFramework/PlayerController.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "TimerManager.h"
#include "UI/Comic/OneCloudComicWidget.h"
#include "Player/OneCloudPlayerController.h"

// 地图的 World Settings 选择 GameMode 蓝图，蓝图再配置开场漫画类。
void AOneCloudGameMode::StartPlay()
{
	Super::StartPlay();
	// Spring Arm 在 PostPhysics 更新。等首帧组件定位后再暂停菜单，
	// 避免启动背景停在角色体内；计时器绑定本 Actor，卸载关卡会自动失效。
	GetWorldTimerManager().SetTimerForNextTick(this, &AOneCloudGameMode::BeginLocalSession);
}

void AOneCloudGameMode::BeginLocalSession()
{
	APlayerController* Player = UGameplayStatics::GetPlayerController(this, 0);
	if (AOneCloudPlayerController* LocalPlayer = Cast<AOneCloudPlayerController>(Player))
	{
		LocalPlayer->BeginSession(OpeningComicClass, UGameplayStatics::HasOption(OptionsString, TEXT("SkipIntro")));
		return;
	}

	UOneCloudComicWidget* OpeningComic = CreateWidget<UOneCloudComicWidget>(Player, OpeningComicClass);
	if (OpeningComic)
	{
		// 放在局内 HUD 上方。组件无法开始播放时把它从视口移除。
		OpeningComic->AddToViewport(100);
		if (!OpeningComic->BeginComic())
		{
			OpeningComic->RemoveFromParent();
		}
	}
}
