#include "OneCloudMenuWidget.h"

#include "Components/Button.h"
#include "Components/ComboBoxString.h"
#include "Components/Slider.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Engine/Engine.h"
#include "Game/OneCloudUserSettings.h"
#include "InputCoreTypes.h"
#include "Player/OneCloudPlayerController.h"

UOneCloudMenuWidget::UOneCloudMenuWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
	SetIsFocusable(true);
}

void UOneCloudMenuWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	PrimaryButton->OnClicked.AddDynamic(this, &UOneCloudMenuWidget::PrimaryClicked);
	RetryButton->OnClicked.AddDynamic(this, &UOneCloudMenuWidget::RetryClicked);
	SettingsButton->OnClicked.AddDynamic(this, &UOneCloudMenuWidget::SettingsClicked);
	HelpButton->OnClicked.AddDynamic(this, &UOneCloudMenuWidget::HelpClicked);
	CreditsButton->OnClicked.AddDynamic(this, &UOneCloudMenuWidget::CreditsClicked);
	QuitButton->OnClicked.AddDynamic(this, &UOneCloudMenuWidget::QuitClicked);
	BackButton->OnClicked.AddDynamic(this, &UOneCloudMenuWidget::BackClicked);
	ApplyButton->OnClicked.AddDynamic(this, &UOneCloudMenuWidget::ApplyClicked);
	DisplayBox->OnSelectionChanged.AddDynamic(this, &UOneCloudMenuWidget::DisplayModeChanged);
	LoadSettings();
}

void UOneCloudMenuWidget::SetMode(EOneCloudMenuMode NewMode)
{
	Mode = NewMode;
	Pages->SetActiveWidgetIndex(0);
	RefreshMainPage();
}

void UOneCloudMenuWidget::RefreshMainPage()
{
	const bool bStart = Mode == EOneCloudMenuMode::Start;
	const bool bConfirm = Mode == EOneCloudMenuMode::ConfirmRetry;
	const bool bEnded = Mode == EOneCloudMenuMode::Victory || Mode == EOneCloudMenuMode::Defeat || Mode == EOneCloudMenuMode::Unavailable;
	FText Title;
	FText Detail;
	FText Action;
	switch (Mode)
	{
	case EOneCloudMenuMode::Start:
		Title = NSLOCTEXT("Menu", "StartTitle", "圣湖守望");
		Detail = NSLOCTEXT("Menu", "StartDetail", "种下新生，让雨守住最后的圣水。");
		Action = NSLOCTEXT("Menu", "Start", "开始守湖");
		break;
	case EOneCloudMenuMode::Pause:
		Title = NSLOCTEXT("Menu", "PauseTitle", "片刻休憩");
		Detail = NSLOCTEXT("Menu", "PauseDetail", "风雨暂歇，圣湖等你归来。");
		Action = NSLOCTEXT("Menu", "Resume", "继续");
		break;
	case EOneCloudMenuMode::ConfirmRetry:
		Title = NSLOCTEXT("Menu", "RetryTitle", "重新开始？");
		Detail = NSLOCTEXT("Menu", "RetryDetail", "本局成长将重置。Esc 返回。");
		Action = NSLOCTEXT("Menu", "Retry", "确认重试");
		break;
	case EOneCloudMenuMode::Victory:
		Title = NSLOCTEXT("Menu", "VictoryTitle", "圣湖仍在呼吸");
		Detail = NSLOCTEXT("Menu", "VictoryDetail", "首领已退。新生的树木将继续守护圣湖。");
		Action = NSLOCTEXT("Menu", "Again", "再次守湖");
		break;
	case EOneCloudMenuMode::Defeat:
		Title = NSLOCTEXT("Menu", "DefeatTitle", "最后一滴圣水");
		Detail = NSLOCTEXT("Menu", "DefeatDetail", "圣水已尽。早些种树，优先打断岸边的窃水者。");
		Action = NSLOCTEXT("Menu", "TryAgain", "再试一次");
		break;
	default:
		Title = NSLOCTEXT("Menu", "UnavailableTitle", "圣湖暂时无法开启");
		Detail = NSLOCTEXT("Menu", "UnavailableDetail", "关卡资源未能就绪，请重试或退出。");
		Action = NSLOCTEXT("Menu", "Reload", "重新载入");
		break;
	}
	Heading->SetText(Title);
	Description->SetText(Detail);
	PrimaryLabel->SetText(Action);
	RetryButton->SetVisibility(!bStart && !bConfirm && !bEnded ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	SettingsButton->SetVisibility(bConfirm ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	HelpButton->SetVisibility(bConfirm ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	CreditsButton->SetVisibility(bConfirm ? ESlateVisibility::Collapsed : ESlateVisibility::Visible);
	BackButton->SetVisibility(bConfirm ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
	if (IsInViewport()) { FocusDefaultControl(); }
}

void UOneCloudMenuWidget::FocusDefaultControl()
{
	if (Pages->GetActiveWidgetIndex() == 0) { PrimaryButton->SetKeyboardFocus(); }
	else if (Pages->GetActiveWidgetIndex() == 2) { ApplyButton->SetKeyboardFocus(); }
	else { BackButton->SetKeyboardFocus(); }
}

void UOneCloudMenuWidget::PrimaryClicked()
{
	if (AOneCloudPlayerController* Player = Cast<AOneCloudPlayerController>(GetOwningPlayer()))
	{
		if (Mode == EOneCloudMenuMode::Start)
		{
			Player->StartGame();
		}
		else if (Mode == EOneCloudMenuMode::Pause)
		{
			Player->ResumeGame();
		}
		else
		{
			Player->ConfirmRetry();
		}
	}
}

void UOneCloudMenuWidget::RetryClicked()
{
	SetMode(EOneCloudMenuMode::ConfirmRetry);
}

void UOneCloudMenuWidget::QuitClicked()
{
	if (AOneCloudPlayerController* Player = Cast<AOneCloudPlayerController>(GetOwningPlayer()))
	{
		Player->QuitGame();
	}
}

void UOneCloudMenuWidget::ShowHelp()
{
	Pages->SetActiveWidgetIndex(1);
	BackButton->SetVisibility(ESlateVisibility::Visible);
	BackButton->SetKeyboardFocus();
}

void UOneCloudMenuWidget::HelpClicked() { ShowHelp(); }

void UOneCloudMenuWidget::SettingsClicked()
{
	LoadSettings();
	Pages->SetActiveWidgetIndex(2);
	BackButton->SetVisibility(ESlateVisibility::Visible);
	ApplyButton->SetKeyboardFocus();
}

void UOneCloudMenuWidget::CreditsClicked()
{
	Pages->SetActiveWidgetIndex(3);
	BackButton->SetVisibility(ESlateVisibility::Visible);
	BackButton->SetKeyboardFocus();
}

void UOneCloudMenuWidget::BackClicked() { Back(); }

void UOneCloudMenuWidget::Back()
{
	if (Pages->GetActiveWidgetIndex() != 0)
	{
		Pages->SetActiveWidgetIndex(0);
		RefreshMainPage();
	}
	else if (Mode == EOneCloudMenuMode::ConfirmRetry)
	{
		SetMode(EOneCloudMenuMode::Pause);
	}
	else if (Mode == EOneCloudMenuMode::Pause)
	{
		if (AOneCloudPlayerController* Player = Cast<AOneCloudPlayerController>(GetOwningPlayer()))
		{
			Player->ResumeGame();
		}
	}
}

FReply UOneCloudMenuWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	if (Event.GetKey() == EKeys::Escape)
	{
		if (!Event.IsRepeat()) { Back(); }
		return FReply::Handled();
	}
	if (Event.GetKey() == EKeys::F1)
	{
		ShowHelp();
		return FReply::Handled();
	}
	return Super::NativeOnKeyDown(Geometry, Event);
}

void UOneCloudMenuWidget::LoadSettings()
{
	const UOneCloudUserSettings* Settings = GEngine ? Cast<UOneCloudUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
	if (!Settings) { return; }
	MasterSlider->SetValue(Settings->MasterVolume);
	MusicSlider->SetValue(Settings->MusicVolume);
	EffectsSlider->SetValue(Settings->EffectsVolume);
	const int32 Quality = Settings->GetOverallScalabilityLevel();
	QualityBox->SetSelectedIndex(Quality >= 0 && Quality <= 3 ? Quality : 4);
	DisplayBox->SetSelectedIndex(Settings->GetFullscreenMode() == EWindowMode::Windowed ? 1 : 0);
	const FIntPoint Resolution = Settings->GetScreenResolution();
	ResolutionBox->SetSelectedIndex(Resolution == FIntPoint(1280, 720) ? 1 : Resolution == FIntPoint(1920, 1080) ? 2 : 0);
	const float Limit = Settings->GetFrameRateLimit();
	FrameLimitBox->SetSelectedIndex(FMath::IsNearlyEqual(Limit, 60.f) ? 1 : FMath::IsNearlyEqual(Limit, 120.f) ? 2 : 0);
	DisplayModeChanged(DisplayBox->GetSelectedOption(), ESelectInfo::Direct);
}

void UOneCloudMenuWidget::DisplayModeChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
	// 无边框使用桌面尺寸；只在窗口模式下提供窗口分辨率选择。
	ResolutionBox->SetIsEnabled(DisplayBox->GetSelectedIndex() == 1);
}

void UOneCloudMenuWidget::ApplyClicked()
{
	UOneCloudUserSettings* Settings = GEngine ? Cast<UOneCloudUserSettings>(GEngine->GetGameUserSettings()) : nullptr;
	if (!Settings) { return; }
	Settings->MasterVolume = MasterSlider->GetValue();
	Settings->MusicVolume = MusicSlider->GetValue();
	Settings->EffectsVolume = EffectsSlider->GetValue();
	if (QualityBox->GetSelectedIndex() >= 0 && QualityBox->GetSelectedIndex() <= 3)
	{
		Settings->SetOverallScalabilityLevel(QualityBox->GetSelectedIndex());
	}
	Settings->SetFullscreenMode(DisplayBox->GetSelectedIndex() == 1 ? EWindowMode::Windowed : EWindowMode::WindowedFullscreen);
	const int32 Resolution = ResolutionBox->GetSelectedIndex();
	if (DisplayBox->GetSelectedIndex() == 1 && (Resolution == 1 || Resolution == 2))
	{
		Settings->SetScreenResolution(Resolution == 1 ? FIntPoint(1280, 720) : FIntPoint(1920, 1080));
	}
	Settings->SetFrameRateLimit(FrameLimitBox->GetSelectedIndex() == 1 ? 60.f : FrameLimitBox->GetSelectedIndex() == 2 ? 120.f : 0.f);
	Settings->ApplySettings(false);
	Settings->ConfirmVideoMode();
	Settings->SaveSettings();
	if (AOneCloudPlayerController* Player = Cast<AOneCloudPlayerController>(GetOwningPlayer()))
	{
		Player->ApplyAudioSettings();
	}
	// 留在设置页，方便继续调节；Esc 返回时重新使用菜单主面板。
}
