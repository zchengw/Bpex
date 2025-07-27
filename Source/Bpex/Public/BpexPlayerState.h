// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "AbilitySystemInterface.h"
#include "BpexPlayerState.generated.h"

class UBpexAbilitySystemComponent;
class UBpexAttributeSet;

/**
 * 
 */
UCLASS()
class BPEX_API ABpexPlayerState : public APlayerState, public IAbilitySystemInterface
{
	GENERATED_BODY()

public:
	ABpexPlayerState();

	/* IAbilitySystemInterface */
	virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
	
	virtual UBpexAttributeSet* GetAttributeSet() const;

protected:
	UPROPERTY()
	TObjectPtr<UBpexAbilitySystemComponent> AbilitySystemComponent;

	UPROPERTY()
	TObjectPtr<UBpexAttributeSet> AttributeSet;
	
};
