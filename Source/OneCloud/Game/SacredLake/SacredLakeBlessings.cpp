#include "SacredLakePrototype.h"

#include "Engine/World.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Weather/OneCloudRain.h"

namespace
{
	constexpr int32 BlessingCount = 9;
	constexpr int32 MaximumBlessingStacks = 2;
	constexpr int32 MaximumBlessingSelections = 6;

	FOneCloudBlessingChoice DescribeBlessing(EForestBlessing Id, int32 Stacks)
	{
		FOneCloudBlessingChoice Choice;
		Choice.Id = Id;
		Choice.CurrentStacks = Stacks;
		Choice.MaxStacks = MaximumBlessingStacks;
		Choice.Path = static_cast<EForestBlessingPath>(static_cast<int32>(Id) / 3);
		switch (Id)
		{
		case EForestBlessing::SacredRain:
			Choice.Name = NSLOCTEXT("Blessing", "SacredRain", "圣雨");
			Choice.Description = NSLOCTEXT("Blessing", "SacredRainEffect", "雨对裸露肉身的伤害 +20%");
			break;
		case EForestBlessing::CloudBasin:
			Choice.Name = NSLOCTEXT("Blessing", "CloudBasin", "云海");
			Choice.Description = NSLOCTEXT("Blessing", "CloudBasinEffect", "云储水上限 +15，不立即补水");
			break;
		case EForestBlessing::LivingSpring:
			Choice.Name = NSLOCTEXT("Blessing", "LivingSpring", "活泉");
			Choice.Description = NSLOCTEXT("Blessing", "LivingSpringEffect", "圣湖自然恢复速度 +15%");
			break;
		case EForestBlessing::DeepFrost:
			Choice.Name = NSLOCTEXT("Blessing", "DeepFrost", "深寒");
			Choice.Description = NSLOCTEXT("Blessing", "DeepFrostEffect", "雪的寒冷积累速度 +25%");
			break;
		case EForestBlessing::WinterGrip:
			Choice.Name = NSLOCTEXT("Blessing", "WinterGrip", "冬缚");
			Choice.Description = NSLOCTEXT("Blessing", "WinterGripEffect", "满寒冷时的移速系数再降低 0.1，最低 0.45");
			break;
		case EForestBlessing::BrittleIce:
			Choice.Name = NSLOCTEXT("Blessing", "BrittleIce", "脆冰");
			Choice.Description = NSLOCTEXT("Blessing", "BrittleIceEffect", "冰雹对高寒冷敌人的伤害 +20%");
			break;
		case EForestBlessing::PiercingHail:
			Choice.Name = NSLOCTEXT("Blessing", "PiercingHail", "穿甲");
			Choice.Description = NSLOCTEXT("Blessing", "PiercingHailEffect", "冰雹对护甲的伤害 +20%");
			break;
		case EForestBlessing::LastingBreach:
			Choice.Name = NSLOCTEXT("Blessing", "LastingBreach", "裂隙");
			Choice.Description = NSLOCTEXT("Blessing", "LastingBreachEffect", "主动冰雹造成的护甲暴露延长 2 秒");
			break;
		case EForestBlessing::HailRhythm:
			Choice.Name = NSLOCTEXT("Blessing", "HailRhythm", "雹律");
			Choice.Description = NSLOCTEXT("Blessing", "HailRhythmEffect", "主动冰雹冷却时间 -15%");
			break;
		}
		return Choice;
	}
}

int32 ASacredLakePrototype::GetForestBlessingStacks(EForestBlessing Blessing) const
{
	const int32 Index = static_cast<int32>(Blessing);
	return Index >= 0 && Index < BlessingCount ? ForestBlessingStacks[Index] : 0;
}

float ASacredLakePrototype::GetForestHailCooldownMultiplier() const
{
	return 1.f - 0.15f * GetForestBlessingStacks(EForestBlessing::HailRhythm);
}

// 树的有效等级、湖云容量和天气加成只有此处重算，反复刷新不会复利叠加。
void ASacredLakePrototype::RefreshForestGrowth()
{
	int32 Growth = 0;
	int32 TreeCount = 0;
	for (const FForestPlot& Plot : ForestPlots)
	{
		Growth += Plot.Level;
		TreeCount += Plot.Level > 0 ? 1 : 0;
	}
	Resources.RefreshGrowth(Growth, TreeCount, GetForestBlessingStacks(EForestBlessing::CloudBasin),
		GetForestBlessingStacks(EForestBlessing::LivingSpring));
	if (SurfaceParameters)
	{
		GetWorld()->GetParameterCollectionInstance(SurfaceParameters)->SetScalarParameterValue(
			TEXT("CloudGrowth01"), static_cast<float>(Growth) / 18.f);
	}
	if (IsValid(Rain))
	{
		Rain->SetForestRainIntensity(1.f + 0.15f * Growth);
		Rain->SetForestRainBudget(Resources.RainCostPerSecond);
		Rain->SetForestWeatherModifiers(
			1.f + 0.20f * GetForestBlessingStacks(EForestBlessing::SacredRain),
			1.f + 0.25f * GetForestBlessingStacks(EForestBlessing::DeepFrost),
			FMath::Max(0.45f, 0.65f - 0.10f * GetForestBlessingStacks(EForestBlessing::WinterGrip)),
			1.f + 0.20f * GetForestBlessingStacks(EForestBlessing::BrittleIce),
			1.f + 0.20f * GetForestBlessingStacks(EForestBlessing::PiercingHail),
			10.f + 2.f * GetForestBlessingStacks(EForestBlessing::LastingBreach));
	}
	RefreshForestRainBudget();
}

void ASacredLakePrototype::AwardForestBlessing()
{
	if (ForestBlessingsEarned >= MaximumBlessingSelections) { return; }
	++ForestBlessingsEarned;
	++PendingBlessingRewards;
	// 同一组连续奖励共用一段轻音乐，选择下一张卡时不重新起播。
	if (PendingBlessingRewards == 1) { StartForestMusic(0.f, true); }
	if (PendingBlessingChoices.IsEmpty()) { GenerateForestBlessingChoices(); }
}

// 每个方向随机给一张未满层的卡；一个方向都满层后，从其余未满项补齐三个不同选项。
void ASacredLakePrototype::GenerateForestBlessingChoices()
{
	PendingBlessingChoices.Reset();
	if (PendingBlessingRewards <= 0) { return; }
	for (int32 Path = 0; Path < 3; ++Path)
	{
		TArray<int32> Available;
		for (int32 Index = Path * 3; Index < Path * 3 + 3; ++Index)
		{
			if (ForestBlessingStacks[Index] < MaximumBlessingStacks) { Available.Add(Index); }
		}
		if (!Available.IsEmpty())
		{
			const int32 Selected = Available[ForestSpawnRandom.RandRange(0, Available.Num() - 1)];
			PendingBlessingChoices.Add(DescribeBlessing(static_cast<EForestBlessing>(Selected), ForestBlessingStacks[Selected]));
		}
	}
	while (PendingBlessingChoices.Num() < 3)
	{
		TArray<int32> Available;
		for (int32 Index = 0; Index < BlessingCount; ++Index)
		{
			if (ForestBlessingStacks[Index] < MaximumBlessingStacks && !PendingBlessingChoices.ContainsByPredicate(
				[Index](const FOneCloudBlessingChoice& Choice) { return static_cast<int32>(Choice.Id) == Index; }))
			{
				Available.Add(Index);
			}
		}
		if (Available.IsEmpty()) { break; }
		const int32 Selected = Available[ForestSpawnRandom.RandRange(0, Available.Num() - 1)];
		PendingBlessingChoices.Add(DescribeBlessing(static_cast<EForestBlessing>(Selected), ForestBlessingStacks[Selected]));
	}
	OnBlessingChoiceChanged.Broadcast();
}

// 祝福界面暂停世界，所以此入口不使用 CanActInForest 的未暂停条件。
bool ASacredLakePrototype::ChooseForestBlessing(int32 ChoiceIndex)
{
	if (!bUseForestRules || Result != ESacredLakeResult::Defending || !PendingBlessingChoices.IsValidIndex(ChoiceIndex))
	{
		return false;
	}
	SettleForestResources(GetWorld()->GetTimeSeconds());
	const int32 Index = static_cast<int32>(PendingBlessingChoices[ChoiceIndex].Id);
	if (ForestBlessingStacks[Index] >= MaximumBlessingStacks) { return false; }
	++ForestBlessingStacks[Index];
	--PendingBlessingRewards;
	PendingBlessingChoices.Reset();
	RefreshForestGrowth();
	UpdateHud();
	if (PendingBlessingRewards > 0) { GenerateForestBlessingChoices(); }
	else
	{
		FadeForestMusic();
		OnBlessingChoiceChanged.Broadcast();
	}
	return true;
}
