// Copyright Epic Games, Inc. All Rights Reserved.

#include "PrototypeUnrealGameMode.h"
#include "PrototypeUnrealCharacter.h"
#include "UObject/ConstructorHelpers.h"

APrototypeUnrealGameMode::APrototypeUnrealGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPerson/Blueprints/BP_FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

}
