#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OneCloudComicFrame.generated.h"

class UTexture2D;
class UImage;

// 画格负责从贴图取样并播放单格过渡。页面蓝图配置贴图和布局，画格蓝图配置时间轴。
UCLASS(Blueprintable)
class ONECLOUD_API UOneCloudComicFrame : public UUserWidget
{
	GENERATED_BODY()
public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comic")
	TObjectPtr<UTexture2D> Picture;

	// 叙事保存在页面的画格实例里；播放器只显示当前格的文字，不拥有故事内容。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comic", meta = (MultiLine = "true"))
	FText Caption;

	// 旧版四宫格以 0–3 选择象限。INDEX_NONE 时使用下面的显式 UV 范围。
	UPROPERTY(EditAnywhere, Category = "Comic", meta = (ClampMin = "-1", ClampMax = "3"))
	int32 SheetPanelIndex = INDEX_NONE;

	// 当前格在整页贴图中的左上、右下位置，用 0 到 1 的归一化坐标表示。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comic", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector2D SheetUVMin = FVector2D::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comic", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	FVector2D SheetUVMax = FVector2D(1.0, 1.0);

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comic", meta = (ClampMin = "0.01"))
	float EntryPlaybackRate = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Comic")
	bool bPageTurnAccent = false;
	// bInstant 为 true 时直接就位，用于复原已读画格或快进过渡。
	void Show(bool bInstant);
	void SetPast(bool bInstant);
	void Hide(bool bInstant);
	void Complete();
	void Freeze();
	bool IsTransitioning() const;

protected:
	virtual void NativePreConstruct() override;
	virtual void OnAnimationFinished_Implementation(const UWidgetAnimation* Animation) override;

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UImage> Artwork;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> Enter;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> Dim;

	UPROPERTY(Transient, meta = (BindWidgetAnim))
	TObjectPtr<UWidgetAnimation> Back;
};
