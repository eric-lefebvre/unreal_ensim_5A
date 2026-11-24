#include "Wave.h"
#include "WaveReceiverComponent.h"
#include "SoundListener.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/Pawn.h"
#include "Components/PointLightComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/World.h"
#include "Engine/OverlapResult.h"

AWave::AWave()
{
	PrimaryActorTick.bCanEverTick = true;

	PointLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PointLight"));
	RootComponent = PointLight;
	PointLight->SetMobility(EComponentMobility::Movable);
	PointLight->bUseInverseSquaredFalloff = false;
	PointLight->IntensityUnits = ELightUnits::Unitless;
}

void AWave::BeginPlay()
{
	Super::BeginPlay();

	MID = UMaterialInstanceDynamic::Create(IsEcho ? EchoMaterial : WaveMaterial, this);
	check(MID);
	MID->SetVectorParameterValue(TEXT("Origin"), FLinearColor(GetActorLocation()));
	MID->SetScalarParameterValue(TEXT("StartTime"), StartTime);
	MID->SetScalarParameterValue(TEXT("Speed"), Speed);
	MID->SetScalarParameterValue(TEXT("GlowDuration"), GlowDuration);
	MID->SetScalarParameterValue(TEXT("Range"), Range);

	PointLight->SetLightFunctionMaterial(MID);
	PointLight->SetIntensity(I0 * BaseIntensity);
	PointLight->SetAttenuationRadius(Range);
	PointLight->SetLightFalloffExponent(LightFalloff);
	PointLight->SetSourceRadius(SourceRadius);
	PointLight->SetLightColor(Color);

	SetLifeSpan(Range / Speed + GlowDuration);

	bAwaitingHearing = IsHeardByPlayer;

	if (IsEcho)
	{
		SetActorTickEnabled(false);
		SetupEchoFace();
		PointLight->SetCastShadows(false);
	}
	else
	{
		Pending.Append(FindReceivers());
	}
	SpawnEchoes();
}

void AWave::Tick(float DeltaSeconds)
{
	Super::Tick(DeltaSeconds);

	if (bAwaitingHearing)
	{
		UpdateHearing();
	}

	const FVector Origin = GetActorLocation();
	const float R = Speed * (GetWorld()->GetTimeSeconds() - StartTime);

	for (int32 i = Pending.Num() - 1; i >= 0; --i)
	{
		UWaveReceiverComponent* Receiver = Pending[i];
		FVector Closest;
		if (Receiver->Mesh->GetClosestPointOnCollision(Origin, Closest) <= R)
		{
			Receiver->RevealFrom(Origin, StartTime, Speed, I0 * EdgeGain, Range, LightFalloff, SourceActor);
			Pending.RemoveAtSwap(i);
		}
	}

	if (Pending.Num() == 0 && !bAwaitingHearing)
	{
		SetActorTickEnabled(false);   // plus rien à révéler ni à faire entendre
	}
}

TArray<UWaveReceiverComponent*> AWave::FindReceivers() const
{
	FCollisionObjectQueryParams ObjectTypes;
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldStatic);
	ObjectTypes.AddObjectTypesToQuery(ECC_WorldDynamic);

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WaveOverlap));
	if (SourceActor)
	{
		Params.AddIgnoredActor(SourceActor);
	}

	TArray<FOverlapResult> Overlaps;
	GetWorld()->OverlapMultiByObjectType(Overlaps, GetActorLocation(), FQuat::Identity, ObjectTypes, FCollisionShape::MakeSphere(Range), Params);

	TSet<AActor*> Actors;
	for (const FOverlapResult& Overlap : Overlaps)
	{
		if (AActor* Actor = Overlap.GetActor())
		{
			Actors.Add(Actor);
		}
	}

	TArray<UWaveReceiverComponent*> Result;
	for (AActor* Actor : Actors)
	{
		TArray<UWaveReceiverComponent*> Receivers;
		Actor->GetComponents(Receivers);
		Result.Append(Receivers);
	}
	return Result;
}

void AWave::SpawnEchoes()
{
	if (!IsSpawningEchoes)
	{
		return;
	}

	for (UWaveReceiverComponent* Receiver : FindReceivers())
	{
		if (!Receiver->bCanEcho || Receiver->Reflectivity * I0 < MinIntensity)
		{
			continue;
		}

		FVector Center, Half, AxisF, AxisR, AxisUp;
		Receiver->GetBox(Center, Half, AxisF, AxisR, AxisUp);

		TryFace(Receiver, Center, AxisF,  Half.X, AxisR, AxisUp, Half.Y, Half.Z);
		TryFace(Receiver, Center, AxisR,  Half.Y, AxisF, AxisUp, Half.X, Half.Z);
		TryFace(Receiver, Center, AxisUp, Half.Z, AxisF, AxisR,  Half.X, Half.Y);
	}
}

void AWave::TryFace(UWaveReceiverComponent* Receiver, const FVector& BoxCenter, const FVector& Axis, float Half, const FVector& U, const FVector& V, float HalfU, float HalfV)
{
	const FVector Origin = GetActorLocation();
	const double Dist = FVector::DotProduct(Origin - BoxCenter, Axis);
	if (FMath::Abs(Dist) <= Half)
	{
		return;   // la source n'est pas devant cette face
	}

	const FVector N = Axis * FMath::Sign(Dist);
	const FVector TargetCenter = BoxCenter + N * Half;
	if (!IsFaceVisible(Receiver, TargetCenter, N, U, V, HalfU, HalfV))
	{
		return;
	}

	const FVector Mirror = Origin - N * (2.0 * FVector::DotProduct(Origin - TargetCenter, N));
	const FTransform SpawnTransform(Mirror);

	AWave* Echo = GetWorld()->SpawnActorDeferred<AWave>(GetClass(), SpawnTransform);
	Echo->StartTime = StartTime;
	Echo->Speed = Speed;
	Echo->I0 = I0 * Receiver->Reflectivity;
	Echo->Range = Range;
	Echo->EdgeGain = EdgeGain;
	Echo->Color = Color;
	Echo->IsEcho = true;
	Echo->IsSpawningEchoes = false;   // un seul rebond
	Echo->LightFalloff = LightFalloff;
	Echo->GlowDuration = GlowDuration;
	Echo->SourceActor = Receiver->GetOwner();
	Echo->FaceCenter = TargetCenter;
	Echo->FaceNormal = N;
	Echo->FaceU = U;
	Echo->FaceV = V;
	Echo->FaceHalfU = HalfU;
	Echo->FaceHalfV = HalfV;
	Echo->FinishSpawning(SpawnTransform);
}

bool AWave::IsFaceVisible(UWaveReceiverComponent* Receiver, const FVector& TargetCenter, const FVector& N, const FVector& U, const FVector& V, float HalfU, float HalfV) const
{
	const FVector DU = U * (HalfU * 0.9);
	const FVector DV = V * (HalfV * 0.9);
	const FVector Points[5] =
	{
		TargetCenter,
		TargetCenter + DU + DV,
		TargetCenter + DU - DV,
		TargetCenter - DU + DV,
		TargetCenter - DU - DV,
	};

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WaveFace), /*bTraceComplex*/ false, this);
	if (SourceActor)
	{
		Params.AddIgnoredActor(SourceActor);
	}
	Params.AddIgnoredActor(Receiver->GetOwner());

	const FVector Origin = GetActorLocation();
	FHitResult Hit;
	for (const FVector& P : Points)
	{
		if (!GetWorld()->LineTraceSingleByChannel(Hit, Origin, P + N * 2.0, ECC_Visibility, Params))
		{
			return true;
		}
	}
	return false;
}

void AWave::SetupEchoFace()
{
	MID->SetVectorParameterValue(TEXT("FaceCenter"), FLinearColor(FaceCenter));
	MID->SetVectorParameterValue(TEXT("FaceNormal"), FLinearColor(FaceNormal));
	MID->SetVectorParameterValue(TEXT("FaceU"), FLinearColor(FaceU));
	MID->SetVectorParameterValue(TEXT("FaceV"), FLinearColor(FaceV));
	MID->SetScalarParameterValue(TEXT("FaceHalfU"), FaceHalfU);
	MID->SetScalarParameterValue(TEXT("FaceHalfV"), FaceHalfV);
}

void AWave::UpdateHearing()
{
	APawn* Player = UGameplayStatics::GetPlayerPawn(this, 0);
	const FVector Listener = Player->GetActorLocation();
	const FVector Origin = GetActorLocation();
	const float Dist = FVector::Dist(Origin, Listener);
	const float R = Speed * (GetWorld()->GetTimeSeconds() - StartTime);

	if (R < FMath::Min(Dist, Range))
	{
		return;   // le front n'est pas encore arrivé
	}
	bAwaitingHearing = false;

	if (Dist > Range)
	{
		return;   // hors de portée
	}

	FCollisionQueryParams Params(SCENE_QUERY_STAT(WaveHearing), /*bTraceComplex*/ false, this);
	if (SourceActor)
	{
		Params.AddIgnoredActor(SourceActor);
	}
	Params.AddIgnoredActor(Player);
	FHitResult Hit;
	const bool bThroughWalls = GetWorld()->LineTraceSingleByChannel(Hit, Origin, Listener, ECC_Visibility, Params);

	checkf(Player->Implements<USoundListener>(), TEXT("Le pawn joueur doit implementer l'interface SoundListener"));
	ISoundListener::Execute_OnSoundHeard(Player, Origin, Range, bThroughWalls);
}
