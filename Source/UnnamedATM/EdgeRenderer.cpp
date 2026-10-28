#include "EdgeRenderer.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Materials/MaterialInstanceDynamic.h"

AEdgeRenderer::AEdgeRenderer()
{
	PrimaryActorTick.bCanEverTick = false;

	Lines = CreateDefaultSubobject<UInstancedStaticMeshComponent>(TEXT("Lines"));
	RootComponent = Lines;
	Lines->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	Lines->SetCastShadow(false);
	Lines->NumCustomDataFloats = 2;   // 0 = instant d'apparition, 1 = luminosité
}

void AEdgeRenderer::BeginPlay()
{
	Super::BeginPlay();

	UMaterialInstanceDynamic* MID = Lines->CreateDynamicMaterialInstance(0);
	if (!ensureMsgf(MID, TEXT("EdgeRenderer : aucun material sur Lines")))
	{
		return;
	}
	MID->SetScalarParameterValue(TEXT("HoldTime"), HoldTime);
	MID->SetScalarParameterValue(TEXT("FadeTime"), FadeTime);
	MID->SetScalarParameterValue(TEXT("Intensity"), Intensity);
}

int32 AEdgeRenderer::SetSegment(int32 InstanceIndex, const FVector& A, const FVector& B, float RevealTime, float Brightness)
{
	const FVector Dir = B - A;
	const FTransform Transform(
		FRotationMatrix::MakeFromX(Dir).Rotator(),
		(A + B) * 0.5,
		FVector(Dir.Size() / 100.0, Thickness / 100.0, Thickness / 100.0));

	int32 Index = InstanceIndex;
	if (Index == INDEX_NONE)
	{
		Index = Lines->AddInstance(Transform, /*bWorldSpace*/ true);
	}
	else
	{
		Lines->UpdateInstanceTransform(Index, Transform, /*bWorldSpace*/ true, /*bMarkRenderStateDirty*/ true);
	}
	Lines->SetCustomDataValue(Index, 0, RevealTime, /*bMarkRenderStateDirty*/ true);
	Lines->SetCustomDataValue(Index, 1, Brightness, /*bMarkRenderStateDirty*/ true);
	return Index;
}