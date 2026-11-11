#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "OneCloudEnemyHealthWidget.generated.h"

class UProgressBar;

UCLASS()
class ONECLOUD_API UOneCloudEnemyHealthWidget : public UUserWidget
{
	GENERATED_BODY()
public:
	// 敌人把当前生命比例传入，范围为 0–1，由 Designer 的 Health 控件呈现。
	void SetHealthFraction(float Fraction);

private:
	UPROPERTY(meta = (BindWidget))
	TObjectPtr<UProgressBar> Health;
};
