// Copyright Epic Games, Inc. All Rights Reserved.

#include "TestTaskUnrealGameMode.h"
#include "TestTaskUnrealCharacter.h"
#include "UObject/ConstructorHelpers.h"

ATestTaskUnrealGameMode::ATestTaskUnrealGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
