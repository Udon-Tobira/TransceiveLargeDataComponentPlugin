// Fill out your copyright notice in the Description page of Project Settings.

#include "TransceiveLargeDataComponent.h"

#include "Engine/ActorChannel.h"
#include "LogTransceiveLargeDataComponent.h"

void UTransceiveLargeDataComponent::SendData(
    const TArray<uint8>&          Data,
    ETransceiveLargeDataDirection TransceiveDirection) {
	// Divide data into chunks and enqueue them to the SendQueue
	EnqueueToSendQueueAsChunks(Data);

	// set bSending flag true
	bSending = true;

	// set the direction of transceiving
	Direction = TransceiveDirection;
}

void UTransceiveLargeDataComponent::ReceiveChunk_Server_Implementation(
    const TArray<uint8>& Chunk, bool bLastChunk) {
	ReceiveAChunk(Chunk, bLastChunk);
}

void UTransceiveLargeDataComponent::ReceiveChunk_Client_Implementation(
    const TArray<uint8>& Chunk, bool bLastChunk) {
	ReceiveAChunk(Chunk, bLastChunk);
}

void UTransceiveLargeDataComponent::ReceiveChunk_Multicast_Implementation(
    const TArray<uint8>& Chunk, bool bLastChunk) {
	ReceiveAChunk(Chunk, bLastChunk);
}

void UTransceiveLargeDataComponent::EnqueueToSendQueueAsChunks(
    const TArray<uint8>& Data) {
	// TODO: Allow EnqueueToSendQueueAsChunks to be called
	// again while send queue is not empty.
	checkf(SendQueue.IsEmpty(), TEXT("Calling EnqueueToSendQueueAsChunks again "
	                                 "while send queue is not empty "
	                                 "is currently not supported."));

	// get length of data
	const auto& DataLength = Data.Num();

	// set data length to send
	TotalDataLengthToSend = DataLength;

	// reset length of data which is already sent to 0
	DataLengthAlreadySent = 0;

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
		SendQueue.Enqueue(Chunk), ++SendQueueNum;
	}
}

bool UTransceiveLargeDataComponent::SendoutAChunk() {
	// dequeue a chunk from SendQueue
	TArray<uint8> Chunk;
	// if SendQueue is empty, you're trying to send empty array (not abnormal)
	SendQueue.Dequeue(Chunk), --SendQueueNum;

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

	// add length of this chunk to DataLengthAlreadySent
	DataLengthAlreadySent += Chunk.Num();

	// Notify all recipients that you received a chunk
	OnSentAChunkDynamicDelegate.Broadcast(Chunk, DataLengthAlreadySent,
	                                      TotalDataLengthToSend);
	OnSentAChunkDelegate.Broadcast(Chunk, DataLengthAlreadySent,
	                               TotalDataLengthToSend);

	// log
	UE_LOG(LogTransceiveLargeDataComponent, Log,
	       TEXT("Sendout a chunk. remaining queued chunk num: %u [%s]"),
	       SendQueueNum, *GetFullName());

	return bLastChunk;
}

void UTransceiveLargeDataComponent::ReceiveAChunk(const TArray<uint8>& Chunk,
                                                  bool bLastChunk) {
	// append received chunk to internal buffer
	ReceivedBuffer.Append(Chunk);

	// log
	UE_LOG(LogTransceiveLargeDataComponent, Log,
	       TEXT("A chunk received. ChunkNum: %d, BufferNum: %d [%s]"),
	       Chunk.Num(), ReceivedBuffer.Num(), *GetFullName());

	// if this is last chunk
	if (bLastChunk) {
		// move ReceivedBuffer to local variable ReceivedData
		auto ReceivedData = MoveTemp(ReceivedBuffer);

		// clear ReceivedBuffer
		ReceivedBuffer = decltype(ReceivedBuffer){};

		// Notify all recipients that you received all data
		OnReceivedDataDynamicDelegate.Broadcast(ReceivedData);
		OnReceivedDataDelegate.Broadcast(ReceivedData);
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

	// update delta from last sent (seconds)
	DeltaFromLastSentSeconds =
	    FMath::Clamp(DeltaFromLastSentSeconds + DeltaSeconds, 0, 1.0 / 5.0);

	// if more than 1/5 second has not elapsed since the last sending
	if (DeltaFromLastSentSeconds < 1.0 / 5.0) {
		// do nothing and finish
		return;
	}

	// if there is no actor channel cache
	if (nullptr == ActorChannelCache) {
		// if there is no connection cache
		if (nullptr == ConnectionCache) {
			// if there is no owner cache
			if (nullptr == OwnerCache) {
				// get owner
				OwnerCache = GetOwner();

				// if there is no Owner
				if (nullptr == OwnerCache) {
					// warn to log
					UE_LOG(
					    LogTransceiveLargeDataComponent, Warning,
					    TEXT("There is data that is about to be sent, but this "
					         "TransceiveLargeDataComponent has no Owner. Sending data is "
					         "pending. [%s]"),
					    *GetFullName());

					// finish (pending sending)
					return;
				}
			}

			// get Connection
			ConnectionCache = OwnerCache->GetNetConnection();

			// if there is no Connection
			if (nullptr == ConnectionCache) {
				// warn to log
				UE_LOG(
				    LogTransceiveLargeDataComponent, Warning,
				    TEXT("There is data that is about to be sent, but this "
				         "TransceiveLargeDataComponent has no Connection (but has an "
				         "Owner). Sending data is "
				         "pending. [%s]"),
				    *GetFullName());

				// finish (pending sending)
				return;
			}
		}

		// get Channel
		ActorChannelCache = ConnectionCache->FindActorChannelRef(OwnerCache);

		// if there is no Channel
		if (nullptr == ActorChannelCache) {
			// warn to log
			UE_LOG(
			    LogTransceiveLargeDataComponent, Warning,
			    TEXT("There is data that is about to be sent, but this "
			         "TransceiveLargeDataComponent has no Actor Channel (but has an "
			         "Owner and Connection). Sending data is "
			         "pending. [%s]"),
			    *GetFullName());

			// finish (pending sending)
			return;
		}
	}

	// get reference of number of out reliable bunches
	const auto& NumOutReliableBunches = ActorChannelCache->NumOutRec;

	// max number of reliable bunches I limit
	const int& NumOutReliableBunchesMax =
	    static_cast<double>(RELIABLE_BUFFER) * 0.5;

	// flag whether last chunk was sent or not
	bool bLastChunkSent = false;

	// flag whether at least one chunk or not was sent or not
	bool bSentAtLeastOneChunk = false;

	// SendoutAChunk until limit is reached
	while (!bLastChunkSent && (NumOutReliableBunches + MaxChunkLength / 1000) <
	                              NumOutReliableBunchesMax) {
		// send out a chunk, and update bLastChunkSet value
		bLastChunkSent = SendoutAChunk();

		// set that at least one chunk was sent
		bSentAtLeastOneChunk = true;
	}

	// if at least one chunk was sent
	if (bSentAtLeastOneChunk) {
		// reset delta from last sent (seconds)
		DeltaFromLastSentSeconds = 0;
	}

	// if last chunk sent
	if (bLastChunkSent) {
		// set bSending status false
		bSending = false;
	}
}
