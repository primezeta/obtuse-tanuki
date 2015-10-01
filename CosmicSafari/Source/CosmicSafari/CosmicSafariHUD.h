// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once 
#include "GameFramework/HUD.h"
#include "CosmicSafariHUD.generated.h"

UCLASS()
class ACosmicSafariHUD : public AHUD
{
	GENERATED_BODY()

public:
	ACosmicSafariHUD();

	/** Primary draw call for the HUD */
	virtual void DrawHUD() override;

private:
	/** Crosshair asset pointer */
	class UTexture2D* CrosshairTex;

};

