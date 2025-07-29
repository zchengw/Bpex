// Copyright Epic Games, Inc. All Rights Reserved.

#include "BpexCharacter.h"
#include "Camera/CameraComponent.h"
#include "Components/CapsuleComponent.h"
#include "Components/InputComponent.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/Controller.h"
#include "GameFramework/SpringArmComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "BpexPlayerState.h"
#include "BpexAttributeSet.h"
#include "BpexAbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "BpexInputConfig.h"
#include "BpexMovementComponent.h"


//////////////////////////////////////////////////////////////////////////
// ABpexCharacter

ABpexCharacter::ABpexCharacter(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer.SetDefaultSubobjectClass<UBpexMovementComponent>(ACharacter::CharacterMovementComponentName))
{
	// Set size for collision capsule
	GetCapsuleComponent()->InitCapsuleSize(42.f, 96.0f);

	// Don't rotate when the controller rotates. Let that just affect the camera.
	bUseControllerRotationPitch = false;
	bUseControllerRotationYaw = false;
	bUseControllerRotationRoll = false;

	MovementComponent = Cast<UBpexMovementComponent>(GetCharacterMovement());

	// Configure character movement
	GetCharacterMovement()->bOrientRotationToMovement = true; // Character moves in the direction of input...	
	GetCharacterMovement()->RotationRate = FRotator(0.0f, 500.0f, 0.0f); // ...at this rotation rate

	// Note: For faster iteration times these variables, and many more, can be tweaked in the Character Blueprint
	// instead of recompiling to adjust them
	GetCharacterMovement()->JumpZVelocity = 700.f;
	GetCharacterMovement()->AirControl = 0.35f;
	GetCharacterMovement()->MaxWalkSpeed = 500.f;
	GetCharacterMovement()->MinAnalogWalkSpeed = 20.f;
	GetCharacterMovement()->BrakingDecelerationWalking = 2000.f;

	// Create a camera boom (pulls in towards the player if there is a collision)
	CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));
	CameraBoom->SetupAttachment(RootComponent);
	CameraBoom->TargetArmLength = 400.0f; // The camera follows at this distance behind the character	
	CameraBoom->bUsePawnControlRotation = true; // Rotate the arm based on the controller

	// Create a follow camera
	FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));
	FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName); // Attach the camera to the end of the boom and let the boom adjust to match the controller orientation
	FollowCamera->bUsePawnControlRotation = false; // Camera does not rotate relative to arm

	// Note: The skeletal mesh and anim blueprint references on the Mesh component (inherited from Character) 
	// are set in the derived blueprint asset named ThirdPersonCharacter (to avoid direct content references in C++)
}

UAbilitySystemComponent* ABpexCharacter::GetAbilitySystemComponent() const
{
	return AbilitySystemComponent;
}

void ABpexCharacter::PossessedBy(AController* NewController)
{
	Super::PossessedBy(NewController);

	InitAbilitySystem();

	GiveDefaultAbilities();
}

void ABpexCharacter::OnRep_PlayerState()
{
	Super::OnRep_PlayerState();

	InitAbilitySystem();
}

void ABpexCharacter::BeginPlay()
{
	// Call the base class  
	Super::BeginPlay();

	//Add Input Mapping Context
	if (APlayerController* PlayerController = Cast<APlayerController>(Controller))
	{
		if (UEnhancedInputLocalPlayerSubsystem* Subsystem = ULocalPlayer::GetSubsystem<UEnhancedInputLocalPlayerSubsystem>(PlayerController->GetLocalPlayer()))
		{
			Subsystem->AddMappingContext(DefaultMappingContext, 0);
		}
	}
}

void ABpexCharacter::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	// 类似塞尔达爬墙的输入方式: 
	// 1. 没有特定的输入键位
	// 2. 距离墙面足够近
	// 3. 有向墙的速度
	if (!MovementComponent->IsClimbing())
	{
		MovementComponent->PressedStartClimbing();
	}
}

//////////////////////////////////////////////////////////////////////////
// Input

void ABpexCharacter::SetupPlayerInputComponent(class UInputComponent* PlayerInputComponent)
{
	// Set up action bindings
	if (UEnhancedInputComponent* EnhancedInputComponent = CastChecked<UEnhancedInputComponent>(PlayerInputComponent)) {

		//Jumping
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Triggered, this, &ABpexCharacter::ProcessTriggerJump);
		EnhancedInputComponent->BindAction(JumpAction, ETriggerEvent::Completed, this, &ABpexCharacter::ProcessCompleteJump);

		//Moving
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ABpexCharacter::Move);
		EnhancedInputComponent->BindAction(MoveAction, ETriggerEvent::Completed, this, &ABpexCharacter::EndMove);

		//Looking
		EnhancedInputComponent->BindAction(LookAction, ETriggerEvent::Triggered, this, &ABpexCharacter::Look);

		//Flying
		EnhancedInputComponent->BindAction(FlyAction, ETriggerEvent::Started, this, &ABpexCharacter::ProcessTriggerFly);

		//Combat
		if (InputConfigAsset)
		{
			for (FInputConfig& Cfg : InputConfigAsset->InputConfigs)
			{
				EnhancedInputComponent->BindAction(Cfg.InputAction, ETriggerEvent::Started, this, &ABpexCharacter::ProcessSkillInput, Cfg.SkillTags);
			}
		}
	}
}

void ABpexCharacter::Move(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D MovementVector = Value.Get<FVector2D>();

	bIsMovingBackward = (MovementVector.Y < 0);

	if (Controller != nullptr)
	{
		// find out which way is forward
		const FRotator Rotation = Controller->GetControlRotation();
		const FRotator YawRotation(0, Rotation.Yaw, 0);

		// get forward vector
		FVector ForwardDirection;
		// get right vector 
		FVector RightDirection;

		if (MovementComponent->IsClimbing())
		{
			FVector ClimbSurfaceNormal = MovementComponent->GetClimbSurfaceNormal();

			ForwardDirection = FVector::CrossProduct(MovementComponent->GetClimbSurfaceNormal(), -GetActorRightVector());
			RightDirection = FVector::CrossProduct(MovementComponent->GetClimbSurfaceNormal(), GetActorUpVector());

		}
		else if (MovementComponent->IsFlying())
		{
			ForwardDirection = FRotationMatrix(Rotation).GetUnitAxis(EAxis::X);
			RightDirection = FRotationMatrix(Rotation).GetUnitAxis(EAxis::Y);		
		}
		else
		{
			ForwardDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X);
			RightDirection = FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y);
		}

		// add movement 
		AddMovementInput(ForwardDirection, MovementVector.Y);
		AddMovementInput(RightDirection, MovementVector.X);
	}
}

void ABpexCharacter::EndMove(const FInputActionValue& Value)
{
	bIsMovingBackward = false;
}

void ABpexCharacter::Look(const FInputActionValue& Value)
{
	// input is a Vector2D
	FVector2D LookAxisVector = Value.Get<FVector2D>();

	if (Controller != nullptr)
	{
		// add yaw and pitch input to controller
		AddControllerYawInput(LookAxisVector.X);
		AddControllerPitchInput(LookAxisVector.Y);
	}
}

void ABpexCharacter::ProcessTriggerJump()
{
	if (MovementComponent->IsClimbing())
	{
		if (bIsMovingBackward)
		{
			MovementComponent->PressedLeaveClimbing();
			bIsMovingBackward = false;
		}
		else
		{
			MovementComponent->PressedClimbDash();
		}
	}
	else if (MovementComponent->IsFlying())
	{
		AddMovementInput(FVector::UpVector);
	}
	else
	{
		Super::Jump();
	}
}

void ABpexCharacter::ProcessCompleteJump()
{
	if (!MovementComponent->IsClimbing())
	{
		Super::StopJumping();
	}
}

void ABpexCharacter::ProcessTriggerFly()
{
	MovementComponent->PressedFlying();
}

void ABpexCharacter::ActivateSkillByTags(const FGameplayTagContainer SkillTags)
{
	if (!AbilitySystemComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("AbilitySystemComponent is null"));
		return;
	}

	if (!MovementComponent->IsMovingOnGround())
	{
		return;
	}

	AbilitySystemComponent->TryActivateAbilitiesByTag(SkillTags);	
}

void ABpexCharacter::ProcessSkillInput(const FGameplayTagContainer SkillTags)
{
	ActivateSkillByTags(SkillTags);
	
	if (bCanPreinput)
	{
		PreinputTags = SkillTags;
	}
}

void ABpexCharacter::InitAbilitySystem()
{
	ABpexPlayerState* BpexPlayerState = GetPlayerState<ABpexPlayerState>();

	check(BpexPlayerState);

	if (AbilitySystemComponent = CastChecked<UBpexAbilitySystemComponent>(BpexPlayerState->GetAbilitySystemComponent()))
	{
		AbilitySystemComponent->InitAbilityActorInfo(BpexPlayerState, this);
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("AbilitySystemComponent cast failed"));
	}
	
	AttributeSet = BpexPlayerState->GetAttributeSet();
}

void ABpexCharacter::GiveDefaultAbilities()
{
	check(AbilitySystemComponent);

	if (!HasAuthority()) return;

	for (TSubclassOf<UGameplayAbility> Ability : DefaultAbilities)
	{
		const FGameplayAbilitySpec AbilitySpec(Ability, 1);
		AbilitySystemComponent->GiveAbility(AbilitySpec);
	}
}

void ABpexCharacter::ActivatePreinput()
{
	ActivateSkillByTags(PreinputTags);
}

void ABpexCharacter::ResetPreinput()
{
	PreinputTags.Reset();
}