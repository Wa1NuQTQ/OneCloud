#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "NiagaraDataInterfaceExport.h"
#include "TimerManager.h"
#include "OneCloudRain.generated.h"

class AOneCloudEnemy;
class APointLight;
class UInstancedStaticMeshComponent;
class UMaterialInterface;
class UNiagaraComponent;
class UNiagaraSystem;
class URainImpactEffects;
class USoundBase;
class USoundAttenuation;
class USoundWave;
class UStaticMesh;
struct FHitResult;
enum class ERainImpactSurface : uint8;

UENUM(BlueprintType)
enum class EOneCloudPrecipitation : uint8
{
	Rain,
	Snow,
	Hail
};

/** 从蓝图给同一次天气表现配置粒子和声音。技能结算由 C++ 负责。 */
USTRUCT(BlueprintType)
struct FWeatherPresentation
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TObjectPtr<UNiagaraSystem> Effect = nullptr;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	TObjectPtr<USoundBase> Sound = nullptr;

	// 粒子以厘米为单位制作，缩放只影响演出，不改变技能的伤害半径。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather")
	FVector EffectScale = FVector::OneVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (Units = "cm"))
	FVector EffectOffset = FVector::ZeroVector;

	// 对循环模板给出明确寿命，避免一次施法留下永久运行的粒子组件。
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.0", Units = "s"))
	float DurationSeconds = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Weather", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float SoundVolume = 0.6f;
};

DECLARE_MULTICAST_DELEGATE_TwoParams(FWeatherEnemyInterrupt, AOneCloudEnemy*, bool);
DECLARE_MULTICAST_DELEGATE_OneParam(FOnRainEmissionChanged, bool);

/** 管理天气发射、落点反馈、持续降水判定和主动天气技能。 */
UCLASS()
class ONECLOUD_API AOneCloudRain : public AActor, public INiagaraParticleCallbackHandler
{
	GENERATED_BODY()

public:
	AOneCloudRain();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rain")
	bool bStartRaining = true;

	/** 开关新粒子发射。已经在空中的降水仍有落地时间。 */
	UFUNCTION(BlueprintCallable, Category = "Rain")
	void SetRaining(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Rain")
	bool IsRaining() const { return bIsRaining; }

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void SetPrecipitation(EOneCloudPrecipitation Mode);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void CyclePrecipitation();

	UFUNCTION(BlueprintPure, Category = "Weather")
	EOneCloudPrecipitation GetPrecipitation() const { return Precipitation; }

	void SetForestRainIntensity(float Intensity);
	// 总水量只分配一次：全域基础雨与局部天气共享这一预算。
	void SetForestRainBudget(float WaterRate);
	void SetForestWeatherModifiers(float FleshRain, float ChillGain, float MinimumSpeed,
		float ChilledHail, float ArmoredHail, float ArmorBreakSeconds);
	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetBackgroundRainShare() const;
	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetLocalWeatherShare() const;
	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetWeatherArrivalProgress() const;
	EOneCloudPrecipitation GetArrivedPrecipitation() const { return ArrivedRain.Precipitation; }
	bool HasArrivedRainFocus() const { return ArrivedRain.bEmitting && ArrivedRain.bFocused; }
	FVector GetArrivedRainFocusLocation() const { return ArrivedRain.Focus; }
	float GetArrivedRainFocusRadiusCm() const { return ArrivedRain.FocusRadiusCm; }
	bool HasEffectiveRainHit() const { return bHasEffectiveRainHit; }
	bool HasEffectiveFocusHit() const { return bHasEffectiveFocusHit; }
	bool HasCounteredArmor() const { return bHasCounteredArmor; }
	bool HasWaterColumnTarget(FVector GroundTarget) const;
	TDelegate<bool(const AOneCloudEnemy*)> CanInterruptExtractor;

	UFUNCTION(BlueprintPure, Category = "Weather")
	float GetForestRainIntensity() const { return ForestRainIntensity; }

	UFUNCTION(BlueprintPure, Category = "Weather")
	bool CanTargetWeather(FVector GroundTarget) const;

	UFUNCTION(BlueprintCallable, Category = "Rain")
	bool SetRainFocus(FVector GroundTarget);

	// 预览和实际聚雨共用此计算。返回圈心与半径（厘米），失败时不改变聚雨状态。
	bool ResolveRainFocus(FVector GroundTarget, FVector& Focus, float& RadiusCm) const;

	UFUNCTION(BlueprintCallable, Category = "Rain")
	void ClearRainFocus();

	UFUNCTION(BlueprintPure, Category = "Rain")
	bool HasRainFocus() const { return bRainFocused; }

	UFUNCTION(BlueprintPure, Category = "Rain")
	FVector GetRainFocusLocation() const { return RainFocusLocation; }

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void CastWaterColumn(FVector GroundTarget);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void CastSnowScreen(FVector GroundTarget);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void CastHail(FVector GroundTarget);

	UFUNCTION(BlueprintCallable, Category = "Weather")
	void CastMeteor(FVector GroundTarget);

	// 对局结束和 Actor 退出都走这里，收回临时减速、计时器与场景表现。
	void CancelWeatherSkills();

	// 通知关卡规则打断取水。第二个参数为 true 时还要寻找可达的干地退路。
	FWeatherEnemyInterrupt OnWeatherInterrupt;
	TDelegate<bool(const AOneCloudEnemy*)> CanPushBackExtractor;
	FOnRainEmissionChanged OnRainEmissionChanged;

	// 主降水组件提供雨的资产和发射位置。雪和冰雹按实例配置创建子组件。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rain")
	TObjectPtr<UNiagaraComponent> NiagaraComponent;

	// 处理有数量预算的近景特效。声音与战斗结算仍由本 Actor 负责。
	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Rain")
	TObjectPtr<URainImpactEffects> ImpactEffects;

	// 雨滴落在不同材质表面时的音效
	UPROPERTY(EditAnywhere, Category = "Rain|Audio")
	TObjectPtr<USoundBase> SandImpactSound;
	UPROPERTY(EditAnywhere, Category = "Rain|Audio")
	TObjectPtr<USoundBase> WaterImpactSound;
	UPROPERTY(EditAnywhere, Category = "Rain|Audio")
	TObjectPtr<USoundBase> LeafImpactSound;
	UPROPERTY(EditAnywhere, Category = "Rain|Audio")
	TObjectPtr<USoundBase> MetalImpactSound;
	UPROPERTY(EditAnywhere, Category = "Rain|Audio")
	TObjectPtr<USoundBase> SoilImpactSound;
	UPROPERTY(EditAnywhere, Category = "Rain|Audio")
	TObjectPtr<USoundBase> FleshImpactSound;
	// 索引沿用 ERainImpactSurface；每项是该表面独立制作的随机单次接触声。
	UPROPERTY(EditAnywhere, Category = "Rain|Audio")
	TArray<TObjectPtr<USoundBase>> SnowImpactSounds;
	UPROPERTY(EditAnywhere, Category = "Rain|Audio")
	TArray<TObjectPtr<USoundBase>> HailImpactSounds;

	// 落点声的空间衰减资产。听感随监听器到落点的距离变化。
	UPROPERTY(EditAnywhere, Category = "Rain|Audio")
	TObjectPtr<USoundAttenuation> ImpactAttenuation;

	// 落点声的整体音量倍数。雪还会在回调中进一步降低音量。
	UPROPERTY(EditAnywhere, Category = "Rain|Audio", meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ImpactVolume = 1.0f;

	// 旧地图的逐滴伤害，命中时使用实际射线结果。
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Rain|Damage", meta = (ClampMin = "0.0"))
	float DamagePerDrop = 0.1f;

	// 森林地图按覆盖和落地时间结算伤害，独立于 Niagara 回读和相机。
	UPROPERTY(EditAnywhere, Category = "Rain|Damage")
	bool bUseWeatherRainDamage = false;

	UPROPERTY(EditAnywhere, Category = "Rain|Damage", meta = (ClampMin = "0.0"))
	float WeatherRainDamagePerSecond = 3.0f;

	// 从发射状态改变到地面战斗结算生效的延迟。
	UPROPERTY(EditAnywhere, Category = "Rain|Damage", meta = (ClampMin = "0.0", Units = "s"))
	float RainFlightSeconds = 7.0f;

	// 以云的 X/Y 为中心，向每侧延伸的矩形半宽。
	UPROPERTY(EditAnywhere, Category = "Rain|Coverage", meta = (Units = "cm"))
	FVector2D RainCoverageHalfExtentCm = FVector2D(5200.0, 4000.0);

	UPROPERTY(EditAnywhere, Category = "Rain|Coverage", meta = (ClampMin = "100.0", Units = "cm"))
	float RainFocusRadiusCm = 1500.f;

	UPROPERTY(EditInstanceOnly, Category = "Weather|Precipitation")
	TObjectPtr<UNiagaraSystem> SnowPrecipitationSystem;

	UPROPERTY(EditInstanceOnly, Category = "Weather|Precipitation")
	TObjectPtr<UNiagaraSystem> HailPrecipitationSystem;

	UPROPERTY(EditAnywhere, Category = "Weather|Presentation")
	FWeatherPresentation WaterColumnPresentation;

	UPROPERTY(EditAnywhere, Category = "Weather|Presentation")
	FWeatherPresentation SnowPresentation;

	UPROPERTY(EditAnywhere, Category = "Weather|Presentation")
	FWeatherPresentation HailPresentation;

	UPROPERTY(EditAnywhere, Category = "Weather|Presentation")
	FWeatherPresentation MeteorWarningPresentation;

	UPROPERTY(EditAnywhere, Category = "Weather|Presentation")
	FWeatherPresentation MeteorImpactPresentation;

	// 大范围技能需要比近距离单滴声音更远的传播距离。
	UPROPERTY(EditAnywhere, Category = "Weather|Presentation")
	TObjectPtr<USoundAttenuation> WeatherSkillAttenuation;

	UPROPERTY(EditAnywhere, Category = "Weather|Presentation")
	TObjectPtr<UMaterialInterface> WeatherMarkerMaterial;
	UPROPERTY(EditAnywhere, Category = "Weather|Presentation")
	TObjectPtr<UStaticMesh> WeatherMarkerRingMesh;

	// 场景预先布置的闪电补光灯。灯可移动并关闭阴影，运行时切换可见性。
	UPROPERTY(EditInstanceOnly, Category = "Weather|Storm")
	TObjectPtr<APointLight> LightningLight;

	UPROPERTY(EditAnywhere, Category = "Weather|Storm")
	TObjectPtr<USoundBase> ThunderSound;

	UPROPERTY(EditAnywhere, Category = "Weather|Storm")
	TObjectPtr<USoundAttenuation> ThunderAttenuation;

	// Niagara 将候选落点、来源系统和大世界坐标偏移传回游戏线程。
	virtual void ReceiveParticleData_Implementation(const TArray<FBasicParticleData>& Data,
		UNiagaraSystem* NiagaraSystem, const FVector& SimulationPositionOffset) override;

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 保存一次发射状态。ArrivesAt 是该状态在地面开始生效的世界时间。
	struct FRainPhase
	{
		double ArrivesAt = 0.0;
		FVector Focus = FVector::ZeroVector;
		EOneCloudPrecipitation Precipitation = EOneCloudPrecipitation::Rain;
		float Intensity = 1.f;
		float FocusRadiusCm = 0.f;
		float LocalShare = 0.f;
		bool bEmitting = false;
		bool bFocused = false;
	};

	USoundBase* SelectImpactSound(ERainImpactSurface Surface);
	USoundBase* SelectWeatherImpactSound(ERainImpactSurface Surface, bool bSnow, bool bHail);
	void CacheSandSounds();
	USoundBase* SelectSandSound();
	UNiagaraComponent* CreatePrecipitationComponent(UNiagaraSystem* System);
	void UpdateNiagaraCoverage();
	void UpdatePrecipitationEmission();
	float CalculateLocalShare(float RadiusCm) const;
	void QueueRainPhase();
	void ResolveWeatherRain();
	void ClearPrecipitationSnow();
	void ScheduleLightning();
	void FlashLightning();
	void HideLightning();
	void PlayThunder(FVector Position);
	void UpdateSnowScreen();
	void EndSnowScreen();
	void ResolveHailImpact(FVector GroundTarget);
	void ResolveMeteorImpact(FVector GroundTarget);
	UNiagaraComponent* PlayWeatherPresentation(const FWeatherPresentation& Presentation, FVector Position);
	UInstancedStaticMeshComponent* ShowWeatherMarker(FVector Center, float RadiusCm, FLinearColor Color,
		float LifetimeSeconds);

	UPROPERTY(Transient)
	TArray<TObjectPtr<USoundWave>> CachedSandSounds;
	UPROPERTY(Transient)
	TObjectPtr<USoundBase> CachedSandPreset;
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> SnowPrecipitationComponent;
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> HailPrecipitationComponent;
	UPROPERTY(Transient)
	TObjectPtr<UNiagaraComponent> LocalRainComponent;
	TArray<int32> AvailableSandIndices;
	int32 LastSandIndex = INDEX_NONE;
	bool bIsRaining = false;
	bool bRainFocused = false;
	EOneCloudPrecipitation Precipitation = EOneCloudPrecipitation::Rain;
	float ForestRainIntensity = 1.f;
	float ForestWaterRate = 1.f;
	float FleshRainMultiplier = 1.f;
	float ChillGainMultiplier = 1.f;
	float MinimumSnowSpeed = 0.65f;
	float ChilledHailMultiplier = 1.f;
	float ArmoredHailMultiplier = 1.f;
	float ArmorExposureSeconds = 10.f;
	bool bHasEffectiveRainHit = false;
	bool bHasEffectiveFocusHit = false;
	bool bHasCounteredArmor = false;
	FVector RainFocusLocation = FVector::ZeroVector;
	// 地面当前采用的状态，以及正在飞行、尚未抵达的后续变化。
	FRainPhase ArrivedRain;
	TArray<FRainPhase> PendingRain;
	bool bHasChilledEnemies = false;
	double LastRainUpdate = 0.0;
	FTimerHandle WeatherRainTimer;
	FTimerHandle LightningTimer;
	FTimerHandle LightningEndTimer;
	FTimerHandle ThunderTimer;
	FTimerHandle SnowUpdateTimer;
	FTimerHandle SnowEndTimer;
	FTimerHandle MeteorTimer;
	TArray<FTimerHandle> HailTimers;
	// 技能只观察敌人和临时组件的生命周期，敌人销毁后弱引用会失效。
	TSet<TWeakObjectPtr<AOneCloudEnemy>> SlowedEnemies;
	TSet<TWeakObjectPtr<AOneCloudEnemy>> HailHits;
	TArray<TWeakObjectPtr<UNiagaraComponent>> WeatherEffects;
	TArray<TWeakObjectPtr<UInstancedStaticMeshComponent>> WeatherMarkers;
	FVector SnowCenter = FVector::ZeroVector;
	TWeakObjectPtr<UNiagaraComponent> SnowEffect;

	UPROPERTY()
	TObjectPtr<UStaticMesh> WeatherMarkerMesh;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> SnowMarker;

	UPROPERTY(Transient)
	TObjectPtr<UInstancedStaticMeshComponent> MeteorMarker;
};
