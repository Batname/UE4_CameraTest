// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Networking.h"
#include "Interfaces/IImageWrapper.h"
#include "Interfaces/IImageWrapperModule.h"

#include <thread>
#include <chrono>
#include <future>
#include<mutex>

#include "UDPClientActor.generated.h"

class AUDPClientActor;

class FCamFrame
{
public:
	FCamFrame(FString YourChosenSocketName, FString TheIP, int32 ThePort, AUDPClientActor* UDPClientActor);
	~FCamFrame();

	bool Begin();
	void ReadSocket();
	void ReleaseSockets();

private:
	FString SocketName;
	FString TheIP;
	int32 ThePort;
	AUDPClientActor* UDPClientActor;

	std::mutex mutex;

	FSocket* ListenerSocket;
	FSocket* ConnectionSocket;
	FIPv4Endpoint RemoteAddressForConnection;
	long int TotalDataSize;

	std::thread ReadSocketThread;
	std::atomic_bool bIsReadSocketThreadRunning;

	FSocket* CreateTCPConnectionListener(const int32 ReceiveBufferSize = 2 * 1024 * 1024);

	// Wait for socket connection
	void TCPConnectionListener();


	//Format String IP4 to number array
	bool FormatIP4ToNumber(FString& TheIP, uint8(&Out)[4]);
};

class UMaterialInstanceDynamic;
class UTexture2D;

UCLASS()
class CAMERATEST_API AUDPClientActor : public AActor
{
	GENERATED_BODY()

	friend class FCamFrame;
	
public:	
	// Sets default values for this actor's properties
	AUDPClientActor();

	//Initialize the static mesh component
	UPROPERTY(VisibleAnywhere)
	UStaticMeshComponent* StaticMesh;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	/** Called whenever this actor is being removed from a level */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

// TCP thread
protected:
	std::thread CamFrameThread;
	FCamFrame* CamFrame;
	std::atomic_bool bIsFrameThreadRunning;
	std::mutex mutex;


private:

	uint32 NoDataCounter = 0;
	uint32 MaxNoDataCounter = 100;


// COPY TEXTURE
protected:
	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category=CameraTest)
	UMaterialInstanceDynamic* DynamicMaterialCamFrame = nullptr;

	UPROPERTY(VisibleAnywhere, BlueprintReadWrite, Category = CameraTest)
	UTexture2D* CameraTextureFrame = nullptr;

	//IImageWrapperPtr ImageWrapper;
	TArray<uint8> FrameDataArray;

	void CopyCamFrame(const TArray<uint8>& DataArray);

public:
	UFUNCTION(BlueprintCallable, Category = CameraTest)
	FORCEINLINE UTexture2D* GetCameraTextureFrame() { return CameraTextureFrame; }
};
