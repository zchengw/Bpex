// Copyright Epic Games, Inc. All Rights Reserved.

#include "BpexGameMode.h"
#include "BpexCharacter.h"
#include "UObject/ConstructorHelpers.h"

ABpexGameMode::ABpexGameMode()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnBPClass(TEXT("/Game/ThirdPerson/Blueprints/BP_ThirdPersonCharacter"));
	if (PlayerPawnBPClass.Class != NULL)
	{
		DefaultPawnClass = PlayerPawnBPClass.Class;
	}
}
