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
#include <mutex>

#include "UDPClientActor.generated.h"

class AUDPClientActor;
class FCamFrame;
class UMaterialInstanceDynamic;
class UTexture2D;
class UStaticMeshComponent;

class FCamFrame
{
public:
	FCamFrame(AUDPClientActor* UDPClientActor);
	~FCamFrame();

	bool Begin();
	void ReadSocket();
	void ReleaseSockets();

private:
	FString SocketName;
	AUDPClientActor* UDPClientActor;

	std::atomic_bool bIsReadSocketThreadRunning;
	std::thread ReadSocketThread;

	std::mutex mutex;

	FSocket* ListenerSocket;
	FSocket* ConnectionSocket;
	FIPv4Endpoint RemoteAddressForConnection;
	long int TotalDataSize;

	FSocket* CreateTCPConnectionListener(const int32 ReceiveBufferSize = 2 * 1024 * 1024);

	// Wait for socket connection
	void TCPConnectionListener();


	//Format String IP4 to number array
	bool FormatIP4ToNumber(FString& TheIP, uint8(&Out)[4]);
};

UCLASS()
class CAMERATEST_API AUDPClientActor : public AActor
{
	GENERATED_BODY()

	friend class FCamFrame;
	
public:	
	// Sets default values for this actor's properties
	AUDPClientActor();

protected:
	//Initialize the static mesh component
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = CameraTest)
	UStaticMeshComponent* StaticMesh;

	UPROPERTY(BlueprintReadWrite, Category = CameraTest)
	UMaterialInstanceDynamic* DynamicMaterialCamFrame;

	UPROPERTY(BlueprintReadWrite, Category = CameraTest)
	UTexture2D* CamTextureFrame;

	UPROPERTY(BlueprintReadWrite, Category = CameraTest)
	FString SocketName = "";

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = CameraTest)
	FString TheIP = "127.0.0.1";

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = CameraTest)
	int32 ThePort = 0;

	UPROPERTY(BlueprintReadWrite, Category = CameraTest)
	int32 NoDataCounter = 0;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = CameraTest)
	int32 MaxNoDataCounter = 100;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = CameraTest)
	int32 FrameWidth = 648;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = CameraTest)
	int32 FrameHeight = 488;

	UPROPERTY(EditDefaultsOnly, BlueprintReadWrite, Category = CameraTest)
	int32 BytesPerColor = 4;

	//IImageWrapperPtr ImageWrapper;
	TArray<uint8> FrameDataArray;

protected:
	UFUNCTION(BlueprintCallable, Category = CameraTest)
	void CopyCamFrame(const TArray<uint8>& DataArray);

private:
	int32 FrameSize;

// TCP thread
protected:
	std::thread CamFrameThread;
	FCamFrame* CamFrame = nullptr;
	std::atomic_bool bIsFrameThreadRunning = false;
	std::mutex mutex;

protected:
	// Called when the game starts or when spawned
	virtual void BeginPlay() override;

	/** Called whenever this actor is being removed from a level */
	virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:	
	// Called every frame
	virtual void Tick(float DeltaTime) override;

};
