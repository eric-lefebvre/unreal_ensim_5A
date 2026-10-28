#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Wave.generated.h"

class UPointLightComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class UWaveReceiverComponent;

UCLASS()
class UNNAMEDATM_API AWave : public AActor
{
	GENERATED_BODY()

public:
	AWave();

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Wave")
	TObjectPtr<UPointLightComponent> PointLight;

	// --- Réglages de classe (Class Defaults de BP_Wave) ---

	UPROPERTY(EditDefaultsOnly, Category = "Wave|Setup")
	TObjectPtr<UMaterialInterface> WaveMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Wave|Setup")
	TObjectPtr<UMaterialInterface> EchoMaterial;

	UPROPERTY(EditDefaultsOnly, Category = "Wave|Setup")
	float BaseIntensity = 8.f;

	UPROPERTY(EditDefaultsOnly, Category = "Wave|Setup")
	float MinIntensity = 0.1f;

	UPROPERTY(EditDefaultsOnly, Category = "Wave|Setup")
	float LightFalloff = 2.5f;

	UPROPERTY(EditDefaultsOnly, Category = "Wave|Setup")
	float GlowDuration = 0.5f;

	// --- Paramètres de spawn (mêmes noms que les anciennes variables BP) ---

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Spawn", meta = (ExposeOnSpawn = true))
	float StartTime = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Spawn", meta = (ExposeOnSpawn = true))
	float Speed = 1500.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Spawn", meta = (ExposeOnSpawn = true))
	float I0 = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Spawn", meta = (ExposeOnSpawn = true))
	float Range = 1200.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Spawn", meta = (ExposeOnSpawn = true))
	float SourceRadius = 10.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Spawn", meta = (ExposeOnSpawn = true))
	float EdgeGain = 1.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Spawn", meta = (ExposeOnSpawn = true))
	bool IsEcho = false;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Spawn", meta = (ExposeOnSpawn = true))
	bool IsSpawningEchoes = true;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Spawn", meta = (ExposeOnSpawn = true))
	TObjectPtr<AActor> SourceActor;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Spawn", meta = (ExposeOnSpawn = true))
	FVector FaceCenter = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Spawn", meta = (ExposeOnSpawn = true))
	FVector FaceNormal = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Spawn", meta = (ExposeOnSpawn = true))
	FVector FaceU = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Spawn", meta = (ExposeOnSpawn = true))
	FVector FaceV = FVector::ZeroVector;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Spawn", meta = (ExposeOnSpawn = true))
	float FaceHalfU = 0.f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Wave|Spawn", meta = (ExposeOnSpawn = true))
	float FaceHalfV = 0.f;

	virtual void Tick(float DeltaSeconds) override;

protected:
	virtual void BeginPlay() override;

private:
	TArray<UWaveReceiverComponent*> FindReceivers() const;
	void SpawnEchoes();
	void TryFace(UWaveReceiverComponent* Receiver, const FVector& BoxCenter, const FVector& Axis, float Half, const FVector& U, const FVector& V, float HalfU, float HalfV);
	bool IsFaceVisible(UWaveReceiverComponent* Receiver, const FVector& TargetCenter, const FVector& N, const FVector& U, const FVector& V, float HalfU, float HalfV) const;
	void SetupEchoFace();

	UPROPERTY()
	TObjectPtr<UMaterialInstanceDynamic> MID;

	UPROPERTY()
	TArray<TObjectPtr<UWaveReceiverComponent>> Pending;
};
