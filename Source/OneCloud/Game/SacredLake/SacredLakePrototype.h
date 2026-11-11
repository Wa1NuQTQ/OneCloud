#pragma once

#include "CoreMinimal.h"
#include "ForestTypes.h"
#include "SacredLakeResources.h"
#include "GameFramework/Actor.h"
#include "SacredLakePrototype.generated.h"

class AOneCloudEnemy;
class AOneCloudRain;
class APlayerController;
class UAudioComponent;
class UEnhancedInputComponent;
class UEnhancedInputLocalPlayerSubsystem;
class UGameViewportClient;
class UInputAction;
class UInputMappingContext;
class UMaterialInterface;
class UMaterialParameterCollection;
class USacredLakeWidget;
class USceneComponent;
class USoundAttenuation;
class USoundBase;
class USplineMeshComponent;
class UStaticMesh;

UENUM(BlueprintType)
enum class ESacredLakeResult : uint8
{
	Defending,
	Won,
	Dry,
	Unavailable
};

// 结果只在规则完成清理后通知一次，控制器负责显示菜单而不参与胜负计算。
DECLARE_MULTICAST_DELEGATE_OneParam(FSacredLakeResultChanged, ESacredLakeResult);

UENUM(BlueprintType)
enum class EForestLakePhase : uint8
{
	Preparing,
	Assault,
	Rest,
	Finished
};

USTRUCT(BlueprintType)
struct FSacredLakeWaveSpawn
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Forest", meta = (ClampMin = "0"))
	int32 SpawnPointIndex = 0;

	UPROPERTY(EditAnywhere, Category = "Forest", meta = (ClampMin = "0", Units = "s"))
	float DelaySeconds = 0.f;

	UPROPERTY(EditAnywhere, Category = "Forest")
	TSubclassOf<AOneCloudEnemy> EnemyClass;
};

USTRUCT(BlueprintType)
struct FSacredLakeWave
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, Category = "Forest")
	TArray<FSacredLakeWaveSpawn> Spawns;
};

// 圣湖关卡规则。关卡和蓝图提供场景引用，C++ 持有湖水、敌人流程、施法和胜负。
UCLASS()
class ONECLOUD_API ASacredLakePrototype : public AActor
{
	GENERATED_BODY()

public:
	ASacredLakePrototype();

	FSacredLakeResultChanged OnResultChanged;
	FSimpleMulticastDelegate OnBlessingChoiceChanged;

	// 关卡水面由规则升降。材质参数让湿岸和其他材质同时取得当前世界水位。
	UPROPERTY(EditAnywhere, Category = "Lake")
	TObjectPtr<AActor> WaterSurface;

	UPROPERTY(EditAnywhere, Category = "Lake")
	TObjectPtr<UMaterialParameterCollection> SurfaceParameters;

	UPROPERTY(EditAnywhere, Category = "Lake", meta = (ClampMin = "0", ClampMax = "130"))
	float InitialWater = 60.f;

	UPROPERTY(EditAnywhere, Category = "Lake", meta = (Units = "cm"))
	float MaximumWaterZOffset = 120.f;

	UPROPERTY(EditAnywhere, Category = "Lake", meta = (Units = "cm"))
	float MinimumWaterZOffset = -140.f;

	// 相对开局水面原点的 X、Y 半宽（厘米），用于判断地面是否处于可淹没的范围。
	UPROPERTY(EditAnywhere, Category = "Lake", meta = (Units = "cm"))
	FVector2D WaterFootprint = FVector2D(4250.0, 3250.0);

	UPROPERTY(EditAnywhere, Category = "Lake|Enemy", meta = (Units = "cm", ClampMin = "3000"))
	float ShoreSearchRadius = 9000.f;

	UPROPERTY(EditAnywhere, Category = "Lake|Enemy")
	TSubclassOf<AOneCloudEnemy> EnemyClass;

	// 仅旧单怪原型使用；森林图按 ForestSpawnHalfExtentCm 的环带随机生成。
	UPROPERTY(EditInstanceOnly, Category = "Lake|Enemy")
	TArray<TObjectPtr<AActor>> SpawnPoints;

	UPROPERTY(EditAnywhere, Category = "Lake|Enemy", meta = (ClampMin = "0"))
	int32 SpawnSide = 0;

	UPROPERTY(EditAnywhere, Category = "Lake|Enemy", meta = (ClampMin = "0", Units = "s"))
	float SpawnDelaySeconds = 5.f;

	UPROPERTY(EditAnywhere, Category = "Lake|Enemy")
	TObjectPtr<UMaterialInterface> SiphonMaterial;

	UPROPERTY(EditAnywhere, Category = "Lake")
	TObjectPtr<AOneCloudRain> Rain;

	UPROPERTY(EditAnywhere, Category = "Lake")
	TSubclassOf<USacredLakeWidget> HudClass;

	UPROPERTY(EditAnywhere, Category = "Lake|Input")
	TObjectPtr<UInputMappingContext> InputMappingContext;

	UPROPERTY(EditAnywhere, Category = "Lake|Input")
	TObjectPtr<UInputAction> RainAction;

	UPROPERTY(EditAnywhere, Category = "Lake|Input")
	TObjectPtr<UInputAction> RetryAction;

	// 森林地图启用连续出怪与水循环，旧月夜原型仍使用单怪流程。
	UPROPERTY(EditAnywhere, Category = "Forest")
	bool bUseForestRules = false;

	// 可选的专用地形查询目标，避免树干和景物被识别成地面。
	UPROPERTY(EditInstanceOnly, Category = "Forest")
	TObjectPtr<AActor> GroundActor;

	UPROPERTY(EditAnywhere, Category = "Forest|Enemy")
	TSubclassOf<AOneCloudEnemy> ArmoredEnemyClass;

	// 旧地图实例保留的序列化字段，当前连续出怪使用下方的首领与精英设置。
	UPROPERTY()
	TArray<FSacredLakeWave> ForestWaves;

	UPROPERTY(EditAnywhere, Category = "Forest|Enemy")
	TSubclassOf<AOneCloudEnemy> BossEnemyClass;

	UPROPERTY(EditAnywhere, Category = "Forest|Enemy", meta = (ClampMin = "1", Units = "s"))
	float ForestFirstEliteDelaySeconds = 120.f;

	UPROPERTY(EditAnywhere, Category = "Forest|Enemy", meta = (ClampMin = "1", Units = "s"))
	float ForestBossDelaySeconds = 990.f;

	UPROPERTY(EditAnywhere, Category = "Forest|Enemy", meta = (ClampMin = "1"))
	int32 ForestOrdinaryEnemyLimit = 18;

	// 固定非零值可复现出怪方向。零值会在开局生成随机序列。
	UPROPERTY(EditAnywhere, Category = "Forest|Enemy")
	int32 ForestSpawnSeed = 0;

	// 以湖心为中心的出雾环带半轴；连续随机方向，不依赖旧原型的四个出生标记。
	UPROPERTY(EditAnywhere, Category = "Forest|Enemy", meta = (ClampMin = "1000", Units = "cm"))
	FVector2D ForestSpawnHalfExtentCm = FVector2D(6800.0, 6250.0);

	UPROPERTY(EditAnywhere, Category = "Forest|Audio")
	TObjectPtr<USoundBase> ForestInterludeMusic;

	UPROPERTY(EditAnywhere, Category = "Forest|Audio")
	TObjectPtr<USoundBase> ForestSiphonCue;

	UPROPERTY(EditAnywhere, Category = "Forest|Audio")
	TObjectPtr<USoundAttenuation> ForestSiphonCueAttenuation;

	// 旧地图序列化兼容；森林自由种植不再读取预设树位。
	UPROPERTY()
	TArray<TObjectPtr<AActor>> PlantingSpots;

	// 世界 XY，单位厘米。椭圆限制湖心岛范围，地面仍需高于最高水位。
	UPROPERTY(EditAnywhere, Category = "Forest|Planting", meta = (Units = "cm"))
	FVector2D PlantingIslandCenterXY = FVector2D(350.0, -650.0);

	UPROPERTY(EditAnywhere, Category = "Forest|Planting", meta = (Units = "cm"))
	FVector2D PlantingIslandHalfExtentCm = FVector2D(1100.0, 600.0);

	UPROPERTY(EditAnywhere, Category = "Forest|Planting")
	TSubclassOf<AActor> PlantedTreeClass;

	UPROPERTY(EditAnywhere, Category = "Forest|Planting")
	TObjectPtr<UStaticMesh> PlantedTreeMesh;

	UPROPERTY(EditAnywhere, Category = "Forest|Planting", meta = (ClampMin = "0", Units = "cm"))
	float PlantingRangeCm = 3500.f;

	UPROPERTY(EditInstanceOnly, Category = "Forest|Cloud")
	TObjectPtr<AActor> CloudRefillAnchor;

	UPROPERTY(EditAnywhere, Category = "Forest|Cloud", meta = (ClampMin = "1", Units = "cm"))
	float CloudRefillRadiusCm = 650.f;

	UPROPERTY(EditAnywhere, Category = "Forest|Input")
	TObjectPtr<UInputAction> PlantAction;

	UPROPERTY(EditAnywhere, Category = "Forest|Input")
	TObjectPtr<UInputAction> RefillAction;

	UPROPERTY(EditAnywhere, Category = "Forest|Input")
	TObjectPtr<UInputAction> FocusAction;

	UPROPERTY(EditAnywhere, Category = "Forest|Input")
	TObjectPtr<UInputAction> PrecipitationAction;

	UPROPERTY(EditAnywhere, Category = "Forest|Input")
	TObjectPtr<UInputAction> WaterColumnAction;

	UPROPERTY(EditAnywhere, Category = "Forest|Input")
	TObjectPtr<UInputAction> SnowAction;

	UPROPERTY(EditAnywhere, Category = "Forest|Input")
	TObjectPtr<UInputAction> HailAction;

	UPROPERTY(EditAnywhere, Category = "Forest|Input")
	TObjectPtr<UInputAction> MeteorAction;

	UPROPERTY(EditAnywhere, Category = "Forest|Input")
	TObjectPtr<UInputAction> SidestepAction;

	// HUD 和蓝图读取的运行状态。数值写入集中在规则与 FSacredLakeResources。
	UFUNCTION(BlueprintPure, Category = "Lake")
	float GetLakeWater() const { return Resources.LakeWater; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	float GetLakeCapacity() const { return Resources.LakeCapacity; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	float GetCloudWater() const { return Resources.CloudWater; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	float GetCloudCapacity() const { return Resources.CloudCapacity; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	int32 GetSeedCount() const { return Resources.Seeds; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	int32 GetMatureTreeCount() const { return Resources.MatureTrees; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	float GetForestGrowth() const { return static_cast<float>(Resources.Growth); }

	UFUNCTION(BlueprintPure, Category = "Forest")
	float GetForestRainWaterRate() const { return Resources.RainCostPerSecond; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	float GetNaturalCloudTransferRate() const { return Resources.NaturalCloudTransferPerSecond; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	int32 GetForestPlotLevel(int32 PlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Forest")
	bool GetForestPlotIsGrowing(int32 PlotIndex) const;

	UFUNCTION(BlueprintPure, Category = "Forest|Blessings")
	TArray<FOneCloudBlessingChoice> GetPendingBlessingChoices() const { return PendingBlessingChoices; }

	UFUNCTION(BlueprintPure, Category = "Forest|Blessings")
	bool HasPendingForestBlessing() const { return !PendingBlessingChoices.IsEmpty(); }

	UFUNCTION(BlueprintCallable, Category = "Forest|Blessings")
	bool ChooseForestBlessing(int32 ChoiceIndex);

	UFUNCTION(BlueprintPure, Category = "Forest|Blessings")
	int32 GetForestBlessingStacks(EForestBlessing Blessing) const;

	float GetForestHailCooldownMultiplier() const;

	// 请求与实际发射分开：云空时暂停，攒够五秒雨量后自动恢复玩家原来的开雨请求。
	UFUNCTION(BlueprintCallable, Category = "Forest")
	void SetForestRainRequested(bool bRequested);

	UFUNCTION(BlueprintPure, Category = "Forest")
	bool IsForestRainRequested() const { return bForestRainRequested; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	float GetForestPressure01() const { return ForestPressure01; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	EForestLakePhase GetForestPhase() const { return ForestPhase; }

	UFUNCTION(BlueprintPure, Category = "Forest")
	bool IsForestBossActive() const;

	// 指定地面位置种植；瞄准树根附近时只培育该树，不替换为另一个目标。
	UFUNCTION(BlueprintCallable, Category = "Forest")
	bool TryPlantAtLocation(FVector GroundTarget);

	UFUNCTION(BlueprintPure, Category = "Forest")
	bool CanPlantAtLocation(FVector GroundTarget, FVector& PlantGround) const;

	UFUNCTION(BlueprintPure, Category = "Forest")
	bool GetForestTreeGround(int32 TreeIndex, FVector& Ground) const;

	void CancelForestPlanting();
	bool IsForestPlanting() const { return bForestPlantingPreview; }

	// 旧蓝图入口兼容；现在读取准星落点，不再选择最近的预设树位。
	UFUNCTION(BlueprintCallable, Category = "Forest")
	bool TryPlantAtClosestPlot();

	// 记录补云按键是否按住，实际转移还取决于和补云点的距离及水量。
	UFUNCTION(BlueprintCallable, Category = "Forest")
	void SetRefillingCloud(bool bEnabled);

	UFUNCTION(BlueprintPure, Category = "Forest")
	bool IsNearCloudRefillAnchor() const;

	// 发起施法请求。动画提交时才检查剩余水量、扣费并产生效果。
	UFUNCTION(BlueprintCallable, Category = "Forest")
	bool RequestForestSkill(int32 SkillSlot);

	bool TrySpendCloudWater(float Amount);

	UFUNCTION(BlueprintPure, Category = "Lake")
	float GetWaterHeightCm() const { return WaterHeightCm; }

	UFUNCTION(BlueprintPure, Category = "Lake")
	ESacredLakeResult GetResult() const { return Result; }

	UFUNCTION(BlueprintPure, Category = "Lake")
	AOneCloudEnemy* GetExtractor() const { return Extractor; }

	UFUNCTION(BlueprintPure, Category = "Lake")
	bool IsExtracting() const { return bExtracting; }

	UFUNCTION(BlueprintCallable, Category = "Lake")
	void Retry();

protected:
	virtual void BeginPlay() override;
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
	// 按职责定位实现，先看关卡入口 SacredLakePrototype.cpp，再进入各专题文件。
	void BindPrototypeInput(APlayerController& Player);
	void RequestRetryFromInput();
	void BindForestInput(UEnhancedInputComponent& Input);
	void SpawnExtractor();
	void SeekShore();
	void ScheduleShoreRetry();
	void BeginExtraction();
	void ExtractWater();
	void StopExtraction();
	void UpdateWater();
	void UpdateSiphon(const FVector& WaterPoint);
	void UpdateHud();
	void ToggleRain();
	void OnExtractorDefeated();
	void Finish(ESacredLakeResult NewResult);
	bool TraceGround(const FVector& XY, FVector& Ground) const;
	bool IsWet(const FVector& Ground) const;
	bool FindNearbyWater(const FVector& Position, FVector& WaterPoint) const;
	bool FindShoreGoal(AOneCloudEnemy& Enemy, const FVector& Start, FVector& Goal) const;
	bool IsDryPath(const TArray<FVector>& Points) const;
	bool IsShoreOccupied(const AOneCloudEnemy& Enemy, const FVector& Position) const;
	AOneCloudEnemy* SpawnEnemy(const AActor& Marker, TSubclassOf<AOneCloudEnemy> Class);
	void PositionSiphon(USplineMeshComponent& Stream, const AOneCloudEnemy& Enemy, const FVector& WaterPoint);

	// 森林局的计时、资源和出怪由 SacredLakeForest.cpp 组织。
	void BeginForest();
	void EndForest();
	void UpdateForest();
	void SettleForestResources(double Now);
	void ForestRainEmissionChanged(bool bEmitting);
	void RefreshForestRainBudget();
	void ForestRainBudgetExpired();
	void UpdateCloudRefillProximity();
	void StopCloudRefillHold();
	void UpdateForestSpawns(double Now);
	void AwardForestSeed();
	void RefreshForestGrowth();
	void AwardForestBlessing();
	void GenerateForestBlessingChoices();
	void StartForestMusic(float StartTimeSeconds, bool bPlayWhenPaused = false);
	void FadeForestMusic();
	bool ResolveForestPlant(FVector GroundTarget, int32& PlotIndex, FVector& PlantGround, FText& BlockReason) const;
	void UpdateForestPlots(double Now);
	enum class EForestEnemyRank : uint8 { Ordinary, Elite, Boss };
	bool SpawnForestEnemy(EForestEnemyRank Rank);
	void UpdateForestEnemies(double Now);
	void SeekForestShore(AOneCloudEnemy& Enemy);
	void ForestShoreReached(AOneCloudEnemy* Enemy);
	void ForestShoreFailed(AOneCloudEnemy* Enemy);
	void ForestEnemyDefeated(AOneCloudEnemy* Enemy);
	bool CanInterruptForestExtractor(const AOneCloudEnemy* Enemy, bool bPushBack) const;
	void InterruptForestExtractor(AOneCloudEnemy* Enemy, bool bPushBack);
	bool CanActInForest() const;
	bool ResolveForestAim(FVector& Target) const;
	bool ResolvePlantingAim(FVector& Target) const;
	void ToggleForestFocus();
	void CycleForestPrecipitation();
	void PlantFromInput();
	void StartRefillFromInput();
	void StopRefillFromInput();
	void SidestepFromInput();
	void WaterColumnFromInput();
	void SnowFromInput();
	void HailFromInput();
	void MeteorFromInput();
	bool IsForestSkillUnlocked(int32 Slot) const;
	bool CanSpendOnForestSkill(int32 Slot) const;
	bool CanCommitForestSkill(int32 Slot, const FVector& Target) const;
	void CommitForestSkill(uint32 CastId);
	void CancelForestSkill(uint32 CastId);
	void UpdateForestSkillHud() const;
	void UpdateForestGuidance() const;
	void UpdateForestObjective() const;
	void ShowForestFeedback(const FText& Message, bool bWarning = true) const;
	FText GetForestSkillBlockReason(int32 Slot) const;

	// 六个运行时名额；Ground 是本局选定后保持不变的树根世界位置。
	struct FForestPlot
	{
		TWeakObjectPtr<AActor> Tree;
		FVector MatureScale = FVector::OneVector;
		FVector Ground = FVector::ZeroVector;
		FVector GrowthStartScale = FVector::OneVector;
		double MatureAtSeconds = 0.0;
		double GrowthStartedSeconds = 0.0;
		int32 Level = 0;
		bool bPlanted = false;
		bool bMature = false;
	};

	enum class EForestEnemyBehavior : uint8 { Flesh, Runner, Armored };

	// 一只敌人的寻岸、抽水与打断状态。弱引用只观察由世界或 Actor 管理的对象。
	struct FForestEnemy
	{
		TWeakObjectPtr<AOneCloudEnemy> Enemy;
		TWeakObjectPtr<USplineMeshComponent> Stream;
		EForestEnemyRank Rank = EForestEnemyRank::Ordinary;
		EForestEnemyBehavior Behavior = EForestEnemyBehavior::Flesh;
		FVector ReservedShore = FVector::ZeroVector;
		FVector WaterPoint = FVector::ZeroVector;
		FVector LastShoreCheckLocation = FVector::ZeroVector;
		float LastShoreCheckHeightCm = 0.f;
		// 世界绝对秒数，依状态表示下次寻岸或下次扣水。
		double NextActionAtSeconds = 0.0;
		double ProtectedUntilSeconds = 0.0;
		bool bHasReservedShore = false;
		bool bMoving = false;
		bool bRetreating = false;
		bool bSiphoning = false;
		bool bBossCharging = false;
	};

	FForestEnemy* FindForestEnemy(const AOneCloudEnemy* Enemy);
	void ReleaseForestShore(FForestEnemy& State);
	void UpdateForestExtraction(double Now);
	void RefreshForestEnemyState();
	bool RepositionForestBoss(FForestEnemy& State);

	UPROPERTY(VisibleAnywhere, Category = "Lake")
	TObjectPtr<USplineMeshComponent> Siphon;

	UPROPERTY(Transient)
	TObjectPtr<AOneCloudEnemy> Extractor;

	UPROPERTY(Transient)
	TObjectPtr<USacredLakeWidget> Hud;

	UPROPERTY(Transient)
	TObjectPtr<UAudioComponent> ForestMusic;

	TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> LocalInput;
	TWeakObjectPtr<UGameViewportClient> ForestViewport;
	bool bForestPreviousRefraction = false;
	TWeakObjectPtr<USceneComponent> CloudRefillPawnRoot;
	FVector WaterOrigin = FVector::ZeroVector;
	FSacredLakeResources Resources;
	float WaterHeightCm = 0.f;
	ESacredLakeResult Result = ESacredLakeResult::Defending;
	bool bExtracting = false;
	FTimerHandle SpawnTimer;
	FTimerHandle ShoreRetryTimer;
	FTimerHandle ExtractionTimer;
	FTimerHandle ForestTimer;
	FTimerHandle ForestGuidanceTimer;
	FTimerHandle ForestRainBudgetTimer;
	TArray<FForestPlot> ForestPlots;
	bool bForestPlantingPreview = false;
	TArray<FForestEnemy> ForestEnemies;
	TWeakObjectPtr<AOneCloudEnemy> ForestBoss;
	FRandomStream ForestSpawnRandom;
	EForestLakePhase ForestPhase = EForestLakePhase::Preparing;
	int32 OrdinaryKillsSinceSeed = 0;
	float ForestPressure01 = 0.f;
	double ForestLastUpdateSeconds = 0.0;
	double ForestStartedSeconds = 0.0;
	double ForestBossArrivesSeconds = 0.0;
	double NextForestOrdinarySpawnSeconds = 0.0;
	double NextForestEliteSpawnSeconds = 0.0;
	double NextForestBossSpawnSeconds = 0.0;
	double BossSpawnBlockedSinceSeconds = -1.0;
	double NextForestSiphonCueSeconds = 0.0;
	double ForestReliefEndsSeconds = 0.0;
	float NextBossRepositionHealthFraction = 2.f / 3.f;
	int32 ForestSpawnCount = 0;
	int32 ForestEliteSpawnCount = 0;
	// 按住是输入状态，范围内是资源结算状态。离开后仍可按住并返回仪式点。
	bool bCloudRefillHeld = false;
	bool bCloudRefillInRange = false;
	bool bForestRainEmitting = false;
	bool bForestRainRequested = false;
	bool bForestRainWaitingForWater = false;
	bool bHasStartedForestRain = false;
	// 引导由实际成功的行为推进，单纯按键、动画或失败尝试不会算作完成。
	bool bHasRefilledForestCloud = false;
	// 九项祝福、每项最多两层。奖励可排队，不覆盖当前尚未选择的三张卡。
	int32 ForestBlessingStacks[9] = {};
	int32 ForestBlessingsEarned = 0;
	int32 PendingBlessingRewards = 0;
	TArray<FOneCloudBlessingChoice> PendingBlessingChoices;

	// 技能槽与 SkillCosts 同序。施法编号用于过滤延迟到达的动画通知。
	double SkillReadyAtSeconds[4] = {};
	FVector LastFocusRequest = FVector::ZeroVector;
	FVector PendingSkillTarget = FVector::ZeroVector;
	uint32 ForestCastId = 0;
	int32 PendingSkillSlot = INDEX_NONE;
	bool bMeteorUsed = false;
};
