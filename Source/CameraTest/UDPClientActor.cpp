// Fill out your copyright notice in the Description page of Project Settings.

#include "UDPClientActor.h"
#include "Runtime/Engine/Public/TimerManager.h"

#include "Engine/Texture2D.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Runtime/Engine/Classes/Components/StaticMeshComponent.h"
#include "Runtime/CoreUObject/Public/UObject/ConstructorHelpers.h"


FCamFrame::FCamFrame(AUDPClientActor* UDPClientActor)
	: UDPClientActor(UDPClientActor)
	, ListenerSocket(nullptr)
	, ConnectionSocket(nullptr)
	, TotalDataSize(0)
	, bIsReadSocketThreadRunning(false)
{
	FString SocketName = FString::Printf(TEXT("CamSocket_port_%d"), UDPClientActor->ThePort);
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
		UE_LOG(LogTemp, Warning, TEXT("StartTCPReceiver>> Listen socket could not be created! ~> %s %d"), *UDPClientActor->TheIP, UDPClientActor->ThePort);
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

			if (TotalDataSize > UDPClientActor->FrameSize)
			{
				break;
			}

			// Copy to image array buffer
			Buffer.Append(ReceivedData.GetData(), ReceivedData.Num());
		}
		//UE_LOG(LogTemp, Warning, TEXT("Total Data read! %d, Buffer size %d"), TotalDataSize, Buffer.Num());

		//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

		// Just for prevent crash
		if (Buffer.Num() == UDPClientActor->FrameSize)
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
	if (!FormatIP4ToNumber(UDPClientActor->TheIP, IP4Nums))
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid IP! Expecting 4 parts separated by "));

		return false;
	}

	//~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

	//Create Socket
	FIPv4Endpoint Endpoint(FIPv4Address(IP4Nums[0], IP4Nums[1], IP4Nums[2], IP4Nums[3]), UDPClientActor->ThePort);
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
	RootComponent = StaticMesh;

	static ConstructorHelpers::FObjectFinder<UStaticMesh> PlaneMeshAsset(TEXT("StaticMesh'/Game/Meshes/PlaneMesh.PlaneMesh'"));
	if (PlaneMeshAsset.Succeeded())
	{
		StaticMesh->SetStaticMesh(PlaneMeshAsset.Object);

		static ConstructorHelpers::FObjectFinder<UMaterial> FrameMatAsset(TEXT("Material'/Game/FirstPersonCPP/Blueprints/FrameMat.FrameMat'"));
		if (FrameMatAsset.Succeeded())
		{
			StaticMesh->SetMaterial(0, FrameMatAsset.Object);
		}
	}

	// Set size of the frame
	FrameSize = FrameWidth * FrameHeight * BytesPerColor;

	// allocate array
	FrameDataArray.AddUninitialized(FrameSize);
}

// Called when the game starts or when spawned
void AUDPClientActor::BeginPlay()
{
	Super::BeginPlay();

	UE_LOG(LogTemp, Warning, TEXT(" AUDPClientActor::BeginPlay()"));

	// Create texture and dynamic material
	CamTextureFrame = UTexture2D::CreateTransient(FrameWidth, FrameHeight, PF_B8G8R8A8);
	CamTextureFrame->MipGenSettings = TextureMipGenSettings::TMGS_NoMipmaps;
	CamTextureFrame->CompressionSettings = TextureCompressionSettings::TC_VectorDisplacementmap;
	CamTextureFrame->SRGB = 0;
	CamTextureFrame->AddToRoot();		// Guarantee no garbage collection by adding it as a root reference
	CamTextureFrame->UpdateResource();	// Update the texture with new variable values.


	// Create a new texture region with the width and height of our dynamic texture
	UpdateTextureRegion = new FUpdateTextureRegion2D(0, 0, 0, 0, FrameWidth, FrameHeight);
	BufferSizeSqrt = FrameWidth * BytesPerColor;


	DynamicMaterialCamFrame = StaticMesh->CreateAndSetMaterialInstanceDynamic(0);

	// Run thread
	if (bIsFrameThreadRunning == false)
	{
		bIsFrameThreadRunning = true;
		CamFrameThread = std::thread([&]
		{
			CamFrame = new FCamFrame(this);
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
	delete UpdateTextureRegion;
}

// Called every frame
void AUDPClientActor::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);

	if (CamFrame)
	{
		if (FrameDataArray.Num() > 0)
		{
			//CopyCamFrame(FrameDataArray);

			TArray<uint8> FrameDataArrayCopy;

			mutex.lock();
			FrameDataArrayCopy.Append(FrameDataArray.GetData(), FrameDataArray.Num());
			mutex.unlock();

			UpdateTextureRegions(CamTextureFrame, 0, 1, UpdateTextureRegion, BufferSizeSqrt, (uint32)4, FrameDataArray.GetData(), false);

			if (DynamicMaterialCamFrame)
			{
				DynamicMaterialCamFrame->SetTextureParameterValue(FName("CamFrame"), CamTextureFrame);
			}
		}
	}

}

void AUDPClientActor::UpdateTextureRegions(UTexture2D* Texture, int32 MipIndex, uint32 NumRegions, FUpdateTextureRegion2D* Regions, uint32 SrcPitch, uint32 SrcBpp, uint8* SrcData, bool bFreeData)
{
	if (Texture->Resource)
	{
		struct FUpdateTextureRegionsData
		{
			FTexture2DResource* Texture2DResource;
			int32 MipIndex;
			uint32 NumRegions;
			FUpdateTextureRegion2D* Regions;
			uint32 SrcPitch;
			uint32 SrcBpp;
			uint8* SrcData;
		};

		FUpdateTextureRegionsData* RegionData = new FUpdateTextureRegionsData;

		RegionData->Texture2DResource = (FTexture2DResource*)Texture->Resource;
		RegionData->MipIndex = MipIndex;
		RegionData->NumRegions = NumRegions;
		RegionData->Regions = Regions;
		RegionData->SrcPitch = SrcPitch;
		RegionData->SrcBpp = SrcBpp;
		RegionData->SrcData = SrcData;

		ENQUEUE_UNIQUE_RENDER_COMMAND_TWOPARAMETER(
			UpdateTextureRegionsData,
			FUpdateTextureRegionsData*, RegionData, RegionData,
			bool, bFreeData, bFreeData,
			{
				for (uint32 RegionIndex = 0; RegionIndex < RegionData->NumRegions; ++RegionIndex)
				{
					int32 CurrentFirstMip = RegionData->Texture2DResource->GetCurrentFirstMip();
					if (RegionData->MipIndex >= CurrentFirstMip)
					{
						RHIUpdateTexture2D(
							RegionData->Texture2DResource->GetTexture2DRHI(),
							RegionData->MipIndex - CurrentFirstMip,
							RegionData->Regions[RegionIndex],
							RegionData->SrcPitch,
							RegionData->SrcData
							+ RegionData->Regions[RegionIndex].SrcY * RegionData->SrcPitch
							+ RegionData->Regions[RegionIndex].SrcX * RegionData->SrcBpp
						);
					}
				}
		if (bFreeData)
		{
			FMemory::Free(RegionData->Regions);
			FMemory::Free(RegionData->SrcData);
		}
		delete RegionData;
			});
	}
}
