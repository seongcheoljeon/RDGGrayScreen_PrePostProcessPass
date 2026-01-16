# RDGGrayScreen_PrePostProcessPass Plugin

## 개요
UE5용 커스텀 포스트 프로세싱 플러그인으로, RDG (Render Dependency Graph)와 Compute Shader를 활용하여 화면에 Grayscale 효과를 적용합니다. `PrePostProcessPass_RenderThread` 방식을 사용하여 렌더링 파이프라인에 직접 통합됩니다.

**버전**: 1.0  
**작성자**: Seongcheol Jeon  
**타겟 엔진**: Unreal Engine 5.7+  
**설명**: Gray screen using RDG + SceneViewExtension (PrePostProcessPass_RenderThread)

---

## 플러그인 구조

```
RDGGrayScreen_PrePostProcessPass/
├── Source/RDGGrayScreen_PrePostProcessPass/
│   ├── Public/
│   │   ├── RDGGrayScreen_PrePostProcessPass.h  # 모듈 정의
│   │   ├── RDGGrayScreenExtension.h            # SceneViewExtension 클래스
│   │   ├── RDGGrayScreenSubsystem.h            # WorldSubsystem 클래스
│   │   ├── RDGGrayScreenShaders.h              # Global Compute Shader 정의
│   │   ├── RDGGrayScreenTestActor.h            # 테스트용 Actor
│   │   └── RDGGrayScreenLog.h                  # 로깅 카테고리
│   ├── Private/
│   │   ├── (위 헤더 파일들의 구현부)
│   └── RDGGrayScreen_PrePostProcessPass.Build.cs  # 빌드 설정
├── Shaders/Private/
│   └── GrayScreen.usf                          # Compute Shader 코드
└── RDGGrayScreen_PrePostProcessPass.uplugin    # 플러그인 메타데이터
```

---

## 핵심 아키텍처

### 1. WorldSubsystem 기반 파라미터 관리
```cpp
URDGGrayScreenSubsystem
```
- **역할**: 플러그인의 진입점, SceneViewExtension 생성 및 라이프사이클 관리
- **Game Thread 파라미터**: `_gamethread_gray_screen`, `_gamethread_gray_gamma`
- **Thread 분리**: Game Thread와 Render Thread 데이터 완전 분리
- **Thread-safe**: `FCriticalSection`으로 동기화

### 2. SceneViewExtension - PrePostProcessPass 방식
```cpp
FRDGGrayScreenExtension : public FWorldSceneViewExtension
```
- **핵심 메서드**: `PrePostProcessPass_RenderThread(FRDGBuilder&, ...)`
- **실행 시점**: BasePass/Translucency 완료 직후, DOF/MotionBlur/Tonemap 등 본격적인 Post Processing Pass들이 시작되기 **직전**
- **Render Thread 파라미터**: `_renderthread_gray_screen`, `_renderthread_gray_gamma`
- **직접 수정**: SceneColor 텍스처를 직접 수정

### 3. RDG Compute Shader
```cpp
FRDGGrayScreenShaders : public FGlobalShader
```
- **셰이더 타입**: Compute Shader (SF_Compute)
- **Thread Group**: 8×8×1
- **Permutation**: `GAMMA_ENABLED` (bool) - 감마 보정 ON/OFF

---

## 렌더링 파이프라인 통합

### PrePostProcessPass 방식 동작 흐름

```
BeginRenderViewFamily()
    └→ TransferStateToExtension()  // GT → RT 파라미터 전송

PrePostProcessPass_RenderThread(FRDGBuilder&, ...)
    ├→ Inputs.Validate()  // 입력 검증
    ├→ SceneColorTexture 추출
    ├→ Output RDG Texture 생성 (TexCreate_UAV)
    ├→ Shader Parameters 설정
    ├→ Permutation 선택 (Gamma ON/OFF)
    ├→ FComputeShaderUtils::AddPass()
    └→ AddDrawTexturePass()  // Output → SceneColor 복사
```

### 렌더링 Pass 순서
```
1. BasePass (Opaque Geometry)
2. Lighting
3. Translucency
4. [PrePostProcessPass_RenderThread 실행]  ← 이 플러그인의 실행 시점
5. Depth of Field (DOF)
6. Motion Blur
7. Tonemap
8. [SubscribeToPostProcessingPass(Tonemap) 콜백 실행]  ← Subscribe 플러그인의 실행 시점
9. FXAA
10. Post Process Materials
```

**핵심 차이**:
- **PrePostProcessPass**: 포스트 프로세싱 효과들(DOF, MotionBlur, Tonemap) **이전**에 실행
- **Subscribe**: Subscribe Pass(Tonemap) **이후**에 실행

---

## Subscribe 방식과의 차이점

| 항목 | PrePostProcessPass 방식 (이 플러그인) | Subscribe 방식 |
|------|----------------------------------------|----------------|
| **오버라이드 메서드** | `PrePostProcessPass_RenderThread()` | `SubscribeToPostProcessingPass()` |
| **실행 시점** | BasePass/Translucency 직후, DOF/MotionBlur/Tonemap **이전** | Subscribe 특정 Pass **이후** (예: Tonemap 완료 후) |
| **SceneColor 접근** | `FPostProcessingInputs` 통해 직접 접근 | `FPostProcessMaterialInputs` 통해 접근 |
| **렌더링 방식** | SceneColor 직접 수정 | Delegate 콜백 통해 수정 |
| **유연성** | 하나의 Extension만 처리 가능 | 여러 Extension이 같은 Pass에 구독 가능 |
| **통합 복잡도** | 간단 (직접 호출) | 복잡 (Delegate 체인) |
| **디버깅** | 직관적 (콜스택 명확) | 복잡 (Delegate 추적 필요) |

---

## 주요 구현 세부사항

### 1. Thread-Safe 파라미터 전송 (Game Thread → Render Thread)

**완전 분리된 데이터 구조**
```cpp
// URDGGrayScreenSubsystem (Game Thread)
float _gamethread_gray_screen = 0.0f;
float _gamethread_gray_gamma = 1.0f;
mutable FCriticalSection _gamethread_parameter_cs;

// FRDGGrayScreenExtension (Render Thread)
float _renderthread_gray_screen = 0.0;
float _renderthread_gray_gamma = 1.0;
mutable FCriticalSection _render_parameter_cs;
```

**전송 메커니즘**
```cpp
void URDGGrayScreenSubsystem::TransferStateToExtension(FRDGGrayScreenExtension* extension) const
{
    if (!extension) return;
    
    float local_gray_screen, local_gray_gamma;
    {
        FScopeLock lock_(&_gamethread_parameter_cs);
        local_gray_screen = _gamethread_gray_screen;
        local_gray_gamma = _gamethread_gray_gamma;
    }
    
    if (::IsInRenderingThread())
    {
        // Render Thread에서 직접 호출 시 (PIE/Game)
        extension->SetParameters(local_gray_screen, local_gray_gamma);
    }
    else
    {
        // Game Thread에서 호출 시 (Editor Viewport)
        ENQUEUE_RENDER_COMMAND(TransferSCiiRDGGrayScreenState)(
            [extension, local_gray_screen, local_gray_gamma](FRHICommandListImmediate& rhi_cmd_list) -> void
            {
                extension->SetParameters(local_gray_screen, local_gray_gamma);
            });
    }
}
```

**특징**:
- Game Thread와 Render Thread 데이터 완전 분리
- `BeginRenderViewFamily`에서 호출
- Editor Viewport는 Game Thread, PIE/Game은 Render Thread에서 호출됨

---

### 2. PrePostProcessPass_RenderThread 구현

**입력 검증 및 텍스처 추출**
```cpp
void FRDGGrayScreenExtension::PrePostProcessPass_RenderThread(
    FRDGBuilder& GraphBuilder, 
    const FSceneView& InView,
    const FPostProcessingInputs& Inputs)
{
    check(::IsInRenderingThread());

    // Render Thread 데이터 읽기
    float local_gray_screen, local_gray_gamma;
    GetParameters(local_gray_screen, local_gray_gamma);

    if (FMath::IsNearlyZero(local_gray_screen))
    {
        return;  // 효과가 0이면 스킵
    }

    // 입력 검증
    Inputs.Validate();

    // SceneColor 텍스처 추출
    FRDGTextureRef scene_color_texture = (*Inputs.SceneTextures)->SceneColorTexture;
    if (!scene_color_texture)
    {
        UE_LOG(LogRDGGrayScreen, Error, TEXT("SceneColorTexture is NULL!"));
        return;
    }

    FIntPoint texture_size = scene_color_texture->Desc.Extent;
    // ...
}
```

**Output 텍스처 생성 및 Compute Pass**
```cpp
    // Output 텍스처 생성 (UAV 플래그 필수)
    FRDGTextureDesc output_desc = scene_color_texture->Desc;
    output_desc.Flags |= TexCreate_UAV | TexCreate_ShaderResource;
    output_desc.ClearValue = FClearValueBinding::Black;
    FRDGTexture* output_texture = GraphBuilder.CreateTexture(output_desc, TEXT("GrayScreenOutput"));

    // Shader Parameters 설정
    FRDGGrayScreenShaders::FParameters* pass_parameters = 
        GraphBuilder.AllocParameters<FRDGGrayScreenShaders::FParameters>();
    pass_parameters->InputTexture = scene_color_texture;
    pass_parameters->InputSampler = TStaticSamplerState<SF_Bilinear, AM_Clamp, AM_Clamp, AM_Clamp>::GetRHI();
    pass_parameters->OutputTexture = GraphBuilder.CreateUAV(output_texture);
    pass_parameters->GrayScreen = local_gray_screen;
    pass_parameters->GrayGamma = local_gray_gamma;
    pass_parameters->InputTextureSize = FVector2f(texture_size.X, texture_size.Y);

    // Permutation 설정
    FRDGGrayScreenShaders::FPermutationDomain permutation_vector;
    bool is_gamma_enabled = !FMath::IsNearlyEqual(local_gray_gamma, 1.0f, KINDA_SMALL_NUMBER);
    permutation_vector.Set<FRDGGrayScreenShaders::FGammaEnabled>(is_gamma_enabled);

    // Shader 가져오기
    FGlobalShaderMap* global_shader_map = GetGlobalShaderMap(InView.FeatureLevel);
    TShaderMapRef<FRDGGrayScreenShaders> compute_shader(global_shader_map, permutation_vector);

    // Thread Group 계산
    const int32 thread_group_size = 8;
    FIntVector thread_group_count(
        FMath::DivideAndRoundUp(texture_size.X, thread_group_size),
        FMath::DivideAndRoundUp(texture_size.Y, thread_group_size),
        1
    );

    // GPU Stat Scope
    RDG_GPU_STAT_SCOPE(GraphBuilder, RDGGrayScreenEffect);

    // 상세한 이벤트 이름
    FString event_name = FString::Printf(
        TEXT("RDGGrayScreenPass(Screen=%.2f,Gamma=%.2f,Permutation=%s)"),
        local_gray_screen,
        local_gray_gamma,
        is_gamma_enabled ? TEXT("GammaEnabled") : TEXT("GammaDisabled")
    );

    // Compute Pass 추가
    FComputeShaderUtils::AddPass(
        GraphBuilder,
        RDG_EVENT_NAME("%s", *event_name),
        ERDGPassFlags::Compute,
        compute_shader,
        pass_parameters,
        thread_group_count
    );

    // Output을 SceneColor로 복사
    AddDrawTexturePass(GraphBuilder, InView, output_texture, scene_color_texture);
}
```

---

### 3. AddDrawTexturePass를 통한 텍스처 교체

**SceneColor 직접 수정**
```cpp
// output_texture (Compute Shader 결과)를 scene_color_texture로 복사
AddDrawTexturePass(GraphBuilder, InView, output_texture, scene_color_texture);
```

**Subscribe 방식과의 차이**:
- **Subscribe 방식**: `OverrideOutput`이 존재하면 그쪽으로 복사, 아니면 새 텍스처 반환
- **PrePostProcessPass 방식**: 항상 SceneColor를 직접 수정

---

### 4. Shader Permutation을 통한 조건부 컴파일

**C++ 측 Permutation 정의**
```cpp
class FGammaEnabled : SHADER_PERMUTATION_BOOL("GAMMA_ENABLED");
using FPermutationDomain = TShaderPermutationDomain<FGammaEnabled>;
```

**런타임 Permutation 선택**
```cpp
FRDGGrayScreenShaders::FPermutationDomain permutation_vector;
bool is_gamma_enabled = !FMath::IsNearlyEqual(local_gray_gamma, 1.0f, KINDA_SMALL_NUMBER);
permutation_vector.Set<FRDGGrayScreenShaders::FGammaEnabled>(is_gamma_enabled);
TShaderMapRef<FRDGGrayScreenShaders> compute_shader(global_shader_map, permutation_vector);
```

**USF 측 조건부 코드**
```hlsl
#if GAMMA_ENABLED
    float exposure = max(GrayGamma, 0.0);
    float3 gray_result = grayscale * exposure;
#else
    float3 gray_result = grayscale;
#endif
```

---

## Compute Shader 구현

### GrayScreen.usf

**Grayscale 변환 공식**
```hlsl
float GetLuminance(float3 color)
{
    return dot(color, float3(0.299, 0.587, 0.114));  // ITU-R BT.601 계수
}
```

**메인 Compute Shader**
```hlsl
[numthreads(8, 8, 1)]
void MainPS(uint3 thread_id : SV_DispatchThreadID)
{
    uint2 pixel_pos = thread_id.xy;
    
    // 전체 텍스처 범위 체크
    if(pixel_pos.x >= InputTextureSize.x || pixel_pos.y >= InputTextureSize.y)
        return;

    // UV 계산
    float2 uv = (float2(pixel_pos) + 0.5) / InputTextureSize;
    
    // 원본 색상 샘플링
    float4 original_color = InputTexture.SampleLevel(InputSampler, uv, 0);
    
    // Grayscale 변환
    float luminance = GetLuminance(original_color.rgb);
    float3 grayscale = (float3)luminance;

#if GAMMA_ENABLED
    float exposure = max(GrayGamma, 0.0);
    float3 gray_result = grayscale * exposure;
#else
    float3 gray_result = grayscale;
#endif

    float3 final_color = lerp(original_color.rgb, gray_result, GrayScreen);
    
    OutputTexture[pixel_pos] = float4(final_color, original_color.a);
}
```

---

## 빌드 설정

### RDGGrayScreen_PrePostProcessPass.Build.cs

**Renderer Private Path 접근**
```csharp
string renderer_private_path = Path.Combine(EngineDirectory, "Source/Runtime/Renderer");

PrivateIncludePaths.AddRange(new string[] {
    Path.Combine(renderer_private_path, "Private"),
    Path.Combine(renderer_private_path, "Internal"),
});
```

**종속 모듈**
- **Public**: Core, CoreUObject, Engine
- **Private**: RenderCore, Renderer, RHI, Projects

---

## 데이터 흐름

### Game Thread → Render Thread 파라미터 전송

```
ARDGGrayScreenTestActor::Tick()  [Game Thread]
    ↓ SetParameters()
URDGGrayScreenSubsystem::SetParameters()  [Game Thread]
    ↓ _gamethread_gray_screen, _gamethread_gray_gamma 저장
    ↓ (FScopeLock으로 보호)
    ↓
FRDGGrayScreenExtension::BeginRenderViewFamily()  [Game/Render Thread]
    ↓ TransferStateToExtension()
URDGGrayScreenSubsystem::TransferStateToExtension()  [Game/Render Thread]
    ↓ 로컬 복사 (mutex)
    ↓ IsInRenderingThread() 체크
    ↓ YES → 직접 호출 / NO → ENQUEUE_RENDER_COMMAND
FRDGGrayScreenExtension::SetParameters()  [Render Thread]
    ↓ _renderthread_gray_screen, _renderthread_gray_gamma 설정
    ↓ (FScopeLock으로 보호)
    ↓
FRDGGrayScreenExtension::PrePostProcessPass_RenderThread()  [Render Thread]
    ↓ GetParameters() (mutex)
    ↓ Compute Shader 실행
```

---

## 사용 방법

### 1. 플러그인 활성화
`.uplugin` 파일 또는 프로젝트 설정에서 플러그인 활성화

### 2. 테스트 Actor 배치
```cpp
ARDGGrayScreenTestActor
```
- **UPROPERTY**: `_gray_screen` (0.0~1.0), `_gray_gamma` (0.0~3.0)
- **Tick()**: 매 프레임 Subsystem에 파라미터 전송

### 3. 런타임 제어
```cpp
auto* subsystem = GetWorld()->GetSubsystem<URDGGrayScreenSubsystem>();
if (subsystem)
{
    subsystem->SetParameters(0.5f, 1.2f);  // 50% grayscale, gamma 1.2
}
```

---

## 핵심 기술 요약

| 항목 | 구현 방식 |
|------|-----------|
| **렌더링 통합** | `PrePostProcessPass_RenderThread` (Early injection) |
| **Shader 타입** | Global Compute Shader (8×8 thread groups) |
| **RDG 사용** | `FRDGBuilder`, `FRDGTextureRef`, `FComputeShaderUtils::AddPass` |
| **Permutation** | `GAMMA_ENABLED` (bool) - 조건부 컴파일 |
| **Thread Safety** | 완전 분리된 GT/RT 데이터 + `FCriticalSection` |
| **파라미터 전송** | WorldSubsystem → SceneViewExtension (GT → RT) |
| **SceneColor 수정** | `AddDrawTexturePass` (직접 교체) |

---

## Extension 방식 비교

### 1. PrePostProcessPass_RenderThread (이 플러그인)
```cpp
virtual void PrePostProcessPass_RenderThread(
    FRDGBuilder& GraphBuilder, 
    const FSceneView& InView,
    const FPostProcessingInputs& Inputs) override;
```
- **장점**:
    - 간단한 구현 (직접 호출)
    - 명확한 콜스택
    - SceneColor 직접 접근
    - Early injection (DOF/MotionBlur/Tonemap 등 본격적인 PP Pass들 이전에 실행)
- **단점**:
    - 하나의 Extension만 처리 가능
    - 실행 순서 제어 어려움

### 2. SubscribeToPostProcessingPass (비교)
```cpp
virtual void SubscribeToPostProcessingPass(
    EPostProcessingPass Pass,
    const FSceneView& InView,
    FPostProcessingPassDelegateArray& InOutPassCallbacks,
    bool bIsPassEnabled) override;
```
- **장점**:
    - 여러 Extension이 같은 Pass에 구독 가능
    - 실행 순서 제어 가능
    - 특정 Pass 선택 가능 (Tonemap, DOF 등)
- **단점**:
    - Delegate 체인 디버깅 복잡
    - OverrideOutput 처리 필요

### 3. PostProcessPassAfterTonemap (비교)
```cpp
virtual FScreenPassTexture PostProcessPassAfterTonemap_RenderThread(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    const FPostProcessMaterialInputs& InOutInputs) override;
```
- **장점**:
    - Tonemap 이후 정확한 시점
    - 간단한 FScreenPassTexture 반환
- **단점**:
    - Tonemap 이후로만 제한
    - 하나의 Extension만 처리 가능

---

## 주요 디버깅 포인트

### 1. Shader가 컴파일되지 않는 경우
```cpp
// FRDGGrayScreen_PrePostProcessPassModule::StartupModule() 확인
AddShaderSourceDirectoryMapping(TEXT("/Plugin/RDGGrayScreen_PrePostProcessPass"), plugin_shader_dir);
```

### 2. 화면에 효과가 나타나지 않는 경우
- `_gray_screen > 0.0` 확인
- `ShouldBeActiveThisFrame()` 반환값 확인
- `PrePostProcessPass_RenderThread` 호출 여부 확인
- `IsActiveThisFrame_Internal()` 반환값 확인

### 3. Thread Safety 문제
- **Game Thread 데이터**: `_gamethread_gray_screen`, `_gamethread_gray_gamma`
- **Render Thread 데이터**: `_renderthread_gray_screen`, `_renderthread_gray_gamma`
- 각각 별도의 `FCriticalSection`으로 보호
- `ENQUEUE_RENDER_COMMAND` 사용 확인

### 4. BeginRenderViewFamily 호출 시점
- **Editor Viewport**: Game Thread에서 호출
- **PIE/Game**: Render Thread에서 호출
- `IsInRenderingThread()` 체크 필수

---

## 확장 가능성

### 1. 다른 Post-Processing 시점
```cpp
// PostProcessPassAfterFXAA 등 다른 메서드 오버라이드
virtual FScreenPassTexture PostProcessPassAfterFXAA_RenderThread(
    FRDGBuilder& GraphBuilder,
    const FSceneView& View,
    const FPostProcessMaterialInputs& InOutInputs) override;
```

### 2. 추가 Shader Permutation
```cpp
class FUseCustomLuma : SHADER_PERMUTATION_BOOL("USE_CUSTOM_LUMA");
using FPermutationDomain = TShaderPermutationDomain<FGammaEnabled, FUseCustomLuma>;
```

### 3. Dynamic Parameter Buffer
```cpp
// Uniform Buffer 또는 Structured Buffer로 파라미터 확장
SHADER_PARAMETER_RDG_BUFFER_SRV(StructuredBuffer<FMyParams>, ParamBuffer)
```

---

## PrePostProcessPass vs Subscribe 선택 가이드

### PrePostProcessPass 사용이 적합한 경우:
- **Early injection**이 필요한 경우 (DOF/MotionBlur/Tonemap 등의 본격적인 PP Pass들 이전에 실행)
- 단일 Extension만 사용하는 경우
- 간단한 구현과 디버깅이 중요한 경우
- SceneColor를 직접 수정해야 하는 경우

### Subscribe 사용이 적합한 경우:
- **특정 Post-Processing Pass 이후** 실행이 필요한 경우 (예: Tonemap 완료 후)
- 여러 Extension이 같은 Pass에 구독해야 하는 경우
- 실행 순서 제어가 필요한 경우
- Delegate 체인을 통한 복잡한 워크플로우가 필요한 경우

---

## 참고 자료

- **엔진 소스**: `Engine/Source/Runtime/Renderer/Private/PostProcess/`
- **SceneViewExtension**: `Engine/Source/Runtime/Engine/Public/SceneViewExtension.h`
- **FPostProcessingInputs**: `Engine/Source/Runtime/Renderer/Public/PostProcess/PostProcessInputs.h`
- **RDG 문서**: `Engine/Source/Runtime/RenderCore/Public/RenderGraphBuilder.h`

---

## 라이센스 및 저작권
Copyright Epic Games, Inc. All Rights Reserved.  
Author: Seongcheol Jeon (https://github.com/seongcheoljeon)
