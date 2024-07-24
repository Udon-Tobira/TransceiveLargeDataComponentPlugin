// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"

#include "TransceiveLargeDataComponent.generated.h"

// delegate called when a data is received
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEventReceivedDataDelegate,
                                            const TArray<uint8>&, Data);

UCLASS(ClassGroup = (Custom), meta = (BlueprintSpawnableComponent))
class TRANSCEIVELARGEDATA_API UTransceiveLargeDataComponent
    : public UActorComponent {
	GENERATED_BODY()

#pragma region Network Features
public:
	UFUNCTION(BlueprintCallable)
	void SendDataToServer(TArray<uint8> Data);

public:
	UPROPERTY(BlueprintAssignable)
	FEventReceivedDataDelegate EventReceivedDataDelegate;

private:
	UFUNCTION(Server, Reliable)
	void ReceiveChunk_Server(const TArray<uint8>& Chunk, bool bLastChunk);

	UFUNCTION(Client, Reliable)
	void SendReceivedAck_Client();

private:
	void SendoutAChunk();

private:
	TArray<uint8>         ReceivedBuffer;
	TQueue<TArray<uint8>> SendQueue;
#pragma endregion

public:
	UTransceiveLargeDataComponent();
};
