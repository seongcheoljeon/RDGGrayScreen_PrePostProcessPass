


#include "RDGGrayScreenTestActor.h"
#include "RDGGrayScreenLog.h"
#include "RDGGrayScreenSubsystem.h"


// Sets default values
ARDGGrayScreenTestActor::ARDGGrayScreenTestActor()
{
	PrimaryActorTick.bCanEverTick = true;

}

// Called when the game starts or when spawned
void ARDGGrayScreenTestActor::BeginPlay()
{
	Super::BeginPlay();
	
	UE_LOG(LogRDGGrayScreen, Log, TEXT("RDGGrayScreenTestActor: Startd!"));

	URDGGrayScreenSubsystem* subsystem = GetWorld()->GetSubsystem<URDGGrayScreenSubsystem>();
	if (subsystem && subsystem->GetExtension())
	{
		UE_LOG(LogRDGGrayScreen, Log, TEXT("RDGGrayScreenTestActor: Extension found!"));
	}
	else
	{
		UE_LOG(LogRDGGrayScreen, Error, TEXT("RDGGrayScreenTestActor: Extension NOT found!"));
	}
}

void ARDGGrayScreenTestActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

    // BeginRenderViewFamily에서 자동으로 Render Thread로 전달됨
	URDGGrayScreenSubsystem* subsystem = GetWorld()->GetSubsystem<URDGGrayScreenSubsystem>();
	if (subsystem)
	{
		subsystem->SetParameters(this->_gray_screen, this->_gray_gamma);
	}

	// debug
	static double last_logtime = 0.0;
	double curt_time = FPlatformTime::Seconds();
	if (curt_time - last_logtime > 1.0)
	{
		UE_LOG(LogRDGGrayScreen, Warning, TEXT("RDGGrayScreenTestActor [GT]: GrayScreen=%.3f, GrayGamma=%.3f")
			, this->_gray_screen, this->_gray_gamma);
		last_logtime = curt_time;
	}
}


