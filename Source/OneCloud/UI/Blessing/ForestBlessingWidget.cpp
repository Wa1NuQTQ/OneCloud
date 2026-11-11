#include "ForestBlessingWidget.h"

#include "Components/Button.h"
#include "Components/Image.h"
#include "Components/TextBlock.h"
#include "Game/SacredLake/ForestTypes.h"
#include "HAL/PlatformTime.h"
#include "InputCoreTypes.h"

UForestBlessingWidget::UForestBlessingWidget(const FObjectInitializer& Initializer) : Super(Initializer)
{
	SetIsFocusable(true);
}

void UForestBlessingWidget::NativeOnInitialized()
{
	Super::NativeOnInitialized();
	ChoiceOne->OnClicked.AddDynamic(this, &UForestBlessingWidget::ChooseFirst);
	ChoiceTwo->OnClicked.AddDynamic(this, &UForestBlessingWidget::ChooseSecond);
	ChoiceThree->OnClicked.AddDynamic(this, &UForestBlessingWidget::ChooseThird);
}

void UForestBlessingWidget::SetChoices(TConstArrayView<FOneCloudBlessingChoice> Choices)
{
	ChoiceCount = FMath::Min(3, Choices.Num());
	bChoiceCommitted = false;
	// 同帧获得多次奖励时，第二次鼠标点击不应替玩家选掉下一组。
	AcceptChoicesAtRealSeconds = FPlatformTime::Seconds() + 0.6;
	UButton* Buttons[] = {ChoiceOne, ChoiceTwo, ChoiceThree};
	UTextBlock* Names[] = {NameOne, NameTwo, NameThree};
	UTextBlock* Descriptions[] = {DescriptionOne, DescriptionTwo, DescriptionThree};
	UTextBlock* Stacks[] = {StackOne, StackTwo, StackThree};
	UImage* Paths[] = {PathOne, PathTwo, PathThree};
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Buttons); ++Index)
	{
		Buttons[Index]->SetVisibility(Index < ChoiceCount ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
		if (Index >= ChoiceCount) { continue; }
		const FOneCloudBlessingChoice& Choice = Choices[Index];
		Names[Index]->SetText(Choice.Name);
		Descriptions[Index]->SetText(Choice.Description);
		Stacks[Index]->SetText(FText::Format(NSLOCTEXT("ForestBlessing", "Stacks", "获得后 {0} / {1}"),
			FText::AsNumber(Choice.CurrentStacks + 1), FText::AsNumber(Choice.MaxStacks)));
		const FLinearColor Color = Choice.Path == EForestBlessingPath::Rain ? FLinearColor(0.42f, 0.82f, 0.69f)
			: (Choice.Path == EForestBlessingPath::Snow ? FLinearColor(0.5f, 0.74f, 0.95f) : FLinearColor(0.85f, 0.73f, 0.47f));
		Paths[Index]->SetColorAndOpacity(Color);
		UTexture2D* Texture = Choice.Path == EForestBlessingPath::Rain ? RainPathIcon
			: (Choice.Path == EForestBlessingPath::Snow ? SnowPathIcon : HailPathIcon);
		if (Texture) { Paths[Index]->SetBrushFromTexture(Texture); }
	}
}

void UForestBlessingWidget::FocusFirstChoice()
{
	if (ChoiceCount > 0) { ChoiceOne->SetKeyboardFocus(); }
}

FReply UForestBlessingWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& Event)
{
	const FKey Key = Event.GetKey();
	if (Key == EKeys::One || Key == EKeys::NumPadOne) { if (!Event.IsRepeat()) { Choose(0); } return FReply::Handled(); }
	if (Key == EKeys::Two || Key == EKeys::NumPadTwo) { if (!Event.IsRepeat()) { Choose(1); } return FReply::Handled(); }
	if (Key == EKeys::Three || Key == EKeys::NumPadThree) { if (!Event.IsRepeat()) { Choose(2); } return FReply::Handled(); }
	// 此处只消费选择，不将数字键透传为战斗技能；Esc 也不会丢掉本次奖励。
	if (Key == EKeys::Escape) { return FReply::Handled(); }
	return Super::NativeOnKeyDown(Geometry, Event);
}

void UForestBlessingWidget::ChooseFirst() { Choose(0); }
void UForestBlessingWidget::ChooseSecond() { Choose(1); }
void UForestBlessingWidget::ChooseThird() { Choose(2); }

void UForestBlessingWidget::Choose(int32 Index)
{
	if (!bChoiceCommitted && Index >= 0 && Index < ChoiceCount && FPlatformTime::Seconds() >= AcceptChoicesAtRealSeconds)
	{
		// 一个点击只提交一次，实际可选性与叠层上限仍由规则最终验证。
		bChoiceCommitted = true;
		OnChoiceSelected.Broadcast(Index);
	}
}
