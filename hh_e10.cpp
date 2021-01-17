#include <math.h>	// sine
#include <stdint.h> // provides standard ints for our typedef; uint8_t = 8 bits

#define internal static
#define local_persist static
#define global_variable static

#define Pi32 3.1415926535f

typedef int8_t int8;   // = unisgned char	with range  0 - 255
typedef int16_t int16; // unisgned short				0 - 65535
typedef int32_t int32; // unisgned int					0 - 4,294,967,295
typedef int64_t int64; // unisgned long long			0 - 1.84469 E+19  = 18,446,900,000,000,000,000 = 18.4 quintillion

typedef int32 bool32;

typedef uint8_t uint8;
typedef uint16_t uint16;
typedef uint32_t uint32;
typedef uint64_t uint64;

typedef float real32;
typedef double real64;

#include "handmade.cpp"

#include <windows.h>
#include <stdio.h>	// printf
#include <malloc.h> // alloca
#include <xinput.h> // Gamepad controller
#include <dsound.h> // DirectSound

#include "win32_handmade.h"

global_variable bool GlobalRunning;						 // global for now (statics are automatically initialized to zero)
global_variable win32_offscreen_buffer GlobalBackBuffer; // globals not created on stack; its static data memory when we launch .exe
global_variable LPDIRECTSOUNDBUFFER GlobalSecondaryBuffer;

#define X_INPUT_GET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_STATE *pState)
typedef X_INPUT_GET_STATE(x_input_get_state);
X_INPUT_GET_STATE(XInputGetStateStub)
{
	return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_get_state *XInputGetState_ = XInputGetStateStub; // the variable if of type x_input_get_state
#define XInputGetState XInputGetState_

#define X_INPUT_SET_STATE(name) DWORD WINAPI name(DWORD dwUserIndex, XINPUT_VIBRATION *pVibration)
typedef X_INPUT_SET_STATE(x_input_set_state);
X_INPUT_SET_STATE(XInputSetStateStub)
{
	return (ERROR_DEVICE_NOT_CONNECTED);
}
global_variable x_input_set_state *XInputSetState_ = XInputSetStateStub; // the variable if of type x_input_get_state
#define XInputSetState XInputSetState_

#define DIRECT_SOUND_CREATE(name) HRESULT WINAPI name(LPCGUID lpcGuidDevice, LPDIRECTSOUND *ppDS, LPUNKNOWN pUnkOuter) // Direct Sound signature
typedef DIRECT_SOUND_CREATE(direct_sound_create);

//Functions

internal debug_read_file_result
DEBUGPlatformReadEntireFile(char *Filename)
{
	debug_read_file_result Result = {};
	HANDLE FileHandle = CreateFileA(Filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		LARGE_INTEGER FileSize;
		if (GetFileSizeEx(FileHandle, &FileSize))
		{
			uint32 FileSize32 = SafeTruncateUInt64(FileSize.QuadPart);
			Result.Contents = VirtualAlloc(0, FileSize32, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
			if (Result.Contents)
			{
				DWORD BytesRead;
				if (ReadFile(FileHandle, Result.Contents, FileSize32, &BytesRead, 0) && (FileSize32 == BytesRead))
				{
					Result.ContentsSize = FileSize32;
	 			}
				else
				{
					DEBUGPlatformFreeFileMemory(Result.Contents);
					Result.Contents = 0;
				}
			}
			else
			{
				// Error log Virtual Alloc failure
			}
		}
		else
		{
			// Log error getting File size
		}
		CloseHandle(FileHandle);
	}
	else
	{
		// Invalid File Handle
	}
	return (Result);
}

internal void
DEBUGPlatformFreeFileMemory(void *Memory)
{
	VirtualFree(Memory, 0, MEM_RELEASE);
}

internal bool32
DEBUGPlatformWriteEntireFile(char *Filename, uint32 MemorySize, void *Memory)
{
	bool32 Result = false;
	HANDLE FileHandle = CreateFileA(Filename, GENERIC_WRITE, 0, 0, CREATE_ALWAYS, 0, 0);
	if (FileHandle != INVALID_HANDLE_VALUE)
	{
		DWORD BytesWritten;
		if (WriteFile(FileHandle, Memory, MemorySize, &BytesWritten, 0))
		{
			Result = (BytesWritten == MemorySize);
		}
		else
		{
			//  Log Could not write the file
		}
		CloseHandle(FileHandle);
	}
	else
	{
		// FileHandle invalid
	}
	return (Result);
}

internal void
win32ProcessXInputDigitalButton(WORD XInputButtonState,
								WORD ButtonBit,
								game_button_state *OldState,
								game_button_state *NewState)
{
	NewState->EndedDown =
		((XInputButtonState & ButtonBit) == ButtonBit);

	NewState->HalfTransitionCount =
		(OldState->EndedDown != NewState->EndedDown) ? 1 : 0;
}

internal void Win32LoadXInput(void)
{
	HMODULE XInputLibrary = LoadLibraryA("xinput1_4.dll"); // ask for 1_4 first.
	if (XInputLibrary)
	{
		OutputDebugStringA("xinput1_4.dll loaded \n");
	}
	if (!XInputLibrary)
	{
		HMODULE XInputLibrary = LoadLibraryA("xinput1_3.dll");
		if (XInputLibrary)
		{
			OutputDebugStringA("xinput1_3.dll loaded \n");
		}
	}
	if (!XInputLibrary)
	{
		HMODULE XInputLibrary = LoadLibraryA("xinput9_1_0.dll");
		if (XInputLibrary)
		{
			OutputDebugStringA("xinput9_1_0.dll loaded \n");
		}
	}

	if (XInputLibrary)
	{
		XInputGetState = (x_input_get_state *)GetProcAddress(XInputLibrary, "XInputGetState");
		if (!XInputGetState)
		{
			XInputGetState = XInputGetStateStub;
		}

		XInputSetState = (x_input_set_state *)GetProcAddress(XInputLibrary, "XInputSetState");
		if (!XInputSetState)
		{
			XInputSetState = XInputSetStateStub;
		}
	}
	else
	{
	}
}

internal void Win32InitDSound(HWND Window, int32 SamplesPerSecond, int32 BufferSize)
{
	// Load the Library like we did with the controller.  Doing this ourselves allows the user to play the game even if sound cannot be loaded.
	HMODULE DSoundLibrary = LoadLibraryA("dsound.dll");

	if (DSoundLibrary)
	{
		// Get a Direct Sound object!  (Casey thinks Direct sound OOP for the API is a nightmare) - cooperative mode
		direct_sound_create *DirectSoundCreate = // need to cast as GetProc returns a void*
			(direct_sound_create *)GetProcAddress(DSoundLibrary, "DirectSoundCreate");

		// TODO:  Doublecheck that this works on XP - DirectSound8 or 7?
		LPDIRECTSOUND DirectSound;
		if (DirectSoundCreate && SUCCEEDED(DirectSoundCreate(0, &DirectSound, 0))) // if we get lib and (create the sound object in our var
																				   // then return an HRESULT which w check via the Windows Macro SUCCEEDED)
		{
			OutputDebugStringA("dsound.dll loaded & DirectSound object created\n");
			// We write them in as [Left Right Left Right ...] as we have 2 channels
			WAVEFORMATEX WaveFormat = {};
			WaveFormat.wFormatTag = WAVE_FORMAT_PCM;
			WaveFormat.nChannels = 2;
			WaveFormat.nSamplesPerSec = SamplesPerSecond;
			WaveFormat.wBitsPerSample = 16;													 // CD is 16 bits
																							 // = 2 * 16 / 8 = 4 bytes per unit of [Left Right]
			WaveFormat.nBlockAlign = (WaveFormat.nChannels * WaveFormat.wBitsPerSample) / 8; //   Divide by bits/byte to get how many bytes per single unit of the channel (e.g how big is a [Left+Right])
			WaveFormat.nAvgBytesPerSec = WaveFormat.nBlockAlign * WaveFormat.nSamplesPerSec;
			WaveFormat.cbSize = 0;

			if (SUCCEEDED(DirectSound->SetCooperativeLevel(Window, DSSCL_PRIORITY))) // Set cooperative Level (Have to!) // do not need to look up the address as the Vtable for the
																					 // returned LPDIRECTSOUND struct instance DirectSound is used (as DirectSoundCreate is a virtual function?).
																					 //The vtable was loaded with the DLL
			{
				DSBUFFERDESC BufferDescription = {}; // have to set the struct fields to zero before you write to it
				BufferDescription.dwSize = sizeof(BufferDescription);
				BufferDescription.dwFlags = DSBCAPS_PRIMARYBUFFER;

				// "Create" a primary buffer so that you can set the old school "mode" of it
				// todo: DSBCAPS_GLOBALFOCUS?

				LPDIRECTSOUNDBUFFER PrimaryBuffer; // Must set size to 0 initially
				if (SUCCEEDED(DirectSound->CreateSoundBuffer(&BufferDescription, &PrimaryBuffer, 0)))
				{
					HRESULT Error = PrimaryBuffer->SetFormat(&WaveFormat);
					if (SUCCEEDED(Error))
					{
						// Format is now set!  which is what all the above code really is for.  Kernel mixer may do this nowadays but this is needed with this approach
						OutputDebugStringA("Primary sound buffer format is set\n");
					}
					else
					{
					}
				}
				else
				{
				}
			}
			else
			{
			}

			// "Create" a secondary buffer that we actually write to.
			// Todo DBSCAPS_GETCURRENTPOSITION2
			DSBUFFERDESC BufferDescription = {}; // have to set the struct fields to zero before you write to it
			BufferDescription.dwSize = sizeof(BufferDescription);
			BufferDescription.dwFlags = 0;
			BufferDescription.dwBufferBytes = BufferSize;
			BufferDescription.lpwfxFormat = &WaveFormat;
			HRESULT Error = DirectSound->CreateSoundBuffer(&BufferDescription, &GlobalSecondaryBuffer, 0);
			if (SUCCEEDED(Error))
			{
				OutputDebugStringA("Secondary sound buffer created successfully.\n");
			}
		}
		else
		{
		}
	}
	else
	{
	}
}

internal win32_window_dimensions Win32GetWindowDimension(HWND Window)
{
	win32_window_dimensions Result;

	RECT ClientRect;
	GetClientRect(Window, &ClientRect);				   // gives us back the writeable region on the window
	Result.Width = ClientRect.right - ClientRect.left; // left and top are always 0
	Result.Height = ClientRect.bottom - ClientRect.top;

	return {Result};
}

internal void Win32ResizeDIBSection(win32_offscreen_buffer *Buffer, int Width, int Height) //Device Independent Bitmap
{
	if (Buffer->Memory) // is Memeory > 0.  If yes we already allocated it so clear it to rewrite
						//use -> b/c passing by reference.   changing all these to struct is "porting"
	{
		VirtualFree(Buffer->Memory, 0, MEM_RELEASE);
	}

	Buffer->Width = Width;
	Buffer->Height = Height;

	// info needed by Windows to Blit.. "We are passing you memory and this is what it looks like".  DIB = Device Independent Buffer
	Buffer->Info.bmiHeader.biSize = sizeof(Buffer->Info.bmiHeader);
	Buffer->Info.bmiHeader.biWidth = Buffer->Width;
	Buffer->Info.bmiHeader.biHeight = -Buffer->Height; // negative is clue for Windows that this is a top down DIB; the first 3 bytes to draw are the color for the Top Left pixel. (vs bottom left)
	Buffer->Info.bmiHeader.biPlanes = 1;
	Buffer->Info.bmiHeader.biBitCount = 32; // Each pixel's size.  Will give us 4 byte chunks.  Full window size is W x H x biBitCount
	Buffer->Info.bmiHeader.biCompression = BI_RGB;

	int BytesPerPixel = 4;
	int BitmapMemorySize = (Buffer->Width * Buffer->Height) * BytesPerPixel;					  // How big is the buffer
	Buffer->Memory = VirtualAlloc(0, BitmapMemorySize, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE); // Asks the OS to allocate memory for us ( in multiples of "pages" of 4096 bytes)
																								  // Without this the code would error "Access violation writing location" as you are trying to touch a page that has not been put into physical memory
	Buffer->Pitch = Width * BytesPerPixel;														  // Sets the Row size (aka stride)
}

internal void Win32DisplayBufferInWindow(win32_offscreen_buffer *Buffer, HDC DeviceContext,
										 int WindowWidth, int WindowHeight)
//,int X, int Y, int Width, int Height)		// WM_PAINT does give us back the 'Dirty Rectangle' to repaint.  But it may be
// buggy when 1st called.  Instead, b/c we redraw the whole window as we need be able to do that fast at ~ 30 FPS anyway to look good.
{
	// Use win32_offscreen_buffer* as a parameter as stack may lose track of it.

	StretchDIBits(DeviceContext, // copies and streches the color data for a rectangle of pixels in a DIB to another Rect defined by the window's size. Used to be slower than BitBlt.  Just using this before OpenGL
				  0, 0, WindowWidth, WindowHeight,
				  0, 0, Buffer->Width, Buffer->Height, // Aspect ratio correction
				  Buffer->Memory,					   // Size & Location of the buffer
				  &Buffer->Info,					   // Tells Windows the buffer's format
				  DIB_RGB_COLORS, SRCCOPY);			   // flags that tell it to use straight RGB colors. Just copy the buffer (no operations)
}

internal LRESULT CALLBACK Win32MainWindowCallback(HWND Window,
												  UINT Message,
												  WPARAM WParam,
												  LPARAM LParam) // templated from MSDN Callback function
{
	LRESULT Result = 0;

	switch (Message)
	{
	case WM_SIZE:
	{
	}
	break;

	case WM_DESTROY:
	{
		GlobalRunning = false;
	}
	break;

	case WM_CLOSE:
	{
		GlobalRunning = false;
	}
	break;

	case WM_ACTIVATEAPP:
	{
		OutputDebugStringA("WM_ACTIVATEAPP\n");
	}
	break;

	case WM_SYSKEYDOWN:
	case WM_SYSKEYUP:
	case WM_KEYDOWN:
	case WM_KEYUP:
	{
		uint32 VKCode = WParam;						// The key
		bool WasDown = ((LParam & (1 << 30)) != 0); // LParam holds key stae flags. 30th is the previous state bit. = 1 if key was down
		bool IsDown = ((LParam & (1 << 31)) == 0);	// 31st bit = Transition state flag.  = 1 if up
		if (WasDown != IsDown)						// At a Key Press
		{
			if (VKCode == 'W') // Need to use Capital Letter
			{
			}

			else if (VKCode == 'A')
			{
			}

			else if (VKCode == 'S')
			{
			}

			else if (VKCode == 'D')
			{
			}

			else if (VKCode == 'Q')
			{
			}

			else if (VKCode == 'E')
			{
			}

			else if (VKCode == VK_UP)
			{
			}

			else if (VKCode == VK_LEFT)
			{
			}

			else if (VKCode == VK_DOWN)
			{
			}

			else if (VKCode == VK_RIGHT)
			{
			}

			else if (VKCode == VK_ESCAPE)
			{
				OutputDebugStringA("ESCAPE: ");
				if (IsDown)
				{
					OutputDebugStringA("Is down ");
				}
				if (WasDown)
				{
					OutputDebugStringA("Was down");
				}
				OutputDebugStringA("\n");
			}
			else if (VKCode == VK_SPACE)
			{
			}
		}
		bool32 AltKeyWasDown = (LParam & (1 << 29)); // checks for ALT key down.  Note bool32 is a typedef to avoid actual bool conversation when it is not needed
		if ((VKCode == VK_F4) && AltKeyWasDown)		 // F4 = Close Window
		{
			GlobalRunning = FALSE;
		}
	}
	break;

	case WM_PAINT:
	{
		PAINTSTRUCT Paint;
		HDC DeviceContext = BeginPaint(Window, &Paint); //Need Begin AND End to tell Windows that you are done updating the Dirty Rectangle Region
		int X = Paint.rcPaint.left;
		int Y = Paint.rcPaint.top;
		int Width = Paint.rcPaint.right - Paint.rcPaint.left;
		int Height = Paint.rcPaint.bottom - Paint.rcPaint.top;

		win32_window_dimensions Dimensions = Win32GetWindowDimension(Window);
		Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext,
								   Dimensions.Width, Dimensions.Height);
		EndPaint(Window, &Paint);
	}
	break;

	default: // This handles all Messages that Windows requires a behavior on.  As an example you need to Begin:End Pain in a WM_PAINT call.  If we
			 // did not have our WM_PAINT case above, nor this default call, Windows would never see that you updated the dirty rect and would flood
			 // our messages with WM_PAINT calls.
	{
		//OutputDebugStringA("DEFAULT\n");
		Result = DefWindowProcA(Window, Message, WParam, LParam); // Calls Windows Default Behavior
	}
	break;
	}

	return (Result);
}

internal void Win32ClearBuffer(win32_sound_output *SoundOutput)
{
	VOID *Region1; // Two regions on lock to adjust (if needed) for ring buffer wraparound
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;

	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(0, SoundOutput->SecondaryBufferSize,
											  &Region1, &Region1Size,
											  &Region2, &Region2Size,
											  0)))
	{
		uint8 *DestSample = (uint8 *)Region1;
		for (DWORD ByteIndex = 0;
			 ByteIndex < Region1Size;
			 ++ByteIndex)
		{
			*DestSample++ = 0; // seeting all bytes in buffer tp 0
		}
		DestSample = (uint8 *)Region2;
		for (DWORD ByteIndex = 0;
			 ByteIndex < Region2Size;
			 ++ByteIndex)
		{
			*DestSample++ = 0;
		}
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

internal void Win32FillSoundBuffer(win32_sound_output *SoundOutput, DWORD ByteToLock, DWORD BytesToWrite,
								   game_sound_output_buffer *SourceBuffer)
{

	VOID *Region1; // Two regions on lock to adjust (if needed) for ring buffer wraparound
	DWORD Region1Size;
	VOID *Region2;
	DWORD Region2Size;

	if (SUCCEEDED(GlobalSecondaryBuffer->Lock(ByteToLock, BytesToWrite,
											  &Region1, &Region1Size,
											  &Region2, &Region2Size,
											  0)))

	{ // do SUCCEEDED check to see if we violate size limits.  (We should not since we are only sizing, asking and pointing in 32 bit increments but...
		// TODO Assert that Region1 and 2Size is valid
		DWORD Region1SampleCount = Region1Size / SoundOutput->BytesPerSample; // converts the size of a region (in bytes) to samples (which we set to 4 bytes32/bits)
		int16 *DestSample = (int16 *)Region1;
		int16 *SourceSample = SourceBuffer->Samples;

		for (DWORD SampleIndex = 0;
			 SampleIndex < Region1SampleCount; // This is in samples (not bytes)
			 ++SampleIndex)
		{
			// Just copying the Platform Sound buffer in to the Windows API
			*DestSample++ = *SourceSample++; // for left
			*DestSample++ = *SourceSample++; // for right

			++SoundOutput->RunningSampleIndex;
		}

		DWORD Region2SampleCount = Region2Size / SoundOutput->BytesPerSample;
		DestSample = (int16 *)Region2;
		for (DWORD SampleIndex = 0;
			 SampleIndex < Region2SampleCount;
			 ++SampleIndex)
		{
			*DestSample++ = *SourceSample++;
			*DestSample++ = *SourceSample++;

			++SoundOutput->RunningSampleIndex;
		}
		GlobalSecondaryBuffer->Unlock(Region1, Region1Size, Region2, Region2Size);
	}
}

//************************* MAIN
int CALLBACK WinMain(HINSTANCE Instance,
					 HINSTANCE PrevInstance,
					 PSTR CommandLine,
					 INT ShowCode)
{
	Win32LoadXInput();

	WNDCLASSA WindowClass = {};

	Win32ResizeDIBSection(&GlobalBackBuffer, 1280, 720);

	WindowClass.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
	WindowClass.lpfnWndProc = Win32MainWindowCallback;
	WindowClass.hInstance = Instance;
	WindowClass.lpszClassName = "HandmadeHeroWindowClass";

	if (RegisterClassA(&WindowClass))
	{
		HWND Window = CreateWindowExA(
			0,
			WindowClass.lpszClassName,
			"Handmade Hero",
			WS_OVERLAPPEDWINDOW | WS_VISIBLE,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			CW_USEDEFAULT,
			0,
			0,
			Instance,
			0);
		if (Window)
		{
			HDC DeviceContext = GetDC(Window);

			win32_sound_output SoundOutput = {};

			SoundOutput.SamplesPerSecond = 48000;
			SoundOutput.RunningSampleIndex = 0;
			SoundOutput.BytesPerSample = sizeof(int16) * 2;
			SoundOutput.SecondaryBufferSize = SoundOutput.SamplesPerSecond * SoundOutput.BytesPerSample;
			SoundOutput.LatencySampleCount = SoundOutput.SamplesPerSecond / 15;
			Win32InitDSound(Window, SoundOutput.SamplesPerSecond, SoundOutput.SecondaryBufferSize);
			Win32ClearBuffer(&SoundOutput); // Empty it at start
			GlobalSecondaryBuffer->Play(0, 0, DSBPLAY_LOOPING);

			GlobalRunning = true;

			int16 *Samples = (int16 *)VirtualAlloc(0, SoundOutput.SecondaryBufferSize,
												   MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

			LPVOID BaseAddress;

#if HANDMADE_INTERNAL
			BaseAddress = (LPVOID)Terabytes(2); // No real reason for this address except it is "safe"
												// Code mistankingly greyed out as we compile from cl
#else
			BaseAddress = 0;
#endif

			game_memory GameMemory = {};
			GameMemory.PermanentStorageSize = Megabytes(64);
			GameMemory.TemporaryStorageSize = Gigabytes(4);

			uint64 TotalSize = GameMemory.PermanentStorageSize + GameMemory.TemporaryStorageSize;

			GameMemory.PermanentStorage = VirtualAlloc(BaseAddress, TotalSize,
													   MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);

			GameMemory.TemporaryStorage = ((uint8 *)GameMemory.PermanentStorage + GameMemory.PermanentStorageSize);

			if (GameMemory.TemporaryStorage && GameMemory.PermanentStorage && Samples)
			{

				LARGE_INTEGER PerfCountFrequencyResult;
				QueryPerformanceFrequency(&PerfCountFrequencyResult);
				int64 PerfCountFrequency = PerfCountFrequencyResult.QuadPart;

				game_input Input[2] = {};
				game_input *OldInput = &Input[0];
				game_input *NewInput = &Input[1];

				LARGE_INTEGER LastCounter;
				QueryPerformanceCounter(&LastCounter);
				uint64 LastCycleCount = __rdtsc();

				while (GlobalRunning)
				{
					MSG Message;

					while (PeekMessage(&Message, 0, 0, 0, PM_REMOVE))
					{
						if (Message.message == WM_QUIT)
						{
							GlobalRunning = false;
						}
						TranslateMessage(&Message);
						DispatchMessage(&Message);
					}

					int MaxControllerCount = XUSER_MAX_COUNT;

					if (MaxControllerCount > ArrayCount(NewInput->Controllers))
					{
						MaxControllerCount = ArrayCount(NewInput->Controllers);
					}

					for (DWORD ControllerIndex = 0;
						 ControllerIndex < MaxControllerCount;
						 ControllerIndex++)
					{
						game_controller_input *OldController = &OldInput->Controllers[ControllerIndex];
						game_controller_input *NewController = &NewInput->Controllers[ControllerIndex];

						XINPUT_STATE ControllerState;
						if (XInputGetState(ControllerIndex, &ControllerState) == ERROR_SUCCESS)
						{

							XINPUT_GAMEPAD *Pad = &ControllerState.Gamepad;

							bool Up = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_UP);
							bool Down = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_DOWN);
							bool Left = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_LEFT);
							bool Right = (Pad->wButtons & XINPUT_GAMEPAD_DPAD_RIGHT);

							real32 X;
							if (Pad->sThumbLX < 0)
							{
								X = (real32)Pad->sThumbLX / 32768.0f;
							}
							else
							{
								X = (real32)Pad->sThumbLX / 32767.0f;
							}
							NewController->StartX = OldController->EndX;
							NewController->MinX = NewController->MaxX = NewController->EndX = X;

							real32 Y;
							if (Pad->sThumbLY < 0)
							{
								Y = (real32)Pad->sThumbLY / 32768.0f;
							}
							else
							{
								Y = (real32)Pad->sThumbLY / 32767.0f;
							}
							NewController->StartY = OldController->EndY;
							NewController->MinY = NewController->MaxY = NewController->EndY = Y;

							NewController->IsAnalog = true;

							win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_Y,
															&OldController->Up,
															&NewController->Up);
							win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_A,
															&OldController->Down,
															&NewController->Down);
							win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_X,
															&OldController->Left,
															&NewController->Left);
							win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_B,
															&OldController->Right,
															&NewController->Right);
							win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_LEFT_SHOULDER,
															&OldController->LeftShoulder,
															&NewController->LeftShoulder);
							win32ProcessXInputDigitalButton(Pad->wButtons, XINPUT_GAMEPAD_RIGHT_SHOULDER,
															&OldController->RightShoulder,
															&NewController->RightShoulder);
							//bool Start = (Pad->wButtons & XINPUT_GAMEPAD_START);
							//bool Back = (Pad->wButtons & XINPUT_GAMEPAD_BACK);
						}

						else
						{
							// controller is not available
						}
					}
					DWORD ByteToLock = 0;
					DWORD TargetCursor = 0;
					DWORD BytesToWrite = 0;
					DWORD PlayCursor = 0;
					DWORD WriteCursor = 0;

					bool32 SoundIsValid = 0;

					if (SUCCEEDED(GlobalSecondaryBuffer->GetCurrentPosition(&PlayCursor, &WriteCursor)))
					{
						ByteToLock = (SoundOutput.RunningSampleIndex * SoundOutput.BytesPerSample) %
									 SoundOutput.SecondaryBufferSize;
						TargetCursor =
							((PlayCursor +
							  (SoundOutput.LatencySampleCount * SoundOutput.BytesPerSample)) %
							 SoundOutput.SecondaryBufferSize);
						BytesToWrite;
						if (ByteToLock > TargetCursor)
						{
							BytesToWrite = SoundOutput.SecondaryBufferSize - ByteToLock;
							BytesToWrite += TargetCursor;
						}
						else
						{
							BytesToWrite = TargetCursor - ByteToLock;
						}

						SoundIsValid = true;
					}

					game_sound_output_buffer SoundBuffer = {};
					SoundBuffer.SamplesPerSecond = SoundOutput.SamplesPerSecond;
					SoundBuffer.SampleCount = BytesToWrite / SoundOutput.BytesPerSample;
					SoundBuffer.Samples = Samples;

					game_offscreen_buffer Buffer = {};
					Buffer.Memory = GlobalBackBuffer.Memory;
					Buffer.Width = GlobalBackBuffer.Width;
					Buffer.Height = GlobalBackBuffer.Height;
					Buffer.Pitch = GlobalBackBuffer.Pitch;

					GameUpdateAndRender(&Buffer, &SoundBuffer, NewInput, &GameMemory);

					if (SoundIsValid)
					{
						Win32FillSoundBuffer(&SoundOutput, ByteToLock, BytesToWrite, &SoundBuffer);
					}

					win32_window_dimensions Dimensions = Win32GetWindowDimension(Window);
					Win32DisplayBufferInWindow(&GlobalBackBuffer, DeviceContext, Dimensions.Width, Dimensions.Height);

					uint64 EndCycleCount = __rdtsc();
					LARGE_INTEGER EndCounter;
					QueryPerformanceCounter(&EndCounter);

					uint64 CyclesElapsed = EndCycleCount - LastCycleCount;
					int64 CounterElapsed = EndCounter.QuadPart - LastCounter.QuadPart;
					real32 MSPerFrame = (((real32)CounterElapsed * 1000.0f) / ((real32)PerfCountFrequency)); // ( 1000 milseconds per sec * # cycles /1 frame) / cycles/second = seconds per frame.  * 1000 due to integer math on a fast process.
					real32 FPS = (real32)PerfCountFrequency / (real32)CounterElapsed;
					real32 MCPF = ((real32)CyclesElapsed / (1000.0f * 1000.0f)); // int32 so we can %d in printf

					//				char LogBuffer[256];
					//				// NOTE: wspringf is build into windows but cannot do floats (fractional seconds etc).  The C Runtime Library can using sprintf (uses stdio.h)
					//				sprintf(LogBuffer, "%fms/f, %ff/s, %fmc/f\n", MSPerFrame, FPS, MCPF);
					//				OutputDebugStringA(LogBuffer);

					LastCounter = EndCounter;
					LastCycleCount = EndCycleCount;

					game_input *Temp = NewInput;
					NewInput = OldInput;
					OldInput = Temp;

					//ReleaseDC(Window, DeviceContext);  // Releases the Get above to tll Windows it does not need to freeze state anymore
				}
			}
			else
			{
			}
		}
		else
		{
		}
	}
	else
	{
	}

	return (0);
}