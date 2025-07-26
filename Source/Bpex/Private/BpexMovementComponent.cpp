// Fill out your copyright notice in the Description page of Project Settings.

#include "BpexMovementComponent.h"
#include "CustomMovementMode.h"
#include "Components/CapsuleComponent.h"
#include "GameFramework/Character.h"

#include "DrawDebugHelpers.h"

void UBpexMovementComponent::BeginPlay()
{
	Super::BeginPlay();

	ClimbQueryParams.AddIgnoredActor(GetOwner());
	CapsuleComponent = CharacterOwner->GetCapsuleComponent();
	NextClimbNormal = FVector::One();

	AnimInstance = GetCharacterOwner()->GetMesh()->GetAnimInstance();

	bUseControllerDesiredRotation_Default = bUseControllerDesiredRotation;
	bOrientRotationToMovement_Default = bOrientRotationToMovement;
}

void UBpexMovementComponent::TickComponent(float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UBpexMovementComponent::OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity)
{
	if (!IsClimbing() && bIsToClimb)
	{
		SetMovementMode(EMovementMode::MOVE_Custom, ECustomMovementMode::CMOVE_Climbing);
		UE_LOG(LogTemp, Warning, TEXT("Change to climbing mode"));
	}

	if (!IsFlying() && bIsToFly)
	{
		SetMovementMode(EMovementMode::MOVE_Flying);
		UE_LOG(LogTemp, Warning, TEXT("Change to flying mode"));
	}

	Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
}

void UBpexMovementComponent::OnMovementModeChanged(EMovementMode PreviousMovementMode, uint8 PreviousCustomMode)
{
	if (IsClimbing())
	{
		bOrientRotationToMovement = false;
		StopMovementImmediately();
	}
	else if (IsFlying())
	{
		bUseControllerDesiredRotation = true;
		bOrientRotationToMovement = false;
	}

	// set back
	if (PreviousMovementMode == MOVE_Custom && PreviousCustomMode == CMOVE_Climbing)
	{
		bOrientRotationToMovement = bOrientRotationToMovement_Default;

		FRotator StandRotation = FRotator(0, UpdatedComponent->GetComponentRotation().Yaw, 0);
		UpdatedComponent->SetRelativeRotation(StandRotation);

		StopMovementImmediately();
	}
	else if (PreviousMovementMode == MOVE_Flying)
	{
		bUseControllerDesiredRotation = bUseControllerDesiredRotation_Default;
		bOrientRotationToMovement = bOrientRotationToMovement_Default;
	}

	Super::OnMovementModeChanged(PreviousMovementMode, PreviousCustomMode);
}

void UBpexMovementComponent::PhysCustom(float deltaTime, int32 Iterations)
{
	switch (CustomMovementMode)
	{
	case CMOVE_Climbing:
		PhysClimbing(deltaTime, Iterations);
		break;
	}

	Super::PhysCustom(deltaTime, Iterations);
}

float UBpexMovementComponent::GetMaxSpeed() const
{
	return IsClimbing() ? MaxClimbingSpeed : Super::GetMaxSpeed();
}

float UBpexMovementComponent::GetMaxAcceleration() const
{
	return IsClimbing() ? MaxClimbingAcceleration : Super::GetMaxAcceleration();
}

void UBpexMovementComponent::PhysClimbing(float deltaTime, int32 Iterations)
{
	if (deltaTime < MIN_TICK_TIME)
	{
		return;
	}

	if (ShouldStopClimbing() || (!SweepClimableSurface() && !AnimInstance->IsAnyMontagePlaying()))
	{
		LeaveClimbing(deltaTime, Iterations);
		return;
	}

	CalcClimbSurfaceInfo();

	RestorePreAdditiveRootMotionVelocity();

	// Calculates velocity if not being controlled by root motion.
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		CalcVelocity(deltaTime, 0.f, false, GetMaxAcceleration());
	}

	ApplyRootMotionToVelocity(deltaTime);

	FVector OldLocation = UpdatedComponent->GetComponentLocation();

	MoveAlongWall(deltaTime);

	// Velocity based on distance traveled.
	if (!HasAnimRootMotion() && !CurrentRootMotion.HasOverrideVelocity())
	{
		Velocity = (UpdatedComponent->GetComponentLocation() - OldLocation) / deltaTime;
	}

	SnapToClimbingSurface(deltaTime);

	TryClimbUpLedge();
}

void UBpexMovementComponent::PressedStartClimbing()
{
	// 暂时设置无法从飞行到爬墙状态
	if (IsFlying()) return;
	
	// 1. 距离墙面足够近
	// 2. 有向墙的速度
	// 3. not playing montage
	if (!IsClimbing() && IsFowardCloseWall() && EyeHeightTrace() && !AnimInstance->IsAnyMontagePlaying())
	{
		bIsToClimb = true;
	}
}

void UBpexMovementComponent::PressedLeaveClimbing()
{
	bIsToClimb = false;
}

void UBpexMovementComponent::PressedClimbDash()
{
	bIsToClimbDash = true;
}

bool UBpexMovementComponent::IsClimbing() const
{
	return MovementMode == EMovementMode::MOVE_Custom && CustomMovementMode == ECustomMovementMode::CMOVE_Climbing;
}

FVector UBpexMovementComponent::GetClimbSurfaceNormal()
{
	return NextClimbNormal;
}

bool UBpexMovementComponent::IsFowardCloseWall()
{
	// 角色右前方和左前方是否同时检测到物体
	// 正前方三角形区域
	// TODO：倾斜墙面的处理
	FVector ForwardVector = UpdatedComponent->GetForwardVector();
	FVector RightVector = UpdatedComponent->GetRightVector();

	FVector LeftRay = (ForwardVector - RightVector * FMath::Tan(FMath::DegreesToRadians(25))).GetSafeNormal();
	FVector RightRay = (ForwardVector + RightVector * FMath::Tan(FMath::DegreesToRadians(25))).GetSafeNormal();

	float DetectLength = CollisionCapsuleRadius + 10.0f;

	FVector Start = GetActorLocation();
	FVector LeftEnd = Start + DetectLength * LeftRay;
	FVector RightEnd = Start + DetectLength * RightRay;

	//DrawDebugLine(GetWorld(), Start, LeftEnd, FColor::Yellow, false);
	//DrawDebugLine(GetWorld(), Start, RightEnd, FColor::Yellow, false);

	FHitResult HitResultLeft;
	FHitResult HitResultRight;

	bool bIsHit = GetWorld()->LineTraceSingleByChannel(
		HitResultLeft,
		Start,
		LeftEnd,
		ECC_WorldStatic,
		ClimbQueryParams
	);

	if (bIsHit)
	{
		bIsHit = GetWorld()->LineTraceSingleByChannel(
			HitResultRight,
			Start,
			RightEnd,
			ECC_WorldStatic,
			ClimbQueryParams
		);
	}

	if (bIsHit)
	{
		// 加速度和速度都要朝向墙面
		// 判断速度和加速的方向是否接近角色的正面
		float VelocityDot = FVector::DotProduct(ForwardVector, Velocity.GetSafeNormal());
		float AccelerationDot = FVector::DotProduct(ForwardVector, GetCurrentAcceleration().GetSafeNormal());
		return VelocityDot > 0.1f && AccelerationDot > 0.8f;
	}

	return false;
}

bool UBpexMovementComponent::IsDownCloseFloor()
{
	// 检测距离地面是否足够近
	FVector Start = GetActorLocation();
	FVector End = Start + (CollisionCapsuleHalfHeight + FloorTraceOffset) * FVector::DownVector;

	FHitResult HitResult = DoLineTraceSingleByChannel(Start, End);

	if (HitResult.bBlockingHit)
	{
		// 地面是否足够平
		bool bIsWalkableFloor = HitResult.Normal.Z > GetWalkableFloorZ();
		if (!bIsWalkableFloor) return false;

		// 速度要朝向地面
		float VelocityDot = FVector::DotProduct(-HitResult.Normal, Velocity.GetSafeNormal());
		return VelocityDot > 0.f;
	}

	return false;
}

bool UBpexMovementComponent::IsUpCloseLedge()
{
	bool bCloseToLedge = IsCloseWalkableLedge();

	if (bCloseToLedge)
	{
		// 速度要朝向角色up方向
		float VelocityDot = FVector::DotProduct(UpdatedComponent->GetUpVector(), Velocity.GetSafeNormal());
		bool bIsMovingTowardsLedge = VelocityDot > 0.0f;
		return bIsMovingTowardsLedge;
	}
	return false;
}

void UBpexMovementComponent::MoveAlongWall(float deltaTime)
{
	const FVector Adjusted = Velocity * deltaTime;

	FHitResult Hit(1.f);

	SafeMoveUpdatedComponent(Adjusted, GetInterpClimbRotation(deltaTime), true, Hit);

	if (Hit.Time < 1.f)
	{
		//UE_LOG(LogTemp, Warning, TEXT("Hit: MoveAlongWall"));

		DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 9.0f, 12, FColor::Yellow, false, 5.0f);

		HandleImpact(Hit, deltaTime, Adjusted);
		SlideAlongSurface(Adjusted, (1.f - Hit.Time), Hit.Normal, Hit, true);
	}
}

void UBpexMovementComponent::SnapToClimbingSurface(float deltaTime)
{
	const FVector Forward = UpdatedComponent->GetForwardVector();
	const FVector Location = UpdatedComponent->GetComponentLocation();
	const FQuat Rotation = UpdatedComponent->GetComponentQuat();

	const FVector ForwardDifference = (NextClimbPosition - Location).ProjectOnTo(Forward);

	const FVector Offset = -NextClimbNormal * (ForwardDifference.Length() - DistanceFromSurface);

	const FVector Delta = Offset * MaxClimbingSpeed * deltaTime;

	DrawDebugLine(GetWorld(), Location, Location + Delta, FColor::Red, false);

	FHitResult Hit(1.f);
	SafeMoveUpdatedComponent(Delta, Rotation, true, Hit);

	if (Hit.Time < 1.f)
	{
		//UE_LOG(LogTemp, Warning, TEXT("Hit: SnapToClimbingSurface"));

		DrawDebugSphere(GetWorld(), Hit.ImpactPoint, 9.0f, 12, FColor::Red, false, 5.0f);

		HandleImpact(Hit, deltaTime, Delta);
		SlideAlongSurface(Delta, (1.f - Hit.Time), Hit.Normal, Hit, true);
	}
	
	//UpdatedComponent->MoveComponent(Delta, Rotation, true);
}

bool UBpexMovementComponent::ShouldStopClimbing()
{
	// 1. 到达地面
	// 2. 手动退出爬墙
	// 3. 墙面倾斜度不适合攀爬

	if (!IsSlopeClimbable())
	{
		UE_LOG(LogTemp, Warning, TEXT("slope not climbable"));
		return true;
	}

	bool bIsFloor = IsDownCloseFloor();
	if (bIsFloor) UE_LOG(LogTemp, Warning, TEXT("bIsFloor is true"));

	if (!bIsToClimb) UE_LOG(LogTemp, Warning, TEXT("bIsToClimb is false"));

	return !bIsToClimb || bIsFloor;
}

void UBpexMovementComponent::LeaveClimbing(float deltaTime, int32 Iterations)
{
	bIsToClimb = false;
	SetMovementMode(EMovementMode::MOVE_Falling);
	StartNewPhysics(deltaTime, Iterations);
	UE_LOG(LogTemp, Warning, TEXT("LeaveClimbing to MOVE_Falling"));
}

bool UBpexMovementComponent::TryClimbUpLedge()
{
	if (AnimInstance && LedgeClimbMontage && AnimInstance->Montage_IsPlaying(LedgeClimbMontage))
	{
		return false;
	}

	if (IsUpCloseLedge())
	{
		const FRotator StandRotation = FRotator(0, UpdatedComponent->GetComponentRotation().Yaw, 0);
		UpdatedComponent->SetRelativeRotation(StandRotation);

		float Result = AnimInstance->Montage_Play(LedgeClimbMontage);

		FOnMontageBlendingOutStarted MontageDelegate;
		MontageDelegate.BindUObject(this, &UBpexMovementComponent::OnClimbMontageEnd);
		AnimInstance->Montage_SetBlendingOutDelegate(MontageDelegate, LedgeClimbMontage);

		if (Result == 0.f)
		{
			UE_LOG(LogTemp, Error, TEXT("Montage failed to play!"));
		}
		return true;
	}

	return false;
}

void UBpexMovementComponent::OnClimbMontageEnd(UAnimMontage* Montage, bool bInterrupted)
{
	UE_LOG(LogTemp, Warning, TEXT("Trigger"));
	if (Montage == LedgeClimbMontage)
	{
		bIsToClimb = false;

		if (!bInterrupted)
		{
			SetMovementMode(EMovementMode::MOVE_Walking);
			UE_LOG(LogTemp, Warning, TEXT("OnClimbMontageEnd to MOVE_Walking"));

		}
	}
}

bool UBpexMovementComponent::EyeHeightTrace()
{
	const FVector Start = UpdatedComponent->GetComponentLocation() + UpdatedComponent->GetUpVector() * (EyeHeight + EyeHeightSphereRadius);

	FVector Forward = UpdatedComponent->GetForwardVector();
	//Forward.Z = 0;

	const FVector End = Start + Forward * EyeHeightTraceDistance;

	FHitResult Hit = DoSphereSweepMultiByChannel(Start, End);

	return Hit.bBlockingHit;
}

bool UBpexMovementComponent::IsCloseWalkableLedge()
{
	const FVector Start = UpdatedComponent->GetComponentLocation() + UpdatedComponent->GetUpVector() * (EyeHeight + EyeHeightSphereRadius);
	
	FVector Forward = UpdatedComponent->GetForwardVector();
	//Forward.Z = 0;

	const FVector EyeTraceEnd = Start + Forward * EyeHeightTraceDistance;

	FHitResult Hit = DoSphereSweepMultiByChannel(Start, EyeTraceEnd);
	
	if (Hit.bBlockingHit) return false;

	// 也可能存在足够倾斜可以攀爬的斜面
	const FVector FloorTraceEnd = EyeTraceEnd - UpdatedComponent->GetUpVector() * (EyeHeightSphereRadius + CollisionCapsuleHalfHeight + EyeHeight);
	FHitResult FloorHit = DoLineTraceSingleByChannel(EyeTraceEnd, FloorTraceEnd);

	DrawDebugSphere(GetWorld(), EyeTraceEnd, 5.0f, 12, FColor::Blue, false, 1.0f);
	DrawDebugLine(GetWorld(), EyeTraceEnd, FloorTraceEnd, FColor::Blue, false, 1.0f);

	if (FloorHit.bBlockingHit)
	{
		// 地面如果not walkable就继续爬
		bool bIsWalkableFloor = FloorHit.Normal.Z > GetWalkableFloorZ();

		UE_LOG(LogTemp, Warning, TEXT("IsWalkableFloor? : %d"), bIsWalkableFloor);

		return bIsWalkableFloor;
	}

	return true;
}

FQuat UBpexMovementComponent::GetInterpClimbRotation(float deltaTime)
{
	const FQuat Current = UpdatedComponent->GetComponentQuat();

	if (HasAnimRootMotion() || CurrentRootMotion.HasOverrideVelocity())
	{
		return Current;
	}

	const FQuat Target = FRotationMatrix::MakeFromX(-NextClimbNormal).ToQuat();

	return FMath::QInterpTo(Current, Target, deltaTime, ClimbingRotateSpeed);
}

void UBpexMovementComponent::CalcClimbSurfaceInfo()
{
	NextClimbNormal = FVector::ZeroVector;
	NextClimbPosition = FVector::ZeroVector;

	for (const auto& Hit : ClimableSurfaceHitResults)
	{
		DrawDebugPoint(GetWorld(), Hit.Location, 10.f, FColor::Red, false);
		DrawDebugPoint(GetWorld(), Hit.ImpactPoint, 10.f, FColor::Blue, false);

		NextClimbNormal += Hit.ImpactNormal;
		NextClimbPosition += Hit.ImpactPoint;
	}

	NextClimbNormal = NextClimbNormal.GetSafeNormal();
	NextClimbPosition /= ClimableSurfaceHitResults.Num();
}

bool UBpexMovementComponent::SweepClimableSurface()
{
	const FVector Start = UpdatedComponent->GetComponentLocation();
	const FVector End = Start + UpdatedComponent->GetForwardVector() * SweepClimableSurfaceOffset;

	FRotator CurrentRotation = UpdatedComponent->GetComponentRotation();
	FQuat RotationQuat = CurrentRotation.Quaternion();

	ClimableSurfaceHitResults = DoCapsuleSweepMultiByChannel(Start, End, RotationQuat);

	return !ClimableSurfaceHitResults.IsEmpty();
}

TArray<FHitResult> UBpexMovementComponent::DoCapsuleSweepMultiByChannel(const FVector& Start, const FVector& End, const FQuat& Rot)
{
	const FCollisionShape CollisionShape = FCollisionShape::MakeCapsule(CollisionCapsuleRadius, CollisionCapsuleHalfHeight);

	TArray<FHitResult> OutHits;
	bool IsHit = GetWorld()->SweepMultiByChannel(OutHits, Start, End, Rot, ECC_WorldStatic, CollisionShape, ClimbQueryParams);

	DrawDebugCapsule(GetWorld(), End, CollisionCapsuleHalfHeight, CollisionCapsuleRadius, Rot, FColor::Green);

	return OutHits;
}

FHitResult UBpexMovementComponent::DoSphereSweepMultiByChannel(const FVector& Start, const FVector& End)
{
	const FCollisionShape CollisionShape = FCollisionShape::MakeSphere(EyeHeightSphereRadius);

	FHitResult OutHit;
	bool IsHit = GetWorld()->SweepSingleByChannel(OutHit, Start, End, FQuat::Identity, ECC_WorldStatic, CollisionShape, ClimbQueryParams);

	DrawDebugSphere(GetWorld(), Start, 5, 5, FColor::Yellow);

	DrawDebugSphere(GetWorld(), End, EyeHeightSphereRadius, 5, FColor::Yellow);

	return OutHit;
}

FHitResult UBpexMovementComponent::DoLineTraceSingleByChannel(const FVector& Start, const FVector& End)
{
	FHitResult OutHit;

	bool bIsHit = GetWorld()->LineTraceSingleByChannel(
		OutHit,
		Start,
		End,
		ECC_WorldStatic,
		ClimbQueryParams
	);

	DrawDebugLine(GetWorld(), Start, End, FColor::Yellow, false);

	return OutHit;
}

bool UBpexMovementComponent::IsSlopeClimbable()
{
	return true;
	float DotResult = FVector::DotProduct(FVector::UpVector, NextClimbNormal);

	if (DotResult > MinSlope && DotResult < MaxSlope)
	{
		return true;
	}

	return false;
}

void UBpexMovementComponent::PressedFlying()
{
	// 暂时设置无法从爬墙到飞行状态
	if (IsClimbing()) return;

	if (!IsFlying())
	{
		bIsToFly = true;
		UE_LOG(LogTemp, Warning, TEXT("Pressed to fly"));
	}
	else
	{
		bIsToFly = false;
		UE_LOG(LogTemp, Warning, TEXT("Pressed to Exit fly"));
	}
}

void UBpexMovementComponent::PhysFlying(float deltaTime, int32 Iterations)
{
	if (!bIsToFly)
	{
		SetMovementMode(EMovementMode::MOVE_Falling);
		StartNewPhysics(deltaTime, Iterations);
		UE_LOG(LogTemp, Warning, TEXT("Leave Flying"));
	}

	Super::PhysFlying(deltaTime, Iterations);
}
