// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "BpexInputConfig.generated.h"

class UInputAction;

USTRUCT(BlueprintType)
struct FInputConfig
{
	GENERATED_BODY()

	UPROPERTY(EditDefaultsOnly, Category = Input)
	TObjectPtr<UInputAction> InputAction;

	UPROPERTY(EditDefaultsOnly, Category = Input)
	FGameplayTagContainer SkillTags;
};

/**
 * 
 */
UCLASS()
class BPEX_API UBpexInputConfig : public UDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditDefaultsOnly, Category = Input)
	TArray<FInputConfig> InputConfigs;
};
