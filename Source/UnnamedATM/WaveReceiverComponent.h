#pragma once

#include "CoreMinimal.h"
#include "Components/SceneComponent.h"
#include "WaveReceiverComponent.generated.h"

class UMeshEdgesAsset;
class UStaticMeshComponent;
class AEdgeRenderer;

UCLASS(Blueprintable, ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class UNNAMEDATM_API UWaveReceiverComponent : public USceneComponent
{
	GENERATED_BODY()

public:
	UWaveReceiverComponent();

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave")
	TObjectPtr<UMeshEdgesAsset> EdgesAsset;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave")
	float Reflectivity = 0.3f;

	// L'objet peut-il renvoyer un écho ?
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave")
	bool bCanEcho = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave")
	float SegmentLength = 75.f;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Wave")
	int32 BisectionSteps = 4;

	// Le Static Mesh auquel ce composant est attaché (rempli au BeginPlay)
	UPROPERTY(BlueprintReadOnly, Category = "Wave")
	TObjectPtr<UStaticMeshComponent> Mesh;

	UFUNCTION(BlueprintCallable, Category = "Wave")
	void RevealFrom(FVector Origin, float StartTime, float Speed, float I0, float Range, float FalloffExponent, AActor* IgnoreActor);

	// Boîte englobante orientée du mesh, en espace monde (utilisée pour les échos)
	void GetBox(FVector& Center, FVector& Half, FVector& AxisF, FVector& AxisR, FVector& AxisUp) const;

protected:
	virtual void BeginPlay() override;

private:
	void BuildSegments();
	bool TraceVisible(const FVector& P, const FVector& Origin, AActor* IgnoreActor) const;

	TArray<FVector> Points;
	TArray<int32> SegA;
	TArray<int32> SegB;
	TArray<float> SegRevealTime;
	TArray<float> SegBrightness;
	TArray<int32> SegInstance;

	UPROPERTY()
	TObjectPtr<AEdgeRenderer> Renderer;
};
