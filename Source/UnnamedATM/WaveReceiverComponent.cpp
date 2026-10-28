#include "WaveReceiverComponent.h"
#include "MeshEdgesAsset.h"
#include "EdgeRenderer.h"
#include "Components/StaticMeshComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/World.h"

UWaveReceiverComponent::UWaveReceiverComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void UWaveReceiverComponent::BeginPlay()
{
	Super::BeginPlay();

	Mesh = Cast<UStaticMeshComponent>(GetAttachParent());
	if (!ensureMsgf(Mesh, TEXT("%s : le WaveReceiver doit etre attache a un Static Mesh Component"), *GetOwner()->GetName()))
	{
		return;
	}

	Renderer = Cast<AEdgeRenderer>(UGameplayStatics::GetActorOfClass(this, AEdgeRenderer::StaticClass()));
	if (!ensureMsgf(Renderer, TEXT("Aucun EdgeRenderer dans le niveau")))
	{
		return;
	}

	if (EdgesAsset)
	{
		BuildSegments();
	}
}

void UWaveReceiverComponent::BuildSegments()
{
	const FTransform MeshTransform = Mesh->GetComponentTransform();

	for (const FEdgeSegment& Edge : EdgesAsset->Edges)
	{
		const FVector WStart = MeshTransform.TransformPosition(Edge.Start);
		const FVector WEnd = MeshTransform.TransformPosition(Edge.End);
		const int32 N = FMath::Max(1, FMath::CeilToInt(FVector::Dist(WStart, WEnd) / SegmentLength));
		const int32 Base = Points.Num();

		for (int32 k = 0; k <= N; ++k)
		{
			Points.Add(FMath::Lerp(WStart, WEnd, static_cast<double>(k) / N));
		}
		for (int32 k = 0; k < N; ++k)
		{
			SegA.Add(Base + k);
			SegB.Add(Base + k + 1);
			SegRevealTime.Add(-1000.f);
			SegBrightness.Add(0.f);
			SegInstance.Add(INDEX_NONE);
		}
	}

	check(SegB.Num() == SegA.Num() && SegRevealTime.Num() == SegA.Num()
		&& SegBrightness.Num() == SegA.Num() && SegInstance.Num() == SegA.Num());
}

bool UWaveReceiverComponent::TraceVisible(const FVector& P, const FVector& Origin, AActor* IgnoreActor) const
{
	FCollisionQueryParams Params(SCENE_QUERY_STAT(WaveReveal), /*bTraceComplex*/ true);
	if (IgnoreActor)
	{
		Params.AddIgnoredActor(IgnoreActor);
	}

	FHitResult Hit;
	const FVector End = P + (Origin - P).GetSafeNormal() * 3.0;   // s'arrête 3 cm avant le point
	return !GetWorld()->LineTraceSingleByChannel(Hit, Origin, End, ECC_Visibility, Params);
}

void UWaveReceiverComponent::RevealFrom(FVector Origin, float StartTime, float Speed, float I0, float Range, float FalloffExponent, AActor* IgnoreActor)
{
	if (!Renderer || Points.Num() == 0)
	{
		return;
	}

	const float Now = GetWorld()->GetTimeSeconds();

	TArray<int8> Visibility;
	Visibility.Init(-1, Points.Num());

	auto IsVisible = [&](int32 Index)
		{
			if (Visibility[Index] < 0)
			{
				Visibility[Index] = TraceVisible(Points[Index], Origin, IgnoreActor) ? 1 : 0;
			}
			return Visibility[Index] == 1;
		};

	auto RevealTimeOf = [&](const FVector& A, const FVector& B)
		{
			return StartTime + FVector::Dist(Origin, (A + B) * 0.5) / Speed;
		};

	// Même atténuation que la Point Light de l'onde (Inverse Squared Falloff désactivé)
	auto BrightnessAt = [&](const FVector& P)
		{
			const float Ratio = FVector::Dist(Origin, P) / Range;
			return I0 * FMath::Pow(FMath::Clamp(1.f - Ratio * Ratio, 0.f, 1.f), FalloffExponent);
		};

	for (int32 s = 0; s < SegA.Num(); ++s)
	{
		const FVector& A = Points[SegA[s]];
		const FVector& B = Points[SegB[s]];
		const float Brightness = BrightnessAt((A + B) * 0.5);

		if (Brightness <= 0.f)
		{
			continue;   // hors de portée de l'onde
		}

		const bool bStillLit = Now - SegRevealTime[s] <= Renderer->HoldTime;
		if (bStillLit && Brightness <= SegBrightness[s])
		{
			continue;   // déjà allumé au moins aussi fort
		}

		const bool bA = IsVisible(SegA[s]);
		const bool bB = IsVisible(SegB[s]);

		if (bA && bB)
		{
			const float RevealTime = RevealTimeOf(A, B);
			SegInstance[s] = Renderer->SetSegment(SegInstance[s], A, B, RevealTime, Brightness);
			SegRevealTime[s] = RevealTime;
			SegBrightness[s] = Brightness;
		}
		else if (bA != bB)
		{
			const FVector StartVis = bA ? A : B;
			FVector VisPt = StartVis;
			FVector HidPt = bA ? B : A;

			for (int32 i = 0; i < BisectionSteps; ++i)
			{
				const FVector Mid = (VisPt + HidPt) * 0.5;
				if (TraceVisible(Mid, Origin, IgnoreActor))
				{
					VisPt = Mid;
				}
				else
				{
					HidPt = Mid;
				}
			}
			SegInstance[s] = Renderer->SetSegment(SegInstance[s], StartVis, VisPt, RevealTimeOf(StartVis, VisPt), Brightness);
		}
	}
}
