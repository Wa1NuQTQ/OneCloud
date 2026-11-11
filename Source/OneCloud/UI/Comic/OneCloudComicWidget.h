#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OneCloudComicWidget.generated.h"

class UAudioComponent;
class UButton;
class UImage;
class UPanelWidget;
class USoundBase;
class UTextBlock;
class UWidgetSwitcher;
class UOneCloudComicFrame;

DECLARE_MULTICAST_DELEGATE(FOneCloudReadingFinished);

// 播放器负责页序、格序和输入生命周期，具体页面布局和时间轴由 UMG 子蓝图配置。
UCLASS(Blueprintable)
class ONECLOUD_API UOneCloudComicWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UOneCloudComicWidget(const FObjectInitializer& Initializer);
	// 阅读结束后由本地控制器恢复 HUD，无需轮询漫画状态。
	FOneCloudReadingFinished OnReadingFinished;

	// 在页面可读且有本地控制器时进入阅读，成功后返回 true。
	UFUNCTION(BlueprintCallable, Category = "Comic")
	bool BeginComic();

	UFUNCTION(BlueprintCallable, Category = "Comic")
	void AdvancePanel();

	UFUNCTION(BlueprintCallable, Category = "Comic")
	void PreviousPanel();

	UFUNCTION(BlueprintCallable, Category = "Comic")
	void FinishComic();

	UFUNCTION(BlueprintPure, Category = "Comic")
	bool IsTransitioning() const;

	// 页索引从 0 开始，已揭开的画格按数量记录，从第一格开始阅读。
	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Comic")
	int32 CurrentPageIndex = 0;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Comic")
	int32 RevealedPanelCount = 1;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Comic", meta = (ClampMin = "0", ClampMax = "1"))
	float UnreadProgressOpacity = 0.18f;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Comic|Audio")
	TObjectPtr<USoundBase> BackgroundMusic;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Comic|Audio", meta = (ClampMin = "0", ClampMax = "1"))
	float MusicVolume = 0.18f;

protected:
	virtual void NativeOnInitialized() override;
	virtual void NativeDestruct() override;
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;
	virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnMouseButtonDoubleClick(const FGeometry& Geometry, const FPointerEvent& Event) override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	// 页面查找、转场与世界恢复由播放器内部处理。
	bool CacheFrames();
	const TArray<UOneCloudComicFrame*>& GetFrames(int32 PageIndex) const;
	void ShowPage(bool bAnimateLast);
	void UpdateProgress();
	void RequestPageTurn(int32 PageIndex, int32 PanelCount);
	void ShowPendingPage();
	void PlayPageTurnAccent();
	void PlayTransition(UWidgetAnimation* Animation);
	void CompleteTransition();
	void ResumeWorld();
	void RestoreGame();

	// 这些 BindWidget 指针绑定到 Designer 的同名控件。
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UWidgetSwitcher> PageHost;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Backdrop;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> ReadingLayer;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> ControlsRoot;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UPanelWidget> ProgressBox;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> BackButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> NextButton;

	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UButton> SkipButton;

	// 字幕在画格之外的独立底栏显示；旧漫画没有该控件时仍按原样播放。
	UPROPERTY(meta = (BindWidgetOptional))
	TObjectPtr<UTextBlock> StoryCaption;

	// 这些 BindWidgetAnim 指针绑定到 Designer 的同名时间轴。
	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> Opening;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> TurnOut;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> TurnIn;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> ExitToBlack;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> RevealWorld;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> MusicAudio;

	UPROPERTY(Transient)
	TObjectPtr<UWidgetAnimation> ActiveTransition;
	// 画格仍由各页的 WidgetTree 持有。这里仅缓存本次阅读会访问的引用。
	TArray<TArray<UOneCloudComicFrame*>> PageFrames;
	// 翻页退场完成前保留当前页和当前格，目标单独暂存于此。
	int32 PendingPage = 0;
	int32 PendingCount = 1;
	bool bForwardTurn = false;
	bool bActive = false;
	bool bExiting = false;
	bool bPausedByComic = false;
	bool bPreviousCursor = false;
	bool bPreviousRendering = true;
};
