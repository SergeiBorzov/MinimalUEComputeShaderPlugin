// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RHIResources.h"
#include "Components/SceneCaptureComponent2D.h"
#include "Engine/TextureRenderTarget2D.h"

#include "TestActor.generated.h"


class FRHICommandListImmediate;

UCLASS()
class TESTPLUGIN_API ATestActor : public AActor
{
	GENERATED_BODY()
public:	
	ATestActor();
public:
	virtual void BeginPlay() override;
	virtual void Tick(float DeltaTime) override;
	//virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
	static void OnReadback(float Value);
private:
	void InitResources_RenderThread();
	void ExecuteComputeShader_RenderThread(FRHICommandListImmediate& RHICmdList);

	UPROPERTY(VisibleAnywhere)
	USceneCaptureComponent2D* SceneCaptureComponent2D = nullptr;
	UPROPERTY(VisibleAnywhere)
	UTextureRenderTarget2D* RenderTarget2D = nullptr;

	FStructuredBufferRHIRef OutputBufferRef;
	FUnorderedAccessViewRHIRef OutputBufferUAV;
	float DummyArray[1024] = { 0.0f };
};
