// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "MeshEdgesAsset.generated.h"

class UStaticMesh;

USTRUCT(BlueprintType)
struct FEdgeSegment
{
	GENERATED_BODY()

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector Start = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadOnly)
	FVector End = FVector::ZeroVector;
};

UCLASS(BlueprintType)
class UNNAMEDATM_API UMeshEdgesAsset : public UDataAsset
{
	GENERATED_BODY()
	
public:
	UPROPERTY(EditAnywhere, Category = "Source")
	TObjectPtr<UStaticMesh> SourceMesh;

	// Angle minimal entre deux faces pour que leur arête commune soit gardée
	UPROPERTY(EditAnywhere, Category = "Source", meta = (ClampMin = "0", ClampMax = "180"))
	float AngleThresholdDeg = 30.f;

	// Arêtes en coordonnées locales du mesh
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Edges")
	TArray<FEdgeSegment> Edges;

#if WITH_EDITOR
	UFUNCTION(CallInEditor, Category = "Source")
	void Generate();
#endif
};
