#include "tmb/989snd.h"
#include "common.h"

#include <eekernel.h>
#include <eetypes.h>
#include <libcdvd.h>
#include <sifcmd.h>
#include <sifrpc.h>
#include <stdio.h>
#include <string.h>

u_int snd_SendIOPCommandAndWait(int command, int data_used, int* in_data);
void snd_SendIOPCommandNoWait(
    int command, int data_used, int* in_data, SndCompleteProc done, u_long u_data);
void snd_SendPendingLoadCommand();
void snd_SendCurrentBatch();
int snd_StreamSafeCdSync(int);
void snd_PostMessage();

struct SndMessageData { // 0x20
    /* 0x00 */ int data[7];
    /* 0x1c */ int command;
};

typedef struct SndMessageData* SndMessageDataPtr;

struct SndCommandReturnDef { // 0x10
    /* 0x0 */ SndCompleteProc done;
    /* 0x8 */ u_long u_data;
};

struct SndCommandBuffer { // 0x1000
    /* 0x0000 */ int num_commands;
    /* 0x0004 */ struct SndMessageData command[128];
};

typedef struct SndCommandReturnDef* SndCommandReturnDefPtr;

u_int* gCommBusy = 0;
int gAwaitingInts = 0;
int gStreamingInited = 0;
void (*gCdCallback)(int) = NULL;
int gSSRead = 0;
int gSSReadDone = 0;
int gLoadingFromFS = 0;

extern struct SndCommandBuffer gSndCommandBuffer1;
extern struct SndCommandBuffer gSndCommandBuffer2;
extern struct SndCommandReturnDef gSndCommandReturnDef1[128];
extern struct SndCommandReturnDef gSndCommandReturnDef2[128];
extern unsigned int gActualReturnValues1[132];
extern unsigned int gActualReturnValues2[132];

struct SndCommandBuffer* gSndCommandBuffePtr[2] = { &gSndCommandBuffer1, &gSndCommandBuffer2 };
struct SndCommandReturnDef* gSndCommandReturnDefPtr[2]
    = { gSndCommandReturnDef1, gSndCommandReturnDef2 };
u_int* gReturnValuesPtr[2] = { gActualReturnValues1, gActualReturnValues2 };
int gCommandFillBuffer = 0;
int gCaching = 0;
static SndCompleteProc gLoadCB = NULL;
static u_long gLoadUserData = 0;
u_int gLoadCommand = 0;
int gLoadAwaits = 0;

char unk0x48718[32] = { };

static sceSifClientData gSLClientData;

// Aligment for DMA transfer buffers
unsigned int gSyncBuffer[16] __attribute__((aligned(16))) = { };
int gSyncSendBuffer[16] __attribute__((aligned(16))) = { };
char gSyncStringBuffer[256] __attribute__((aligned(16))) = { };
struct SndCommandBuffer gSndCommandBuffer1 __attribute__((aligned(64))) = { };
struct SndCommandBuffer gSndCommandBuffer2 __attribute__((aligned(64))) = { };
struct SndCommandReturnDef gSndCommandReturnDef1[128] = { };
struct SndCommandReturnDef gSndCommandReturnDef2[128] = { };

// Not actually sure on their size
unsigned int gActualReturnValues1[132] __attribute__((aligned(64))) = { };
unsigned int gActualReturnValues2[132] __attribute__((aligned(64))) = { };

enum {
    SL_INIT = 0,
    SL_CLOSE = 1,
    SL_LOADBANK = 2,
    SL_LOADBANKBYLOC = 3,
    SL_LOADMMD = 4,
    SL_LOADMMDBYLOC = 5,
    SL_UNLOADBANK_A = 6,
    SL_UNLOADMIDI_A = 7,
    SL_RESOLVEBANKS = 8,
    SL_SETMASTERVOL_A = 9,
    SL_GETMASTERVOL = 10,
    SL_SETPLAYBACKMODE_A = 11,
    SL_GETPLAYBACKMODE = 12,
    SL_SETMIXERMODE_A = 13,
    SL_SETREVERBTYPE = 14,
    SL_SETREVERBDEPTH = 15,
    SL_AUTOVERB = 16,
    SL_PLAYSOUND = 17,
    SL_PLAYSOUND_A = 18,
    SL_PAUSESOUND = 19,
    SL_CONTINUESOUND = 20,
    SL_STOPSOUND = 21,
    SL_PAUSEALLSOUNDS = 22,
    SL_CONTINUEALLSOUNDS = 23,
    SL_STOPALLSOUNDS = 24,
    SL_SOUNDISSTILLPLAYING = 25,
    SL_ISSOUNDALOOPER = 26,
    SL_SETSOUNDVOLPAN_A = 27,
    SL_GETSOUNDORIGPITCH = 28,
    SL_GETSOUNDCURRPITCH = 29,
    SL_SETSOUNDPITCH_A = 30,
    SL_SETSOUNDPITCHBEND_A = 31,
    SL_SETSOUNDPITCHMOD_A = 32,
    SL_SETSOUNDPARAMS = 33,
    SL_AUTOVOL_A = 34,
    SL_AUTOPAN_A = 35,
    SL_AUTOPITCH_A = 36,
    SL_AUTOPITCHBEND_A = 37,
    SL_GETGLOBALEXCITE = 38,
    SL_SETGLOBALEXCITE_A = 39,
    SL_GETMIDIREGISTER = 40,
    SL_SETMIDIREGISTER_A = 41,
    SL_INITVAGSTREAMINGEX = 42,
    SL_PLAYVAGSTREAMBYNAME = 43,
    SL_PLAYVAGSTREAMBYLOC = 44,
    SL_PAUSEVAGSTREAM = 45,
    SL_CONTINUEVAGSTREAM = 46,
    SL_STOPVAGSTREAM = 47,
    SL_GETVAGSTREAMQUEUECOUNT = 48,
    SL_GETVAGSTREAMLOC = 49,
    SL_GETVAGSTREAMTIMEREMAINING = 50,
    SL_GETVAGSTREAMTOTALTIME = 51,
    SL_STOPALLVAGSTREAMS = 52,
    SL_CLOSEVAGSTREAMING = 53,
    SL_STREAMSAFECHECKCDIDLE = 54,
    SL_STREAMSAFECDBREAK = 55,
    SL_STREAMSAFECDREAD = 56,
    SL_ALLOCIOPRAM = 57,
    SL_FREEIOPRAM = 58,
    SL_INITMOVIESOUND = 59,
    SL_CLOSEMOVIESOUND = 60,
    SL_RESETMOVIESOUND = 61,
    SL_STARTMOVIESOUND = 62,
    SL_PAUSEMOVIESOUND = 63,
    SL_GETTRANSSTATUS = 64,
    SL_GETSTICK = 65,
    SL_GETVOICEALLOC = 66,
    SL_LOCKVALLOC = 67,
    SL_EXTERNVALLOC = 68,
    SL_EXTERNVFREE = 69,
    SL_UNLOCKVALLOC = 70,
    SL_SRAMMALLOC = 71,
    SL_SRAMMARKUSED = 72,
    SL_SRAMFREE = 73,
    SL_COMMAND_BATCH = 74,
};

void snd_StartSoundSystem(void)
{
    int i;

    gSndCommandBuffePtr[0] = &gSndCommandBuffer1;
    gSndCommandBuffePtr[1] = &gSndCommandBuffer2;
    gReturnValuesPtr[0] = gActualReturnValues1;
    gReturnValuesPtr[1] = gActualReturnValues2;
    gSndCommandReturnDefPtr[0] = gSndCommandReturnDef1;
    gSndCommandReturnDefPtr[1] = gSndCommandReturnDef2;

    sceSifInitRpc(0);

    while (1) {
        if (sceSifBindRpc(&gSLClientData, 0x123456, 0) < 0) {
            printf("error: sceSifBindRpc in %s, at line %d\n", "989snd.c", 0x5f);
            while (1)
                ;
        }
        i = 10000;
        while (i--)
            ;
        if (gSLClientData.serve != 0) {
            break;
        }
    }

    sceSifSetSreg(8, 0);
    sceSifSetSreg(9, 0);

    gSndCommandBuffer1.num_commands = 0;
    gSndCommandBuffer2.num_commands = 0;

    snd_SendIOPCommandAndWait(SL_INIT, 0, 0);
}

int snd_FlushSoundCommands()
{
    SndCompleteProc done;
    u_long u_data;
    int which, x;

    if (gCommBusy && snd_GotReturns()) {
        if (gLoadingFromFS) {
            if (gLoadCB) {
                gLoadCB(gSyncBuffer[1], gLoadUserData);
            }
            gLoadCB = NULL;
            gLoadingFromFS = 0;
        } else {
            which = gCommandFillBuffer != 1;
            for (x = 0; x < gSndCommandBuffePtr[which]->num_commands; x++) {
                done = gSndCommandReturnDefPtr[which][x].done;
                if (gSndCommandReturnDefPtr[which][x].done) {
                    u_data = gSndCommandReturnDefPtr[which][x].u_data;
                    done(gReturnValuesPtr[which][x + 1], u_data);
                }
            }
        }
    }

    if (!gCommBusy) {
        if (gLoadAwaits) {
            snd_SendPendingLoadCommand();
            gLoadAwaits = 0;
        } else if (gSndCommandBuffePtr[gCommandFillBuffer]->num_commands && !gCaching) {
            snd_SendCurrentBatch();
        }
    }

    if (gSSRead) {
        snd_StreamSafeCdSync(1);
        if (gSSReadDone) {
            gSSRead = 0;
            gSSReadDone = 0;
            if (gCdCallback) {
                gCdCallback(1);
            }
        }
    }

    if (gCommBusy) {
        return 1;
    }

    return 0;
}

void snd_StopSoundSystem()
{
    snd_SendIOPCommandAndWait(SL_CLOSE, 0, NULL);
    gStreamingInited = 0;
}

u_int snd_GetTick()
{
    return snd_SendIOPCommandAndWait(SL_GETSTICK, 0, NULL);
}

void snd_GetTick_CB(SndCompleteProc cb, u_long user_data)
{
    snd_SendIOPCommandNoWait(SL_GETSTICK, 0, NULL, cb, user_data);
}

int snd_GotReturns(void)
{
    FlushCache(0);
    if (gCommBusy == NULL) {
        return 1;
    }

    if (*gCommBusy == -1 && (gCommBusy[gAwaitingInts + 1] == -1)) {
        gCommBusy = NULL;
        return 1;
    }

    return 0;
}

void snd_PrepareReturnBuffer(u_int* buffer, int num_ints)
{
    gAwaitingInts = num_ints;
    gCommBusy = buffer;
    buffer[num_ints + 1] = 0;
    buffer[0] = 0;
}

SoundBankPtr snd_BankLoad(char* name, int offset)
{
    SoundBankPtr ret;

    *(int*)&gSyncStringBuffer[0] = offset;
    strcpy(&gSyncStringBuffer[4], name);

    while (gCommBusy) {
        snd_FlushSoundCommands();
    }

    snd_PrepareReturnBuffer(gSyncBuffer, 1);

    while (sceSifCheckStatRpc(&gSLClientData.rpcd) != 0) {
        printf("989snd.c: RPC collision!\n");
        snd_FlushSoundCommands();
        FlushCache(0);
    }

    sceSifCallRpc(&gSLClientData, SL_LOADBANK, 1, gSyncStringBuffer, strlen(name) + 4, gSyncBuffer,
        0xc, 0, 0);
    while (!snd_GotReturns())
        ;

    ret = (SoundBankPtr)gSyncBuffer[1];

    if (gSndCommandBuffePtr[gCommandFillBuffer]->num_commands) {
        snd_SendCurrentBatch();
    }

    return ret;
}

int snd_BankLoad_CB(char* name, int offset, SndCompleteProc cb, u_long user_data)
{
    if (gLoadCB) {
        return 0;
    }

    gLoadCB = cb;
    gLoadUserData = user_data;
    gLoadCommand = 2;
    *(int*)&gSyncStringBuffer[0] = offset;
    strcpy(&gSyncStringBuffer[4], name);

    if (gCommBusy) {
        gLoadAwaits = 1;
        return 1;
    }

    gLoadAwaits = 0;
    snd_SendPendingLoadCommand();

    return 1;
}

SoundBankPtr snd_BankLoadByLoc(int loc, int offset)
{
    int data[2];

    if (snd_StreamSafeCdSync(1) == 1) {
        printf("snd_BankLoadByLoc: CD BUSY!\n");
        return 0;
    }

    data[0] = (int)loc;
    data[1] = (int)offset;

    snd_SendIOPCommandAndWait(SL_LOADBANKBYLOC, 2, data);
}

SoundBankPtr snd_BankLoadByLoc_CB(int loc, int offset, SndCompleteProc cb, u_long user_data)
{
    int data[2];

    data[0] = (int)loc;
    data[1] = (int)offset;

    snd_SendIOPCommandNoWait(SL_LOADBANKBYLOC, 2, data, cb, user_data);
}

void snd_SendPendingLoadCommand()
{
    gCommBusy = gSyncBuffer;
    gLoadingFromFS = 1;
    snd_PrepareReturnBuffer(gSyncBuffer, 1);
    while (sceSifCheckStatRpc(&gSLClientData.rpcd) != 0) {
        printf("989snd.c: RPC collision!\n");
        snd_FlushSoundCommands();
        FlushCache(0);
    }

    sceSifCallRpc(&gSLClientData, gLoadCommand, 1, gSyncStringBuffer,
        strlen(&gSyncStringBuffer[4]) + 4, gSyncBuffer, 0xc, 0, 0);
}

MMDPtr snd_MMDLoad(char* name, int offset)
{
    MMDPtr ret;

    *(int*)&gSyncStringBuffer[0] = offset;
    strcpy(&gSyncStringBuffer[4], name);

    while (gCommBusy)
        ;

    snd_FlushSoundCommands();
    snd_PrepareReturnBuffer(gSyncBuffer, 1);

    while (sceSifCheckStatRpc(&gSLClientData.rpcd) != 0) {
        printf("989snd.c: RPC collision!\n");
        snd_FlushSoundCommands();
        FlushCache(0);
    }

    sceSifCallRpc(
        &gSLClientData, 4, 1, gSyncStringBuffer, strlen(name) + 4, gSyncBuffer, 0x4, 0, 0);

    while (!snd_GotReturns())
        ;

    ret = (MMDPtr)gSyncBuffer[1];

    if (gSndCommandBuffePtr[gCommandFillBuffer]->num_commands) {
        snd_SendCurrentBatch();
    }

    return ret;
}

int snd_MMDLoad_CB(char* name, int offset, SndCompleteProc cb, u_long user_data)
{
    if (gLoadCB) {
        return 0;
    }

    gLoadCB = cb;
    gLoadUserData = user_data;
    gLoadCommand = 4;
    *(int*)&gSyncStringBuffer[0] = offset;
    strcpy(&gSyncStringBuffer[4], name);

    if (gCommBusy) {
        gLoadAwaits = 1;
        return 1;
    }

    gLoadAwaits = 0;
    snd_SendPendingLoadCommand();

    return 1;
}

MMDPtr snd_MMDLoadByLoc(int loc, int offset)
{
    int data[2];

    data[0] = (int)loc;
    data[1] = (int)offset;

    snd_SendIOPCommandAndWait(SL_LOADMMDBYLOC, 2, data);
}

void snd_MMDLoadByLoc_CB(int loc, int offset, SndCompleteProc cb, u_long user_data)
{
    int data[2];

    data[0] = (int)loc;
    data[1] = (int)offset;

    snd_SendIOPCommandNoWait(SL_LOADMMDBYLOC, 2, data, cb, user_data);
}

void snd_ResolveBankXREFS()
{
    snd_SendIOPCommandNoWait(SL_RESOLVEBANKS, 0, NULL, NULL, 0);
}

void snd_UnloadBank(SoundBankPtr bank)
{
    int data[1];

    data[0] = (int)bank;

    snd_SendIOPCommandNoWait(SL_UNLOADBANK_A, 1, data, NULL, 0);
}

void snd_UnloadBank_CB(SoundBankPtr bank, SndCompleteProc cb, u_long user_data)
{
    int data[1];

    data[0] = (int)bank;

    snd_SendIOPCommandNoWait(SL_UNLOADBANK_A, 1, data, cb, user_data);
}

void snd_UnloadMMD(MMDPtr mmd)
{
    int data[1];

    data[0] = (int)mmd;

    snd_SendIOPCommandNoWait(SL_UNLOADMIDI_A, 1, data, NULL, 0);
}

void snd_UnloadMMD_CB(MMDPtr mmd, SndCompleteProc cb, u_long user_data)
{
    int data[1];

    data[0] = (int)mmd;

    snd_SendIOPCommandNoWait(SL_UNLOADMIDI_A, 1, data, cb, user_data);
}

void snd_SetMasterVolume(int which, int vol)
{
    int data[2];

    data[0] = which;
    data[1] = vol;

    snd_SendIOPCommandNoWait(SL_SETMASTERVOL_A, 2, data, NULL, 0);
}

int snd_GetMasterVolume(int which)
{
    int data[1];

    data[0] = which;

    return snd_SendIOPCommandAndWait(SL_GETMASTERVOL, 1, data);
}

void snd_GetMasterVolume_CB(int which, SndCompleteProc cb, u_long user_data)
{
    int data[1];

    data[0] = which;

    snd_SendIOPCommandNoWait(SL_GETMASTERVOL, 1, data, cb, user_data);
}

void snd_SetPlaybackMode(int mode)
{
    int data[1];

    data[0] = mode;

    snd_SendIOPCommandNoWait(SL_SETPLAYBACKMODE_A, 1, data, NULL, 0);
}

int snd_GetPlaybackMode()
{
    return snd_SendIOPCommandAndWait(SL_GETPLAYBACKMODE, 0, NULL);
}

void snd_GetPlaybackMode_CB(SndCompleteProc cb, u_long user_data)
{

    snd_SendIOPCommandNoWait(SL_GETPLAYBACKMODE, 0, NULL, cb, user_data);
}

void snd_SetMixerMode(int channel_mode, int reverb_mode)
{
    int data[2];

    data[0] = channel_mode;
    data[1] = reverb_mode;

    snd_SendIOPCommandNoWait(SL_SETMIXERMODE_A, 2, data, NULL, 0);
}

u_int snd_PlaySoundVolPanPMPB(
    SoundBankPtr bank, int sound, int vol, int pan, int pitch_mod, int bend)
{
    int data[6];

    data[0] = (int)bank;
    data[1] = sound;
    data[2] = vol;
    data[3] = pan;
    data[4] = pitch_mod;
    data[5] = bend;

    return snd_SendIOPCommandAndWait(SL_PLAYSOUND, 6, data);
}

void snd_PlaySoundVolPanPMPB_A(
    SoundBankPtr bank, int sound, int vol, int pan, int pitch_mod, int bend)
{
    int data[6];

    data[0] = (int)bank;
    data[1] = sound;
    data[2] = vol;
    data[3] = pan;
    data[4] = pitch_mod;
    data[5] = bend;

    snd_SendIOPCommandNoWait(SL_PLAYSOUND_A, 6, data, NULL, 0);
}

void snd_PlaySoundVolPanPMPB_CB(SoundBankPtr bank, int sound, int vol, int pan, int pitch_mod,
    int bend, SndCompleteProc cb, u_long user_data)
{
    int data[6];

    data[0] = (int)bank;
    data[1] = sound;
    data[2] = vol;
    data[3] = pan;
    data[4] = pitch_mod;
    data[5] = bend;

    snd_SendIOPCommandNoWait(SL_PLAYSOUND, 6, data, cb, user_data);
}

void snd_StopSound(u_int handle)
{
    int data[1];

    data[0] = handle;

    snd_SendIOPCommandNoWait(SL_STOPSOUND, 1, data, NULL, 0);
}

void snd_PauseSound(u_int handle)
{
    int data[1];

    data[0] = handle;

    snd_SendIOPCommandNoWait(SL_PAUSESOUND, 1, data, NULL, 0);
}

void snd_ContinueSound(u_int handle)
{
    int data[1];

    data[0] = handle;

    snd_SendIOPCommandNoWait(SL_CONTINUESOUND, 1, data, NULL, 0);
}

void snd_StopAllSounds(u_int handle)
{
    snd_SendIOPCommandNoWait(SL_STOPALLSOUNDS, 0, NULL, NULL, 0);
}

void snd_PauseAllSoundsInGroup(u_int groups)

{
    int data[1];

    data[0] = groups;

    snd_SendIOPCommandNoWait(SL_PAUSEALLSOUNDS, 1, data, NULL, 0);
}

void snd_ContinueAllSoundsInGroup(u_int groups)

{
    int data[1];

    data[0] = groups;

    snd_SendIOPCommandNoWait(SL_CONTINUEALLSOUNDS, 1, data, NULL, 0);
}

u_int snd_SoundIsStillPlaying(u_int handle)
{
    int data[1];

    data[0] = handle;

    return snd_SendIOPCommandAndWait(SL_SOUNDISSTILLPLAYING, 1, data);
}

void snd_SoundIsStillPlaying_CB(u_int handle, SndCompleteProc cb, u_long user_data)
{
    int data[1];

    data[0] = handle;

    snd_SendIOPCommandNoWait(SL_SOUNDISSTILLPLAYING, 1, data, cb, user_data);
}

int snd_IsSoundALooper(SoundBankPtr bank, u_int sound)
{
    int data[2];

    data[0] = (int)bank;
    data[1] = sound;

    return snd_SendIOPCommandAndWait(SL_ISSOUNDALOOPER, 2, data);
}

void snd_IsSoundALooper_CB(SoundBankPtr bank, u_int sound, SndCompleteProc cb, u_long user_data)
{
    int data[2];

    data[0] = (int)bank;
    data[1] = sound;

    snd_SendIOPCommandNoWait(SL_ISSOUNDALOOPER, 2, data, cb, user_data);
}

void snd_SetSoundVolPan(u_int handle, int vol, int pan)
{
    int data[3];

    data[0] = handle;
    data[1] = vol;
    data[2] = pan;

    snd_SendIOPCommandNoWait(SL_SETSOUNDVOLPAN_A, 3, data, NULL, 0);
}

int snd_GetSoundOriginalPitch(SoundBankPtr bank, int index)
{
    int data[2];

    data[0] = (int)bank;
    data[1] = index;

    return snd_SendIOPCommandAndWait(SL_GETSOUNDORIGPITCH, 2, data);
}

void snd_GetSoundOriginalPitch_CB(
    SoundBankPtr bank, int index, SndCompleteProc cb, u_long user_data)
{
    int data[2];

    data[0] = (int)bank;
    data[1] = index;

    snd_SendIOPCommandNoWait(SL_GETSOUNDORIGPITCH, 2, data, cb, user_data);
}

int snd_GetSoundCurrentPitch(u_int handle)
{
    int data[1];

    data[0] = handle;

    return snd_SendIOPCommandAndWait(SL_GETSOUNDCURRPITCH, 1, data);
}

void snd_GetSoundCurrentPitch_CB(u_int handle, SndCompleteProc cb, u_long user_data)
{
    int data[1];

    data[0] = handle;

    snd_SendIOPCommandNoWait(SL_GETSOUNDCURRPITCH, 1, data, cb, user_data);
}

void snd_SetSoundPitch(u_int handle, int pitch)
{
    int data[2];

    data[0] = handle;
    data[1] = pitch;

    snd_SendIOPCommandNoWait(SL_SETSOUNDPITCH_A, 2, data, NULL, 0);
}

void snd_SetSoundPitchBend(u_int handle, int bend)
{
    int data[2];

    data[0] = handle;
    data[1] = bend;

    snd_SendIOPCommandNoWait(SL_SETSOUNDPITCHBEND_A, 2, data, NULL, 0);
}

void snd_SetSoundPitchModifier(u_int handle, int mod)
{
    int data[2];

    data[0] = handle;
    data[1] = mod;

    snd_SendIOPCommandNoWait(SL_SETSOUNDPITCHMOD_A, 2, data, NULL, 0);
}

u_int snd_SetSoundParams(u_int handle, u_int mask, int vol, int pan, int pitch_mod, int bend)
{
    int data[6];

    data[0] = handle;
    data[1] = mask;
    data[2] = vol;
    data[3] = pan;
    data[4] = pitch_mod;
    data[5] = bend;

    return snd_SendIOPCommandAndWait(SL_SETSOUNDPARAMS, 6, data);
}

void snd_SetSoundParams_A(u_int handle, u_int mask, int vol, int pan, int pitch_mod, int bend)
{
    snd_SetSoundParams_CB(handle, mask, vol, pan, pitch_mod, bend, NULL, 0);
}

void snd_SetSoundParams_CB(u_int handle, u_int mask, int vol, int pan, int pitch_mod, int bend,
    SndCompleteProc cb, u_long user_data)
{
    int data[6];

    data[0] = handle;
    data[1] = mask;
    data[2] = vol;
    data[3] = pan;
    data[4] = pitch_mod;
    data[5] = bend;

    snd_SendIOPCommandNoWait(SL_SETSOUNDPARAMS, 6, data, cb, user_data);
}

void snd_AutoVol(u_int handle, int vol, int delta_time, int delta_from)
{
    int data[4];

    data[0] = handle;
    data[1] = vol;
    data[2] = delta_time;
    data[3] = delta_from;

    snd_SendIOPCommandNoWait(SL_AUTOVOL_A, 4, data, NULL, 0);
}

void snd_AutoPan(u_int handle, int pan, int dir, int delta_time, int delta_from)
{
    int data[5];

    data[0] = handle;
    data[1] = pan;
    data[2] = dir;
    data[3] = delta_time;
    data[4] = delta_from;

    snd_SendIOPCommandNoWait(SL_AUTOPAN_A, 5, data, NULL, 0);
}

void snd_AutoPitch(u_int handle, int pitch, int delta_time, int delta_from)
{
    int data[4];

    data[0] = handle;
    data[1] = pitch;
    data[2] = delta_time;
    data[3] = delta_from;

    snd_SendIOPCommandNoWait(SL_AUTOPITCH_A, 4, data, NULL, 0);
}

void snd_AutoPitchBend(u_int handle, int pb, int delta_time, int delta_from)
{
    int data[4];

    data[0] = handle;
    data[1] = pb;
    data[2] = delta_time;
    data[3] = delta_from;

    snd_SendIOPCommandNoWait(SL_AUTOPITCHBEND_A, 4, data, NULL, 0);
}

// Did this 989snd version not support more than one midi reg?
// Or did have global midi regs, like it does for SFX?
// Odd, should check the IOP module for what's up
void snd_SetMIDIRegister(u_int handle, int value)
{
    int data[2];

    data[0] = handle;
    data[1] = value;

    snd_SendIOPCommandNoWait(SL_SETMIDIREGISTER_A, 2, data, NULL, 0);
}

int snd_GetMIDIRegister(u_int handle)
{
    int data[1];

    data[0] = handle;

    return snd_SendIOPCommandAndWait(SL_GETMIDIREGISTER, 1, data);
}

void snd_GetMIDIRegister_CB(u_int handle, SndCompleteProc cb, u_long user_data)
{
    int data[1];

    data[0] = handle;

    snd_SendIOPCommandNoWait(SL_GETMIDIREGISTER, 1, data, cb, user_data);
}

void snd_SetGlobalExcite(int value)
{
    int data[1];

    data[0] = value;

    snd_SendIOPCommandNoWait(SL_SETGLOBALEXCITE_A, 1, data, NULL, 0);
}

int snd_GetGlobalExcite()
{
    return snd_SendIOPCommandAndWait(SL_GETGLOBALEXCITE, 0, NULL);
}

int snd_GetGlobalExcite_CB(SndCompleteProc cb, u_long user_data)
{
    snd_SendIOPCommandNoWait(SL_GETGLOBALEXCITE, 0, NULL, cb, user_data);
}

int snd_GetVoiceAllocation(int core)
{
    int data[1];

    data[0] = core;

    return snd_SendIOPCommandAndWait(SL_GETVOICEALLOC, 1, data);
}

int snd_GetVoiceAllocation_CB(int core, SndCompleteProc cb, u_long user_data)
{
    int data[1];

    data[0] = core;

    snd_SendIOPCommandNoWait(SL_GETVOICEALLOC, 1, data, cb, user_data);
}

u_int snd_SendIOPCommandAndWait(int command, int data_used, int* in_data)
{
    u_int ret_val;
    int x;

    for (x = 0; x < data_used; x++) {
        gSyncSendBuffer[x] = in_data[x];
    }

    while (gCommBusy) {
        snd_FlushSoundCommands();
        FlushCache(0);
    }

    snd_PrepareReturnBuffer(gSyncBuffer, 1);
    while (sceSifCheckStatRpc(&gSLClientData.rpcd)) {
        printf("989snd.c: RPC collision!\n");
        snd_FlushSoundCommands();
        FlushCache(0);
    }

    if (data_used) {
        sceSifCallRpc(&gSLClientData, command, 1, gSyncSendBuffer, data_used * sizeof(int),
            gSyncBuffer, 0xc, NULL, NULL);
    } else {
        sceSifCallRpc(&gSLClientData, command, 1, NULL, 0, gSyncBuffer, 0xc, NULL, NULL);
    }

    while (!snd_GotReturns())
        ;

    ret_val = gSyncBuffer[1];

    if (gSndCommandBuffePtr[gCommandFillBuffer]->num_commands && !gCaching) {
        snd_SendCurrentBatch();
    }

    return ret_val;
}

void snd_SendIOPCommandNoWait(
    int command, int data_used, int* in_data, SndCompleteProc done, u_long u_data)
{
    int x, was_caching = 0, y;
    SndMessageDataPtr msg;

    if (!gCaching && !gCommBusy && !data_used && !done) {
        snd_PrepareReturnBuffer(gSyncBuffer, 1);

        while (sceSifCheckStatRpc(&gSLClientData.rpcd)) {
            printf("989snd.c: RPC collision!\n");
            snd_FlushSoundCommands();
            FlushCache(0);
        }

        sceSifCallRpc(&gSLClientData, command, 1, NULL, 0, gSyncBuffer, 0xc, NULL, NULL);
        return;
    }

    y = 0;

    while (gSndCommandBuffePtr[gCommandFillBuffer]->num_commands == 128) {
        if (gCaching != 0) {
            gCaching = 0;
            was_caching = 1;
        }
        snd_FlushSoundCommands();
        if (y == 1) {
            printf("snd_SendIOPCommandNoWait: BUFFER %d FULL(%d)! Sound stalled waiting for IOP "
                   "comands to finish!\n",
                gCommandFillBuffer, gSndCommandBuffePtr[gCommandFillBuffer]->num_commands);
        }
        y++;
    }

    if (y) {
        printf("snd_SendIOPCommandNoWait: continueing (%d).\n", y);
    }

    if (was_caching) {
        gCaching = 1;
    }

    msg = &gSndCommandBuffePtr[gCommandFillBuffer]
               ->command[gSndCommandBuffePtr[gCommandFillBuffer]->num_commands];

    msg->command = command;
    for (x = 0; x < data_used; x++) {
        msg->data[x] = in_data[x];
    }

    gSndCommandReturnDefPtr[gCommandFillBuffer]
                           [gSndCommandBuffePtr[gCommandFillBuffer]->num_commands]
                               .done = done;
    gSndCommandReturnDefPtr[gCommandFillBuffer]
                           [gSndCommandBuffePtr[gCommandFillBuffer]->num_commands]
                               .u_data = u_data;
    snd_PostMessage();
}

void snd_PostMessage()
{
    gSndCommandBuffePtr[gCommandFillBuffer]->num_commands
        = gSndCommandBuffePtr[gCommandFillBuffer]->num_commands + 1;
    snd_FlushSoundCommands();
}

void snd_SendCurrentBatch()
{
    snd_PrepareReturnBuffer(gReturnValuesPtr[gCommandFillBuffer],
        gSndCommandBuffePtr[gCommandFillBuffer]->num_commands);

    while (sceSifCheckStatRpc(&gSLClientData.rpcd) != 0) {
        printf("989snd.c: RPC collision!\n");
        snd_FlushSoundCommands();
        FlushCache(0);
    }

    // TODO SL enum wrong here
    sceSifCallRpc(&gSLClientData, SL_COMMAND_BATCH, 1, gSndCommandBuffePtr[gCommandFillBuffer],
        (gSndCommandBuffePtr[gCommandFillBuffer]->num_commands * sizeof(struct SndMessageData)) | 4,
        gReturnValuesPtr[gCommandFillBuffer],
        (gSndCommandBuffePtr[gCommandFillBuffer]->num_commands * sizeof(int)) + 8, 0, 0);

    gCommandFillBuffer = gCommandFillBuffer != 1;
    gSndCommandBuffePtr[gCommandFillBuffer]->num_commands = 0;
}

void snd_StartCaching()
{
    gCaching = 1;
}

void snd_SendCache()
{
    gCaching = 0;
    snd_FlushSoundCommands();
}

// Args uncertain, one missing compared to reference
void snd_InitVAGStreamingEx(int num_channels, int buffer_size, unsigned int read_mode)
{
    int data[3];

    if (gStreamingInited == 1) {
        return;
    }

    snd_StreamSafeCdSync(0);
    gStreamingInited = 1;
    data[0] = num_channels;
    data[1] = buffer_size;
    data[2] = read_mode;

    snd_SendIOPCommandNoWait(SL_INITVAGSTREAMINGEX, 3, data, NULL, 0);
}

void snd_StopVAGStream(u_int stream)
{
    int data[1];

    data[0] = stream;

    snd_SendIOPCommandNoWait(SL_STOPVAGSTREAM, 1, data, NULL, 0);
}

void snd_StopAllStreams()
{
    snd_SendIOPCommandNoWait(SL_STOPALLVAGSTREAMS, 0, NULL, NULL, 0);
}

void snd_CloseVAGStreaming()
{
    if (!gStreamingInited) {
        return;
    }

    snd_StreamSafeCdSync(0);
    gStreamingInited = 0;
    snd_SendIOPCommandNoWait(SL_CLOSEVAGSTREAMING, 0, NULL, NULL, 0);
}

u_int snd_PlayVAGStreamByLoc(int loc1, int loc2, int offset1, int offset2, int vol, int pan,
    int vol_group, u_int queue, u_int flags)
{
    int data[7];

    data[0] = loc1;
    data[1] = loc2;
    data[2] = vol << 0x10 | offset1 & 0xffff;
    data[3] = pan << 0x10 | offset2 & 0xffff;
    data[4] = vol_group;
    data[5] = queue;
    data[6] = flags;

    return snd_SendIOPCommandAndWait(SL_PLAYVAGSTREAMBYLOC, 7, data);
}

void snd_PlayVAGStreamByLoc_A(int loc1, int loc2, int offset1, int offset2, int vol, int pan,
    int vol_group, u_int queue, u_int flags)
{
    snd_PlayVAGStreamByLoc_CB(
        loc1, loc2, offset1, offset2, vol, pan, vol_group, queue, flags, NULL, 0);
}

void snd_PlayVAGStreamByLoc_CB(int loc1, int loc2, int offset1, int offset2, int vol, int pan,
    int vol_group, u_int queue, u_int flags, SndCompleteProc cb, u_long user_data)
{
    int data[7];

    data[0] = loc1;
    data[1] = loc2;
    data[2] = vol << 0x10 | offset1 & 0xffff;
    data[3] = pan << 0x10 | offset2 & 0xffff;
    data[4] = vol_group;
    data[5] = queue;
    data[6] = flags;

    snd_SendIOPCommandNoWait(SL_PLAYVAGSTREAMBYLOC, 7, data, cb, user_data);
}

void snd_PauseVAGStream(u_int stream)
{
    int data[1];

    data[0] = stream;

    snd_SendIOPCommandNoWait(SL_PAUSEVAGSTREAM, 1, data, NULL, 0);
}

void snd_ContinueVAGStream(u_int stream)
{
    int data[1];

    data[0] = stream;

    snd_SendIOPCommandNoWait(SL_CONTINUEVAGSTREAM, 1, data, NULL, 0);
}

int snd_GetVAGStreamQueueCount(u_int stream)
{
    int data[1];

    data[0] = stream;

    return snd_SendIOPCommandAndWait(SL_GETVAGSTREAMQUEUECOUNT, 1, data);
}

void snd_GetVAGStreamQueueCount_CB(u_int stream, SndCompleteProc cb, u_long user_data)
{
    int data[1];

    data[0] = stream;

    snd_SendIOPCommandNoWait(SL_GETVAGSTREAMQUEUECOUNT, 1, data, cb, user_data);
}

u_int snd_GetVAGStreamLoc(u_int stream)
{
    int data[1];

    data[0] = stream;

    snd_SendIOPCommandAndWait(SL_GETVAGSTREAMLOC, 1, data);
}

void snd_GetVAGStreamLoc_CB(u_int stream, SndCompleteProc cb, u_long user_data)
{
    int data[1];

    data[0] = stream;

    snd_SendIOPCommandNoWait(SL_GETVAGSTREAMLOC, 1, data, cb, user_data);
}

u_int snd_GetVAGStreamTimeRemaining(u_int stream)
{
    int data[1];

    data[0] = stream;

    snd_SendIOPCommandAndWait(SL_GETVAGSTREAMTIMEREMAINING, 1, data);
}

void snd_GetVAGStreamTimeRemaining_CB(u_int stream, SndCompleteProc cb, u_long user_data)
{
    int data[1];

    data[0] = stream;

    snd_SendIOPCommandNoWait(SL_GETVAGSTREAMTIMEREMAINING, 1, data, cb, user_data);
}

u_int snd_GetVAGStreamTotalTime(u_int stream)
{
    int data[1];

    data[0] = stream;

    snd_SendIOPCommandAndWait(SL_GETVAGSTREAMTOTALTIME, 1, data);
}

void snd_GetVAGStreamTotalTime_CB(u_int stream, SndCompleteProc cb, u_long user_data)
{
    int data[1];

    data[0] = stream;

    snd_SendIOPCommandNoWait(SL_GETVAGSTREAMTOTALTIME, 1, data, cb, user_data);
}

int snd_StreamSafeCheckCDIdle(int block_ee_iop)
{
    int data[1];

    data[0] = block_ee_iop;

    return snd_SendIOPCommandAndWait(SL_STREAMSAFECHECKCDIDLE, 1, data);
}

void snd_StreamSafeCheckCDIdle_CB(int block_ee_iop, SndCompleteProc cb, u_long user_data)
{
    int data[1];

    data[0] = block_ee_iop;

    snd_SendIOPCommandNoWait(SL_STREAMSAFECHECKCDIDLE, 1, data, cb, user_data);
}

int snd_StreamSafeCdRead(u_int lbn, u_int sectors, void* buf, sceCdRMode* mode)
{
    int data[3];

    if (gStreamingInited == 0) {
        return sceCdRead(lbn, sectors, buf, mode);
    }

    if (snd_StreamSafeCdSync(1) == 1) {
        return 0;
    }

    sceSifSetSreg(8, 1);
    sceSifSetSreg(9, 0);

    data[0] = lbn;
    data[1] = sectors;
    data[2] = (int)buf;

    snd_SendIOPCommandNoWait(SL_STREAMSAFECDREAD, 3, data, NULL, 0);

    gSSReadDone = 0;
    gSSRead = 1;

    return 1;
}

int snd_StreamSafeCdSync(int mode)
{
    if (gStreamingInited == 0) {
        return sceCdSync(mode);
    }

    gSSReadDone = !sceSifGetSreg(8);
    if (gSSReadDone != 1) {
        if (mode == 1) {
            return 1;
        }

        while (!gSSReadDone) {
            gSSReadDone = !sceSifGetSreg(8);
            snd_FlushSoundCommands();
        }
    }

    return 0;
}

int snd_StreamSafeCdBreak(void)
{
    if (gStreamingInited == 0) {
        return sceCdBreak();
    }

    snd_SendIOPCommandNoWait(SL_STREAMSAFECDBREAK, 0, NULL, NULL, 0);
    return 1;
}

int snd_StreamSafeCdGetError(void)
{
    if (!gStreamingInited) {
        return sceCdGetError();
    }

    return sceSifGetSreg(9);
}

int* snd_StreamSafeCdCallback(void (*cb)(int))
{
    void (*func_hold)(int);

    if (!gStreamingInited) {
        func_hold = sceCdCallback(cb);
        return func_hold;
    }

    func_hold = gCdCallback;
    gCdCallback = cb;

    return (int*)func_hold;
}

void snd_SetReverbType(int core, int type)
{
    int data[2];

    data[0] = core;
    data[1] = type;

    snd_SendIOPCommandNoWait(SL_SETREVERBTYPE, 2, data, NULL, 0);
}

void snd_SetReverbDepth(int core, int left, int right)
{
    int data[3];

    data[0] = core;
    data[1] = left;
    data[2] = right;

    snd_SendIOPCommandNoWait(SL_SETREVERBDEPTH, 3, data, NULL, 0);
}

void snd_AutoReverb(int core, int depth, int delta_time, int channel_flags)
{
    int data[4];

    data[0] = core;
    data[1] = depth;
    data[2] = delta_time;
    data[3] = channel_flags;

    snd_SendIOPCommandNoWait(SL_AUTOVERB, 4, data, NULL, 0);
}

int snd_LockVoiceAllocator()
{
    return snd_SendIOPCommandAndWait(SL_LOCKVALLOC, 0, 0);
}

int snd_ExternVoiceAlloc(int vol_group, int priority)
{
    int data[2];

    data[0] = vol_group;
    data[1] = priority;

    return snd_SendIOPCommandAndWait(SL_EXTERNVALLOC, 2, data);
}

void snd_ExternVoiceFree(int voice)
{
    int data[1];

    data[0] = voice;

    snd_SendIOPCommandNoWait(SL_EXTERNVFREE, 1, data, NULL, 0);
}

void snd_UnlockVoiceAllocator()
{
    snd_SendIOPCommandNoWait(SL_UNLOCKVALLOC, 0, NULL, NULL, 0);
}

int snd_SRAMMalloc(int size)
{
    int data[1];

    data[0] = (int)size;

    return snd_SendIOPCommandAndWait(SL_SRAMMALLOC, 1, data);
}

int snd_SRAMMarkUsed(int loc, int size)
{
    int data[2];

    data[0] = (int)loc;
    data[1] = (int)size;

    return snd_SendIOPCommandAndWait(SL_SRAMMARKUSED, 2, data);
}

void snd_SRAMFree(int loc, int size)
{
    int data[2];

    data[0] = (int)loc;
    data[1] = (int)size;

    snd_SendIOPCommandNoWait(SL_SRAMFREE, 2, data, NULL, 0);
}

int snd_InitMovieSound(int a0, int a1, int a2, int a3)
{
    int data[4];

    data[0] = a0;
    data[1] = a1;
    data[2] = a2;
    data[3] = a3;

    return snd_SendIOPCommandAndWait(SL_INITMOVIESOUND, 4, data);
}

void snd_ResetMovieSound()
{
    snd_SendIOPCommandAndWait(SL_RESETMOVIESOUND, 0, NULL);
}

int snd_PauseMovieSound()
{
    return snd_SendIOPCommandAndWait(SL_PAUSEMOVIESOUND, 0, NULL);
}

void snd_CloseMovieSound()
{
    snd_SendIOPCommandAndWait(SL_CLOSEMOVIESOUND, 0, NULL);
}

void snd_StartMovieSound(int a0, int a1, int a2)
{
    int data[3];

    data[0] = a0;
    data[1] = a1;
    data[2] = a2;

    snd_SendIOPCommandAndWait(SL_STARTMOVIESOUND, 3, data);
}

int snd_GetTransStatus()
{
    return snd_SendIOPCommandAndWait(SL_GETTRANSSTATUS, 0, NULL);
}

int snd_GetDopplerPitchMod(int approaching_mph)
{
    return (approaching_mph * 0x5f4) / 0x2e5;
}
