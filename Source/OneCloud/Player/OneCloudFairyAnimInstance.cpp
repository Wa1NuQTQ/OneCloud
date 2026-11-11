#include "OneCloudFairyAnimInstance.h"
#include "OneCloudFairy.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Animation/AnimSequence.h"

void UOneCloudFairyAnimInstance::NativeUpdateAnimation(float DeltaSeconds)
{
	Super::NativeUpdateAnimation(DeltaSeconds);
	APawn* Pawn = TryGetPawnOwner();
	FlightSpeed = Pawn ? Pawn->GetVelocity().Size() : 0.f;
	const ACharacter* Character = Cast<ACharacter>(Pawn);
	if (!Character)
	{
		return;
	}
	const FVector Acceleration = Character->GetCharacterMovement()->GetCurrentAcceleration();
	// 起停看加速度是否进入或退出输入状态。松开方向键后还有惯性，此时可以先播停止动画。
	const bool bHasMovementInput = !Acceleration.IsNearlyZero();
	PoseTimeRemaining = FMath::Max(0.f, PoseTimeRemaining - DeltaSeconds);

	if (bHasMovementInput != bHadMovementInput)
	{
		if (AOneCloudFairy* Fairy = Cast<AOneCloudFairy>(Pawn))
		{
			Fairy->SetWingMoving(bHasMovementInput);
		}
		EnterPose(bHasMovementInput ? EOneCloudFlightPose::Start : EOneCloudFlightPose::Stop,
			bHasMovementInput ? StartAnimation : StopAnimation);
	}
	else if (PoseTimeRemaining <= 0.f)
	{
		EnterPose(bHasMovementInput ? EOneCloudFlightPose::Cruise : EOneCloudFlightPose::Hover);
	}

	// 点积越接近 -1，期望方向越接近反向。触发后锁定，面朝新方向或松开输入再解锁。
	const FVector DesiredDirection = Acceleration.GetSafeNormal2D();
	const float FacingDot = FVector::DotProduct(Character->GetActorForwardVector(), DesiredDirection);
	if (FacingDot > 0.7f || !bHasMovementInput)
	{
		bTurnLatched = false;
	}
	if (bHasMovementInput && bHadMovementInput && !bTurnLatched && !DesiredDirection.IsNearlyZero() && FacingDot < -0.3f)
	{
		EnterPose(EOneCloudFlightPose::Turn, TurnAnimation);
		bTurnLatched = true;
	}
	bHadMovementInput = bHasMovementInput;
}

void UOneCloudFairyAnimInstance::EnterPose(EOneCloudFlightPose Pose, const UAnimSequence* Animation)
{
	FlightPose = Pose;
	PoseTimeRemaining = Animation ? Animation->GetPlayLength() : 0.f;
}
