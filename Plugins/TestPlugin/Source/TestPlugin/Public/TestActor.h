// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RHIResources.h"


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
private:
	void InitResources_RenderThread();
	void ExecuteComputeShader_RenderThread(FRHICommandListImmediate& RHICmdList);

	FStructuredBufferRHIRef OutputBufferRef;
	FUnorderedAccessViewRHIRef OutputBufferUAV;
	float SavedValue = 0.0f;
};
