#include "OneCloudComicWidget.h"
#include "Animation/WidgetAnimation.h"
#include "Blueprint/WidgetBlueprintLibrary.h"
#include "Blueprint/WidgetTree.h"
#include "Components/AudioComponent.h"
#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "GameFramework/PlayerController.h"
#include "InputCoreTypes.h"
#include "Kismet/GameplayStatics.h"
#include "OneCloudComicFrame.h"

// 开场与输入接管

UOneCloudComicWidget::UOneCloudComicWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
	SetIsFocusable(true);
	SetVisibility(ESlateVisibility::Visible);
}

// 按钮和键鼠都调用同一组阅读命令，按键只负责决定调用哪个命令。
void UOneCloudComicWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	BackButton->OnClicked.AddDynamic(this, &UOneCloudComicWidget::PreviousPanel);
	NextButton->OnClicked.AddDynamic(this, &UOneCloudComicWidget::AdvancePanel);
	SkipButton->OnClicked.AddDynamic(this, &UOneCloudComicWidget::FinishComic);
}

// 先验证页面结构，再暂停世界并接管输入，避免坏页面让玩家卡在 UI 模式。
bool UOneCloudComicWidget::BeginComic()
{
	APlayerController* Player = GetOwningPlayer();
	if (bActive || !Player || !PageHost || !CacheFrames())
	{
		return false;
	}
	bActive = true;
	bExiting = false;
	bPreviousCursor = Player->bShowMouseCursor;
	bPreviousRendering = UGameplayStatics::GetEnableWorldRendering(this);
	bPausedByComic = !UGameplayStatics::IsGamePaused(this) && UGameplayStatics::SetGamePaused(this, true);
	UGameplayStatics::SetEnableWorldRendering(this, false);
	Player->bShowMouseCursor = true;
	UWidgetBlueprintLibrary::SetInputMode_UIOnlyEx(Player, this, EMouseLockMode::DoNotLock, true);
	SetUserFocus(Player);
	Backdrop->SetRenderOpacity(1.f);
	ReadingLayer->SetRenderOpacity(1.f);
	PageHost->SetRenderOpacity(1.f);
	ControlsRoot->SetRenderOpacity(1.f);
	CurrentPageIndex = 0;
	RevealedPanelCount = 1;
	ShowPage(true);
	PlayTransition(Opening);
	if (BackgroundMusic && MusicVolume > 0.f)
	{
		// 二维 UI 音乐在暂停时继续播放，淡入到 CreateSound2D 已设置的组件音量。
		MusicAudio = UGameplayStatics::CreateSound2D(this, BackgroundMusic, MusicVolume);
		if (MusicAudio)
		{
			MusicAudio->bIsMusic = true;
			MusicAudio->FadeIn(Opening->GetEndTime(), 1.f);
		}
	}
	return true;
}

// 玩家输入与阅读命令

// 消费漫画用到的按键，其他输入沿用父类的处理。
FReply UOneCloudComicWidget::NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (!bActive)
	{
		return FReply::Unhandled();
	}
	if (Event.GetEffectingButton() == EKeys::LeftMouseButton)
	{
		AdvancePanel();
	}
	else if (Event.GetEffectingButton() == EKeys::RightMouseButton)
	{
		PreviousPanel();
	}
	else
	{
		return Super::NativeOnMouseButtonDown(Geometry, Event);
	}
	return FReply::Handled();
}

FReply UOneCloudComicWidget::NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event)
{
	return NativeOnMouseButtonDown(Geometry, Event);
}

// 长按按键时只接受第一次按下，防止系统的重复键事件连续跳格。
FReply UOneCloudComicWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (!bActive || (Key != EKeys::Escape && Key != EKeys::Left && Key != EKeys::Right && Key != EKeys::SpaceBar &&
					 Key != EKeys::Enter))
	{
		return Super::NativeOnKeyDown(Geometry, Event);
	}
	if (!Event.IsRepeat())
	{
		if (Key == EKeys::Escape)
		{
			FinishComic();
		}
		else if (Key == EKeys::Left)
		{
			PreviousPanel();
		}
		else
		{
			AdvancePanel();
		}
	}
	return FReply::Handled();
}

// 推进顺序是当前页下一格、下一页、退出。动画途中再次推进会先完成当前过渡。
void UOneCloudComicWidget::AdvancePanel()
{
	if (!bActive || bExiting)
	{
		return;
	}
	if (IsTransitioning())
	{
		CompleteTransition();
		return;
	}
	if (RevealedPanelCount < GetFrames(CurrentPageIndex).Num())
	{
		const TArray<UOneCloudComicFrame*>& Frames = GetFrames(CurrentPageIndex);
		Frames[RevealedPanelCount - 1]->SetPast(true);
		Frames[RevealedPanelCount]->Show(false);
		++RevealedPanelCount;
		UpdateProgress();
	}
	else if (CurrentPageIndex + 1 < PageHost->GetChildrenCount())
	{
		RequestPageTurn(CurrentPageIndex + 1, 1);
	}
	else
	{
		FinishComic();
	}
}

// 页内隐藏最后一格，退过页首时回到上一页的最后一格。
void UOneCloudComicWidget::PreviousPanel()
{
	if (!bActive || bExiting)
	{
		return;
	}
	if (IsTransitioning())
	{
		CompleteTransition();
		return;
	}
	if (RevealedPanelCount > 1)
	{
		GetFrames(CurrentPageIndex)[RevealedPanelCount - 1]->Hide(false);
		--RevealedPanelCount;
		GetFrames(CurrentPageIndex)[RevealedPanelCount - 1]->Show(true);
		UpdateProgress();
	}
	else if (CurrentPageIndex > 0)
	{
		const int32 PreviousPage = CurrentPageIndex - 1;
		RequestPageTurn(PreviousPage, GetFrames(PreviousPage).Num());
	}
}

// 保留当前画格的演出姿态并淡到黑屏，之后再恢复世界画面。
void UOneCloudComicWidget::FinishComic()
{
	if (!bActive || bExiting)
	{
		return;
	}
	bExiting = true;
	if (IsValid(MusicAudio))
	{
		MusicAudio->FadeOut(ExitToBlack->GetEndTime() + RevealWorld->GetEndTime(), 0.f);
	}
	if (ActiveTransition)
	{
		PauseAnimation(ActiveTransition);
	}
	ActiveTransition = nullptr;
	FlushAnimations();
	for (UOneCloudComicFrame* Frame : GetFrames(CurrentPageIndex))
	{
		Frame->Freeze();
	}
	PlayTransition(ExitToBlack);
}

// 整页动画与转场

// 暂存目标页与已读格数，等旧页退场结束后再切换 WidgetSwitcher。
void UOneCloudComicWidget::RequestPageTurn(int32 PageIndex, int32 PanelCount)
{
	PendingPage = PageIndex;
	PendingCount = PanelCount;
	bForwardTurn = PageIndex > CurrentPageIndex;
	PlayTransition(TurnOut);
}

void UOneCloudComicWidget::PlayTransition(UWidgetAnimation* Animation)
{
	ActiveTransition = Animation;
	PlayAnimation(Animation);
}

// 动画结束回调驱动翻页和退出，只接受当前过渡的回调。
void UOneCloudComicWidget::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);
	if (Animation != ActiveTransition)
	{
		return;
	}
	ActiveTransition = nullptr;
	if (Animation == Opening)
	{
		PageHost->SetRenderOpacity(1.f);
		ControlsRoot->SetRenderOpacity(1.f);
	}
	else if (Animation == TurnOut)
	{
		ShowPendingPage();
		PlayTransition(TurnIn);
	}
	else if (Animation == TurnIn)
	{
		PageHost->SetRenderOpacity(1.f);
		ControlsRoot->SetRenderOpacity(1.f);
		PlayPageTurnAccent();
	}
	else if (Animation == ExitToBlack)
	{
		// 黑屏时重新渲染世界，保持 UI 输入直到遮罩揭开。
		ResumeWorld();
		PlayTransition(RevealWorld);
	}
	else if (Animation == RevealWorld)
	{
		RestoreGame();
		RemoveFromParent();
		OnReadingFinished.Broadcast();
	}
}

// 整页转场和单格进出都会阻止立即推进，已读格的变暗动画可以并行播放。
bool UOneCloudComicWidget::IsTransitioning() const
{
	if (ActiveTransition)
	{
		return true;
	}
	if (bActive)
	{
		for (UOneCloudComicFrame* Frame : GetFrames(CurrentPageIndex))
		{
			if (Frame->IsTransitioning())
			{
				return true;
			}
		}
	}
	return false;
}

// 中途快进时直接落到过渡完成的状态，并同步完成当前页上的画格动画。
void UOneCloudComicWidget::CompleteTransition()
{
	if (ActiveTransition)
	{
		UWidgetAnimation* Animation = ActiveTransition;
		// Stop 可能同步触发回调，清除标记后由这里直接补齐转场结果。
		ActiveTransition = nullptr;
		StopAllAnimations();
		FlushAnimations();
		if (Animation == TurnOut)
		{
			ShowPendingPage();
		}
		PageHost->SetRenderOpacity(1.f);
		ControlsRoot->SetRenderOpacity(1.f);
	}
	for (UOneCloudComicFrame* Frame : GetFrames(CurrentPageIndex))
	{
		Frame->Complete();
	}
}

// 画格查找与页面显示

// PageHost 的子控件顺序就是页序，每页 Panels 的子控件顺序就是格序。
bool UOneCloudComicWidget::CacheFrames()
{
	PageFrames.Reset();
	for (UWidget* ChildPage : PageHost->GetAllChildren())
	{
		UUserWidget* Page = Cast<UUserWidget>(ChildPage);
		UPanelWidget* Panels = Page ? Cast<UPanelWidget>(Page->WidgetTree->FindWidget(TEXT("Panels"))) : nullptr;
		TArray<UOneCloudComicFrame*>& Frames = PageFrames.AddDefaulted_GetRef();
		if (Panels)
		{
			for (UWidget* Child : Panels->GetAllChildren())
			{
				if (UOneCloudComicFrame* Frame = Cast<UOneCloudComicFrame>(Child))
				{
					Frames.Add(Frame);
				}
			}
		}
		if (Frames.IsEmpty())
		{
			PageFrames.Reset();
			return false;
		}
	}
	return !PageFrames.IsEmpty();
}

const TArray<UOneCloudComicFrame*>& UOneCloudComicWidget::GetFrames(int32 PageIndex) const
{
	return PageFrames[PageIndex];
}

void UOneCloudComicWidget::ShowPendingPage()
{
	CurrentPageIndex = PendingPage;
	RevealedPanelCount = PendingCount;
	ShowPage(false);
}

// 翻到一页时同时恢复所有已读格，并隐藏超出当前阅读进度的格子。
void UOneCloudComicWidget::ShowPage(bool bAnimateLast)
{
	PageHost->SetActiveWidgetIndex(CurrentPageIndex);
	const TArray<UOneCloudComicFrame*>& Frames = GetFrames(CurrentPageIndex);
	const int32 LatestPanel = RevealedPanelCount - 1;
	for (int32 Index = 0; Index < Frames.Num(); ++Index)
	{
		UOneCloudComicFrame* Frame = Frames[Index];
		if (Index > LatestPanel)
		{
			Frame->Hide(true);
			continue;
		}

		Frame->Show(!bAnimateLast || Index != LatestPanel);
		if (Index < LatestPanel)
		{
			Frame->SetPast(true);
		}
	}
	UpdateProgress();
}

// 正向翻页完成后，可为新页的第一格补播它自己的入场动画。
void UOneCloudComicWidget::PlayPageTurnAccent()
{
	if (!bForwardTurn)
	{
		return;
	}
	UOneCloudComicFrame* Frame = GetFrames(CurrentPageIndex)[RevealedPanelCount - 1];
	if (Frame->bPageTurnAccent)
	{
		Frame->Show(false);
	}
}

// 字幕跟随当前最后一格；进度点累计整部漫画的已读格数，允许各页格数不同。
void UOneCloudComicWidget::UpdateProgress()
{
	if (StoryCaption)
	{
		const TArray<UOneCloudComicFrame*>& Frames = GetFrames(CurrentPageIndex);
		const int32 LatestPanel = RevealedPanelCount - 1;
		const FText Caption = Frames.IsValidIndex(LatestPanel) ? Frames[LatestPanel]->Caption : FText::GetEmpty();
		StoryCaption->SetText(Caption);
		StoryCaption->SetVisibility(Caption.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
	}
	int32 Read = RevealedPanelCount;
	for (int32 Page = 0; Page < CurrentPageIndex; ++Page)
	{
		Read += GetFrames(Page).Num();
	}
	for (int32 Index = 0; Index < ProgressBox->GetChildrenCount(); ++Index)
	{
		ProgressBox->GetChildAt(Index)->SetRenderOpacity(Index < Read ? 1.f : UnreadProgressOpacity);
	}
}

// 退出与清理

// 恢复进入前的渲染状态，只有本控件暂停的世界才由它解除暂停。
void UOneCloudComicWidget::ResumeWorld()
{
	UGameplayStatics::SetEnableWorldRendering(this, bPreviousRendering);
	if (bPausedByComic)
	{
		UGameplayStatics::SetGamePaused(this, false);
		bPausedByComic = false;
	}
}

// 撤销本次阅读占用的动画、音频与输入。正常退出和外部移除控件共享这一入口。
void UOneCloudComicWidget::RestoreGame()
{
	if (!bActive)
	{
		return;
	}

	// 停止并求值动画可能同步触发结束回调，先清掉当前过渡和阅读状态。
	bActive = false;
	ActiveTransition = nullptr;
	if (IsValid(MusicAudio))
	{
		MusicAudio->Stop();
	}
	MusicAudio = nullptr;
	StopAllAnimations();
	FlushAnimations();
	ResumeWorld();
	for (const TArray<UOneCloudComicFrame*>& Frames : PageFrames)
	{
		for (UOneCloudComicFrame* Frame : Frames)
		{
			Frame->Hide(true);
		}
	}
	PageFrames.Reset();
	if (APlayerController* Player = GetOwningPlayer())
	{
		Player->bShowMouseCursor = bPreviousCursor;
		UWidgetBlueprintLibrary::SetInputMode_GameOnly(Player, true);
		UWidgetBlueprintLibrary::SetFocusToGameViewport();
	}
}

void UOneCloudComicWidget::NativeDestruct()
{
	RestoreGame();
	Super::NativeDestruct();
}
