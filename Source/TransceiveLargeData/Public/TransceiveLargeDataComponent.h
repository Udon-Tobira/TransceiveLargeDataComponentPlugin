// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "TransceiveLargeDataDirection.h"

#include "TransceiveLargeDataComponent.generated.h"

// delegate called when a data is received
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FEventReceivedDataDelegate,
                                            const TArray<uint8>&, Data);

UCLASS(meta = (BlueprintSpawnableComponent))
class TRANSCEIVELARGEDATA_API UTransceiveLargeDataComponent
    : public UActorComponent {
	GENERATED_BODY()

#pragma region Network Features
	// Blueprint functions
public:
	UFUNCTION(BlueprintCallable)
	void SendData(TArray<uint8>                 Data,
	              ETransceiveLargeDataDirection TransceiveDirection);

	// Blueprint delegates
public:
	UPROPERTY(BlueprintAssignable)
	FEventReceivedDataDelegate EventReceivedDataDelegate;

	// private RPC functions
private:
	UFUNCTION(Server, Reliable)
	void ReceiveChunk_Server(const TArray<uint8>& Chunk, bool bLastChunk);

	UFUNCTION(Client, Reliable)
	void ReceiveChunk_Client(const TArray<uint8>& Chunk, bool bLastChunk);

	UFUNCTION(NetMulticast, Reliable)
	void ReceiveChunk_Multicast(const TArray<uint8>& Chunk, bool bLastChunk);

	// private functions
private:
	void EnqueueToSendQueueAsChunks(const TArray<uint8>& Data);
	bool SendoutAChunk();
	void ReceiveChunk(const TArray<uint8>& Chunk, bool bLastChunk);
	bool HaveSomethingToSend() const;

	// private fields
private:
	TArray<uint8>                 ReceivedBuffer;
	TQueue<TArray<uint8>>         SendQueue;
	bool                          bSending = false;
	ETransceiveLargeDataDirection Direction;

	// cache
private:
	// A pointer to const would work, but since unreal does not have const, it
	// cannot have it either.
	AActor* OwnerCache = nullptr;
	// A pointer to const would work, but since unreal does not have const, it
	// cannot have it either.
	UNetConnection*      ConnectionCache          = nullptr;
	const UActorChannel* ActorChannelCache        = nullptr;
	float                DeltaFromLastSentSeconds = 0.0f;

#pragma endregion

public:
	UTransceiveLargeDataComponent();

	virtual void
	    TickComponent(float DeltaSeconds, enum ELevelTick TickType,
	                  FActorComponentTickFunction* ThisTickFunction) override;
};
