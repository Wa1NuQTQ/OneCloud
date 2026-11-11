#pragma once

#include "CoreMinimal.h"
#include "ForestTypes.generated.h"

UENUM(BlueprintType)
enum class EForestBlessing : uint8
{
	SacredRain,
	CloudBasin,
	LivingSpring,
	DeepFrost,
	WinterGrip,
	BrittleIce,
	PiercingHail,
	LastingBreach,
	HailRhythm
};

UENUM(BlueprintType)
enum class EForestBlessingPath : uint8
{
	Rain,
	Snow,
	Hail
};

// 规则提供显示数据，UI 只负责呈现，不复制效果公式。
USTRUCT(BlueprintType)
struct FOneCloudBlessingChoice
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly)
	EForestBlessing Id = EForestBlessing::SacredRain;
	UPROPERTY(BlueprintReadOnly)
	EForestBlessingPath Path = EForestBlessingPath::Rain;
	UPROPERTY(BlueprintReadOnly)
	FText Name;
	UPROPERTY(BlueprintReadOnly)
	FText Description;
	UPROPERTY(BlueprintReadOnly)
	int32 CurrentStacks = 0;
	UPROPERTY(BlueprintReadOnly)
	int32 MaxStacks = 2;
};
