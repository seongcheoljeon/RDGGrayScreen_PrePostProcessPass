#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "RDGGrayScreenTestActor.generated.h"

/*
* RDGGrayScreen 효과 테스트용 Actor
* Level에 배치하고 파라미터를 조정하면 실시간으로 효과가 반영됨.
*
* 데이터 흐름:
* TestActor (Game Thread)
*	→ Subsystem::SetParameters (Game Thread 저장)
*	→ BeginRenderViewFamily (Render Thread 복사)
*	→ Extension (Render Thread 사용)
*/

UCLASS()
class RDGGRAYSCREEN_PREPOSTPROCESSPASS_API ARDGGrayScreenTestActor : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	ARDGGrayScreenTestActor();

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GrayScreen", meta=(ClampMin="0.0", ClampMax="1.0"))
	float _gray_screen = 0.0f;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="GrayScreen", meta=(ClampMin="0.0", ClampMax="3.0"))
	float _gray_gamma = 1.0f;
};
