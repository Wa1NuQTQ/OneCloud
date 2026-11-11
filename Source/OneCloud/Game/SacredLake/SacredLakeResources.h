#pragma once

#include "CoreMinimal.h"

// 纯数值账本，单位为水量点；湖水和云水之间的转移始终等量，不从碰撞回调凭空回水。
struct FSacredLakeResources
{
	// 湖盆对应最高水位的储量。种树可以解锁容量，但不能超过这个上限。
	static constexpr float PhysicalCapacity = 130.f;
	// 种植与补云共同保留的湖水。敌人抽水仍可使湖水降到零。
	static constexpr float LakeReserve = 10.f;
	static constexpr float PlantingCost = 5.f;
	static constexpr int32 MaxTreeLevel = 3;
	static constexpr int32 MaxPlantingPlots = 6;

	void Initialize(float InitialLakeWater, bool bForest)
	{
		LakeCapacity = bForest ? 100.f : PhysicalCapacity;
		CloudCapacity = 45.f;
		LakeWater = FMath::Clamp(InitialLakeWater, 0.f, LakeCapacity);
		CloudWater = bForest ? 35.f : 0.f;
		Seeds = bForest ? 1 : 0;
		Growth = MatureTrees = 0;
		LakeRecoveryPerSecond = 0.35f;
		NaturalCloudTransferPerSecond = 0.30f;
		RainCostPerSecond = 1.f;
		LastNaturalTransfer = LastEmergencyTransfer = 0.f;
	}

	// 每次从基础重算。容量提升只扩大容器，不会自动填满水。
	void RefreshGrowth(int32 NewGrowth, int32 TreeCount, int32 CloudBlessings, int32 RecoveryBlessings)
	{
		Growth = NewGrowth;
		MatureTrees = TreeCount;
		LakeCapacity = FMath::Min(PhysicalCapacity, 100.f + 5.f * Growth);
		CloudCapacity = 45.f + 5.f * Growth + 15.f * CloudBlessings;
		LakeRecoveryPerSecond = (0.35f + 0.16f * Growth) * (1.f + 0.15f * RecoveryBlessings);
		NaturalCloudTransferPerSecond = 0.30f + 0.09f * Growth;
		RainCostPerSecond = 1.f + 0.04f * Growth;
	}

	// 关卡先检查树位并生成树，成功后才通过 Plant 支付。
	bool CanPlant() const
	{
		return Seeds > 0 && LakeWater >= LakeReserve + PlantingCost;
	}

	void Plant()
	{
		LakeWater -= PlantingCost;
		--Seeds;
	}

	bool CanRefillCloud() const
	{
		return LakeWater > LakeReserve && CloudWater < CloudCapacity;
	}

	// 结算一段状态稳定的世界时间。恢复增加湖水，转移把湖水等量交给云，降水消耗云水。
	void Advance(float Seconds, bool bRaining, bool bRefilling)
	{
		const float Recovery = Seconds * LakeRecoveryPerSecond;
		const float RainCost = bRaining ? RainCostPerSecond * Seconds : 0.f;
		const float AvailableLake = LakeWater + Recovery;
		const float CloudSpace = FMath::Max(0.f, CloudCapacity - CloudWater + RainCost);
		const float NaturalReserve = FMath::Max(30.f, 0.30f * LakeCapacity);
		const float NaturalTargetSpace = FMath::Max(0.f, 0.70f * CloudCapacity - CloudWater + RainCost);
		LastNaturalTransfer = FMath::Max(0.f, FMath::Min3(NaturalCloudTransferPerSecond * Seconds,
			AvailableLake - NaturalReserve, NaturalTargetSpace));
		LastEmergencyTransfer = bRefilling ? FMath::Max(0.f, FMath::Min3(4.f * Seconds,
			AvailableLake - LastNaturalTransfer - LakeReserve, CloudSpace - LastNaturalTransfer)) : 0.f;
		const float Transfer = LastNaturalTransfer + LastEmergencyTransfer;
		LakeWater = FMath::Clamp(AvailableLake - Transfer, 0.f, LakeCapacity);
		CloudWater = FMath::Clamp(CloudWater + Transfer - RainCost, 0.f, CloudCapacity);
	}

	// 主动技能一次性扣费，失败时保留原余额。
	bool SpendCloud(float Amount)
	{
		if (!FMath::IsFinite(Amount) || Amount <= 0.f || CloudWater < Amount)
		{
			return false;
		}
		CloudWater -= Amount;
		return true;
	}

	void Extract(float Amount)
	{
		LakeWater = FMath::Max(0.f, LakeWater - Amount);
	}

	float LakeWater = 0.f;
	float LakeCapacity = PhysicalCapacity;
	float CloudWater = 0.f;
	float CloudCapacity = 45.f;
	float LakeRecoveryPerSecond = 0.35f;
	float NaturalCloudTransferPerSecond = 0.30f;
	float RainCostPerSecond = 1.f;
	float LastNaturalTransfer = 0.f;
	float LastEmergencyTransfer = 0.f;
	int32 Seeds = 0;
	int32 MatureTrees = 0;
	int32 Growth = 0;
};
