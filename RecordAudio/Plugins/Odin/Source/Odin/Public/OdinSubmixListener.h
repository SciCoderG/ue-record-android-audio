/* Copyright (c) 2022-2023 4Players GmbH. All rights reserved. */

#pragma once

#include "Engine/GameEngine.h"
#include "Runtime/Launch/Resources/Version.h"
#include "odin_sdk.h"
#include "Sound/SampleBufferIO.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "ISubmixBufferListener.h"
#else
#include "AudioDevice.h"
#endif

#include "OdinSubmixListener.generated.h"

UCLASS(ClassGroup = Utility, BlueprintType)
class ODIN_API UOdinSubmixListener : public UObject, public ISubmixBufferListener
{
	GENERATED_BODY()

public:
	UOdinSubmixListener(const class FObjectInitializer& PCIP);
	virtual ~UOdinSubmixListener() override;

	void StartSubmixListener();
	void StopSubmixListener();
	void SetRoom(OdinRoomHandle handle);

	DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FWriteWaveFileFinished, FString, Path);
	FWriteWaveFileFinished OnWriteWaveFileFinished;

	UFUNCTION(BlueprintCallable)
	void StartSavingBuffer();
	UFUNCTION(BlueprintCallable)
	void StopSavingAndWriteBuffer(FString FileName);


protected:

#if PLATFORM_DESKTOP
	int32 OdinSampleRate   = 48000;
#elif PLATFORM_ANDROID || PLATFORM_IOS
	int32 OdinSampleRate   = 44100;
#endif
	int32 OdinChannels = 2;

private:
	FCriticalSection submix_cs_;
	bool bInitialized;
	OdinRoomHandle current_room_handle;
	OdinResamplerHandle resampler_handle;

	FThreadSafeBool bIsSavingBuffer = false;
	Audio::TSampleBuffer<> SavedBuffer;
	Audio::FSoundWavePCMWriter Writer;
	
	virtual void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 InNumSamples,
	                               int32 InNumChannels, const int32 InSampleRate, double) override;
};
