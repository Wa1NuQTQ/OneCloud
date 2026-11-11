#pragma once

#include "Blueprint/UserWidget.h"
#include "ForestSkillWidget.generated.h"

class UImage;
class UProgressBar;
class UTextBlock;
class UTexture2D;
struct FForestSkillHudState;

// 每个技能槽共用同一 Designer；图标、按键和大小都可直接在子蓝图调整。
UCLASS()
class ONECLOUD_API UForestSkillWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	void Refresh(const FForestSkillHudState& State, float AvailableCloudWater);
	UPROPERTY(EditAnywhere, Category = "Appearance") TObjectPtr<UTexture2D> SkillTexture;
	UPROPERTY(EditAnywhere, Category = "Appearance") FText KeyGlyph;

protected:
	virtual void NativePreConstruct() override;

private:
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> SkillIcon;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> LockIcon;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> MissingWaterIcon;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> TargetBlockedIcon;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UProgressBar> CooldownBar;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> CostLabel;
	UPROPERTY(meta = (BindWidgetOptional)) TObjectPtr<UImage> CostWater;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> KeyLabel;
};
