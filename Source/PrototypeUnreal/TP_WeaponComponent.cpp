// Copyright Epic Games, Inc. All Rights Reserved.


#include "TP_WeaponComponent.h"
#include "PrototypeUnrealCharacter.h"
#include "PrototypeUnrealProjectile.h"
#include "GameFramework/PlayerController.h"
#include "Camera/PlayerCameraManager.h"
#include "Camera/CameraComponent.h"
#include "Kismet/GameplayStatics.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "Animation/AnimInstance.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"

// Sets default values for this component's properties
UTP_WeaponComponent::UTP_WeaponComponent()
{
	// Default offset from the character location for projectiles to spawn
	MuzzleOffset = FVector(100.0f, 0.0f, 10.0f);
}

void UTP_WeaponComponent::Fire()
{
	if (Character == nullptr || Character->GetController() == nullptr)
	{
		return;
	}

	// Try and fire a projectile
	if (ProjectileClass != nullptr)
	{
		UWorld* const World = GetWorld();
		if (World != nullptr)
		{
			APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());
			const FRotator SpawnRotation = PlayerController->PlayerCameraManager->GetCameraRotation();
			// MuzzleOffset is in camera space, so transform it to world space before offsetting from the character location to find the final muzzle position
			const FVector SpawnLocation = GetOwner()->GetActorLocation() + SpawnRotation.RotateVector(MuzzleOffset);
	
			//Set Spawn Collision Handling Override
			FActorSpawnParameters ActorSpawnParams;
			ActorSpawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButDontSpawnIfColliding;
	
			// Spawn the projectile at the muzzle
			auto projectile = World->SpawnActor<APrototypeUnrealProjectile>(ProjectileClass, SpawnLocation, SpawnRotation, ActorSpawnParams);

			if (projectile) {
				projectile->Owner = this;
			}
		}
	}
	
	// Try and play the sound if specified
	if (FireSound != nullptr)
	{
		UGameplayStatics::PlaySoundAtLocation(this, FireSound, Character->GetActorLocation());
	}
	
	// Try and play a firing animation if specified
	if (FireAnimation != nullptr)
	{
		// Get the animation object for the arms mesh
		UAnimInstance* AnimInstance = Character->GetMesh1P()->GetAnimInstance();
		if (AnimInstance != nullptr)
		{
			AnimInstance->Montage_Play(FireAnimation, 1.f);
		}
	}
}

void UTP_WeaponComponent::CastBoom()
{
	if (Character->CastSpell(25)) {
		FHitResult point = GetWhereAiming();
		Boom(point.Location, point.Normal.Rotation());
	}
}

FHitResult UTP_WeaponComponent::GetWhereAiming()
{
	FHitResult hitResult;
	APlayerController* PlayerController = Cast<APlayerController>(Character->GetController());
	const FVector Start = PlayerController->PlayerCameraManager->GetCameraLocation();
	const FVector End = PlayerController->PlayerCameraManager->GetCameraRotation().Vector() * 5000 + Start;

	GetWorld()->LineTraceSingleByChannel(hitResult, Start, End, ECollisionChannel::ECC_Visibility);
	
	// DrawDebugLine(GetWorld(), Start, End, FColor::Red, 0, 2);

	return hitResult;
}

bool UTP_WeaponComponent::AttachWeapon(APrototypeUnrealCharacter* TargetCharacter)
{
	Character = TargetCharacter;

	// Check that the character is valid, and has no weapon component yet
	if (Character == nullptr || Character->GetInstanceComponents().FindItemByClass<UTP_WeaponComponent>())
	{
		return false;
	}

	Character->Weapon = this;

	// Attach the weapon to the First Person Character
	FAttachmentTransformRules AttachmentRules(EAttachmentRule::SnapToTarget, true);
	AttachToComponent(Character->GetMesh1P(), AttachmentRules, FName(TEXT("GripPoint")));

	// add the weapon as an instance component to the character
	Character->AddInstanceComponent(this);

	// Set up action bindings
	if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			// Set the priority of the mapping to 1, so that it overrides the Jump action with the Fire action when using touch input
			Subsystem->AddMappingContext(FireMappingContext, 1);
		}

		if (UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerController->InputComponent))
		{
			// Fire
			EnhancedInputComponent->BindAction(FireAction, ETriggerEvent::Triggered, this, &UTP_WeaponComponent::Fire);

			EnhancedInputComponent->BindAction(BoomAction, ETriggerEvent::Triggered, this, &UTP_WeaponComponent::CastBoom);
		}
	}

	return true;
}

void UTP_WeaponComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	if (Character == nullptr)
	{
		return;
	}

	if (APlayerController* PlayerController = Cast<APlayerController>(Character->GetController()))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->RemoveMappingContext(FireMappingContext);
		}
	}
}

void UTP_WeaponComponent::Boom(FVector boomLocation, FRotator boomRotation) {
	DrawDebugSphere(GetWorld(), boomLocation, BoomRadius, 25, FColor::Green, 0, 2);

	boomRotation = FRotator::MakeFromEuler({
		boomRotation.Roll,
		boomRotation.Pitch - 90,
		boomRotation.Yaw,
	});

	TArray<FHitResult> hitResults;
	GetWorld()->SweepMultiByChannel(hitResults, boomLocation, boomLocation, FQuat(), ECollisionChannel::ECC_Visibility, FCollisionShape::MakeSphere(BoomRadius));

	UE_LOG(LogTemp, Warning, TEXT("Boom found '%d' objects"), hitResults.Num());

	UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), BoomEffect, boomLocation, boomRotation, BoomEffectScale);

	for (auto hit : hitResults) {
		if (hit.Component.IsValid()) {
			if (hit.Component->IsSimulatingPhysics()) {
				FVector force = hit.Component->GetComponentLocation() - boomLocation;
				if (!force.Normalize()) {
					UE_LOG(LogTemp, Error, TEXT("Failed to normalize boom force"));
				} else {
					force *= BoomPower;
					hit.Component->AddImpulse(force);
				}
			}
		}
	}
}

void UTP_WeaponComponent::Aim() {
	FHitResult point = GetWhereAiming();

	FVector location = point.Location;
	FRotator rotation = point.Normal.Rotation();
	DrawDebugSphere(GetWorld(), location, 10, 10, FColor::Blue, 0, 2);

	rotation = FRotator::MakeFromEuler({
		rotation.Roll,
		rotation.Pitch - 90,
		rotation.Yaw,
	});

	if (!CurrentAimEffect) {
		CurrentAimEffect = UNiagaraFunctionLibrary::SpawnSystemAtLocation(GetWorld(), AimEffect, location, rotation, AimEffectScale);
	}

	CurrentAimEffect->SetWorldLocation(location);
	CurrentAimEffect->SetWorldRotation(rotation);
}
