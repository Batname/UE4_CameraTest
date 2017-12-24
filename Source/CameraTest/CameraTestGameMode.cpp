// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "CameraTestGameMode.h"
#include "CameraTestHUD.h"
#include "CameraTestCharacter.h"
#include "UObject/ConstructorHelpers.h"

ACameraTestGameMode::ACameraTestGameMode()
	: Super()
{
	// set default pawn class to our Blueprinted character
	static ConstructorHelpers::FClassFinder<APawn> PlayerPawnClassFinder(TEXT("/Game/FirstPersonCPP/Blueprints/FirstPersonCharacter"));
	DefaultPawnClass = PlayerPawnClassFinder.Class;

	// use our custom HUD class
	HUDClass = ACameraTestHUD::StaticClass();
}
