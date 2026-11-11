#include "OneCloudEnemy.h"

#include "AIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Engine/World.h"
#include "UI/Hud/OneCloudEnemyHealthWidget.h"
#include "GameFramework/CharacterMovementComponent.h"

namespace
{
	constexpr double HitReactionIntervalSeconds = 1.0;
	constexpr float CorpseHoldSeconds = 1.5f;
}

AOneCloudEnemy::AOneCloudEnemy()
{
	PrimaryActorTick.bCanEverTick = false;
	// 放在关卡里和运行时生成的怪物都交给 AIController，移动更新由引擎组件执行。
	AIControllerClass = AAIController::StaticClass();
	AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
	HealthBar = CreateDefaultSubobject<UWidgetComponent>(TEXT("HealthBar"));
	HealthBar->SetupAttachment(GetRootComponent());
	HealthBar->SetWidgetSpace(EWidgetSpace::Screen);
	HealthBar->SetDrawSize(FVector2D(90.f, 6.f));
	HealthBar->SetVisibility(false);
	HealthBar->SetCollisionEnabled(ECollisionEnabled::NoCollision);
}

void AOneCloudEnemy::BeginPlay()
{
	MaxHealth = FMath::IsFinite(MaxHealth) ? FMath::Max(MaxHealth, 1.f) : 100.f;
	CurrentHealth = MaxHealth;
	HealthBar->SetRelativeLocation(FVector(0.f, 0.f, GetCapsuleComponent()->GetUnscaledCapsuleHalfHeight() + 35.f));
	HealthBar->SetVisibility(false);
	// 雨滴的 Visibility 射线用移动胶囊命中，随后按敌人的护甲配置区分落点表面。
	GetCapsuleComponent()->SetCollisionResponseToChannel(ECC_Visibility, ECR_Block);
	// 屏幕外省去姿态图更新，但继续推进攻击蒙太奇，避免再次入镜时仍卡在攻击中。
	GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered;
	Super::BeginPlay();
	if (IsValid(MoveTarget))
	{
		if (!MoveToTarget(MoveTarget))
		{
			UE_LOG(LogTemp, Warning, TEXT("%s could not navigate to %s."), *GetName(), *GetNameSafe(MoveTarget));
		}
	}
}

void AOneCloudEnemy::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);
	// 监听路径跟随组件的完成通知，在 UnPossessed 中从同一组件解绑。
	if (AAIController* AI = Cast<AAIController>(NewController))
	{
		if (UPathFollowingComponent* PathFollowing = AI->GetPathFollowingComponent())
		{
			PathFollowing->OnRequestFinished.AddUObject(this, &AOneCloudEnemy::OnMoveFinished);
		}
	}
}

void AOneCloudEnemy::UnPossessed()
{
	ActiveTargetMoveId.Reset();
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		if (UPathFollowingComponent* PathFollowing = AI->GetPathFollowingComponent())
		{
			PathFollowing->OnRequestFinished.RemoveAll(this);
		}
	}
	Super::UnPossessed();
}

// 寻路从入口读到引擎请求，再顺着完成回调看抵达结果。
bool AOneCloudEnemy::MoveToTarget(AActor* Target)
{
	if (!IsValid(Target))
	{
		return false;
	}

	FAIMoveRequest Request(Target);
	return RequestMove(Request, Target);
}

bool AOneCloudEnemy::MoveToShore(const FVector& Destination)
{
	// 规则已经将岸边点投影到导航网格并验过干地，保留这个目标坐标。
	FAIMoveRequest Request(Destination);
	Request.SetProjectGoalLocation(false);
	return RequestMove(Request, nullptr);
}

void AOneCloudEnemy::CancelShoreMove()
{
	ActiveTargetMoveId.Reset();
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
}

bool AOneCloudEnemy::RequestMove(FAIMoveRequest& Request, AActor* Target)
{
	AAIController* AI = Cast<AAIController>(GetController());
	if (!IsAlive() || !AI)
	{
		return false;
	}

	Request.SetUsePathfinding(true);
	// 禁止部分路径，避免将走到断路边缘误认为已经抵达目标。
	Request.SetAllowPartialPath(false);
	Request.SetNavigationFilter(AI->GetDefaultNavigationFilterClass());
	Request.SetAcceptanceRadius(AcceptanceRadiusCm);
	Request.SetReachTestIncludesAgentRadius(false);
	Request.SetCanStrafe(false);

	// MoveTo 可能同步完成或中断上一条路径，提交前先废弃旧编号。
	ActiveTargetMoveId.Reset();
	const FPathFollowingRequestResult Move = AI->MoveTo(Request);
	if (Move.Code == EPathFollowingRequestResult::Failed)
	{
		return false;
	}

	MoveTarget = Target;
	if (Move.Code == EPathFollowingRequestResult::AlreadyAtGoal)
	{
		// 引擎在 MoveTo 返回前同步发送完成通知，直接处理这次抵达。
		NotifyMoveArrival();
		return true;
	}

	ActiveTargetMoveId = Move.MoveId.GetID();
	if (UAnimInstance* Animation = GetMesh()->GetAnimInstance())
	{
		if (AttackMontage && Animation->Montage_IsPlaying(AttackMontage))
		{
			Animation->Montage_Stop(AttackMontage->BlendOut.GetBlendTime(), AttackMontage);
		}
	}
	return true;
}

void AOneCloudEnemy::OnMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result)
{
	// AIController 可以收到多个来源的路径请求，只有当前编号属于这次移动。
	if (!ActiveTargetMoveId.IsSet() || ActiveTargetMoveId.GetValue() != RequestID.GetID())
	{
		return;
	}

	ActiveTargetMoveId.Reset();
	if (Result.IsSuccess())
	{
		NotifyMoveArrival();
	}
	else if (!IsValid(MoveTarget))
	{
		OnShoreMoveFailed.Broadcast();
	}
}

void AOneCloudEnemy::NotifyMoveArrival()
{
	// 旧地图给 Actor 目标并由敌人播攻击。森林图给世界坐标并通知圣湖规则进入取水流程。
	if (IsValid(MoveTarget))
	{
		TryAttackOnArrival();
	}
	else
	{
		OnShoreReached.Broadcast();
	}
}

void AOneCloudEnemy::TryAttackOnArrival()
{
	if (bAttackOnArrival && IsValid(MoveTarget))
	{
		TryPlayAttack(true);
	}
}

// 播放攻击动作。森林图的实际取水由圣湖规则结算。
bool AOneCloudEnemy::PlayAttack()
{
	return TryPlayAttack(false);
}

bool AOneCloudEnemy::TryPlayAttack(bool bAllowInterruptingHitReaction)
{
	if (!IsAlive() || !AttackMontage)
	{
		return false;
	}
	UAnimInstance* Animation = GetMesh()->GetAnimInstance();
	if (!Animation)
	{
		return false;
	}

	UAnimMontage* HitReactionToInterrupt = nullptr;
	if (Animation->IsAnyMontagePlaying())
	{
		if (!bAllowInterruptingHitReaction || !HitMontage || HitMontage == AttackMontage)
		{
			return false;
		}
		// 抵达只触发一次。允许覆盖受击及其淡出阶段，保留其他蒙太奇的播放限制。
		for (const FAnimMontageInstance* Instance : Animation->MontageInstances)
		{
			if (Instance && Instance->Montage != HitMontage)
			{
				return false;
			}
		}
		HitReactionToInterrupt = HitMontage;
	}

	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
	GetCharacterMovement()->StopMovementImmediately();
	const bool bAttackStarted = Animation->Montage_Play(AttackMontage) > 0.f;
	// 同组动画会在 Montage_Play 中自动混合。异组受击仍活跃时单独淡出。
	if (bAttackStarted && HitReactionToInterrupt && Animation->Montage_IsActive(HitReactionToInterrupt))
	{
		Animation->Montage_Stop(HitReactionToInterrupt->BlendOut.GetBlendTime(), HitReactionToInterrupt);
	}
	return bAttackStarted;
}

// 生命结算和表现。扣完生命值后进入受击或死亡分支。
float AOneCloudEnemy::TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
	AController* EventInstigator, AActor* DamageCauser)
{
	// 负数不作为治疗；零生命值不再接受伤害。
	if (!IsAlive() || !FMath::IsFinite(DamageAmount) || DamageAmount <= 0.f)
	{
		return 0.f;
	}
	const float AppliedDamage = Super::TakeDamage(
		FMath::Min(DamageAmount, CurrentHealth), DamageEvent, EventInstigator, DamageCauser);
	const float HealthLost = FMath::Clamp(AppliedDamage, 0.f, CurrentHealth);
	if (HealthLost <= 0.f)
	{
		return 0.f;
	}

	CurrentHealth -= HealthLost;
	if (IsAlive())
	{
		UpdateHealthBar();
		TryPlayHitReaction();
	}
	else
	{
		Die();
	}
	return HealthLost;
}

void AOneCloudEnemy::UpdateHealthBar()
{
	UUserWidget* Widget = HealthBar->GetUserWidgetObject();
	if (!Widget)
	{
		HealthBar->InitWidget();
		Widget = HealthBar->GetUserWidgetObject();
	}
	if (UOneCloudEnemyHealthWidget* HealthWidget = Cast<UOneCloudEnemyHealthWidget>(Widget))
	{
		HealthWidget->SetHealthFraction(CurrentHealth / MaxHealth);
		HealthBar->SetVisibility(true);
	}
}

void AOneCloudEnemy::TryPlayHitReaction()
{
	const double NowSeconds = GetWorld()->GetTimeSeconds();
	// 密集命中仍会扣血，但受击动作有独立的时间间隔，也会让出正在播放的蒙太奇。
	if (!HitMontage || NowSeconds < NextHitReactionTimeSeconds)
	{
		return;
	}

	UAnimInstance* Animation = GetMesh()->GetAnimInstance();
	if (Animation && !Animation->IsAnyMontagePlaying())
	{
		Animation->Montage_Play(HitMontage);
		NextHitReactionTimeSeconds = NowSeconds + HitReactionIntervalSeconds;
	}
}

void AOneCloudEnemy::Die()
{
	// 先废弃寻路请求，再停移动。停止过程中产生的完成回调不应让死亡单位进入取水。
	MoveTarget = nullptr;
	ActiveTargetMoveId.Reset();
	if (AAIController* AI = Cast<AAIController>(GetController()))
	{
		AI->StopMovement();
	}
	GetCharacterMovement()->DisableMovement();
	DetachFromControllerPendingDestroy();
	SetActorEnableCollision(false);
	HealthBar->SetVisibility(false);
	float DeathDuration = 0.f;
	if (UAnimInstance* Animation = GetMesh()->GetAnimInstance())
	{
		Animation->Montage_Stop(0.08f);
		if (DeathMontage)
		{
			DeathDuration = Animation->Montage_Play(DeathMontage);
		}
	}
	// 死亡蒙太奇保持末帧，展示片刻后由引擎生命周期定时销毁。
	SetLifeSpan(DeathDuration + CorpseHoldSeconds);
	OnDefeated.Broadcast();
}
