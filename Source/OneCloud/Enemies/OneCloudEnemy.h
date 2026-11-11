#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "Misc/Optional.h"
#include "OneCloudEnemy.generated.h"

class UAnimMontage;
class UWidgetComponent;
struct FAIRequestID;
struct FPathFollowingResult;
struct FAIMoveRequest;

/** 执行寻路和动作，管理生命及天气状态。圣湖规则负责选岸、取水和胜负。 */
UCLASS()
class ONECLOUD_API AOneCloudEnemy : public ACharacter
{
	GENERATED_BODY()

public:
	AOneCloudEnemy();

	virtual float TakeDamage(float DamageAmount, const FDamageEvent& DamageEvent,
		AController* EventInstigator, AActor* DamageCauser) override;

	UFUNCTION(BlueprintPure, Category = "Health")
	bool IsAlive() const { return CurrentHealth > 0.f; }

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Rain", meta = (DisplayName = "Armored Rain Surface"))
	bool bArmoredRainSurface = false;

	bool IsArmoredRainSurface() const { return bArmoredRainSurface; }
	float GetWeatherRainDamageMultiplier() const;
	void BreakWeatherArmor(float DurationSeconds);
	void SetSnowSlowed(bool bSlowed);
	bool UpdatePrecipitationChill(float ChillChange);
	// 关卡生成变体后一次性设置绝对生命和移速，不修改共享蓝图默认值。
	void ConfigureForestVariant(float Health, float SpeedCmPerSecond, bool bArmored);
	float GetHealthFraction() const { return MaxHealth > 0.f ? CurrentHealth / MaxHealth : 0.f; }
	float GetCurrentHealth() const { return CurrentHealth; }
	float GetPrecipitationChill() const { return PrecipitationChill; }
	void SetWeatherMinimumSpeedMultiplier(float Multiplier);

	UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "Navigation")
	TObjectPtr<AActor> MoveTarget;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Navigation", meta = (ClampMin = "0", Units = "cm"))
	float AcceptanceRadiusCm = 80.f;

	// 返回是否接受了完整路径请求；抵达后的停止和转向由引擎处理。
	UFUNCTION(BlueprintCallable, Category = "Navigation")
	bool MoveToTarget(AActor* Target);

	/** 执行规则层选好的干岸目标，抵达或失败通过下方的委托通知规则。 */
	bool MoveToShore(const FVector& Destination);
	void CancelShoreMove();
	FSimpleMulticastDelegate OnShoreReached;
	FSimpleMulticastDelegate OnShoreMoveFailed;
	FSimpleMulticastDelegate OnDefeated;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Attack")
	TObjectPtr<UAnimMontage> AttackMontage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Attack")
	bool bAttackOnArrival = true;

	UFUNCTION(BlueprintCallable, Category = "Attack")
	bool PlayAttack();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Health")
	TObjectPtr<UWidgetComponent> HealthBar;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health")
	TObjectPtr<UAnimMontage> HitMontage;

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health")
	TObjectPtr<UAnimMontage> DeathMontage;

protected:
	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "Health", meta = (ClampMin = "1.0"))
	float MaxHealth = 100.f;

	UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "Health")
	float CurrentHealth = 0.f;

	virtual void BeginPlay() override;
	virtual void PossessedBy(AController* NewController) override;
	virtual void UnPossessed() override;

private:
	void OnMoveFinished(FAIRequestID RequestID, const FPathFollowingResult& Result);
	bool RequestMove(FAIMoveRequest& Request, AActor* Target);
	void NotifyMoveArrival();
	void TryAttackOnArrival();
	bool TryPlayAttack(bool bAllowInterruptingHitReaction);
	void UpdateHealthBar();
	void TryPlayHitReaction();
	void Die();
	void UpdateSnowSlow();

	// 只接受当前请求的完成通知，忽略撤销、换目标或其他系统的路径回调。
	TOptional<uint32> ActiveTargetMoveId;
	// 第一次减速时保存原速，最后一种减速结束后按原值恢复。
	TOptional<float> WalkSpeedBeforeSnow;
	bool bSnowScreenSlowed = false;
	float PrecipitationChill = 0.f;
	float WeatherMinimumSpeedMultiplier = 0.65f;
	double NextHitReactionTimeSeconds = 0.0;
	double WeatherArmorBrokenUntil = 0.0;
};
