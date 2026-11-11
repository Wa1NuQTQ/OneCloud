#include "OneCloudFairy.h"

#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "TimerManager.h"

namespace
{
	const FName SkillCommitNotify(TEXT("SkillCommit"));
	constexpr float SidestepDistanceCm = 270.f;
	constexpr float SidestepDurationSeconds = 0.18f;
	constexpr float SidestepCooldownSeconds = 2.f;
}

// 施法演出的入口、释放通知与收尾。
bool AOneCloudFairy::BeginSkillCast(int32 SkillSlot, FSimpleDelegate OnCommit, FSimpleDelegate OnCanceled)
{
	if (IsSkillCastAnimating())
	{
		OnCanceled.ExecuteIfBound();
		return true;
	}

	UAnimMontage* Montage = SkillMontages.IsValidIndex(SkillSlot) ? SkillMontages[SkillSlot].Get() : nullptr;
	UAnimInstance* Anim = GetMesh()->GetAnimInstance();
	if (!Montage || !Anim || Anim->Montage_Play(Montage, 1.f, EMontagePlayReturnType::Duration, 0.f, false) <= 0.f)
	{
		return false;
	}

	// 同一个 Montage 资产可以重复播放。本次播放的实例 ID 才是通知的匹配依据。
	const FAnimMontageInstance* Instance = Anim->GetActiveInstanceForMontage(Montage);
	if (!Instance)
	{
		Anim->Montage_Stop(0.f, Montage);
		return false;
	}

	CastMontage = Montage;
	CastAnimInstance = Anim;
	CastMontageInstanceId = Instance->GetInstanceID();
	PendingSkillCommit = MoveTemp(OnCommit);
	PendingSkillCancel = MoveTemp(OnCanceled);
	Anim->OnPlayMontageNotifyBegin.AddUniqueDynamic(this, &AOneCloudFairy::OnSkillNotify);
	FOnMontageEnded Ended;
	Ended.BindUObject(this, &AOneCloudFairy::OnSkillMontageEnded);
	Anim->Montage_SetEndDelegate(Ended, Montage);
	return true;
}

void AOneCloudFairy::OnSkillNotify(FName NotifyName, const FBranchingPointNotifyPayload& Payload)
{
	if (NotifyName != SkillCommitNotify || Payload.MontageInstanceID != CastMontageInstanceId ||
		!PendingSkillCommit.IsBound())
	{
		return;
	}

	// 先取走提交回调并清掉取消回调。结算可能同步结束战斗，重入时也只会提交一次。
	FSimpleDelegate Commit = MoveTemp(PendingSkillCommit);
	PendingSkillCommit.Unbind();
	PendingSkillCancel.Unbind();
	Commit.ExecuteIfBound();
}

void AOneCloudFairy::OnSkillMontageEnded(UAnimMontage* Montage, bool /*bInterrupted*/)
{
	if (Montage == CastMontage)
	{
		FinishSkillCast(false);
	}
}

void AOneCloudFairy::CancelSkillCast()
{
	FinishSkillCast(true);
}

void AOneCloudFairy::FinishSkillCast(bool bStopMontage)
{
	// 停播可能触发结束回调。先释放本次状态，重入时不会重复取消。
	UAnimMontage* Montage = CastMontage.Get();
	UAnimInstance* Anim = CastAnimInstance.Get();
	CastMontage = nullptr;
	CastAnimInstance.Reset();
	CastMontageInstanceId = INDEX_NONE;
	PendingSkillCommit.Unbind();
	FSimpleDelegate Canceled = MoveTemp(PendingSkillCancel);
	PendingSkillCancel.Unbind();
	if (Anim)
	{
		Anim->OnPlayMontageNotifyBegin.RemoveDynamic(this, &AOneCloudFairy::OnSkillNotify);
		if (bStopMontage && Montage)
		{
			Anim->Montage_Stop(0.12f, Montage);
		}
	}
	Canceled.ExecuteIfBound();
}

// 侧掠不经过施法通知，直接启动短时移动。
bool AOneCloudFairy::TrySidestep()
{
	if (!IsSidestepReady())
	{
		return false;
	}

	// 优先沿当前移动输入侧掠，无输入时沿相机右侧。开始后保持这个方向直到结束。
	SidestepDirection = GetCharacterMovement()->GetCurrentAcceleration().GetSafeNormal2D();
	if (SidestepDirection.IsNearlyZero())
	{
		const FRotationMatrix CameraYaw(FRotator(0.f, GetControlRotation().Yaw, 0.f));
		SidestepDirection = CameraYaw.GetUnitAxis(EAxis::Y);
	}
	SidestepStartSeconds = GetWorld()->GetTimeSeconds();
	NextSidestepSeconds = SidestepStartSeconds + SidestepCooldownSeconds;
	SidestepDistanceMovedCm = 0.f;
	GetWorldTimerManager().SetTimer(SidestepTimer, this, &AOneCloudFairy::AdvanceSidestep, 1.f / 60.f, true);
	if (SidestepMontage)
	{
		PlayAnimMontage(SidestepMontage);
	}
	return true;
}

bool AOneCloudFairy::IsSidestepReady() const
{
	const UWorld* World = GetWorld();
	return World && GetCharacterMovement()->IsFlying() && !IsSkillCastAnimating() &&
		World->GetTimeSeconds() >= NextSidestepSeconds;
}

float AOneCloudFairy::GetSidestepCooldownRemainingSeconds() const
{
	const UWorld* World = GetWorld();
	return World ? static_cast<float>(FMath::Max(0.0, NextSidestepSeconds - World->GetTimeSeconds())) : 0.f;
}

void AOneCloudFairy::AdvanceSidestep()
{
	// 用已经经过的时间求累计距离，差值才是本次位移。Sweep 让角色在撞到阻挡时停下。
	const float Alpha = FMath::Clamp(static_cast<float>((GetWorld()->GetTimeSeconds() - SidestepStartSeconds) /
		SidestepDurationSeconds), 0.f, 1.f);
	const float DistanceCm = SidestepDistanceCm * FMath::InterpEaseOut(0.f, 1.f, Alpha, 1.7f);
	FHitResult Hit;
	AddActorWorldOffset(SidestepDirection * (DistanceCm - SidestepDistanceMovedCm), true, &Hit);
	SidestepDistanceMovedCm = DistanceCm;
	if (Hit.bBlockingHit || Alpha >= 1.f)
	{
		GetWorldTimerManager().ClearTimer(SidestepTimer);
	}
}
