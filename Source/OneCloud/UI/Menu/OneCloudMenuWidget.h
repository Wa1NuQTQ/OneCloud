#pragma once

#include "Blueprint/UserWidget.h"
#include "OneCloudMenuWidget.generated.h"

class UButton;
class UComboBoxString;
class USlider;
class UTextBlock;
class UWidgetSwitcher;

UENUM(BlueprintType)
enum class EOneCloudMenuMode : uint8
{
	Start, Pause, ConfirmRetry, Victory, Defeat, Unavailable
};

// Designer 保存布局、颜色和帮助内容，这个类只绑定按钮、页面状态和本机设置。
UCLASS()
class ONECLOUD_API UOneCloudMenuWidget : public UUserWidget
{
	GENERATED_BODY()

public:
	UOneCloudMenuWidget(const FObjectInitializer& Initializer);
	void SetMode(EOneCloudMenuMode NewMode);
	void ShowHelp();
	void Back();
	void FocusDefaultControl();

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	UFUNCTION() void PrimaryClicked();
	UFUNCTION() void RetryClicked();
	UFUNCTION() void QuitClicked();
	UFUNCTION() void SettingsClicked();
	UFUNCTION() void HelpClicked();
	UFUNCTION() void CreditsClicked();
	UFUNCTION() void BackClicked();
	UFUNCTION() void ApplyClicked();
	UFUNCTION() void DisplayModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType);
	void LoadSettings();
	void RefreshMainPage();

	UPROPERTY(meta = (BindWidget)) TObjectPtr<UWidgetSwitcher> Pages;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Heading;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> Description;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> PrimaryLabel;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> PrimaryButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> RetryButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> SettingsButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> HelpButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> CreditsButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> QuitButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> BackButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ApplyButton;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> MasterSlider;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> MusicSlider;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<USlider> EffectsSlider;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> QualityBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> DisplayBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> ResolutionBox;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UComboBoxString> FrameLimitBox;

	EOneCloudMenuMode Mode = EOneCloudMenuMode::Start;
};
