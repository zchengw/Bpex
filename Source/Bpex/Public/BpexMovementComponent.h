// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "BpexMovementComponent.generated.h"

/**
 *
 */
UCLASS()
class BPEX_API UBpexMovementComponent : public UCharacterMovementComponent
{
	GENERATED_BODY()

public:
	virtual void BeginPlay() override;

	virtual void TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction) override;

	virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

	virtual void OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode) override;

	virtual void PhysCustom(float deltaTime, int32 Iterations) override;

	virtual float GetMaxSpeed() const override;

	virtual float GetMaxAcceleration() const override;

#pragma region Climbing
public:
	/** Collision Detect Parameters */
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
	int CollisionCapsuleRadius = 50;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
	int CollisionCapsuleHalfHeight = 80;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
	float DistanceFromSurface = 45.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
	float EyeHeight = 100.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
	float MaxSlope = 0.6f;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
	float MinSlope = -0.6f;

	/** Speed Parameters */
	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
	float MaxClimbingSpeed = 120;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
	float MaxClimbingAcceleration = 380;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
	float BrakingDecelerationClimbing = 550.f;

	UPROPERTY(Category = "Character Movement: Climbing", EditAnywhere)
	int ClimbingRotateSpeed = 5.f;

private:
	UPROPERTY(Category = "Character Movement: Climbing", EditDefaultsOnly)
	UAnimMontage* LedgeClimbMontage;

	UPROPERTY()
	UAnimInstance* AnimInstance;

	bool bIsToClimb = false;
	bool bIsToClimbDash = false;

	FVector NextClimbNormal;
	FVector NextClimbPosition;

	TArray<FHitResult> ClimableSurfaceHitResults;

	class UCapsuleComponent* CapsuleComponent;
	FCollisionQueryParams ClimbQueryParams;

	float EyeHeightSphereRadius = 30.f;
	float SweepClimableSurfaceOffset = 10.f;
	float EyeHeightTraceDistance = 75.f;
	float FloorTraceOffset = 20.f;

public:
	UFUNCTION(BlueprintCallable)
	bool IsClimbing() const;

	UFUNCTION(BlueprintCallable)
	void PressedStartClimbing();
	UFUNCTION(BlueprintCallable)
	void PressedLeaveClimbing();
	UFUNCTION(BlueprintCallable)
	void PressedClimbDash();

	FVector GetClimbSurfaceNormal();

private:
	void PhysClimbing(float deltaTime, int32 Iterations);

	bool IsSlopeClimbable(); // TODOTODO

	/* 判断自然退出条件 */
	bool IsFowardCloseWall();
	bool IsDownCloseFloor();
	bool IsUpCloseLedge();

	void MoveAlongWall(float deltaTime);
	void SnapToClimbingSurface(float deltaTime);

	bool ShouldStopClimbing();
	void LeaveClimbing(float deltaTime, int32 Iterations);

	bool TryClimbUpLedge();

	void OnClimbMontageEnd(UAnimMontage* Montage, bool bInterrupted);

	bool EyeHeightTrace();
	bool IsCloseWalkableLedge();

	FQuat GetInterpClimbRotation(float deltaTime);

	void CalcClimbSurfaceInfo();
	bool SweepClimableSurface();

	TArray<FHitResult> DoCapsuleSweepMultiByChannel(const FVector& Start, const FVector& End, const FQuat& Rot = FQuat::Identity);
	FHitResult DoSphereSweepMultiByChannel(const FVector& Start, const FVector& End);
	FHitResult DoLineTraceSingleByChannel(const FVector& Start, const FVector& End);
#pragma endregion

#pragma region Flying
public:
	UFUNCTION(BlueprintCallable)
	void PressedFlying();

protected:
	virtual void PhysFlying(float deltaTime, int32 Iterations) override;

private:
	bool bIsToFly = false;
#pragma endregion
};
