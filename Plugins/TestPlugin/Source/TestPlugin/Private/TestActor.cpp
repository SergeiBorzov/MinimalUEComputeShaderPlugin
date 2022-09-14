// Fill out your copyright notice in the Description page of Project Settings.


#include "TestActor.h"
#include "RHI.h"
#include "RHICommandList.h"
#include "DynamicRHI.h"
#include "RenderGraphUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderTargetPool.h"
#include "RenderGraphResources.h"

#define USE_UNREAL_RDG

class FTestCS : public FGlobalShader
{
public:
	DECLARE_GLOBAL_SHADER(FTestCS);
	SHADER_USE_PARAMETER_STRUCT(FTestCS, FGlobalShader);

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )
#ifndef USE_UNREAL_RDG
		SHADER_PARAMETER_TEXTURE(Texture2D<float>, InputDepthMap)
#else
		SHADER_PARAMETER_RDG_TEXTURE(Texture2D<float>, InputDepthMap)
#endif
		SHADER_PARAMETER_SAMPLER(SamplerState, InputDepthMapSampler)
#ifndef USE_UNREAL_RDG
		SHADER_PARAMETER_UAV(RWStructuredBuffer<float>, OutputBuffer)
#else
		SHADER_PARAMETER_RDG_BUFFER_UAV(RWStructuredBuffer<float>, OutputBuffer)
#endif
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

	SceneCaptureComponent2D = CreateDefaultSubobject<USceneCaptureComponent2D>(TEXT("SceneCaptureComponent"));
	SceneCaptureComponent2D->AttachTo(RootComponent);
	SceneCaptureComponent2D->CaptureSource = ESceneCaptureSource::SCS_SceneDepth;
	SceneCaptureComponent2D->FOVAngle = 45.0f;
	SceneCaptureComponent2D->bCaptureOnMovement = false;
	SceneCaptureComponent2D->bCaptureEveryFrame = true;

	static ConstructorHelpers::FObjectFinder<UTextureRenderTarget2D> RenderTarget2DRef(TEXT("/Game/TestTarget"));
	RenderTarget2D = RenderTarget2DRef.Object;

	//RenderTarget2D = NewObject<UTextureRenderTarget2D>();
	//RenderTarget2D->AddToRoot();
	//RenderTarget2D->SizeX = 512;
	//RenderTarget2D->SizeY = 512;
	//RenderTarget2D->RenderTargetFormat = ETextureRenderTargetFormat::RTF_R32f;
	//RenderTarget2D->bAutoGenerateMips = false;
	//RenderTarget2D->bForceLinearGamma = false;
	//RenderTarget2D->bGPUSharedFlag = true;
	//RenderTarget2D->UpdateResource();

	SceneCaptureComponent2D->TextureTarget = RenderTarget2D;
}


void ATestActor::InitResources_RenderThread()
{
#ifndef USE_UNREAL_RDG
	check(IsInRenderingThread());

	TResourceArray<float> Data;
	Data.Init(0.0f, 1024);
	struct FRHIResourceCreateInfo CreateInfo;
	CreateInfo.ResourceArray = &Data;
	OutputBufferRef = RHICreateStructuredBuffer(sizeof(float), sizeof(float)*1024, BUF_UnorderedAccess | BUF_ShaderResource, CreateInfo);
	OutputBufferUAV = RHICreateUnorderedAccessView(OutputBufferRef, false, false);

	ensure(OutputBufferRef.IsValid());
	ensure(OutputBufferUAV.IsValid());
#endif
}

void ATestActor::ExecuteComputeShader_RenderThread(FRHICommandListImmediate& RHICmdList)
{
#ifndef USE_UNREAL_RDG
	check(IsInRenderingThread());

	FTestCS::FParameters PassParameters;
	PassParameters.InputDepthMap = SceneCaptureComponent2D->TextureTarget->Resource->GetTexture2DRHI();
	PassParameters.OutputBuffer = OutputBufferUAV;
	PassParameters.InputDepthMapSampler = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

	TShaderMapRef<FTestCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));
	FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, PassParameters, FIntVector(8, 1, 1));

	float* MappedBuffer = (float*)RHILockStructuredBuffer(OutputBufferRef, 0, sizeof(float)*1024, RLM_ReadOnly);
	FMemory::Memcpy(DummyArray, MappedBuffer, sizeof(float)*1024);
	float Value = MappedBuffer[0];
	RHIUnlockStructuredBuffer(OutputBufferRef);
	OnReadback(Value);
#else 
	check(IsInRenderingThread());
	FRDGBuilder GraphBuilder(RHICmdList);
	TShaderMapRef<FTestCS> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel));

	FTestCS::FParameters* PassParameters = GraphBuilder.AllocParameters<FTestCS::FParameters>();

	// Create rdg uav for output buffer
	auto OutputBufferDesc = FRDGBufferDesc::CreateBufferDesc(sizeof(float), 1024);
	//OutputBufferDesc.UnderlyingType = FRDGBufferDesc::EUnderlyingType::VertexBuffer;
	OutputBufferDesc.Usage = BUF_SourceCopy | BUF_UnorderedAccess;

	FRDGBufferRef OutputBuffer = GraphBuilder.CreateBuffer(OutputBufferDesc, TEXT("OutputBuffer"));
	PassParameters->OutputBuffer = GraphBuilder.CreateUAV(FRDGBufferUAVDesc(OutputBuffer, PF_R32_FLOAT));
	PassParameters->InputDepthMapSampler = TStaticSamplerState<SF_Point, AM_Wrap, AM_Wrap, AM_Wrap>::GetRHI();

	auto RHIRef = SceneCaptureComponent2D->TextureTarget->Resource->GetTexture2DRHI();
	FSceneRenderTargetItem Item;
	Item.ShaderResourceTexture = SceneCaptureComponent2D->TextureTarget->Resource->GetTexture2DRHI();

	FPooledRenderTargetDesc PooledRenderTargetDesc = FPooledRenderTargetDesc::Create2DDesc(RHIRef->GetSizeXY(),
		RHIRef->GetFormat(), FClearValueBinding::Black, TexCreate_Dynamic | TexCreate_InputAttachmentRead  | TexCreate_ShaderResource, TexCreate_RenderTargetable, false);

	TRefCountPtr<IPooledRenderTarget> DepthMapPooledRenderTarget;	
	GRenderTargetPool.CreateUntrackedElement(PooledRenderTargetDesc, DepthMapPooledRenderTarget, Item);
	
	auto InputDepthMapRDG = GraphBuilder.RegisterExternalTexture(DepthMapPooledRenderTarget, TEXT("SceneColor"));
	PassParameters->InputDepthMap = InputDepthMapRDG;

	// Determine group count
	FIntVector GroupCount = FIntVector(8, 1, 1);
	GraphBuilder.AddPass
	(
		RDG_EVENT_NAME("DispatchComputeShader"),
		PassParameters,
		ERDGPassFlags::AsyncCompute,
		[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
		{
			FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
		}
	);

	FRHIGPUBufferReadback* GPUBufferReadback = new FRHIGPUBufferReadback(TEXT("ExecuteMySimpleComputeShaderOutput"));
	AddEnqueueCopyPass(GraphBuilder, GPUBufferReadback, OutputBuffer, 0u);

	auto RunnerFunc = [GPUBufferReadback](auto&& RunnerFunc) -> void {
		if (GPUBufferReadback->IsReady()) {
			float DestArray[1024] = { 0 };
			float* MappedBuffer = (float*)GPUBufferReadback->Lock(sizeof(float)*1024);
			float Value = MappedBuffer[0];
			FMemory::Memcpy(DestArray, MappedBuffer, sizeof(float) * 1024);
			GPUBufferReadback->Unlock();

			AsyncTask(ENamedThreads::GameThread, [Value]() {
				ATestActor::OnReadback(Value);
			});
			delete GPUBufferReadback;
		}
		else {
			AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
				RunnerFunc(RunnerFunc);
			});
		}
	};

	AsyncTask(ENamedThreads::ActualRenderingThread, [RunnerFunc]() {
		RunnerFunc(RunnerFunc);
	});
	GraphBuilder.Execute();
#endif
}

// Called when the game starts or when spawned
void ATestActor::BeginPlay()
{
	Super::BeginPlay();
	ENQUEUE_RENDER_COMMAND(InitResources)([this](FRHICommandListImmediate& RHICmdList) { InitResources_RenderThread(); });
	FlushRenderingCommands();
}

// Called every frame
void ATestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
	ENQUEUE_RENDER_COMMAND(RunShader)([this](FRHICommandListImmediate& RHICmdList) { ExecuteComputeShader_RenderThread(RHICmdList); });
}

void ATestActor::OnReadback(float Value)
{
	GEngine->AddOnScreenDebugMessage(-1, 5.0f, FColor::Red, FString::Printf(TEXT("Value: %f"), Value));
}

