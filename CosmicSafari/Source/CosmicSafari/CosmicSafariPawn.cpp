// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CosmicSafari.h"
#include "CosmicSafariPawn.h"

ACosmicSafariPawn::ACosmicSafariPawn()
{
	// Create a spring arm component
	SpringArm = CreateDefaultSubobject<USpringArmComponent>(TEXT("SpringArm0"));
	SpringArm->AttachTo(RootComponent);
	SpringArm->TargetArmLength = 100.0f; // The camera follows at this distance behind the character	
	SpringArm->SocketOffset = FVector(0.0f,0.0f,0.0f);
	SpringArm->bEnableCameraLag = true;
	SpringArm->CameraLagSpeed = 0.0f;
	SpringArm->bUsePawnControlRotation = true;

	// Create camera component 
	Camera = CreateDefaultSubobject<UCameraComponent>(TEXT("Camera0"));
	Camera->AttachTo(SpringArm, USpringArmComponent::SocketName);
	Camera->bUsePawnControlRotation = false; // Don't rotate camera with controller

	MoveSpeed = 2000.0f;
}

void ACosmicSafariPawn::Tick(float DeltaSeconds)
{	
	AddActorLocalOffset(FVector(Movement.X * DeltaSeconds, Movement.Y * DeltaSeconds, Movement.Z * DeltaSeconds), true);
	Super::Tick(DeltaSeconds);
}

void ACosmicSafariPawn::NotifyHit(class UPrimitiveComponent* MyComp, class AActor* Other, class UPrimitiveComponent* OtherComp, bool bSelfMoved, FVector HitLocation, FVector HitNormal, FVector NormalImpulse, const FHitResult& Hit)
{
	Super::NotifyHit(MyComp, Other, OtherComp, bSelfMoved, HitLocation, HitNormal, NormalImpulse, Hit);
	Movement = FVector::ZeroVector;
}


void ACosmicSafariPawn::SetupPlayerInputComponent(class UInputComponent* InputComponent)
{
	check(InputComponent);

	// Bind our control axis' to callback functions
	InputComponent->BindAxis("MouseX", this, &ACosmicSafariPawn::MouseXInput);
	InputComponent->BindAxis("MouseY", this, &ACosmicSafariPawn::MouseYInput);
	InputComponent->BindAxis("Forward", this, &ACosmicSafariPawn::ForwardInput);
	InputComponent->BindAxis("Backward", this, &ACosmicSafariPawn::BackwardInput);
	InputComponent->BindAxis("Left", this, &ACosmicSafariPawn::LeftInput);
	InputComponent->BindAxis("Right", this, &ACosmicSafariPawn::RightInput);
	InputComponent->BindAxis("Up", this, &ACosmicSafariPawn::UpInput);
	InputComponent->BindAxis("Down", this, &ACosmicSafariPawn::DownInput);
}

void ACosmicSafariPawn::MouseXInput(float Val)
{
	AddControllerYawInput(Val);
}

void ACosmicSafariPawn::MouseYInput(float Val)
{
	AddControllerPitchInput(-Val);
}

void ACosmicSafariPawn::ForwardInput(float Val)
{
	static float PrevVal = 0.0f;
	if (!FMath::IsNearlyEqual(Val, PrevVal))
	{
		Movement.X = !FMath::IsNearlyEqual(Val, 0.f) ? MoveSpeed : 0.0f;
		PrevVal = Val;
	}
}

void ACosmicSafariPawn::BackwardInput(float Val)
{
	static float PrevVal = 0.0f;
	if (!FMath::IsNearlyEqual(Val, PrevVal))
	{
		Movement.X = !FMath::IsNearlyEqual(Val, 0.f) ? -MoveSpeed : 0.0f;
		PrevVal = Val;
	}
}

void ACosmicSafariPawn::LeftInput(float Val)
{
	static float PrevVal = 0.0f;
	if (!FMath::IsNearlyEqual(Val, PrevVal))
	{
		Movement.Y = !FMath::IsNearlyEqual(Val, 0.f) ? -MoveSpeed : 0.0f;
		PrevVal = Val;
	}
}

void ACosmicSafariPawn::RightInput(float Val)
{
	static float PrevVal = 0.0f;
	if (!FMath::IsNearlyEqual(Val, PrevVal))
	{
		Movement.Y = !FMath::IsNearlyEqual(Val, 0.f) ? MoveSpeed : 0.0f;
		PrevVal = Val;
	}
}

void ACosmicSafariPawn::UpInput(float Val)
{
	static float PrevVal = 0.0f;
	if (!FMath::IsNearlyEqual(Val, PrevVal))
	{
		Movement.Z = !FMath::IsNearlyEqual(Val, 0.f) ? MoveSpeed : 0.0f;
		PrevVal = Val;
	}
}


void ACosmicSafariPawn::DownInput(float Val)
{
	static float PrevVal = 0.0f;
	if (!FMath::IsNearlyEqual(Val, PrevVal))
	{
		Movement.Z = !FMath::IsNearlyEqual(Val, 0.f) ? -MoveSpeed : 0.0f;
		PrevVal = Val;
	}
}