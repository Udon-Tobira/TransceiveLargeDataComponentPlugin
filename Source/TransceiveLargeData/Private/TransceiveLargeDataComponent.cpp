// Fill out your copyright notice in the Description page of Project Settings.

#include "TransceiveLargeDataComponent.h"

void UTransceiveLargeDataComponent::SendDataToServer(TArray<uint8> Data) {
	// TODO: Allow SendDataToServer to be called again while sending.
	checkf(SendQueue.Empty(), "Calling SendDataToServer again while "
	                          "sending is currently not supported.");

	// constant: max chunk size
	constexpr auto MaxChunkLength = 60 * 1024; // 60KB per Chunk

	// get length of data
	const auto& DataLength = Data.Num();

	// calculate number of enqueued chunks
	// means Ceil((double)DataLength / (double)MaxChunkLength)
	const auto& NumChunks = (DataLength + MaxChunkLength - 1) / MaxChunkLength;

	// if there is no chunk to send
	if (0 == NumChunks) {
		// send empty chunk to server as last chunk
		ReceiveChunk_Server({}, true);
	}

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

	// send out a chunk on SendQueue
	SendoutAChunk();
}

void UTransceiveLargeDataComponent::ReceiveChunk_Server_Implementation(
    const TArray<uint8>& Chunk, bool bLastChunk) {
	// send ack to client
	SendReceivedAck_Client();

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

void UTransceiveLargeDataComponent::SendReceivedAck_Client_Implementation() {
	// If there is data to send
	if (!SendQueue.IsEmpty()) {
		SendoutAChunk();
	}
	// If there is no more data to send
	else {
		// do nothing and return
		return;
	}
}

void UTransceiveLargeDataComponent::SendoutAChunk() {
	// dequeue a chunk from SendQueue
	TArray<uint8> Chunk;
	verify(SendQueue.Dequeue(Chunk));

	// if SendQueue is empty, this is last chunk
	const auto& bLastChunk = SendQueue.IsEmpty();

	// send chunk to server
	ReceiveChunk_Server(Chunk, bLastChunk);
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
