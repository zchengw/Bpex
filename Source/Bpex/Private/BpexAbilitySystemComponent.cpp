// Fill out your copyright notice in the Description page of Project Settings.


#include "BpexAbilitySystemComponent.h"

UBpexAbilitySystemComponent::UBpexAbilitySystemComponent()
{
	PrimaryComponentTick.bCanEverTick = true;
	SetIsReplicated(true);
}