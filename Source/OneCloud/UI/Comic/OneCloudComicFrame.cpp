#include "OneCloudComicFrame.h"
#include "Animation/WidgetAnimation.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

// 预构建也会在 Designer 执行，让编辑和播放使用同一套贴图裁切。
void UOneCloudComicFrame::NativePreConstruct()
{
	Super::NativePreConstruct();
	if (Artwork)
	{
		Artwork->SetBrushFromTexture(Picture, true);
		if (Picture)
		{
			const FVector2f SheetSize(Picture->GetSizeX(), Picture->GetSizeY());
			FVector2f UVMin(FMath::Clamp(SheetUVMin.X, 0.0, 1.0), FMath::Clamp(SheetUVMin.Y, 0.0, 1.0));
			FVector2f UVMax(FMath::Clamp(SheetUVMax.X, 0.0, 1.0), FMath::Clamp(SheetUVMax.Y, 0.0, 1.0));
			float InsetPixels = 0.5f;
			if (SheetPanelIndex >= 0 && SheetPanelIndex < 4)
			{
				UVMin = FVector2f(0.5f * (SheetPanelIndex % 2), 0.5f * (SheetPanelIndex / 2));
				UVMax = UVMin + FVector2f(0.5f, 0.5f);
				InsetPixels = 3.f;
			}
			const FVector2f PanelSize = UVMax - UVMin;
			if (SheetSize.X > 0.f && SheetSize.Y > 0.f && PanelSize.X > 0.f && PanelSize.Y > 0.f)
			{
				// UV 略向内收，避免线性采样从邻格取色。窄格限制内收量以保留有效区域。
				const FVector2f Inset(FMath::Min(InsetPixels / SheetSize.X, PanelSize.X * 0.25f),
					FMath::Min(InsetPixels / SheetSize.Y, PanelSize.Y * 0.25f));
				FSlateBrush Brush = Artwork->GetBrush();
				Brush.SetUVRegion(FBox2f(UVMin + Inset, UVMax - Inset));
				// 画刷的期望尺寸采用画格宽高，画格在页面上的位置仍由 UMG 布局控制。
				Brush.SetImageSize(SheetSize * PanelSize);
				Artwork->SetBrush(Brush);
			}
			else
			{
				Artwork->SetBrushFromTexture(nullptr);
			}
		}
	}
	// Designer 显示整页便于排版，运行时由播放器逐格控制可见性。
	if (IsDesignTime())
	{
		SetVisibility(ESlateVisibility::HitTestInvisible);
	}
}

void UOneCloudComicFrame::Show(bool bInstant)
{
	StopAllAnimations();
	// Stop 会排队求值动画起点；先完成它，再应用本格的最终显示状态。
	FlushAnimations();
	Artwork->SetRenderOpacity(1.f);
	Artwork->SetRenderTransform(FWidgetTransform());
	SetVisibility(ESlateVisibility::HitTestInvisible);
	if (!bInstant)
	{
		PlayAnimation(Enter, 0.f, 1, EUMGSequencePlayMode::Forward, EntryPlaybackRate);
	}
}

// 已读格变暗以突出最新一格。直接就位时从变暗时间轴的末尾取状态。
void UOneCloudComicFrame::SetPast(bool bInstant)
{
	PlayAnimation(Dim, bInstant ? Dim->GetEndTime() : 0.f);
}

void UOneCloudComicFrame::Hide(bool bInstant)
{
	StopAllAnimations();
	FlushAnimations();
	if (bInstant)
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
	else
	{
		// 回看时等退场动画播完再隐藏，保留可见的退场过程。
		PlayAnimation(Back);
	}
}

bool UOneCloudComicFrame::IsTransitioning() const
{
	return IsAnimationPlaying(Enter) || IsAnimationPlaying(Back);
}

void UOneCloudComicFrame::Complete()
{
	if (IsAnimationPlaying(Back))
	{
		Hide(true);
	}
	else if (IsAnimationPlaying(Enter))
	{
		Show(true);
	}
}

// 退出漫画时暂停在当前画面，避免单格动画和整页淡黑同时修改画格外观。
void UOneCloudComicFrame::Freeze()
{
	for (UWidgetAnimation* Animation : {Enter.Get(), Dim.Get(), Back.Get()})
	{
		if (IsAnimationPlaying(Animation))
		{
			PauseAnimation(Animation);
		}
	}
	FlushAnimations();
}

void UOneCloudComicFrame::OnAnimationFinished_Implementation(const UWidgetAnimation* Animation)
{
	Super::OnAnimationFinished_Implementation(Animation);
	if (Animation == Back)
	{
		SetVisibility(ESlateVisibility::Hidden);
	}
}
