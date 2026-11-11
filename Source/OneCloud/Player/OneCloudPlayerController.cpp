#include "OneCloudPlayerController.h"

#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Game/OneCloudUserSettings.h"
#include "Game/SacredLake/SacredLakePrototype.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "Misc/CoreDelegates.h"
#include "UI/Comic/OneCloudComicWidget.h"
#include "UI/Hud/SacredLakeWidget.h"
#include "UI/Blessing/ForestBlessingWidget.h"
#include "UI/Menu/OneCloudMenuWidget.h"

void AOneCloudPlayerController::SetupInputComponent()
{
	Super::SetupInputComponent();
	InputComponent->BindKey(EKeys::Escape, IE_Pressed, this,
		&AOneCloudPlayerController::TogglePauseMenu).bExecuteWhenPaused = true;
	InputComponent->BindKey(EKeys::F1, IE_Pressed, this,
		&AOneCloudPlayerController::ShowHelp).bExecuteWhenPaused = true;
}

void AOneCloudPlayerController::BeginSession(TSubclassOf<UOneCloudComicWidget> ComicClass, bool bSkipIntro)
{
	if (!IsLocalController())
	{
		return;
	}
	OpeningClass = ComicClass;
	for (TActorIterator<ASacredLakePrototype> It(GetWorld()); It; ++It)
	{
		if (It->bUseForestRules)
		{
			LakeRules = *It;
			It->OnResultChanged.AddUObject(this, &AOneCloudPlayerController::ShowResult);
			It->OnBlessingChoiceChanged.AddUObject(this, &AOneCloudPlayerController::RefreshBlessingChoice);
			break;
		}
	}
	TArray<UUserWidget*> Widgets;
	UWidgetBlueprintLibrary::GetAllWidgetsOfClass(this, Widgets, USacredLakeWidget::StaticClass(), true);
	if (!Widgets.IsEmpty())
	{
		GameHud = Cast<USacredLakeWidget>(Widgets[0]);
	}
	ApplyAudioSettings();
	FCoreDelegates::ApplicationWillDeactivateDelegate.AddUObject(this,
		&AOneCloudPlayerController::PauseOnDeactivation);
	if (LakeRules.IsValid() && LakeRules->GetResult() != ESacredLakeResult::Defending)
	{
		bSessionStarted = true;
		ShowResult(LakeRules->GetResult());
		return;
	}
	if (bSkipIntro)
	{
		bSessionStarted = true;
		SetInputMode(FInputModeGameOnly());
		bShowMouseCursor = false;
		SetHudVisible(true);
		RefreshBlessingChoice();
	}
	else
	{
		OpenMenu(EOneCloudMenuMode::Start, true);
		if (!Menu)
		{
			StartGame();
		}
	}
}

void AOneCloudPlayerController::StartGame()
{
	if (bSessionStarted)
	{
		ResumeGame();
		return;
	}
	bSessionStarted = true;
	CloseMenu();
	if (OpeningClass)
	{
		OpeningComic = CreateWidget<UOneCloudComicWidget>(this, OpeningClass);
		if (OpeningComic)
		{
			OpeningComic->OnReadingFinished.AddUObject(this, &AOneCloudPlayerController::OpeningFinished);
			OpeningComic->AddToViewport(100);
			if (OpeningComic->BeginComic())
			{
				SetHudVisible(false);
				return;
			}
			OpeningComic->RemoveFromParent();
			OpeningComic = nullptr;
		}
	}
	OpeningFinished();
}

void AOneCloudPlayerController::OpeningFinished()
{
	OpeningComic = nullptr;
	SetHudVisible(true);
	RefreshBlessingChoice();
}

bool AOneCloudPlayerController::IsMenuOpen() const
{
	return IsValid(Menu) && Menu->IsInViewport();
}

void AOneCloudPlayerController::OpenMenu(EOneCloudMenuMode Mode, bool bPauseWorld)
{
	if (OpeningComic || BlessingWidget || !MenuWidgetClass)
	{
		return;
	}
	if (!Menu)
	{
		Menu = CreateWidget<UOneCloudMenuWidget>(this, MenuWidgetClass);
	}
	if (!Menu)
	{
		return;
	}
	// UI 接管后收不到 F 的松开事件，先结束按住补云，避免恢复后继续暗中扣湖水。
	if (LakeRules.IsValid())
	{
		LakeRules->SetRefillingCloud(false);
		LakeRules->CancelForestPlanting();
	}
	if (bPauseWorld && !UGameplayStatics::IsGamePaused(this))
	{
		bPausedByMenu = UGameplayStatics::SetGamePaused(this, true);
	}
	Menu->SetMode(Mode);
	if (!Menu->IsInViewport())
	{
		Menu->AddToViewport(200);
	}
	SetHudVisible(false);
	bShowMouseCursor = true;
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, Menu, EMouseLockMode::DoNotLock, true);
	Menu->FocusDefaultControl();
}

void AOneCloudPlayerController::CloseMenu()
{
	if (Menu)
	{
		Menu->RemoveFromParent();
	}
	if (bPausedByMenu)
	{
		UGameplayStatics::SetGamePaused(this, false);
		bPausedByMenu = false;
	}
	bShowMouseCursor = false;
	UWidgetBlueprintLibrary::SetInputMode_GameOnly(this, true);
	UWidgetBlueprintLibrary::SetFocusToGameViewport();
	SetHudVisible(true);
	RefreshBlessingChoice();
}

void AOneCloudPlayerController::ResumeGame()
{
	if (bSessionStarted && !OpeningComic && !BlessingWidget && (!LakeRules.IsValid() ||
		LakeRules->GetResult() == ESacredLakeResult::Defending))
	{
		CloseMenu();
	}
}

void AOneCloudPlayerController::TogglePauseMenu()
{
	if (OpeningComic || BlessingWidget || !bSessionStarted)
	{
		return;
	}
	if (IsMenuOpen())
	{
		Menu->Back();
	}
	else if (LakeRules.IsValid() && LakeRules->GetResult() == ESacredLakeResult::Defending)
	{
		OpenMenu(EOneCloudMenuMode::Pause, true);
	}
}

void AOneCloudPlayerController::ShowHelp()
{
	if (OpeningComic || BlessingWidget)
	{
		return;
	}
	if (!IsMenuOpen())
	{
		OpenMenu(bSessionStarted ? EOneCloudMenuMode::Pause : EOneCloudMenuMode::Start, true);
	}
	if (Menu)
	{
		Menu->ShowHelp();
	}
}

void AOneCloudPlayerController::RequestRetry()
{
	if (!LakeRules.IsValid() || OpeningComic || BlessingWidget || !bSessionStarted)
	{
		return;
	}
	if (LakeRules->GetResult() != ESacredLakeResult::Defending)
	{
		ConfirmRetry();
	}
	else
	{
		OpenMenu(EOneCloudMenuMode::ConfirmRetry, true);
	}
}

void AOneCloudPlayerController::ConfirmRetry()
{
	if (LakeRules.IsValid())
	{
		// 由 OpenLevel 重建一局，不在 UI 中手工清理敌人或复制资源重置逻辑。
		CloseMenu();
		LakeRules->Retry();
	}
}

void AOneCloudPlayerController::ShowResult(ESacredLakeResult Result)
{
	CloseBlessingChoice();
	// 不暂停结束后的世界，让空中剩余雨滴和声音自然落完。
	OpenMenu(Result == ESacredLakeResult::Won ? EOneCloudMenuMode::Victory :
		(Result == ESacredLakeResult::Dry ? EOneCloudMenuMode::Defeat : EOneCloudMenuMode::Unavailable), false);
}

void AOneCloudPlayerController::PauseOnDeactivation()
{
	if (bSessionStarted && !OpeningComic && !BlessingWidget && !IsMenuOpen() && LakeRules.IsValid() &&
		LakeRules->GetResult() == ESacredLakeResult::Defending)
	{
		OpenMenu(EOneCloudMenuMode::Pause, true);
	}
}

// 奖励暂停由控制器拥有；规则只拥有待选项目与数值，不修改输入模式。
void AOneCloudPlayerController::RefreshBlessingChoice()
{
	if (!LakeRules.IsValid() || !LakeRules->HasPendingForestBlessing())
	{
		CloseBlessingChoice();
		return;
	}
	// 奖励若在开场或菜单期间产生，等恢复游戏再显示，避免两套暂停互相解除。
	if (!bSessionStarted || OpeningComic || IsMenuOpen() || BlessingWidget || !BlessingWidgetClass) { return; }
	BlessingWidget = CreateWidget<UForestBlessingWidget>(this, BlessingWidgetClass);
	if (!BlessingWidget) { return; }
	LakeRules->SetRefillingCloud(false);
	LakeRules->CancelForestPlanting();
	bPausedByBlessing = !UGameplayStatics::IsGamePaused(this) && UGameplayStatics::SetGamePaused(this, true);
	BlessingWidget->SetChoices(LakeRules->GetPendingBlessingChoices());
	BlessingWidget->OnChoiceSelected.AddUObject(this, &AOneCloudPlayerController::ChooseBlessing);
	BlessingWidget->AddToViewport(220);
	SetHudVisible(false);
	bShowMouseCursor = true;
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(this, BlessingWidget, EMouseLockMode::DoNotLock, true);
	BlessingWidget->FocusFirstChoice();
}

void AOneCloudPlayerController::ChooseBlessing(int32 ChoiceIndex)
{
	if (LakeRules.IsValid() && LakeRules->ChooseForestBlessing(ChoiceIndex))
	{
		if (BlessingWidget && LakeRules->HasPendingForestBlessing())
		{
			// 一次结算可击败两只精英。复用当前暂停界面展示下一组，避免奖励滞留。
			BlessingWidget->SetChoices(LakeRules->GetPendingBlessingChoices());
			BlessingWidget->FocusFirstChoice();
		}
		else
		{
			CloseBlessingChoice();
		}
	}
	else if (BlessingWidget && LakeRules.IsValid())
	{
		BlessingWidget->SetChoices(LakeRules->GetPendingBlessingChoices());
	}
}

void AOneCloudPlayerController::CloseBlessingChoice()
{
	if (!BlessingWidget) { return; }
	BlessingWidget->OnChoiceSelected.RemoveAll(this);
	BlessingWidget->RemoveFromParent();
	BlessingWidget = nullptr;
	if (bPausedByBlessing)
	{
		UGameplayStatics::SetGamePaused(this, false);
		bPausedByBlessing = false;
	}
	bShowMouseCursor = false;
	UWidgetBlueprintLibrary::SetInputMode_GameOnly(this, true);
	UWidgetBlueprintLibrary::SetFocusToGameViewport();
	SetHudVisible(true);
}

void AOneCloudPlayerController::SetHudVisible(bool bVisible)
{
	if (GameHud.IsValid())
	{
		GameHud->SetVisibility(bVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
	}
}

void AOneCloudPlayerController::ApplyAudioSettings()
{
	const UOneCloudUserSettings* Settings = GEngine ? Cast<UOneCloudUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
	if (!Settings || !GetWorld())
	{
		return;
	}
	if (UserMix)
	{
		if (!bAudioMixPushed)
		{
			UGameplayStatics::PushSoundMixModifier(this, UserMix);
			bAudioMixPushed = true;
		}
		if (MusicClass)
		{
			UGameplayStatics::SetSoundMixClassOverride(this, UserMix, MusicClass,
				Settings->MasterVolume * Settings->MusicVolume, 1.f, 0.15f, true);
		}
		if (EffectsClass)
		{
			UGameplayStatics::SetSoundMixClassOverride(this, UserMix, EffectsClass,
				Settings->MasterVolume * Settings->EffectsVolume, 1.f, 0.15f, true);
		}
	}
}

void AOneCloudPlayerController::QuitGame()
{
	UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void AOneCloudPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	FCoreDelegates::ApplicationWillDeactivateDelegate.RemoveAll(this);
	if (LakeRules.IsValid())
	{
		LakeRules->OnResultChanged.RemoveAll(this);
		LakeRules->OnBlessingChoiceChanged.RemoveAll(this);
	}
	if (OpeningComic)
	{
		OpeningComic->OnReadingFinished.RemoveAll(this);
		OpeningComic->RemoveFromParent();
	}
	if (Menu)
	{
		Menu->RemoveFromParent();
	}
	if (BlessingWidget)
	{
		BlessingWidget->OnChoiceSelected.RemoveAll(this);
		BlessingWidget->RemoveFromParent();
	}
	if (bAudioMixPushed)
	{
		UGameplayStatics::PopSoundMixModifier(this, UserMix);
	}
	Super::EndPlay(EndPlayReason);
}
