#pragma once

#include "GameFramework/PlayerController.h"
#include "OneCloudPlayerController.generated.h"

class ASacredLakePrototype;
class UOneCloudComicWidget;
class UOneCloudMenuWidget;
class UForestBlessingWidget;
class USacredLakeWidget;
class USoundMix;
class USoundClass;
enum class ESacredLakeResult : uint8;
enum class EOneCloudMenuMode : uint8;

// 本地的菜单与输入焦点。守湖规则仍由地图里的 SacredLakePrototype 持有。
UCLASS()
class ONECLOUD_API AOneCloudPlayerController : public APlayerController
{
	GENERATED_BODY()

public:
	void BeginSession(TSubclassOf<UOneCloudComicWidget> ComicClass, bool bSkipIntro);
	UFUNCTION(BlueprintCallable, Category = "Session") void StartGame();
	UFUNCTION(BlueprintCallable, Category = "Session") void ResumeGame();
	UFUNCTION(BlueprintCallable, Category = "Session") void RequestRetry();
	UFUNCTION(BlueprintCallable, Category = "Session") void ConfirmRetry();
	UFUNCTION(BlueprintCallable, Category = "Session") void TogglePauseMenu();
	UFUNCTION(BlueprintCallable, Category = "Session") void ShowHelp();
	UFUNCTION(BlueprintCallable, Category = "Session") void QuitGame();
	void ApplyAudioSettings();
	UFUNCTION(BlueprintPure, Category = "Session") bool IsMenuOpen() const;
	bool IsAtStartScreen() const { return !bSessionStarted; }

	UPROPERTY(EditDefaultsOnly, Category = "Interface")
	TSubclassOf<UOneCloudMenuWidget> MenuWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Interface")
	TSubclassOf<UForestBlessingWidget> BlessingWidgetClass;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundMix> UserMix;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundClass> MusicClass;

	UPROPERTY(EditDefaultsOnly, Category = "Audio")
	TObjectPtr<USoundClass> EffectsClass;

protected:
	virtual void SetupInputComponent() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void OpenMenu(EOneCloudMenuMode Mode, bool bPauseWorld);
	void CloseMenu();
	void OpeningFinished();
	void ShowResult(ESacredLakeResult Result);
	void PauseOnDeactivation();
	void SetHudVisible(bool bVisible);
	void RefreshBlessingChoice();
	void CloseBlessingChoice();
	void ChooseBlessing(int32 ChoiceIndex);

	UPROPERTY(Transient)
	TObjectPtr<UOneCloudMenuWidget> Menu;

	UPROPERTY(Transient)
	TObjectPtr<UOneCloudComicWidget> OpeningComic;

	UPROPERTY(Transient)
	TObjectPtr<UForestBlessingWidget> BlessingWidget;

	TSubclassOf<UOneCloudComicWidget> OpeningClass;
	TWeakObjectPtr<ASacredLakePrototype> LakeRules;
	TWeakObjectPtr<USacredLakeWidget> GameHud;
	bool bSessionStarted = false;
	bool bPausedByMenu = false;
	bool bAudioMixPushed = false;
	bool bPausedByBlessing = false;
};
