// Fill out your copyright notice in the Description page of Project Settings.

#include "UDPClientActor.h"
#include "Runtime/Engine/Public/TimerManager.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Runtime/Engine/Classes/Components/StaticMeshComponent.h"


FCamFrame::FCamFrame(FString YourChosenSocketName, FString TheIP, int32 ThePort, AUDPClientActor* UDPClientActor)
	: SocketName(YourChosenSocketName)
	, TheIP(TheIP)
	, ThePort(ThePort)
	, UDPClientActor(UDPClientActor)
	, ListenerSocket(nullptr)
	, ConnectionSocket(nullptr)
	, TotalDataSize(0)
	, bIsReadSocketThreadRunning(false)
{
}

FCamFrame::~FCamFrame()
{

	if (bIsReadSocketThreadRunning == true)
	{
		bIsReadSocketThreadRunning = false;

		std::this_thread::sleep_for(std::chrono::milliseconds(200));

		ReadSocketThread.join();
	}

	ReleaseSockets();
}

bool FCamFrame::Begin()
{
	//Rama's CreateTCPConnectionListener
	ListenerSocket = CreateTCPConnectionListener();

	//Not created?
	if (!ListenerSocket)
	{
		UE_LOG(LogTemp, Warning, TEXT("StartTCPReceiver>> Listen socket could not be created! ~> %s %d"), *TheIP, ThePort);
		return false;
	}

	// Wait for connection
	TCPConnectionListener();

	return true;
}

void FCamFrame::TCPConnectionListener()
{
	while (UDPClientActor->bIsFrameThreadRunning == true)
	{
		//Remote address
		TSharedRef<FInternetAddr> RemoteAddress = ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->CreateInternetAddr();
		bool Pending;

		// handle incoming connections
		if (ListenerSocket->HasPendingConnection(Pending) && Pending)
		{
			UE_LOG(LogTemp, Warning, TEXT("ListenerSocket->HasPendingConnection(Pending) && Pending"));

			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
			//Already have a Connection? destroy previous
			if (ConnectionSocket)
			{
				ConnectionSocket->Close();
				ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket);
			}
			//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

			//New Connection receive!
			ConnectionSocket = ListenerSocket->Accept(*RemoteAddress, TEXT("RamaTCP Received Socket Connection"));

			UE_LOG(LogTemp, Warning, TEXT("TCPConnectionListener >> ConnectionSocket %p"), ConnectionSocket);


			if (ConnectionSocket != NULL)
			{
				//Global cache of current Remote Address
				RemoteAddressForConnection = FIPv4Endpoint(RemoteAddress);

				if (bIsReadSocketThreadRunning == false)
				{
					bIsReadSocketThreadRunning = true;

					ReadSocketThread = std::thread([&]
					{
						ReadSocket();
					});
				}
			}
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
}

void FCamFrame::ReadSocket()
{
	while (bIsReadSocketThreadRunning == true)
	{
		if (!ConnectionSocket)
		{
			UE_LOG(LogTemp, Warning, TEXT("No ConnectionSocket"));
			return;
		}

		//Binary Array!
		TArray<uint8> ReceivedData;
		TArray<uint8> Buffer;

		uint32 Size;
		TotalDataSize = 0;
		while (ConnectionSocket->HasPendingData(Size) && bIsReadSocketThreadRunning == true)
		{
			ReceivedData.Init(0, FMath::Min(Size, 65507u));

			int32 Read = 0;
			ConnectionSocket->Recv(ReceivedData.GetData(), ReceivedData.Num(), Read);

			//UE_LOG(LogTemp, Warning, TEXT("TotalDataSize %d, ReceivedData.Num() %d"), TotalDataSize, ReceivedData.Num());

			TotalDataSize += ReceivedData.Num();

			if (TotalDataSize > 648 * 488 * 4)
			{
				break;
			}

			// Copy to image array buffer
			Buffer.Append(ReceivedData.GetData(), ReceivedData.Num());
		}
		//UE_LOG(LogTemp, Warning, TEXT("Total Data read! %d, Buffer size %d"), TotalDataSize, Buffer.Num());

		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		// Just for prevent crash
		if (Buffer.Num() == 648 * 488 * 4)
		{
			//UE_LOG(LogTemp, Warning, TEXT("Copy to image array buffer Buffer[100] %d"), Buffer[100]);

			// Copy to image array buffer
			mutex.lock();
			UDPClientActor->FrameDataArray.Reset();
			UDPClientActor->FrameDataArray.Append(Buffer.GetData(), Buffer.Num());
			mutex.unlock();
		}

		std::this_thread::sleep_for(std::chrono::milliseconds(5));
	}
}

void FCamFrame::ReleaseSockets()
{
	//Clear all sockets!
	//		makes sure repeat plays in Editor dont hold on to old sockets!
	if (ListenerSocket)
	{
		ListenerSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ListenerSocket);
		ListenerSocket = nullptr;

	}

	if (ConnectionSocket)
	{
		ConnectionSocket->Close();
		ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM)->DestroySocket(ConnectionSocket);
		ConnectionSocket = nullptr;
	}
}

FSocket * FCamFrame::CreateTCPConnectionListener(const int32 ReceiveBufferSize)
{
	uint8 IP4Nums[4];
	if (!FormatIP4ToNumber(TheIP, IP4Nums))
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid IP! Expecting 4 parts separated by "));

		return false;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	//Create Socket
	FIPv4Endpoint Endpoint(FIPv4Address(IP4Nums[0], IP4Nums[1], IP4Nums[2], IP4Nums[3]), ThePort);
	FSocket* ListenSocket = FTcpSocketBuilder(*SocketName)
		.AsReusable()
		.BoundToEndpoint(Endpoint)
		.Listening(8);

	//Set Buffer Size
	int32 NewSize = 0;
	ListenSocket->SetReceiveBufferSize(ReceiveBufferSize, NewSize);

	//Done!
	return ListenSocket;
}

bool FCamFrame::FormatIP4ToNumber(FString & TheIP, uint8(&Out)[4])
{
	//IP Formatting
	TheIP.Replace(TEXT(" "), TEXT(""));

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
	//						   IP 4 Parts

	//String Parts
	TArray<FString> Parts;
	TheIP.ParseIntoArray(Parts, TEXT("."), true);
	if (Parts.Num() != 4)
		return false;

	//String to Number Parts
	for (int32 i = 0; i < 4; ++i)
	{
		Out[i] = FCString::Atoi(*Parts[i]);
	}

	return true;
}


// Sets default values
AUDPClientActor::AUDPClientActor()
{
 	// Set this actor to call Tick() every frame.  You can turn this off to improve performance if you don't need it.
	PrimaryActorTick.bCanEverTick = true;

	//Initialize the static mesh component
	StaticMesh = CreateDefaultSubobject<UStaticMeshComponent>(FName("StaticMesh"));

	// allocate array
	FrameDataArray.AddUninitialized(648 * 488 * 4);
}

// Called when the game starts or when spawned
void AUDPClientActor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT(" AUDPClientActor::BeginPlay()"));

	CameraTextureFrame = UTexture2D::CreateTransient(648, 488, PF_B8G8R8A8);

	DynamicMaterialCamFrame = StaticMesh->CreateAndSetMaterialInstanceDynamic(0);

	// Run thread
	if (bIsFrameThreadRunning == false)
	{
		bIsFrameThreadRunning = true;
		CamFrameThread = std::thread([&]
		{
			CamFrame = new FCamFrame("RamaSocketListener", "127.0.0.1", 8889, this);
			CamFrame->Begin();
		});
	}

}

void AUDPClientActor::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	Super::EndPlay(EndPlayReason);
	//~~~~~~~~~~~~~~~~


	if (bIsFrameThreadRunning == true)
	{
		bIsFrameThreadRunning = false;

		FPlatformProcess::Sleep(1);

		delete CamFrame;
		CamFrame = nullptr;
		CamFrameThread.join();
	}

	FrameDataArray.Empty();
}

// Called every frame
void AUDPClientActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CamFrame)
	{
		if (FrameDataArray.Num() > 0)
		{
			CopyCamFrame(FrameDataArray);
		}
	}

}

void AUDPClientActor::CopyCamFrame(const TArray<uint8>& DataArray)
{
	IImageWrapperModule& ImageWrapperModule = FModuleManager::LoadModuleChecked<IImageWrapperModule>(FName("ImageWrapper"));
	IImageWrapperPtr ImageWrapper = ImageWrapperModule.CreateImageWrapper(EImageFormat::JPEG);



	TArray<uint8> FrameDataArrayCopy;

	mutex.lock();
	FrameDataArrayCopy.Append(DataArray.GetData(), DataArray.Num());
	mutex.unlock();

	bool bIsSet = ImageWrapper->SetRaw(FrameDataArrayCopy.GetData(), FrameDataArrayCopy.Num(), 648, 488, ERGBFormat::BGRA, 8);


	if (ImageWrapper.IsValid() && bIsSet)
	{
		//UE_LOG(LogTemp, Warning, TEXT("ImageWrapper.IsValid() && bIsSet"));

		const TArray<uint8>* UncompressedBGRA = NULL;
		if (ImageWrapper->GetRaw(ERGBFormat::BGRA, 8, UncompressedBGRA))
		{
			// That is slow for copy texture, make it async later
			void* TextureData = CameraTextureFrame->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE);
			FMemory::Memcpy(TextureData, UncompressedBGRA->GetData(), UncompressedBGRA->Num());
			CameraTextureFrame->PlatformData->Mips[0].BulkData.Unlock();

			CameraTextureFrame->UpdateResource();


			if (DynamicMaterialCamFrame)
			{
				DynamicMaterialCamFrame->SetTextureParameterValue(FName("CamFrame"), CameraTextureFrame);
			}
		}
	}
}
