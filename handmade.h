#if !defined(HANDMADE_H)

/*	HAMDMADE_INTERNAL
		0 - Build for public release
		1 - Build for developer only (we assign specific addresses to help debug etc)

	HANDMADE_SLOW:
		0 - No slow code Asserts off.
		1 - Slow code is welcome.  Asserts are on.
*/

#if HANDMADE_SLOW
	#define Assert(Expression) if(!(Expression)) {*(int *)0 = 0;}  
	/* VStudio mistakingly greys the above code out b/c we compile from the command line.
	  This will not hurt the compile */
#else
	#define Assert(Expression)
#endif
 
#define Kilobytes(Value)  ((Value) * 1024LL)
#define Megabytes(Value)  (Kilobytes(Value) * 1024LL)
#define Gigabytes(Value)  (Megabytes(Value) * 1024LL)
#define Terabytes(Value)  (Gigabytes(Value) * 1024LL)

// Services that the platform layer provides to the game

// Services that the game provides to the platform layer

#define ArrayCount(Array) (sizeof(Array) / sizeof((Array)[0]))

inline uint32
SafeTruncateUInt64(uint64 Value)
{
Assert(Value <= 0xFFFFFFFF);
uint32 Result = (uint32)Value; 
return Result;
}

#if HANDMADE_INTERNAL
struct debug_read_file_result
{
uint32 ContentsSize;
void *Contents;
};

internal debug_read_file_result DEBUGPlatformReadEntireFile(char *Filename);
internal void DEBUGPlatformFreeFileMemory(void * Memory);
internal bool32 DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void* Memory);
#endif

struct game_offscreen_buffer
{
	void *Memory;
	int Width;
	int Height;
	int Pitch;
};

struct game_memory
{
	bool32 IsInitialized;  // might move to platform layer later
	uint64 PermanentStorageSize;
	uint64 TemporaryStorageSize;
	void* PermanentStorage;		// All platforms are required to clear  memory to 0 at startup.
	void* TemporaryStorage;
};


struct game_button_state
{
	int HalfTransitionCount;
	bool32 EndedDown;
};

struct game_controller_input
{
	bool32 IsAnalog;

	real32 StartX;
	real32 EndX;
	real32 MinX;
	real32 MaxX;

	real32 MinY;
	real32 MaxY;
	real32 StartY;
	real32 EndY;

	union
	{
		game_button_state Buttons[6];
		struct
		{
			game_button_state Up;
			game_button_state Down;
			game_button_state Left;
			game_button_state Right;
			game_button_state LeftShoulder;
			game_button_state RightShoulder;
		};
	};
};

struct game_input
{
	game_controller_input Controllers[4];
};

struct game_sound_output_buffer
{
	int SamplesPerSecond;
	int SampleCount;
	int16 *Samples;
};



internal void GameUpdateAndRender(game_offscreen_buffer *Buffer, 
								  game_sound_output_buffer *SoundBuffer,
								  game_input *Input,
								  game_memory *Memory);


struct game_state
{
int ToneHz;
int GreenOffset;
int BlueOffset;
};


#define HANDMADE_H
#endif


