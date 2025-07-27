// Fill out your copyright notice in the Description page of Project Settings.


#include "BpexPlayerState.h"
#include "BpexAbilitySystemComponent.h"
#include "BpexAttributeSet.h"

ABpexPlayerState::ABpexPlayerState()
{
	NetUpdateFrequency = 100.f;

	AbilitySystemComponent = CreateDefaultSubobject<UBpexAbilitySystemComponent>("AbilitySystemComponent");
	AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Mixed);

	AttributeSet = CreateDefaultSubobject<UBpexAttributeSet>("AttributeSet");
}

UAbilitySystemComponent* ABpexPlayerState::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

UBpexAttributeSet* ABpexPlayerState::GetAttributeSet() const
{
	return AttributeSet;
}
