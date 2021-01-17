#include "handmade.h"

internal void
GameOutputSound(game_sound_output_buffer *SoundBuffer, int ToneHz) // a sound buffer to put samples into, # samples, tSine, & soon) where in time we want the samples to be.
																   // This means we are not assuming that the sound is just "next"
{
	local_persist real32 tSine; // did this definition locally vs a parameter as it is temporary code
	int16 ToneVolume = 3000;	// asserted as constant as its temp code
	int WavePeriod = SoundBuffer->SamplesPerSecond / ToneHz;

	int16 *SampleOut = SoundBuffer->Samples; // points to SoundBuffer Location

	for (int SampleIndex = 0;
		 SampleIndex < SoundBuffer->SampleCount; // This is in samples (not bytes)
		 SampleIndex++)
	{
		real32 SineValue = sinf(tSine);
		int16 SampleValue = (int16)(SineValue * ToneVolume);

		// fill in the pointer to the buffer
		*SampleOut++ = SampleValue; // for left
		*SampleOut++ = SampleValue; // for right

		tSine += 2.0f * Pi32 * 1.0f / (real32)WavePeriod;
	}
}

internal void RenderWeirdGradient(game_offscreen_buffer *Buffer, int BlueOffset, int GreenOffset)
{
	uint8 *Row = (uint8 *)Buffer->Memory;

	for (int Y = 0; Y < Buffer->Height; ++Y)
	{
		uint32 *Pixel = (uint32 *)Row;
		for (int X = 0; X < Buffer->Width; ++X)
		{

			uint8 Blue = (X + BlueOffset);
			uint8 Green = (Y + GreenOffset);

			*Pixel++ = ((Green << 8) | Blue);
		}
		Row += Buffer->Pitch;
	}
}

internal void GameUpdateAndRender(game_offscreen_buffer *Buffer,
								  game_sound_output_buffer *SoundBuffer,
								  game_input *Input,
								  game_memory *Memory)
{	
	Assert(sizeof(game_state) <= Memory->PermanentStorageSize);
	
	game_state *GameState =(game_state *)Memory->PermanentStorage; // Note win32 platform does notneed to know about game_state

	if(!Memory->IsInitialized)
	{
		GameState->ToneHz = 256;

		char *Filename = __FILE__; 	//"test.cmp";
		debug_read_file_result File = DEBUGPlatformReadEntireFile(Filename);
		if(File.Contents)
		{
			DEBUGPlatformWriteEntireFile("f:/data/test.out", File.ContentsSize, File.Contents);
			DEBUGPlatformFreeFileMemory(File.Contents);
		}
	}

	Memory->IsInitialized = true;

	game_controller_input *Input0 = &Input->Controllers[0];
	if(Input0->IsAnalog)
	{
		// Future analog movement tuning
		GameState->BlueOffset += (int)4.0f * (Input0->EndX);
		GameState->ToneHz = 256 + (int)(128.0f * (Input0->EndY));
	}
	else
	{
		// Future digital movement tuning
	}

	// Input.AButtonEndedDown;
	// Input.AButtonHalfTransitionCount;
	if (Input0->Down.EndedDown)
	{
		GameState->GreenOffset += 1;
	}

	GameOutputSound(SoundBuffer, GameState->ToneHz);
	RenderWeirdGradient(Buffer, GameState->BlueOffset, GameState->GreenOffset);
}