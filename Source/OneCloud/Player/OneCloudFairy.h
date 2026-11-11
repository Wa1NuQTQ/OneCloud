#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "GameFramework/Character.h"
#include "OneCloudFairy.generated.h"

class UCameraComponent;
class UAnimInstance;
class UAnimMontage;
class UAudioComponent;
class USpringArmComponent;
class UInputAction;
class UInputMappingContext;
class USoundAttenuation;
class USoundBase;
struct FInputActionValue;

/** 本地可操控的飞行角色，负责输入、镜头与施法演出。湖水和技能结算由圣湖规则持有。 */
UCLASS()
class ONECLOUD_API AOneCloudFairy : public ACharacter
{
	GENERATED_BODY()
public:
	AOneCloudFairy();
	virtual void PawnClientRestart() override;
	virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

	/**
	 * 播放对应技能的蒙太奇。OnCommit 在释放帧提交规则，OnCanceled 清除未提交的请求。
	 * 返回 false 表示无法播放，调用方可走直接提交的流程。已有演出时在这里取消新请求。
	 */
	bool BeginSkillCast(int32 SkillSlot, FSimpleDelegate OnCommit, FSimpleDelegate OnCanceled);
	bool IsSkillCastAnimating() const { return CastMontage != nullptr; }
	void CancelSkillCast();
	bool IsSidestepReady() const;
	// HUD 读取世界时间下的剩余秒数，暂停时不会继续倒计时。
	float GetSidestepCooldownRemainingSeconds() const;
	bool TrySidestep();
	void SetWingMoving(bool bMoving);

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<USpringArmComponent> CameraArm;
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Camera")
	TObjectPtr<UCameraComponent> Camera;
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputMappingContext> MappingContext;
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> MoveAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> FlyAction;
	UPROPERTY(EditDefaultsOnly, Category = "Input")
	TObjectPtr<UInputAction> LookAction;

	// 数组下标依次对应水柱、雪幕、冰雹、星坠。释放帧用名为 SkillCommit 的 Montage Notify。
	UPROPERTY(EditAnywhere, Category = "Flight|Skills")
	TArray<TObjectPtr<UAnimMontage>> SkillMontages;

	UPROPERTY(EditAnywhere, Category = "Flight|Skills")
	TObjectPtr<UAnimMontage> SidestepMontage;

	UPROPERTY(EditAnywhere, Category = "Flight|Audio")
	TObjectPtr<USoundBase> WingLoopSound;

	UPROPERTY(EditAnywhere, Category = "Flight|Audio")
	TObjectPtr<USoundAttenuation> WingAttenuation;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	void Move(const FInputActionValue& Value);
	void Fly(const FInputActionValue& Value);
	void Look(const FInputActionValue& Value);
	void AdvanceSidestep();
	void StartWingAudio();

	UFUNCTION()
	void OnSkillNotify(FName NotifyName, const FBranchingPointNotifyPayload& Payload);

	void OnSkillMontageEnded(UAnimMontage* Montage, bool bInterrupted);
	void FinishSkillCast(bool bStopMontage);

	UPROPERTY(VisibleAnywhere, Category = "Flight|Audio")
	TObjectPtr<UAudioComponent> WingAudio;

	UPROPERTY(Transient)
	TObjectPtr<UAnimMontage> CastMontage;
	// 动画实例由网格拥有。记录本次播放的实例编号，防止接收其他蒙太奇的同名通知。
	TWeakObjectPtr<UAnimInstance> CastAnimInstance;
	int32 CastMontageInstanceId = INDEX_NONE;
	FSimpleDelegate PendingSkillCommit;
	FSimpleDelegate PendingSkillCancel;

	// 侧掠短时推进，累计距离以厘米计，时间戳来自游戏世界。
	FTimerHandle SidestepTimer;
	FVector SidestepDirection = FVector::ZeroVector;
	double SidestepStartSeconds = 0.0;
	double NextSidestepSeconds = 0.0;
	float SidestepDistanceMovedCm = 0.f;
};
