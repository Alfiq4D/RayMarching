#include "CustomComputeShader.h"
#include "CustomShaders/Public/CustomComputeShader.h"
#include "PixelShaderUtils.h"
#include "RenderCore/Public/RenderGraphUtils.h"
#include "MeshPassProcessor.inl"
#include "StaticMeshResources.h"
#include "DynamicMeshBuilder.h"
#include "RenderGraphResources.h"
#include "GlobalShader.h"
#include "UnifiedBuffer.h"
#include "CanvasTypes.h"
#include "MaterialShader.h"

DECLARE_STATS_GROUP(TEXT("CustomComputeShader"), STATGROUP_CustomComputeShader, STATCAT_Advanced);
DECLARE_CYCLE_STAT(TEXT("CustomComputeShader Execute"), STAT_CustomComputeShader_Execute, STATGROUP_CustomComputeShader);

// Bridge between cpp and HLSL.
class CUSTOMSHADERS_API FCustomComputeShader : public FGlobalShader
{
public:

	DECLARE_GLOBAL_SHADER(FCustomComputeShader);
	SHADER_USE_PARAMETER_STRUCT(FCustomComputeShader, FGlobalShader);

	class FCustomComputeShader_Perm_TEST : SHADER_PERMUTATION_INT("", 1);
	using FPermutationDomain = TShaderPermutationDomain<FCustomComputeShader_Perm_TEST>;

	BEGIN_SHADER_PARAMETER_STRUCT(FParameters, )

		SHADER_PARAMETER_RDG_TEXTURE_UAV(RWTexture2D, RenderTarget)
		SHADER_PARAMETER(FMatrix44f, CameraWorld)
		SHADER_PARAMETER(FMatrix44f, CameraInverseProjection)

	END_SHADER_PARAMETER_STRUCT()

public:
	static bool ShouldCompilePermutation(const FGlobalShaderPermutationParameters& Parameters)
	{
		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		return true;
	}

	static void ModifyCompilationEnvironment(const FGlobalShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment)
	{
		FGlobalShader::ModifyCompilationEnvironment(Parameters, OutEnvironment);

		const FPermutationDomain PermutationVector(Parameters.PermutationId);

		OutEnvironment.SetDefine(TEXT("THREADS_X"), NUM_THREADS_CustomComputeShader_X);
		OutEnvironment.SetDefine(TEXT("THREADS_Y"), NUM_THREADS_CustomComputeShader_Y);
		OutEnvironment.SetDefine(TEXT("THREADS_Z"), NUM_THREADS_CustomComputeShader_Z);
	}
};

// Create the shader.
IMPLEMENT_GLOBAL_SHADER(FCustomComputeShader, "/CustomShadersShaders/CustomComputeShader.usf", "CustomComputeShader", SF_Compute);

void FCustomComputeShaderInterface::DispatchRenderThread(FRHICommandListImmediate& RHICmdList, FCustomComputeShaderDispatchParams Params)
{
	FRDGBuilder GraphBuilder(RHICmdList);
	{
		SCOPE_CYCLE_COUNTER(STAT_CustomComputeShader_Execute);
		DECLARE_GPU_STAT(CustomComputeShader)
		RDG_EVENT_SCOPE(GraphBuilder, "CustomComputeShader");
		RDG_GPU_STAT_SCOPE(GraphBuilder, CustomComputeShader);

		typename FCustomComputeShader::FPermutationDomain PermutationVector;
		TShaderMapRef<FCustomComputeShader> ComputeShader(GetGlobalShaderMap(GMaxRHIFeatureLevel), PermutationVector);

		bool bIsShaderValid = ComputeShader.IsValid();
		if (bIsShaderValid) {
			FCustomComputeShader::FParameters* PassParameters = GraphBuilder.AllocParameters<FCustomComputeShader::FParameters>();

			FRDGTextureDesc Desc(FRDGTextureDesc::Create2D(Params.RenderTarget->GetSizeXY(), PF_B8G8R8A8, FClearValueBinding::Black, TexCreate_RenderTargetable | TexCreate_ShaderResource | TexCreate_UAV));
			FRDGTextureRef TmpTexture = GraphBuilder.CreateTexture(Desc, TEXT("CustomComputeShader_TempTexture"));
			FRDGTextureRef TargetTexture = RegisterExternalTexture(GraphBuilder, Params.RenderTarget->GetRenderTargetTexture(), TEXT("CustomComputeShader_RT"));
			PassParameters->RenderTarget = GraphBuilder.CreateUAV(TmpTexture);
			PassParameters->CameraWorld = FMatrix44f(Params.CameraWorld);
			PassParameters->CameraInverseProjection = FMatrix44f(Params.CameraInverseProjection);

			auto GroupCount = FComputeShaderUtils::GetGroupCount(FIntVector(Params.X, Params.Y, Params.Z), FComputeShaderUtils::kGolden2DGroupSize);
			GraphBuilder.AddPass(
				RDG_EVENT_NAME("ExecuteCustomComputeShader"),
				PassParameters,
				ERDGPassFlags::AsyncCompute,
				[&PassParameters, ComputeShader, GroupCount](FRHIComputeCommandList& RHICmdList)
				{
					FComputeShaderUtils::Dispatch(RHICmdList, ComputeShader, *PassParameters, GroupCount);
				});

			// Check if render target formats are matching.
			if (TargetTexture->Desc.Format == PF_B8G8R8A8)
			{
				AddCopyTexturePass(GraphBuilder, TmpTexture, TargetTexture, FRHICopyTextureInfo());
			}
			else
			{
			#if WITH_EDITOR
				GEngine->AddOnScreenDebugMessage((uint64)42145125184, 6.f, FColor::Red, FString(TEXT("The provided render target has an incompatible format (Should be: RGBA8).")));
			#endif
			}
		}
		else
		{
		#if WITH_EDITOR
			GEngine->AddOnScreenDebugMessage((uint64)42145125184, 6.f, FColor::Red, FString(TEXT("The compute shader is not valid.")));
		#endif
		}
	}
	GraphBuilder.Execute();
}