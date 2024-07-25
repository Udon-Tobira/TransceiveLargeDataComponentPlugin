// Fill out your copyright notice in the Description page of Project Settings.

#include "TransceiveLargeDataComponent.h"

#include "Engine/ActorChannel.h"
#include "LogTransceiveLargeDataComponent.h"

void UTransceiveLargeDataComponent::SendData(
    TArray<uint8> Data, ETransceiveLargeDataDirection TransceiveDirection) {
	// Divide data into chunks and enqueue them to the SendQueue
	EnqueueToSendQueueAsChunks(Data);

	// set bSending flag true
	bSending = true;

	// set the direction of transceiving
	Direction = TransceiveDirection;
}

void UTransceiveLargeDataComponent::ReceiveChunk_Server_Implementation(
    const TArray<uint8>& Chunk, bool bLastChunk) {
	ReceiveChunk(Chunk, bLastChunk);
}

void UTransceiveLargeDataComponent::ReceiveChunk_Client_Implementation(
    const TArray<uint8>& Chunk, bool bLastChunk) {
	ReceiveChunk(Chunk, bLastChunk);
}

void UTransceiveLargeDataComponent::ReceiveChunk_Multicast_Implementation(
    const TArray<uint8>& Chunk, bool bLastChunk) {
	ReceiveChunk(Chunk, bLastChunk);
}

void UTransceiveLargeDataComponent::EnqueueToSendQueueAsChunks(
    const TArray<uint8>& Data) {
	// TODO: Allow EnqueueToSendQueueAsChunks to be called
	// again while send queue is not empty.
	checkf(SendQueue.IsEmpty(),
	       TEXT("Calling " __FUNCTION__ " again "
	                                    "while send queue is not empty "
	                                    "is currently not supported."));

	// constant: max chunk size
	constexpr auto MaxChunkLength = 60 * 1024; // 60KB per Chunk

	// get length of data
	const auto& DataLength = Data.Num();

	// calculate number of enqueued chunks
	// means Ceil((double)DataLength / (double)MaxChunkLength)
	const auto& NumChunks = (DataLength + MaxChunkLength - 1) / MaxChunkLength;

	// divide data into small chunks and enqueue them into SendQueue.
	for (auto ChunkIndex = decltype(NumChunks){0}; ChunkIndex < NumChunks;
	     ++ChunkIndex) {
		// begin index of chunk on Data array
		const auto& ChunkBeginIndexOnData = MaxChunkLength * ChunkIndex;

		// total length of enqueued data
		const auto& EnqueuedDataLength = ChunkBeginIndexOnData;

		// remaining data length to be enqueued to SendQueue
		const auto& RemainingDataLength = DataLength - EnqueuedDataLength;

		// The length of this chunk is the smaller of MaxChunkLength and
		// RemainingDataLength
		const auto& ChunkLength = FMath::Min(MaxChunkLength, RemainingDataLength);

		// get chunk
		const auto& Chunk =
		    TArray<uint8>(&Data[ChunkBeginIndexOnData], ChunkLength);

		// enqueue this chunk
		SendQueue.Enqueue(Chunk);
	}
}

bool UTransceiveLargeDataComponent::SendoutAChunk() {
	// dequeue a chunk from SendQueue
	TArray<uint8> Chunk;
	// if SendQueue is empty, you're trying to send empty array (not abnormal)
	SendQueue.Dequeue(Chunk);

	// if SendQueue is empty, this is last chunk
	const auto& bLastChunk = SendQueue.IsEmpty();

	switch (Direction) {
	// sending to server
	case ETransceiveLargeDataDirection::Server:
		// send chunk to a server
		ReceiveChunk_Server(Chunk, bLastChunk);
		break;

	// sending to client
	case ETransceiveLargeDataDirection::Client:
		// send chunk to a Owning Client
		ReceiveChunk_Client(Chunk, bLastChunk);
		break;

	// sending to all clients
	case ETransceiveLargeDataDirection::Multicast:
		// send chunk to all clients
		ReceiveChunk_Multicast(Chunk, bLastChunk);
		break;

	// otherwise
	default:
		// You never come here.
		checkf(false, TEXT("It is a non-existent Direction."));
	}

	return bLastChunk;
}

void UTransceiveLargeDataComponent::ReceiveChunk(const TArray<uint8>& Chunk,
                                                 bool bLastChunk) {
	// append received chunk to internal buffer
	ReceivedBuffer.Append(Chunk);

	// if this is last chunk
	if (bLastChunk) {
		// move ReceivedBuffer to local variable ReceivedData
		auto ReceivedData = MoveTemp(ReceivedBuffer);

		// clear ReceivedBuffer
		ReceivedBuffer = decltype(ReceivedBuffer){};

		// Notify all recipients that you received all data
		EventReceivedDataDelegate.Broadcast(ReceivedData);
	}
}

bool UTransceiveLargeDataComponent::HaveSomethingToSend() const {
	return !SendQueue.IsEmpty();
}

// Sets default values for this component's properties
UTransceiveLargeDataComponent::UTransceiveLargeDataComponent() {
	// Set this component to be initialized when the game starts, and to be ticked
	// every frame.  You can turn these features off to improve performance if you
	// don't need them.
	PrimaryComponentTick.bCanEverTick = true;

	// turn on replication
	SetIsReplicatedByDefault(true);
}

void UTransceiveLargeDataComponent::TickComponent(
    float DeltaSeconds, ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction) {
	// call TickComponent on super class
	Super::TickComponent(DeltaSeconds, TickType, ThisTickFunction);

	// if not in sending status
	if (!bSending) {
		// do nothing and finish
		return;
	}

	// get owner
	const auto& Owner = GetOwner();

	// if there is no Owner
	if (nullptr == Owner) {
		// warn to log
		UE_LOG(LogTransceiveLargeDataComponent, Warning,
		       TEXT("There is data that is about to be sent, but this "
		            "TranseceiveLargeDataComponent has no Owner. Sending data is "
		            "pending."));

		// finish (pending sending)
		return;
	}

	// get Connection
	const auto& Connection = Owner->GetNetConnection();

	// if there is no Connection
	if (nullptr == Connection) {
		// warn to log
		UE_LOG(LogTransceiveLargeDataComponent, Warning,
		       TEXT("There is data that is about to be sent, but this "
		            "TranseceiveLargeDataComponent has no Connection (but has an "
		            "Owner). Sending data is "
		            "pending."));

		// finish (pending sending)
		return;
	}

	// get Channel
	const auto& Channel = Connection->FindActorChannelRef(Owner);

	// if there is no Channel
	if (nullptr == Channel) {
		// warn to log
		UE_LOG(
		    LogTransceiveLargeDataComponent, Warning,
		    TEXT("There is data that is about to be sent, but this "
		         "TranseceiveLargeDataComponent has no Actor Channel (but has an "
		         "Owner and Connection). Sending data is "
		         "pending."));

		// finish (pending sending)
		return;
	}

	// get reference of number of out reliable bunches
	const auto& NumOutReliableBunches = Channel->NumOutRec;

	// max number of reliable bunches I limit
	// smaller of NumOutReliableBunches + 10 and RELIABLE_BUFFER*0.1
	const auto& NumOutReliableBunchesMax = FMath::Min(
	    NumOutReliableBunches + 10,
	    static_cast<decltype(NumOutReliableBunches)>(
	        static_cast<decltype(NumOutReliableBunches)>(RELIABLE_BUFFER) * 0.1));

	// flag whether last chunk was sent or not
	bool bLastChunkSent = false;

	// SendoutAChunk until limit is reached
	while (!bLastChunkSent && NumOutReliableBunches < NumOutReliableBunchesMax) {
		// send out a chunk, and update bLastChunkSet value
		bLastChunkSent = SendoutAChunk();
	}

	// if last chunk sent
	if (bLastChunkSent) {
		// set bSending status false
		bSending = false;
	}
}
