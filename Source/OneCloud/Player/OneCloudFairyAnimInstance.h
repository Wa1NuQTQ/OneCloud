#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimInstance.h"
#include "OneCloudFairyAnimInstance.generated.h"

class UAnimSequence;

UENUM(BlueprintType)
enum class EOneCloudFlightPose : uint8
{
	Hover,
	Start,
	Cruise,
	Stop,
	Turn
};

/** 读取实际运动供 AnimBP 混合飞行姿态。起步、停止和转身会按动画资产长度保留一段时间。 */
UCLASS()
class ONECLOUD_API UOneCloudFairyAnimInstance : public UAnimInstance
{
	GENERATED_BODY()
public:
	virtual void NativeUpdateAnimation(float DeltaSeconds) override;

	// 单位为厘米/秒，包含升降分量，供动画蓝图读取。
	UPROPERTY(BlueprintReadOnly, Category = "Flight")
	float FlightSpeed = 0.f;

	UPROPERTY(BlueprintReadOnly, Category = "Flight")
	EOneCloudFlightPose FlightPose = EOneCloudFlightPose::Hover;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight")
	TObjectPtr<UAnimSequence> StartAnimation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight")
	TObjectPtr<UAnimSequence> StopAnimation;
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Flight")
	TObjectPtr<UAnimSequence> TurnAnimation;

private:
	void EnterPose(EOneCloudFlightPose Pose, const UAnimSequence* Animation = nullptr);
	// 单位为秒。短暂姿态结束后，可以回到悬停或巡航。
	float PoseTimeRemaining = 0.f;
	bool bHadMovementInput = false;
	bool bTurnLatched = false;
};
