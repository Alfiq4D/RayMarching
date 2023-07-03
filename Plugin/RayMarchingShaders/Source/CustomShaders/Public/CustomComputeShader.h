#pragma once

#include "CoreMinimal.h"
#include "GenericPlatform/GenericPlatformMisc.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "Engine/TextureRenderTarget2D.h"

#include "CustomComputeShader.generated.h"

struct CUSTOMSHADERS_API FCustomComputeShaderDispatchParams
{
	FCustomComputeShaderDispatchParams(int x, int y, int z)
		: X(x)
		, Y(y)
		, Z(z)
	{}

	int X;
	int Y;
	int Z;
	
	FRenderTarget* RenderTarget;
	FMatrix CameraWorld;
	FMatrix CameraInverseProjection;
};

class CUSTOMSHADERS_API FCustomComputeShaderInterface {
public:
	static void DispatchRenderThread(
		FRHICommandListImmediate& RHICmdList,
		FCustomComputeShaderDispatchParams Params
	);

	static void DispatchGameThread(
		FCustomComputeShaderDispatchParams Params
	)
	{
		ENQUEUE_RENDER_COMMAND(SceneDrawCompletion)(
		[Params](FRHICommandListImmediate& RHICmdList)
		{
			DispatchRenderThread(RHICmdList, Params);
		});
	}

	static void Dispatch(
		FCustomComputeShaderDispatchParams Params
	)
	{
		if (IsInRenderingThread()) {
			DispatchRenderThread(GetImmediateCommandList_ForRenderCommand(), Params);
		}else{
			DispatchGameThread(Params);
		}
	}
};

UCLASS()
class CUSTOMSHADERS_API UCustomComputeShaderLibrary : public UObject
{
	GENERATED_BODY()

public:	
	UFUNCTION(BlueprintCallable, meta = (Category = "ComputeShader", WorldContext = "WorldContextObject"))
	static void ExecuteRayMarchingComputeShader(UTextureRenderTarget2D* RT, FMatrix CameraView, FMatrix CameraProjection) {

		FCustomComputeShaderDispatchParams Params(RT->SizeX, RT->SizeY, 1);
		Params.RenderTarget = RT->GameThread_GetRenderTargetResource();
		Params.CameraWorld = CameraView.Inverse();
		Params.CameraInverseProjection = CameraProjection.Inverse();

		FCustomComputeShaderInterface::Dispatch(Params);
	}
};