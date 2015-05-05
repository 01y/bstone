/* ==============================================================
bstone: A source port of Blake Stone: Planet Strike

Copyright (c) 1992-2013 Apogee Entertainment, LLC
Copyright (c) 2013 Boris Bendovsky (bibendovsky@hotmail.com)

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the
Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
============================================================== */


//
//      ID Engine
//      ID_SD.c - Sound Manager for Wolfenstein 3D
//      v1.3 (revised for **********, screwed with for Blake Stone)
//      By Jason Blochowiak
//

//
//      This module handles dealing with generating sound on the appropriate
//              hardware
//
//      Depends on: User Mgr (for parm checking)
//
//      Globals:
//              For User Mgr:
//                      SoundSourcePresent - Sound Source thingie present?
//                      SoundBlasterPresent - SoundBlaster card present?
//                      AdLibPresent - AdLib card present?
//                      SoundMode - What device is used for sound effects
//                              (Use SM_SetSoundMode() to set)
//                      MusicMode - What device is used for music
//                              (Use SM_SetMusicMode() to set)
//                      DigiMode - What device is used for digitized sound effects
//                              (Use SM_SetDigiDevice() to set)
//
//              For Cache Mgr:
//                      NeedsDigitized - load digitized sounds?
//                      NeedsMusic - load music?
//


#include "id_sd.h"
#include "3d_def.h"
#include "bstone_audio_mixer.h"
#include "bstone_memory_binary_reader.h"

extern uint16_t sdStartPCSounds;
extern uint16_t sdStartALSounds;
extern int16_t sdLastSound;
extern int16_t DigiMap[];


// Global variables
bool sd_has_audio = false;
bool sd_is_sound_enabled = false;
bool sd_is_music_enabled = false;
std::atomic_uint32_t TimeCount;

uint8_t** SoundTable;

// Internal variables
static bool SD_Started;
bool nextsoundpos;

static const char* ParmStrings[] = {
    "noal",
    "nosb",
    "nopro",
    "noss",
    "sst",
    "ss1",
    "ss2",
    "ss3",
    nullptr
};

uint16_t* DigiList;

// SoundBlaster variables
static bool sbNoCheck;
static bool sbNoProCheck;

// SoundSource variables
bool ssNoCheck;

// PC Sound variables

// AdLib variables
bool alNoCheck;
bool sqActive;
uint16_t* sqHack;
uint16_t sqHackLen;
bool sqPlayedOnce;

// Internal routines

// BBi
static int music_index = -1;
static bstone::AudioMixer mixer;

int sd_sfx_volume = ::sd_default_sfx_volume;
int sd_music_volume = ::sd_default_music_volume;

//
// Stuff for digitized sounds
//

void SDL_SetupDigi()
{
    const uint16_t* p;
    int pg;
    int i;

    p = static_cast<const uint16_t*>(PM_GetPage(ChunksInFile - 1));
    pg = PMSoundStart;
    for (i = 0; i < static_cast<int>(PMPageSize / (2 * 2)); ++i) {
        if (pg >= ChunksInFile - 1) {
            break;
        }
        pg += (bstone::Endian::le(p[1]) + (PMPageSize - 1)) / PMPageSize;
        p += 2;
    }
    DigiList = new uint16_t[i * 2];

    const uint16_t* src_list = static_cast<const uint16_t*>(
        ::PM_GetPage(ChunksInFile - 1));

    bstone::Endian::le(src_list, i * 2, DigiList);

    for (i = 0; i < sdLastSound; i++) {
        DigiMap[i] = -1;
    }
}

// AdLib Code

///////////////////////////////////////////////////////////////////////////
//
//      SDL_DetectAdLib() - Determines if there's an AdLib (or SoundBlaster
//              emulating an AdLib) present
//
///////////////////////////////////////////////////////////////////////////

static bool SDL_DetectAdLib()
{
    int sdl_result = 0;

    sdl_result = ::SDL_Init(SDL_INIT_AUDIO);

    return sdl_result == 0;
}

////////////////////////////////////////////////////////////////////////////
//
//      SDL_ShutDevice() - turns off whatever device was being used for sound fx
//
////////////////////////////////////////////////////////////////////////////

static void SDL_ShutDevice()
{
    sd_is_sound_enabled = false;
}

///////////////////////////////////////////////////////////////////////////
//
//      SDL_StartDevice() - turns on whatever device is to be used for sound fx
//
///////////////////////////////////////////////////////////////////////////

static void SDL_StartDevice()
{
}

// Public routines

bool SD_EnableSound(
    bool enable)
{
    ::SD_StopSound();

    if (enable && !::sd_has_audio) {
        enable = false;
    }

    auto table_offset = (enable ? ::sdStartALSounds : sdStartPCSounds);

    if (::sd_is_sound_enabled != enable) {
        ::SDL_ShutDevice();
        ::sd_is_sound_enabled = enable;
        ::SoundTable = &::audiosegs[table_offset];
        ::SDL_StartDevice();
    }

    return enable;
}

bool SD_EnableMusic(
    bool enable)
{
    ::SD_MusicOff();

    ::sd_is_music_enabled = enable;

    return enable;
}

void SD_Startup()
{
    if (SD_Started) {
        return;
    }

    ssNoCheck = false;
    alNoCheck = false;
    sbNoCheck = false;
    sbNoProCheck = false;

    switch (::g_args.check_argument(ParmStrings)) {
    case 0: // No AdLib detection
        alNoCheck = true;
        break;

    case 1: // No SoundBlaster detection
        sbNoCheck = true;
        break;

    case 2: // No SoundBlaster Pro detection
        sbNoProCheck = true;
        break;
    }

    TimeCount = 0;

    ::SD_EnableSound(false);
    ::SD_EnableMusic(false);

    ::sd_has_audio = ::SDL_DetectAdLib();

    if (::sd_has_audio) {
        mixer.initialize(44100);
    } else {
        mixer.uninitialize();
    }

    ::SDL_SetupDigi();

    ::SD_Started = true;
}

void SD_Shutdown()
{
    if (!SD_Started) {
        return;
    }

    mixer.uninitialize();

    // Free music data
    for (int i = 0; i < LASTMUSIC; ++i) {
        delete [] static_cast<uint8_t*>(::audiosegs[STARTMUSIC + i]);
    }

    SD_Started = false;
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_SoundPlaying() - returns the sound number that's playing, or 0 if
//              no sound is playing
//
///////////////////////////////////////////////////////////////////////////

bool SD_SoundPlaying()
{
    if (::sd_is_sound_enabled) {
        return ::mixer.is_any_sfx_playing();
    } else {
        return false;
    }
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_StopSound() - if a sound is playing, stops it
//
///////////////////////////////////////////////////////////////////////////

void SD_StopSound()
{
    ::mixer.stop_all_sfx();
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_WaitSoundDone() - waits until the current sound is done playing
//
///////////////////////////////////////////////////////////////////////////

void SD_WaitSoundDone()
{
    while (::SD_SoundPlaying()) {
        ::sys_default_sleep_for();
    }
}


///////////////////////////////////////////////////////////////////////////
//
//      SD_MusicOn() - turns on the sequencer
//
///////////////////////////////////////////////////////////////////////////

void SD_MusicOn()
{
    ::sqActive = true;
    ::mixer.play_adlib_music(music_index, sqHack, sqHackLen);
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_MusicOff() - turns off the sequencer and any playing notes
//
///////////////////////////////////////////////////////////////////////////

void SD_MusicOff()
{
    ::sqActive = false;
    ::mixer.stop_music();
}

///////////////////////////////////////////////////////////////////////////
//
//      SD_StartMusic() - starts playing the music pointed to
//
///////////////////////////////////////////////////////////////////////////

void SD_StartMusic(
    int index)
{
    ::SD_MusicOff();

    ::sqPlayedOnce = false;

    if (::sd_is_music_enabled) {
        ::music_index = index;

        auto music_data = reinterpret_cast<uint16_t*>(
            ::audiosegs[STARTMUSIC + index]);

        auto length = bstone::Endian::le(music_data[0]) + 2;

        ::sqHack = music_data;
        ::sqHackLen = static_cast<uint16_t>(length);

        ::SD_MusicOn();
    } else {
        ::sqPlayedOnce = true;
    }
}

// BBi
void sd_play_sound(
    int sound_index,
    const void* actor,
    bstone::ActorType actor_type,
    bstone::ActorChannel actor_channel)
{
    if (sound_index < 0) {
        return;
    }

    if (!SoundTable) {
        return;
    }

    int actor_index = -1;

    if (actor) {
        switch (actor_type) {
        case bstone::AT_ACTOR:
            actor_index = static_cast<int>(
                static_cast<const objtype*>(actor) - objlist);
            break;

        case bstone::AT_DOOR:
            actor_index = static_cast<int>(
                static_cast<const doorobj_t*>(actor) - doorobjlist);
            break;

        default:
            return;
        }
    }

    const SoundCommon* sound = reinterpret_cast<SoundCommon*>(
        SoundTable[sound_index]);

    if (!sound) {
        return;
    }

    if (::sd_is_sound_enabled && !sound) {
        SD_ERROR(SD_PLAYSOUND_UNCACHED);
    }

    int priority = bstone::Endian::le(sound->priority);

    int digi_index = DigiMap[sound_index];

    if (digi_index != -1) {
        int digi_page = DigiList[(2 * digi_index) + 0];
        int digi_length = DigiList[(2 * digi_index) + 1];
        const void* digi_data = ::PM_GetSoundPage(digi_page);

        mixer.play_pcm_sound(digi_index, priority, digi_data, digi_length,
                             actor_index, actor_type, actor_channel);

        return;
    }

    if (!::sd_is_sound_enabled) {
        return;
    }

    int data_size = audiostarts[sdStartALSounds + sound_index + 1] -
                    audiostarts[sdStartALSounds + sound_index];

    mixer.play_adlib_sound(sound_index, priority, sound, data_size,
                           actor_index, actor_type, actor_channel);
}

void sd_play_actor_sound(
    int sound_index,
    const objtype* actor,
    bstone::ActorChannel actor_channel)
{
    sd_play_sound(
        sound_index,
        actor,
        bstone::AT_ACTOR,
        actor_channel);
}

void sd_play_player_sound(
    int sound_index,
    bstone::ActorChannel actor_channel)
{
    sd_play_sound(
        sound_index,
        player,
        bstone::AT_ACTOR,
        actor_channel);
}

void sd_play_door_sound(
    int sound_index,
    const doorobj_t* door)
{
    sd_play_sound(
        sound_index,
        door,
        bstone::AT_DOOR,
        bstone::AC_VOICE);
}

void sd_play_wall_sound(
    int sound_index)
{
    sd_play_sound(
        sound_index,
        nullptr,
        bstone::AT_WALL,
        bstone::AC_VOICE);
}

void sd_update_positions()
{
    mixer.update_positions();
}

bool sd_is_player_channel_playing(
    bstone::ActorChannel channel)
{
    return mixer.is_player_channel_playing(channel);
}

void sd_set_sfx_volume(
    int volume)
{
    if (volume < ::sd_min_volume) {
        volume = ::sd_min_volume;
    }

    if (volume > ::sd_max_volume) {
        volume = ::sd_max_volume;
    }

    mixer.set_sfx_volume(static_cast<float>(volume) / ::sd_max_volume);
}

void sd_set_music_volume(
    int volume)
{
    if (volume < ::sd_min_volume) {
        volume = ::sd_min_volume;
    }

    if (volume > ::sd_max_volume) {
        volume = ::sd_max_volume;
    }

    mixer.set_music_volume(static_cast<float>(volume) / ::sd_max_volume);
}
// BBi
