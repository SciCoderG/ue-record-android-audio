/* Copyright (c) 2022-2023 4Players GmbH. All rights reserved. */

#include "OdinSubmixListener.h"
#include "Odin.h"
#include "Sound/SoundSubmix.h"
#if ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION >= 3
#include "ISubmixBufferListener.h"
#endif
#include "AudioDevice.h"
#include "Sound/SampleBufferIO.h"


using namespace Audio;

UOdinSubmixListener::UOdinSubmixListener(const class FObjectInitializer& PCIP)
	: Super(PCIP)
{
	resampler_handle = 0;
}

UOdinSubmixListener::~UOdinSubmixListener()
{
	if (bInitialized)
	{
		StopSubmixListener();
	}

	if (resampler_handle != 0)
	{
		odin_resampler_destroy(resampler_handle);
	}
}

void UOdinSubmixListener::StartSubmixListener()
{
	if (bInitialized || !GEngine)
	{
		return;
	}

	FAudioDeviceHandle AudioDevice = GEngine->GetActiveAudioDevice();

	int32 samplerate = AudioDevice->SampleRate;
	if (samplerate != OdinSampleRate)
	{
		UE_LOG(Odin, Warning, TEXT("Creating resampler. Samplerate of %d mismatch %d"),
		       AudioDevice->SampleRate, OdinSampleRate);
		// resampler_handle = odin_resampler_create(samplerate, OdinSampleRate, OdinChannels);
	}

	AudioDevice->RegisterSubmixBufferListener(this);
	bInitialized = true;
}

void UOdinSubmixListener::StopSubmixListener()
{
	if (!bInitialized || !GEngine)
	{
		return;
	}

	FAudioDeviceHandle AudioDevice = GEngine->GetActiveAudioDevice();
	AudioDevice->UnregisterSubmixBufferListener(this);
	bInitialized = false;
}

void UOdinSubmixListener::SetRoom(OdinRoomHandle room)
{
	current_room_handle = room;
}

void UOdinSubmixListener::StartSavingBuffer()
{
	bIsSavingBuffer = true;
	SavedBuffer.Reset();
}


void UOdinSubmixListener::StopSavingAndWriteBuffer(FString FileName)
{
	bIsSavingBuffer = false;
	FString Path = "./";
	
	Writer.BeginWriteToWavFile(SavedBuffer, FileName, Path, [&]()->void
	{
		const FString AbsolutePath = FPaths::ProjectSavedDir() + TEXT("BouncedWavFiles/") + FileName;
		UE_LOG(LogTemp, Warning, TEXT("Saved Recording to: %s"), *AbsolutePath);
		OnWriteWaveFileFinished.Broadcast(AbsolutePath);
	});
}

void UOdinSubmixListener::OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData,
                                            int32 InNumSamples, int32 InNumChannels,
                                            const int32 InSampleRate, double InAudioClock)
{
	if (!bInitialized)
	{
		return;
	}

	FScopeLock Lock(&submix_cs_);

	UE_LOG(Odin, Display, TEXT("In Channels: %d In SampleRate: %d In Num Samples: %d In Audio Clock: %f"), InNumChannels, InSampleRate, InNumSamples, InAudioClock);


	TSampleBuffer<float> buffer(AudioData, InNumSamples, InNumChannels, InSampleRate);
	if (buffer.GetNumChannels() != OdinChannels)
	{
		UE_LOG(Odin, Display, TEXT("Due to differences in Channel Count, remixing buffer from %d Channels to %d OdinChannels"), InNumChannels, OdinChannels);
		buffer.MixBufferToChannels(OdinChannels);
	}
	if(buffer.GetSampleRate() != OdinSampleRate)
	{
		UE_LOG(Odin, Display, TEXT("Detected difference in sample rate: %d In Sample Rate and %d Odin Sample Rate"), InSampleRate, OdinSampleRate);
	}
	
	float* pbuffer = buffer.GetArrayView().GetData();
	OdinReturnCode result =
		odin_audio_process_reverse(current_room_handle, pbuffer, buffer.GetNumSamples());
	
	if(bIsSavingBuffer && InNumSamples > 0)
	{
		if(SavedBuffer.GetNumChannels() != buffer.GetNumChannels() || SavedBuffer.GetSampleRate() != buffer.GetSampleRate())
			SavedBuffer.Append(buffer.GetData(), buffer.GetNumSamples(), buffer.GetNumChannels(), buffer.GetSampleRate());
		else
		{
			SavedBuffer.Append(buffer.GetData(), buffer.GetNumSamples());
		}
	}

	if (odin_is_error(result))
	{
		UE_LOG(Odin, Verbose, TEXT("odin_audio_process_reverse result: %d"), result);
		UE_LOG(Odin, Verbose, TEXT("OnNewSubmixBuffer on %s "),
		       *OwningSubmix->GetFName().ToString());

		StopSubmixListener();
	}
}
