// Fill out your copyright notice in the Description page of Project Settings.


#include "TestActor.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "DynamicRHI.h"
#include "RenderGraphUtils.h"
#include "RenderGraphBuilder.h"

class FTestCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTestCS);
	SHADER_USE_PARAMETER_STRUCT(FTestCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, OutputBuffer)
	END_SHADER_PARAMETER_STRUCT()
public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		return IsFeatureLevelSupported(Parameters.Platform, ERHIFeatureLevel::SM5);
	}

	static inline void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);
	}
};
IMPLEMENT_GLOBAL_SHADER(FTestCS, "/CustomShaders/TestCompute.usf", "Main", SF_Compute);


// Sets default values
ATestActor::ATestActor()
{
	PrimaryActorTick.bCanEverTick = true;
}


void ATestActor::InitResources_RenderThread()
{
	check(IsInRenderingThread());

	TResourceArray<float> Data;
	Data.Init(0.0f, 1);
	struct FRHIResourceCreateInfo CreateInfo;
	CreateInfo.ResourceArray = &Data;
	OutputBufferRef = RHICreateStructuredBuffer(sizeof(float), sizeof(float), BUF_UnorderedAccess | BUF_ShaderResource, CreateInfo);
	OutputBufferUAV = RHICreateUnorderedAccessView(OutputBufferRef, false, false);
}

void ATestActor::ExecuteComputeShader_RenderThread(FRHICommandListImmediate& RHICmdList)
{
	check(IsInRenderingThread());

	RHICmdList.TransitionResource(EResourceTransitionAccess::ERWBarrier, EResourceTransitionPipeline::EGfxToCompute, OutputBufferUAV);

	FTestCS::FParameters PassParameters;
	PassParameters.OutputBuffer = OutputBufferUAV;

	TShaderMapRef<FTestCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, PassParameters, FIntVector(1, 1, 1));

	float* MappedBuffer = (float*)RHILockStructuredBuffer(OutputBufferRef, 0, sizeof(float), RLM_ReadOnly);
	SavedValue = *MappedBuffer;
	//FMemory::Memcpy(cellIndexBuffer.GetData(), cellIndexData, numBoids * sizeof(uint32_t));
	RHIUnlockStructuredBuffer(OutputBufferRef);
}
// Called when the game starts or when spawned
void ATestActor::BeginPlay()
{
	Super::BeginPlay();
	ENQUEUE_RENDER_COMMAND(InitResources)([this](FRHICommandListImmediate& RHICmdList) { InitResources_RenderThread(); });
}

// Called every frame
void ATestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ENQUEUE_RENDER_COMMAND(RunShader)([this](FRHICommandListImmediate& RHICmdList) { ExecuteComputeShader_RenderThread(RHICmdList); });
	FlushRenderingCommands();
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("LOCATION: %f"), SavedValue));
}

