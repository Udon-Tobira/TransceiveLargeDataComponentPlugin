// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "Components/ActorComponent.h"
#include "CoreMinimal.h"
#include "TransceiveLargeDataDirection.h"

#include "TransceiveLargeDataComponent.generated.h"

// delegate for blueprint called when the entire data is received
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FOnReceivedDataDynamicDelegate,
                                            const TArray<uint8>&, Data);
// delegate for C++ called when the entire data is received
DECLARE_MULTICAST_DELEGATE_OneParam(FOnReceivedDataDelegate,
                                    const TArray<uint8>& Data);

UCLASS(meta = (BlueprintSpawnableComponent))
class TRANSCEIVELARGEDATACOMPONENT_API UTransceiveLargeDataComponent
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
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "On Received Data"))
	FOnReceivedDataDynamicDelegate OnReceivedDataDynamicDelegate;

	// C++ delegate
public:
	FOnReceivedDataDelegate OnReceivedDataDelegate;

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
	void ReceiveAChunk(const TArray<uint8>& Chunk, bool bLastChunk);
	bool HaveSomethingToSend() const;

	// private fields
private:
	TArray<uint8>                 ReceivedBuffer;
	TQueue<TArray<uint8>>         SendQueue;
	uint32                        SendQueueNum = 0;
	bool                          bSending     = false;
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

	// constants
private:
	static constexpr auto MaxChunkLength = 60 * 1000; // 60 KB per Chunk
};
