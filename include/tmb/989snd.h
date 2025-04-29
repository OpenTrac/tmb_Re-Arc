#ifndef SND989_H_
#define SND989_H_

#include "sys/types.h"
#include <libcdvd.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*SndCompleteProc)(int, u_long);
typedef void* SoundBankPtr;
typedef void* MMDPtr;

void snd_StartSoundSystem(void);
int snd_FlushSoundCommands();
void snd_StopSoundSystem();
u_int snd_GetTick();
void snd_GetTick_CB(SndCompleteProc cb, u_long user_data);
SoundBankPtr snd_BankLoad(char* name, int offset);
int snd_BankLoad_CB(char* name, int offset, SndCompleteProc cb, u_long user_data);
SoundBankPtr snd_BankLoadByLoc(int loc, int offset);
SoundBankPtr snd_BankLoadByLoc_CB(int loc, int offset, SndCompleteProc cb, u_long user_data);
MMDPtr snd_MMDLoad(char* name, int offset);
int snd_MMDLoad_CB(char* name, int offset, SndCompleteProc cb, u_long user_data);
MMDPtr snd_MMDLoadByLoc(int loc, int offset);
void snd_MMDLoadByLoc_CB(int loc, int offset, SndCompleteProc cb, u_long user_data);
void snd_ResolveBankXREFS();
void snd_UnloadBank(SoundBankPtr bank);
void snd_UnloadBank_CB(SoundBankPtr bank, SndCompleteProc cb, u_long user_data);
void snd_UnloadMMD(MMDPtr mmd);
void snd_UnloadMMD_CB(MMDPtr mmd, SndCompleteProc cb, u_long user_data);
void snd_SetMasterVolume(int which, int vol);
int snd_GetMasterVolume(int which);
void snd_GetMasterVolume_CB(int which, SndCompleteProc cb, u_long user_data);
void snd_SetPlaybackMode(int mode);
int snd_GetPlaybackMode();
void snd_GetPlaybackMode_CB(SndCompleteProc cb, u_long user_data);
void snd_SetMixerMode(int channel_mode, int reverb_mode);
u_int snd_PlaySoundVolPanPMPB(
    SoundBankPtr bank, int sound, int vol, int pan, int pitch_mod, int bend);
void snd_PlaySoundVolPanPMPB_A(
    SoundBankPtr bank, int sound, int vol, int pan, int pitch_mod, int bend);
void snd_PlaySoundVolPanPMPB_CB(SoundBankPtr bank, int sound, int vol, int pan, int pitch_mod,
    int bend, SndCompleteProc cb, u_long user_data);
void snd_StopSound(u_int handle);
void snd_PauseSound(u_int handle);
void snd_ContinueSound(u_int handle);
void snd_StopAllSounds(u_int handle);
void snd_PauseAllSoundsInGroup(u_int groups);
void snd_ContinueAllSoundsInGroup(u_int groups);
u_int snd_SoundIsStillPlaying(u_int handle);
void snd_SoundIsStillPlaying_CB(u_int handle, SndCompleteProc cb, u_long user_data);
int snd_IsSoundALooper(SoundBankPtr bank, u_int sound);
void snd_IsSoundALooper_CB(SoundBankPtr bank, u_int sound, SndCompleteProc cb, u_long user_data);
void snd_SetSoundVolPan(u_int handle, int vol, int pan);
int snd_GetSoundOriginalPitch(SoundBankPtr bank, int index);
void snd_GetSoundOriginalPitch_CB(
    SoundBankPtr bank, int index, SndCompleteProc cb, u_long user_data);
int snd_GetSoundCurrentPitch(u_int handle);
void snd_GetSoundCurrentPitch_CB(u_int handle, SndCompleteProc cb, u_long user_data);
void snd_SetSoundPitch(u_int handle, int pitch);
void snd_SetSoundPitchBend(u_int handle, int bend);
void snd_SetSoundPitchModifier(u_int handle, int mod);
u_int snd_SetSoundParams(u_int handle, u_int mask, int vol, int pan, int pitch_mod, int bend);
void snd_SetSoundParams_A(u_int handle, u_int mask, int vol, int pan, int pitch_mod, int bend);
void snd_SetSoundParams_CB(u_int handle, u_int mask, int vol, int pan, int pitch_mod, int bend,
    SndCompleteProc cb, u_long user_data);
void snd_AutoVol(u_int handle, int vol, int delta_time, int delta_from);
void snd_AutoPan(u_int handle, int pan, int dir, int delta_time, int delta_from);
void snd_AutoPitch(u_int handle, int pitch, int delta_time, int delta_from);
void snd_AutoPitchBend(u_int handle, int pb, int delta_time, int delta_from);
void snd_SetMIDIRegister(u_int handle, int value);
int snd_GetMIDIRegister(u_int handle);
void snd_GetMIDIRegister_CB(u_int handle, SndCompleteProc cb, u_long user_data);
void snd_SetGlobalExcite(int value);
int snd_GetGlobalExcite();
int snd_GetGlobalExcite_CB(SndCompleteProc cb, u_long user_data);
int snd_GetVoiceAllocation(int core);
int snd_GetVoiceAllocation_CB(int core, SndCompleteProc cb, u_long user_data);
void snd_StartCaching();
void snd_SendCache();
void snd_InitVAGStreamingEx(int num_channels, int buffer_size, unsigned int read_mode);
void snd_StopVAGStream(u_int stream);
void snd_StopAllStreams();
void snd_CloseVAGStreaming();
u_int snd_PlayVAGStreamByLoc(int loc1, int loc2, int offset1, int offset2, int vol, int pan,
    int vol_group, u_int queue, u_int flags);
void snd_PlayVAGStreamByLoc_A(int loc1, int loc2, int offset1, int offset2, int vol, int pan,
    int vol_group, u_int queue, u_int flags);
void snd_PlayVAGStreamByLoc_CB(int loc1, int loc2, int offset1, int offset2, int vol, int pan,
    int vol_group, u_int queue, u_int flags, SndCompleteProc cb, u_long user_data);
void snd_PauseVAGStream(u_int stream);
void snd_ContinueVAGStream(u_int stream);
int snd_GetVAGStreamQueueCount(u_int stream);
void snd_GetVAGStreamQueueCount_CB(u_int stream, SndCompleteProc cb, u_long user_data);
u_int snd_GetVAGStreamLoc(u_int stream);
void snd_GetVAGStreamLoc_CB(u_int stream, SndCompleteProc cb, u_long user_data);
u_int snd_GetVAGStreamTimeRemaining(u_int stream);
void snd_GetVAGStreamTimeRemaining_CB(u_int stream, SndCompleteProc cb, u_long user_data);
u_int snd_GetVAGStreamTotalTime(u_int stream);
void snd_GetVAGStreamTotalTime_CB(u_int stream, SndCompleteProc cb, u_long user_data);
int snd_StreamSafeCheckCDIdle(int block_ee_iop);
void snd_StreamSafeCheckCDIdle_CB(int block_ee_iop, SndCompleteProc cb, u_long user_data);
int snd_StreamSafeCdRead(u_int lbn, u_int sectors, void* buf, sceCdRMode* mode);
int snd_StreamSafeCdSync(int mode);
int snd_StreamSafeCdBreak(void);
int snd_StreamSafeCdGetError(void);
int* snd_StreamSafeCdCallback(void (*cb)(int));
void snd_SetReverbType(int core, int type);
void snd_SetReverbDepth(int core, int left, int right);
void snd_AutoReverb(int core, int depth, int delta_time, int channel_flags);
int snd_LockVoiceAllocator();
int snd_ExternVoiceAlloc(int vol_group, int priority);
void snd_ExternVoiceFree(int voice);
void snd_UnlockVoiceAllocator();
int snd_SRAMMalloc(int size);
int snd_SRAMMarkUsed(int loc, int size);
void snd_SRAMFree(int loc, int size);
int snd_InitMovieSound(int a0, int a1, int a2, int a3);
void snd_ResetMovieSound();
int snd_PauseMovieSound();
void snd_CloseMovieSound();
void snd_StartMovieSound(int a0, int a1, int a2);
int snd_GetTransStatus();
int snd_GetDopplerPitchMod(int approaching_mph);

#ifdef __cplusplus
}
#endif

#endif // SND989_H_
