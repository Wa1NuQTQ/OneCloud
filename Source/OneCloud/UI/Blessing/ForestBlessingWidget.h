#pragma once

#include "Blueprint/UserWidget.h"
#include "ForestBlessingWidget.generated.h"

class UButton;
class UImage;
class UTextBlock;
class UTexture2D;
struct FOneCloudBlessingChoice;

DECLARE_MULTICAST_DELEGATE_OneParam(FForestBlessingChosen, int32);

// 三张卡片的排版由 WBP_ForestBlessing 编辑，规则提供文案和可选项目。
UCLASS()
class ONECLOUD_API UForestBlessingWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	UForestBlessingWidget(const FObjectInitializer& Initializer);
	void SetChoices(TConstArrayView<FOneCloudBlessingChoice> Choices);
	void FocusFirstChoice();
	FForestBlessingChosen OnChoiceSelected;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") TObjectPtr<UTexture2D> RainPathIcon;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") TObjectPtr<UTexture2D> SnowPathIcon;
	UPROPERTY(EditDefaultsOnly, Category = "Appearance") TObjectPtr<UTexture2D> HailPathIcon;

protected:
	virtual void NativeOnInitialized() override;
	virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event) override;

private:
	UFUNCTION() void ChooseFirst();
	UFUNCTION() void ChooseSecond();
	UFUNCTION() void ChooseThird();
	void Choose(int32 Index);
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ChoiceOne;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ChoiceTwo;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UButton> ChoiceThree;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> NameOne;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> NameTwo;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> NameThree;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> DescriptionOne;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> DescriptionTwo;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> DescriptionThree;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> StackOne;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> StackTwo;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UTextBlock> StackThree;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> PathOne;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> PathTwo;
	UPROPERTY(meta = (BindWidget)) TObjectPtr<UImage> PathThree;
	int32 ChoiceCount = 0;
	bool bChoiceCommitted = false;
	// 用真实时间而非世界计时器：奖励界面会暂停世界，下一组仍须正常接收输入。
	double AcceptChoicesAtRealSeconds = 0.0;
};
