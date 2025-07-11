/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *
 *   Mupen64plus-ui-console - main.c                                       *
 *   Mupen64Plus homepage: https://mupen64plus.org/                        *
 *   Copyright (C) 2007-2018 Richard42                                     *
 *   Copyright (C) 2008 Ebenblues Nmn Okaygo Tillin9                       *
 *   Copyright (C) 2002 Hacktarux                                          *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.          *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * */

/* This is the main application entry point for the console-only front-end
 * for Mupen64Plus v2.0.
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <SDL.h>
#include <SDL_main.h>
#include <SDL_thread.h>
#include <dlfcn.h>
#include "../../../../analysis_tool/include/analysis_window.h"

#include "cheat.h"
#include "compare_core.h"
#include "core_interface.h"
#include "debugger.h"
#include "m64p_types.h"
#include "main.h"
#include "osal_preproc.h"
#include "osal_files.h"
#include "plugin.h"
#include "version.h"

#ifdef VIDEXT_HEADER
#define xstr(s) str(s)
#define str(s) #s
#include xstr(VIDEXT_HEADER)
#endif

#define PIF_ROM_SIZE 2048

/* Version number for UI-Console config section parameters */
#define CONFIG_PARAM_VERSION     1.00

/** global variables **/
int    g_Verbose = 0;

/** static (local) variables **/
static m64p_handle l_ConfigCore = NULL;
static m64p_handle l_ConfigVideo = NULL;
static m64p_handle l_ConfigUI = NULL;
static m64p_handle l_ConfigTransferPak = NULL;
static m64p_handle l_Config64DD = NULL;

static const char *l_CoreLibPath = NULL;
static const char *l_ConfigDirPath = NULL;
static const char *l_ROMFilepath = NULL;       // filepath of ROM to load & run at startup
static const char *l_SaveStatePath = NULL;     // save state to load at startup

static void *g_analysis_lib = NULL;

#if defined(SHAREDIR)
  static const char *l_DataDirPath = SHAREDIR;
#else
  static const char *l_DataDirPath = NULL;
#endif

static int  *l_TestShotList = NULL;      // list of screenshots to take for regression test support
static int   l_TestShotIdx = 0;          // index of next screenshot frame in list
static int   l_SaveOptions = 1;          // save command-line options in configuration file (enabled by default)
static int   l_CoreCompareMode = 0;      // 0 = disable, 1 = send, 2 = receive
static int   l_LaunchDebugger = 0;

static eCheatMode l_CheatMode = CHEAT_DISABLE;
static char      *l_CheatNumList = NULL;

static void analysis_load(core_do_command_func cmd)
{
    g_analysis_lib = dlopen("../../analysis_tool/build/libanalysis_tool.so", RTLD_NOW);
    if (!g_analysis_lib)
    {
        fprintf(stderr, "Failed to load analysis tool: %s\n", dlerror());
        return;
    }
    void (*start_fn)(core_do_command_func) = dlsym(g_analysis_lib, "analysis_window_start");
    if (start_fn)
        start_fn(cmd);
}

static void analysis_unload(void)
{
    if (!g_analysis_lib) return;
    void (*stop_fn)(void) = dlsym(g_analysis_lib, "analysis_window_stop");
    if (stop_fn)
        stop_fn();
    dlclose(g_analysis_lib);
    g_analysis_lib = NULL;
}

/*********************************************************************************************************
 *  Callback functions from the core
 */

void DebugMessage(int level, const char *message, ...)
{
  char msgbuf[1024];
  va_list args;

  va_start(args, message);
  vsnprintf(msgbuf, 1024, message, args);

  DebugCallback("UI-Console", level, msgbuf);

  va_end(args);
}

void DebugCallback(void *Context, int level, const char *message)
{
#ifdef ANDROID
    if (level == M64MSG_ERROR)
        __android_log_print(ANDROID_LOG_ERROR, (const char *) Context, "%s", message);
    else if (level == M64MSG_WARNING)
        __android_log_print(ANDROID_LOG_WARN, (const char *) Context, "%s", message);
    else if (level == M64MSG_INFO)
        __android_log_print(ANDROID_LOG_INFO, (const char *) Context, "%s", message);
    else if (level == M64MSG_STATUS)
        __android_log_print(ANDROID_LOG_DEBUG, (const char *) Context, "%s", message);
    else if (level == M64MSG_VERBOSE)
    {
        if (g_Verbose)
            __android_log_print(ANDROID_LOG_VERBOSE, (const char *) Context, "%s", message);
    }
    else
        __android_log_print(ANDROID_LOG_ERROR, (const char *) Context, "Unknown: %s", message);
#else
    if (level == M64MSG_ERROR)
        printf("%s Error: %s\n", (const char *) Context, message);
    else if (level == M64MSG_WARNING)
        printf("%s Warning: %s\n", (const char *) Context, message);
    else if (level == M64MSG_INFO)
        printf("%s: %s\n", (const char *) Context, message);
    else if (level == M64MSG_STATUS)
        printf("%s Status: %s\n", (const char *) Context, message);
    else if (level == M64MSG_VERBOSE)
    {
        if (g_Verbose)
            printf("%s: %s\n", (const char *) Context, message);
    }
    else
        printf("%s Unknown: %s\n", (const char *) Context, message);
#endif
}

static void FrameCallback(unsigned int FrameIndex)
{
    // take a screenshot if we need to
    if (l_TestShotList != NULL)
    {
        int nextshot = l_TestShotList[l_TestShotIdx];
        if (nextshot == FrameIndex)
        {
            (*CoreDoCommand)(M64CMD_TAKE_NEXT_SCREENSHOT, 0, NULL);  /* tell the core take a screenshot */
            // advance list index to next screenshot frame number.  If it's 0, then quit
            l_TestShotIdx++;
        }
        else if (nextshot == 0)
        {
            (*CoreDoCommand)(M64CMD_STOP, 0, NULL);  /* tell the core to shut down ASAP */
            free(l_TestShotList);
            l_TestShotList = NULL;
        }
    }
}

static char *formatstr(const char *fmt, ...) ATTR_FMT(1, 2);

char *formatstr(const char *fmt, ...)
{
	int size = 128, ret;
	char *str = (char *)malloc(size), *newstr;
	va_list args;

	/* There are two implementations of vsnprintf we have to deal with:
	 * C99 version: Returns the number of characters which would have been written
	 *              if the buffer had been large enough, and -1 on failure.
	 * Windows version: Returns the number of characters actually written,
	 *                  and -1 on failure or truncation.
	 * NOTE: An implementation equivalent to the Windows one appears in glibc <2.1.
	 */
	while (str != NULL)
	{
		va_start(args, fmt);
		ret = vsnprintf(str, size, fmt, args);
		va_end(args);

		// Successful result?
		if (ret >= 0 && ret < size)
			return str;

		// Increment the capacity of the buffer
		if (ret >= size)
			size = ret + 1; // C99 version: We got the needed buffer size
		else
			size *= 2; // Windows version: Keep guessing

		newstr = (char *)realloc(str, size);
		if (newstr == NULL)
			free(str);
		str = newstr;
	}

	return NULL;
}

static int is_path_separator(char c)
{
    return strchr(OSAL_DIR_SEPARATORS, c) != NULL;
}

char* combinepath(const char* first, const char *second)
{
    size_t len_first, off_second = 0;

    if (first == NULL || second == NULL)
        return NULL;

    len_first = strlen(first);

    while (is_path_separator(first[len_first-1]))
        len_first--;

    while (is_path_separator(second[off_second]))
        off_second++;

    return formatstr("%.*s%c%s", (int) len_first, first, OSAL_DIR_SEPARATORS[0], second + off_second);
}


/*********************************************************************************************************
 *  Configuration handling
 */

static m64p_error OpenConfigurationHandles(void)
{
    float fConfigParamsVersion;
    int bSaveConfig = 0;
    m64p_error rval;
    unsigned int i;

    /* Open Configuration sections for core library and console User Interface */
    rval = (*ConfigOpenSection)("Core", &l_ConfigCore);
    if (rval != M64ERR_SUCCESS)
    {
        DebugMessage(M64MSG_ERROR, "failed to open 'Core' configuration section");
        return rval;
    }

    rval = (*ConfigOpenSection)("Video-General", &l_ConfigVideo);
    if (rval != M64ERR_SUCCESS)
    {
        DebugMessage(M64MSG_ERROR, "failed to open 'Video-General' configuration section");
        return rval;
    }

    rval = (*ConfigOpenSection)("Transferpak", &l_ConfigTransferPak);
    if (rval != M64ERR_SUCCESS)
    {
        DebugMessage(M64MSG_ERROR, "failed to open 'Transferpak' configuration section");
        return rval;
    }

    rval = (*ConfigOpenSection)("64DD", &l_Config64DD);
    if (rval != M64ERR_SUCCESS)
    {
        DebugMessage(M64MSG_ERROR, "failed to open '64DD' configuration section");
        return rval;
    }

    rval = (*ConfigOpenSection)("UI-Console", &l_ConfigUI);
    if (rval != M64ERR_SUCCESS)
    {
        DebugMessage(M64MSG_ERROR, "failed to open 'UI-Console' configuration section");
        return rval;
    }

    if ((*ConfigGetParameter)(l_ConfigUI, "Version", M64TYPE_FLOAT, &fConfigParamsVersion, sizeof(float)) != M64ERR_SUCCESS)
    {
        DebugMessage(M64MSG_WARNING, "No version number in 'UI-Console' config section. Setting defaults.");
        (*ConfigDeleteSection)("UI-Console");
        (*ConfigOpenSection)("UI-Console", &l_ConfigUI);
        bSaveConfig = 1;
    }
    else if (((int) fConfigParamsVersion) != ((int) CONFIG_PARAM_VERSION))
    {
        DebugMessage(M64MSG_WARNING, "Incompatible version %.2f in 'UI-Console' config section: current is %.2f. Setting defaults.", fConfigParamsVersion, (float) CONFIG_PARAM_VERSION);
        (*ConfigDeleteSection)("UI-Console");
        (*ConfigOpenSection)("UI-Console", &l_ConfigUI);
        bSaveConfig = 1;
    }
    else if ((CONFIG_PARAM_VERSION - fConfigParamsVersion) >= 0.0001f)
    {
        /* handle upgrades */
        float fVersion = CONFIG_PARAM_VERSION;
        ConfigSetParameter(l_ConfigUI, "Version", M64TYPE_FLOAT, &fVersion);
        DebugMessage(M64MSG_INFO, "Updating parameter set version in 'UI-Console' config section to %.2f", fVersion);
        bSaveConfig = 1;
    }

    /* Set default values for my Config parameters */
    (*ConfigSetDefaultFloat)(l_ConfigUI, "Version", CONFIG_PARAM_VERSION,  "Mupen64Plus UI-Console config parameter set version number.  Please don't change this version number.");
    (*ConfigSetDefaultString)(l_ConfigUI, "PluginDir", OSAL_CURRENT_DIR, "Directory in which to search for plugins");
    (*ConfigSetDefaultString)(l_ConfigUI, "VideoPlugin", "mupen64plus-video-rice" OSAL_DLL_EXTENSION, "Filename of video plugin");
    (*ConfigSetDefaultString)(l_ConfigUI, "AudioPlugin", "mupen64plus-audio-sdl" OSAL_DLL_EXTENSION, "Filename of audio plugin");
    (*ConfigSetDefaultString)(l_ConfigUI, "InputPlugin", "mupen64plus-input-sdl" OSAL_DLL_EXTENSION, "Filename of input plugin");
    (*ConfigSetDefaultString)(l_ConfigUI, "RspPlugin", "mupen64plus-rsp-hle" OSAL_DLL_EXTENSION, "Filename of RSP plugin");

    for(i = 1; i < 5; ++i) {
        char key[64];
        char desc[2048];
#define SET_DEFAULT_STRING(key_fmt, default_value, desc_fmt) \
        do { \
            snprintf(key, sizeof(key), key_fmt, i); \
            snprintf(desc, sizeof(desc), desc_fmt, i); \
            (*ConfigSetDefaultString)(l_ConfigTransferPak, key, default_value, desc); \
        } while(0)

        SET_DEFAULT_STRING("GB-rom-%u", "", "Filename of the GB ROM to load into transferpak %u");
        SET_DEFAULT_STRING("GB-ram-%u", "", "Filename of the GB RAM to load into transferpak %u");
#undef SET_DEFAULT_STRING
    }

    (*ConfigSetDefaultString)(l_Config64DD, "IPL-ROM", "", "Filename of the 64DD IPL ROM");
    (*ConfigSetDefaultString)(l_Config64DD, "Disk", "", "Filename of the disk to load into Disk Drive");

    if (bSaveConfig && l_SaveOptions && ConfigSaveSection != NULL) { /* ConfigSaveSection was added in Config API v2.1.0 */
        (*ConfigSaveSection)("UI-Console");
        (*ConfigSaveSection)("Transferpak");
    }

    return M64ERR_SUCCESS;
}

static m64p_error SaveConfigurationOptions(void)
{
    /* if shared data directory was given on the command line, write it into the config file */
    if (l_DataDirPath != NULL)
        (*ConfigSetParameter)(l_ConfigCore, "SharedDataPath", M64TYPE_STRING, l_DataDirPath);

    /* if any plugin filepaths were given on the command line, write them into the config file */
    if (g_PluginDir != NULL)
        (*ConfigSetParameter)(l_ConfigUI, "PluginDir", M64TYPE_STRING, g_PluginDir);
    if (g_GfxPlugin != NULL)
        (*ConfigSetParameter)(l_ConfigUI, "VideoPlugin", M64TYPE_STRING, g_GfxPlugin);
    if (g_AudioPlugin != NULL)
        (*ConfigSetParameter)(l_ConfigUI, "AudioPlugin", M64TYPE_STRING, g_AudioPlugin);
    if (g_InputPlugin != NULL)
        (*ConfigSetParameter)(l_ConfigUI, "InputPlugin", M64TYPE_STRING, g_InputPlugin);
    if (g_RspPlugin != NULL)
        (*ConfigSetParameter)(l_ConfigUI, "RspPlugin", M64TYPE_STRING, g_RspPlugin);

    if ((*ConfigHasUnsavedChanges)(NULL))
        return (*ConfigSaveFile)();
    else
        return M64ERR_SUCCESS;
}

/*********************************************************************************************************
 *  Command-line parsing
 */

static void printUsage(const char *progname)
{
    printf("Usage: %s [parameters] [romfile]\n"
           "\n"
           "Parameters:\n"
           "    --noosd                : disable onscreen display\n"
           "    --osd                  : enable onscreen display\n"
           "    --fullscreen           : use fullscreen display mode\n"
           "    --windowed             : use windowed display mode\n"
           "    --resolution (res)     : display resolution (640x480, 800x600, 1024x768, etc)\n"
           "    --nospeedlimit         : disable core speed limiter (should be used with dummy audio plugin)\n"
           "    --cheats (cheat-spec)  : enable or list cheat codes for the given rom file\n"
           "    --corelib (filepath)   : use core library (filepath) (can be only filename or full path)\n"
           "    --configdir (dir)      : force configation directory to (dir); should contain mupen64plus.cfg\n"
           "    --datadir (dir)        : search for shared data files (.ini files, languages, etc) in (dir)\n"
           "    --debug                : launch console-based debugger (requires core lib built for debugging)\n"
           "    --plugindir (dir)      : search for plugins in (dir)\n"
           "    --sshotdir (dir)       : set screenshot directory to (dir)\n"
           "    --gfx (plugin-spec)    : use gfx plugin given by (plugin-spec)\n"
           "    --audio (plugin-spec)  : use audio plugin given by (plugin-spec)\n"
           "    --input (plugin-spec)  : use input plugin given by (plugin-spec)\n"
           "    --rsp (plugin-spec)    : use rsp plugin given by (plugin-spec)\n"
           "    --emumode (mode)       : set emu mode to: 0=Pure Interpreter 1=Interpreter 2=DynaRec\n"
           "    --savestate (filepath) : savestate loaded at startup\n"
           "    --testshots (list)     : take screenshots at frames given in comma-separated (list), then quit\n"
           "    --set (param-spec)     : set a configuration variable, format: ParamSection[ParamName]=Value\n"
           "    --gb-rom-{1,2,3,4}     : define GB cart rom to load inside transferpak {1,2,3,4}\n"
           "    --gb-ram-{1,2,3,4}     : define GB cart ram to load inside transferpak {1,2,3,4}\n"
           "    --dd-ipl-rom           : define 64DD IPL rom\n"
           "    --dd-disk              : define disk to load into the disk drive\n"
           "    --core-compare-send    : use the Core Comparison debugging feature, in data sending mode\n"
           "    --core-compare-recv    : use the Core Comparison debugging feature, in data receiving mode\n"
           "    --nosaveoptions        : do not save the given command-line options in configuration file\n"
           "    --pif (filepath)       : use a binary PIF ROM (filepath) instead of HLE PIF\n"
           "    --verbose              : print lots of information\n"
           "    --help                 : see this help message\n\n"
           "(plugin-spec):\n"
           "    (pluginname)           : filename (without path) of plugin to find in plugin directory\n"
           "    (pluginpath)           : full path and filename of plugin\n"
           "    'dummy'                : use dummy plugin\n\n"
           "(cheat-spec):\n"
           "    'list'                 : show all of the available cheat codes\n"
           "    'all'                  : enable all of the available cheat codes\n"
           "    (codelist)             : a comma-separated list of cheat code numbers to enable,\n"
           "                             with dashes to use code variables (ex 1-2 to use cheat 1 option 2)\n"
           "\n", progname);

    return;
}

static int SetConfigParameter(const char *ParamSpec)
{
    char *ParsedString, *VarName, *VarValue=NULL;
    m64p_handle ConfigSection;
    m64p_type VarType;
    m64p_error rval;

    if (ParamSpec == NULL)
    {
        DebugMessage(M64MSG_ERROR, "ParamSpec is NULL in SetConfigParameter()");
        return 1;
    }

    /* make a copy of the input string */
    ParsedString = (char *) malloc(strlen(ParamSpec) + 1);
    if (ParsedString == NULL)
    {
        DebugMessage(M64MSG_ERROR, "SetConfigParameter() couldn't allocate memory for temporary string.");
        return 2;
    }
    strcpy(ParsedString, ParamSpec);

    /* parse it for the simple section[name]=value format */
    VarName = strchr(ParsedString, '[');
    if (VarName != NULL)
    {
        *VarName++ = 0;
        VarValue = strchr(VarName, ']');
        if (VarValue != NULL)
        {
            *VarValue++ = 0;
        }
    }
    if (VarName == NULL || VarValue == NULL || *VarValue != '=')
    {
        DebugMessage(M64MSG_ERROR, "invalid (param-spec) '%s'", ParamSpec);
        free(ParsedString);
        return 3;
    }
    VarValue++;

    /* then set the value */
    rval = (*ConfigOpenSection)(ParsedString, &ConfigSection);
    if (rval != M64ERR_SUCCESS)
    {
        DebugMessage(M64MSG_ERROR, "SetConfigParameter failed to open config section '%s'", ParsedString);
        free(ParsedString);
        return 4;
    }
    if ((*ConfigGetParameterType)(ConfigSection, VarName, &VarType) == M64ERR_SUCCESS)
    {
        switch(VarType)
        {
            int ValueInt;
            float ValueFloat;
            case M64TYPE_INT:
                ValueInt = atoi(VarValue);
                ConfigSetParameter(ConfigSection, VarName, M64TYPE_INT, &ValueInt);
                break;
            case M64TYPE_FLOAT:
                ValueFloat = (float) atof(VarValue);
                ConfigSetParameter(ConfigSection, VarName, M64TYPE_FLOAT, &ValueFloat);
                break;
            case M64TYPE_BOOL:
                ValueInt = (int) (osal_insensitive_strcmp(VarValue, "true") == 0);
                ConfigSetParameter(ConfigSection, VarName, M64TYPE_BOOL, &ValueInt);
                break;
            case M64TYPE_STRING:
                ConfigSetParameter(ConfigSection, VarName, M64TYPE_STRING, VarValue);
                break;
            default:
                DebugMessage(M64MSG_ERROR, "invalid VarType in SetConfigParameter()");
                return 5;
        }
    }
    else
    {
        ConfigSetParameter(ConfigSection, VarName, M64TYPE_STRING, VarValue);
    }

    free(ParsedString);
    return 0;
}

static int *ParseNumberList(const char *InputString, int *ValuesFound)
{
    const char *str;
    int *OutputList;

    /* count the number of integers in the list */
    int values = 1;
    str = InputString;
    while ((str = strchr(str, ',')) != NULL)
    {
        str++;
        values++;
    }

    /* create a list and populate it with the frame counter values at which to take screenshots */
    if ((OutputList = (int *) malloc(sizeof(int) * (values + 1))) != NULL)
    {
        int idx = 0;
        str = InputString;
        while (str != NULL)
        {
            OutputList[idx++] = atoi(str);
            str = strchr(str, ',');
            if (str != NULL) str++;
        }
        OutputList[idx] = 0;
    }

    if (ValuesFound != NULL)
        *ValuesFound = values;
    return OutputList;
}

static int ParseCommandLineInitial(int argc, const char **argv)
{
    int i;

    /* First phase of command line parsing: read parameters that affect the
       core and the ui-console behavior. */
    for (i = 1; i < argc; i++)
    {
        int ArgsLeft = argc - i - 1;

        if (strcmp(argv[i], "--corelib") == 0 && ArgsLeft >= 1)
        {
            l_CoreLibPath = argv[i+1];
            i++;
        }
        else if (strcmp(argv[i], "--configdir") == 0 && ArgsLeft >= 1)
        {
            l_ConfigDirPath = argv[i+1];
            i++;
        }
        else if (strcmp(argv[i], "--datadir") == 0 && ArgsLeft >= 1)
        {
            l_DataDirPath = argv[i+1];
            i++;
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0)
        {
            printUsage(argv[0]);
            return 1;
        }
        else if (strcmp(argv[i], "--nosaveoptions") == 0)
        {
            l_SaveOptions = 0;
        }
    }

    return 0;
}

static m64p_error ParseCommandLineMain(int argc, const char **argv)
{
    int i;

    /* Second phase of command-line parsing: read all remaining parameters
       except for those that set plugin options. */
    for (i = 1; i < argc; i++)
    {
        int ArgsLeft = argc - i - 1;
        if (strcmp(argv[i], "--noosd") == 0)
        {
            int Osd = 0;
            (*ConfigSetParameter)(l_ConfigCore, "OnScreenDisplay", M64TYPE_BOOL, &Osd);
        }
        else if (strcmp(argv[i], "--osd") == 0)
        {
            int Osd = 1;
            (*ConfigSetParameter)(l_ConfigCore, "OnScreenDisplay", M64TYPE_BOOL, &Osd);
        }
        else if (strcmp(argv[i], "--fullscreen") == 0)
        {
            int Fullscreen = 1;
            (*ConfigSetParameter)(l_ConfigVideo, "Fullscreen", M64TYPE_BOOL, &Fullscreen);
        }
        else if (strcmp(argv[i], "--windowed") == 0)
        {
            int Fullscreen = 0;
            (*ConfigSetParameter)(l_ConfigVideo, "Fullscreen", M64TYPE_BOOL, &Fullscreen);
        }
        else if (strcmp(argv[i], "--nospeedlimit") == 0)
        {
            int EnableSpeedLimit = 0;
            if (g_CoreAPIVersion < 0x020001)
                DebugMessage(M64MSG_WARNING, "core library doesn't support --nospeedlimit");
            else
            {
                if ((*CoreDoCommand)(M64CMD_CORE_STATE_SET, M64CORE_SPEED_LIMITER, &EnableSpeedLimit) != M64ERR_SUCCESS)
                    DebugMessage(M64MSG_ERROR, "core gave error while setting --nospeedlimit option");
            }
        }
        else if ((strcmp(argv[i], "--corelib") == 0 || strcmp(argv[i], "--configdir") == 0 ||
                  strcmp(argv[i], "--datadir") == 0) && ArgsLeft >= 1)
        {   /* already handled in ParseCommandLineInitial (skip the value) */
            i++;
        }
        else if (strcmp(argv[i], "--nosaveoptions") == 0)
        {   /* already handled in ParseCommandLineInitial (no value to skip) */
            ;
        }
        else if (strcmp(argv[i], "--resolution") == 0 && ArgsLeft >= 1)
        {
            const char *res = argv[i+1];
            int xres, yres;
            i++;
            if (sscanf(res, "%ix%i", &xres, &yres) != 2)
                DebugMessage(M64MSG_WARNING, "couldn't parse resolution '%s'", res);
            else
            {
                (*ConfigSetParameter)(l_ConfigVideo, "ScreenWidth", M64TYPE_INT, &xres);
                (*ConfigSetParameter)(l_ConfigVideo, "ScreenHeight", M64TYPE_INT, &yres);
            }
        }
        else if (strcmp(argv[i], "--cheats") == 0 && ArgsLeft >= 1)
        {
            if (strcmp(argv[i+1], "all") == 0)
                l_CheatMode = CHEAT_ALL;
            else if (strcmp(argv[i+1], "list") == 0)
                l_CheatMode = CHEAT_SHOW_LIST;
            else
            {
                l_CheatMode = CHEAT_LIST;
                l_CheatNumList = (char*) argv[i+1];
            }
            i++;
        }
        else if (strcmp(argv[i], "--plugindir") == 0 && ArgsLeft >= 1)
        {
            g_PluginDir = argv[i+1];
            i++;
        }
        else if (strcmp(argv[i], "--sshotdir") == 0 && ArgsLeft >= 1)
        {
            (*ConfigSetParameter)(l_ConfigCore, "ScreenshotPath", M64TYPE_STRING, argv[i+1]);
            i++;
        }
        else if (strcmp(argv[i], "--gfx") == 0 && ArgsLeft >= 1)
        {
            g_GfxPlugin = argv[i+1];
            i++;
        }
        else if (strcmp(argv[i], "--audio") == 0 && ArgsLeft >= 1)
        {
            g_AudioPlugin = argv[i+1];
            i++;
        }
        else if (strcmp(argv[i], "--input") == 0 && ArgsLeft >= 1)
        {
            g_InputPlugin = argv[i+1];
            i++;
        }
        else if (strcmp(argv[i], "--rsp") == 0 && ArgsLeft >= 1)
        {
            g_RspPlugin = argv[i+1];
            i++;
        }
        else if (strcmp(argv[i], "--emumode") == 0 && ArgsLeft >= 1)
        {
            int emumode = atoi(argv[i+1]);
            i++;
            if (emumode < 0 || emumode > 2)
            {
                DebugMessage(M64MSG_WARNING, "invalid --emumode value '%i'", emumode);
                continue;
            }
            if (emumode == 2 && !(g_CoreCapabilities & M64CAPS_DYNAREC))
            {
                DebugMessage(M64MSG_WARNING, "Emulator core doesn't support Dynamic Recompiler.");
                emumode = 1;
            }
            (*ConfigSetParameter)(l_ConfigCore, "R4300Emulator", M64TYPE_INT, &emumode);
        }
        else if (strcmp(argv[i], "--savestate") == 0 && ArgsLeft >= 1)
        {
            l_SaveStatePath = argv[i+1];
            i++;
        }
        else if (strcmp(argv[i], "--testshots") == 0 && ArgsLeft >= 1)
        {
            l_TestShotList = ParseNumberList(argv[i+1], NULL);
            i++;
        }
        else if (strcmp(argv[i], "--set") == 0 && ArgsLeft >= 1)
        {
            /* skip this: it will be handled in ParseCommandLinePlugin */
            i++;
        }
        else if (strcmp(argv[i], "--debug") == 0)
        {
            l_LaunchDebugger = 1;
        }
        else if (strcmp(argv[i], "--core-compare-send") == 0)
        {
            l_CoreCompareMode = 1;
        }
        else if (strcmp(argv[i], "--core-compare-recv") == 0)
        {
            l_CoreCompareMode = 2;
        }
#define PARSE_GB_CART_PARAM(param, key) \
        else if (strcmp(argv[i], param) == 0) \
        { \
            ConfigSetParameter(l_ConfigTransferPak, key, M64TYPE_STRING, argv[i+1]); \
            i++; \
        }
        PARSE_GB_CART_PARAM("--gb-rom-1", "GB-rom-1")
        PARSE_GB_CART_PARAM("--gb-ram-1", "GB-ram-1")
        PARSE_GB_CART_PARAM("--gb-rom-2", "GB-rom-2")
        PARSE_GB_CART_PARAM("--gb-ram-2", "GB-ram-2")
        PARSE_GB_CART_PARAM("--gb-rom-3", "GB-rom-3")
        PARSE_GB_CART_PARAM("--gb-ram-3", "GB-ram-3")
        PARSE_GB_CART_PARAM("--gb-rom-4", "GB-rom-4")
        PARSE_GB_CART_PARAM("--gb-ram-4", "GB-ram-4")
#undef PARSE_GB_CART_PARAM
        else if (strcmp(argv[i], "--dd-ipl-rom") == 0)
        {
            ConfigSetParameter(l_Config64DD, "IPL-ROM", M64TYPE_STRING, argv[i+1]);
            i++;
        }
        else if (strcmp(argv[i], "--dd-disk") == 0)
        {
            ConfigSetParameter(l_Config64DD, "Disk", M64TYPE_STRING, argv[i+1]);
            i++;
        }
        else if (strcmp(argv[i], "--pif") == 0)
        {
            m64p_error pif_status = M64ERR_INVALID_STATE;
            /* load PIF image */
            FILE *pifPtr = fopen(argv[i+1], "rb");
            if (pifPtr != NULL)
            {
                unsigned char *PIF_buffer = (unsigned char *) malloc(PIF_ROM_SIZE);
                if (PIF_buffer != NULL)
                {
                    if (fread(PIF_buffer, 1, PIF_ROM_SIZE, pifPtr) == PIF_ROM_SIZE)
                    {
                        pif_status = (*CoreDoCommand)(M64CMD_PIF_OPEN, PIF_ROM_SIZE, PIF_buffer);
                    }
                    free(PIF_buffer);
                }
                fclose(pifPtr);
            }
            if (pif_status != M64ERR_SUCCESS)
            {
                DebugMessage(M64MSG_ERROR, "core failed to open PIF ROM file '%s'.", argv[i+1]);
            }
            i++;
        }
        else if (ArgsLeft == 0)
        {
            /* this is the last arg, it should be a ROM filename */
            l_ROMFilepath = argv[i];
            return M64ERR_SUCCESS;
        }
        else if (strcmp(argv[i], "--verbose") == 0)
        {
            g_Verbose = 1;
        }
        else
        {
            DebugMessage(M64MSG_WARNING, "unrecognized command-line parameter '%s'", argv[i]);
        }
        /* continue argv loop */
    }

    /* missing ROM filepath */
    DebugMessage(M64MSG_ERROR, "no ROM filepath given");
    return M64ERR_INPUT_INVALID;
}

static m64p_error ParseCommandLinePlugin(int argc, const char **argv)
{
    int i;

    /* Third phase of command-line parsing: read all plugin parameters. */
    for (i = 1; i < argc; i++)
    {
        int ArgsLeft = argc - i - 1;
        if (strcmp(argv[i], "--set") == 0 && ArgsLeft >= 1)
        {
            if (SetConfigParameter(argv[i+1]) != 0)
                return M64ERR_INPUT_INVALID;
            i++;
        }
    }
    return M64ERR_SUCCESS;
}

static char* media_loader_get_filename(void* cb_data, m64p_handle section_handle, const char* section, const char* key)
{
#define MUPEN64PLUS_CFG_NAME "mupen64plus.cfg"
    m64p_handle core_config;
    char value[4096];
    const char* configdir = NULL;
    char* cfgfilepath = NULL;

    /* reset filename */
    char* mem_filename = NULL;

    /* XXX: use external config API to force reload of file content */
    configdir = ConfigGetUserConfigPath();
    if (configdir == NULL) {
        DebugMessage(M64MSG_ERROR, "Can't get user config path !");
        return NULL;
    }

    cfgfilepath = combinepath(configdir, MUPEN64PLUS_CFG_NAME);
    if (cfgfilepath == NULL) {
        DebugMessage(M64MSG_ERROR, "Can't get config file path: %s + %s!", configdir, MUPEN64PLUS_CFG_NAME);
        return NULL;
    }

    if (ConfigExternalOpen(cfgfilepath, &core_config) != M64ERR_SUCCESS) {
        DebugMessage(M64MSG_ERROR, "Can't open config file %s!", cfgfilepath);
        goto release_cfgfilepath;
    }

    if (ConfigExternalGetParameter(core_config, section, key, value, sizeof(value)) != M64ERR_SUCCESS) {
        DebugMessage(M64MSG_ERROR, "Can't get parameter %s", key);
        goto close_config;
    }

    size_t len = strlen(value);
    if (len < 2 || value[0] != '"' || value[len-1] != '"') {
        DebugMessage(M64MSG_ERROR, "Invalid string format %s", value);
        goto close_config;
    }

    value[len-1] = '\0';
    mem_filename = strdup(value + 1);

    ConfigSetParameter(section_handle, key, M64TYPE_STRING, mem_filename);

close_config:
    ConfigExternalClose(core_config);
release_cfgfilepath:
    free(cfgfilepath);
    return mem_filename;
}


static char* media_loader_get_gb_cart_mem_file(void* cb_data, const char* mem, int control_id)
{
    char key[64];

    snprintf(key, sizeof(key), "GB-%s-%u", mem, control_id + 1);
    return media_loader_get_filename(cb_data, l_ConfigTransferPak, "Transferpak", key);
}

static char* media_loader_get_gb_cart_rom(void* cb_data, int control_id)
{
    return media_loader_get_gb_cart_mem_file(cb_data, "rom", control_id);
}

static char* media_loader_get_gb_cart_ram(void* cb_data, int control_id)
{
    return media_loader_get_gb_cart_mem_file(cb_data, "ram", control_id);
}

static char* media_loader_get_dd_rom(void* cb_data)
{
    return media_loader_get_filename(cb_data, l_Config64DD, "64DD", "IPL-ROM");
}

static char* media_loader_get_dd_disk(void* cb_data)
{
    return media_loader_get_filename(cb_data, l_Config64DD, "64DD", "Disk");
}

static m64p_media_loader l_media_loader =
{
    NULL,
    media_loader_get_gb_cart_rom,
    media_loader_get_gb_cart_ram,
    NULL,
    media_loader_get_dd_rom,
    media_loader_get_dd_disk
};


/*********************************************************************************************************
* main function
*/

/* Allow state callback in external module to be specified via build flags (header and function name) */
#ifdef CALLBACK_HEADER
#define xstr(s) str(s)
#define str(s) #s
#include xstr(CALLBACK_HEADER)
#endif

#ifndef CALLBACK_FUNC
#define CALLBACK_FUNC NULL
#endif

#ifndef WIN32
/* Allow external modules to call the main function as a library method.  This is useful for user
 * interfaces that simply layer on top of (rather than re-implement) UI-Console (e.g. mupen64plus-ae).
 */
__attribute__ ((visibility("default")))
#endif
int main(int argc, char *argv[])
{
    int i;

    printf(" __  __                         __   _  _   ____  _             \n");  
    printf("|  \\/  |_   _ _ __   ___ _ __  / /_ | || | |  _ \\| |_   _ ___ \n");
    printf("| |\\/| | | | | '_ \\ / _ \\ '_ \\| '_ \\| || |_| |_) | | | | / __|  \n");
    printf("| |  | | |_| | |_) |  __/ | | | (_) |__   _|  __/| | |_| \\__ \\  \n");
    printf("|_|  |_|\\__,_| .__/ \\___|_| |_|\\___/   |_| |_|   |_|\\__,_|___/  \n");
    printf("             |_|         https://mupen64plus.org/               \n");
    printf("%s Version %i.%i.%i\n\n", CONSOLE_UI_NAME, VERSION_PRINTF_SPLIT(CONSOLE_UI_VERSION));

    /* bootstrap some special parameters from the command line */
    if (ParseCommandLineInitial(argc, (const char **) argv) != 0)
        return 1;

    /* load the Mupen64Plus core library */
    if (AttachCoreLib(l_CoreLibPath) != M64ERR_SUCCESS)
        return 2;

    /* start the Mupen64Plus core library, load the configuration file */
    m64p_error rval = (*CoreStartup)(CORE_API_VERSION, l_ConfigDirPath, l_DataDirPath, "Core", DebugCallback, NULL, CALLBACK_FUNC);
    if (rval != M64ERR_SUCCESS)
    {
        DebugMessage(M64MSG_ERROR, "couldn't start Mupen64Plus core library.");
        DetachCoreLib();
        return 3;
    }

#ifdef VIDEXT_HEADER
    rval = CoreOverrideVidExt(&vidExtFunctions);
    if (rval != M64ERR_SUCCESS)
    {
        DebugMessage(M64MSG_ERROR, "couldn't start VidExt library.");
        DetachCoreLib();
        return 14;
    }
#endif

    /* Open configuration sections */
    rval = OpenConfigurationHandles();
    if (rval != M64ERR_SUCCESS)
    {
        (*CoreShutdown)();
        DetachCoreLib();
        return 4;
    }

    /* parse non-plugin command-line options */
    rval = ParseCommandLineMain(argc, (const char **) argv);
    if (rval != M64ERR_SUCCESS)
    {
        (*CoreShutdown)();
        DetachCoreLib();
        return 5;
    }

    /* Ensure that the core supports comparison feature if necessary */
    if (l_CoreCompareMode != 0 && !(g_CoreCapabilities & M64CAPS_CORE_COMPARE))
    {
        DebugMessage(M64MSG_ERROR, "can't use --core-compare feature with this Mupen64Plus core library.");
        DetachCoreLib();
        return 6;
    }
    compare_core_init(l_CoreCompareMode);
    
    /* Ensure that the core supports the debugger if necessary */
    if (l_LaunchDebugger && !(g_CoreCapabilities & M64CAPS_DEBUGGER))
    {
        DebugMessage(M64MSG_ERROR, "can't use --debug feature with this Mupen64Plus core library.");
        DetachCoreLib();
        return 6;
    }

    /* save the given command-line options in configuration file if requested */
    if (l_SaveOptions)
        SaveConfigurationOptions();

    /* load ROM image */
    FILE *fPtr = fopen(l_ROMFilepath, "rb");
    if (fPtr == NULL)
    {
        DebugMessage(M64MSG_ERROR, "couldn't open ROM file '%s' for reading.", l_ROMFilepath);
        (*CoreShutdown)();
        DetachCoreLib();
        return 7;
    }

    /* get the length of the ROM, allocate memory buffer, load it from disk */
    long romlength = 0;
    fseek(fPtr, 0L, SEEK_END);
    romlength = ftell(fPtr);
    fseek(fPtr, 0L, SEEK_SET);
    unsigned char *ROM_buffer = (unsigned char *) malloc(romlength);
    if (ROM_buffer == NULL)
    {
        DebugMessage(M64MSG_ERROR, "couldn't allocate %li-byte buffer for ROM image file '%s'.", romlength, l_ROMFilepath);
        fclose(fPtr);
        (*CoreShutdown)();
        DetachCoreLib();
        return 8;
    }
    else if (fread(ROM_buffer, 1, romlength, fPtr) != romlength)
    {
        DebugMessage(M64MSG_ERROR, "couldn't read %li bytes from ROM image file '%s'.", romlength, l_ROMFilepath);
        free(ROM_buffer);
        fclose(fPtr);
        (*CoreShutdown)();
        DetachCoreLib();
        return 9;
    }
    fclose(fPtr);

    /* Try to load the ROM image into the core */
    if ((*CoreDoCommand)(M64CMD_ROM_OPEN, (int) romlength, ROM_buffer) != M64ERR_SUCCESS)
    {
        DebugMessage(M64MSG_ERROR, "core failed to open ROM image file '%s'.", l_ROMFilepath);
        free(ROM_buffer);
        (*CoreShutdown)();
        DetachCoreLib();
        return 10;
    }
    free(ROM_buffer); /* the core copies the ROM image, so we can release this buffer immediately */

    /* handle the cheat codes */
    CheatStart(l_CheatMode, l_CheatNumList);
    if (l_CheatMode == CHEAT_SHOW_LIST)
    {
        (*CoreDoCommand)(M64CMD_ROM_CLOSE, 0, NULL);
        (*CoreShutdown)();
        DetachCoreLib();
        return 11;
    }

    /* search for and load plugins */
    rval = PluginSearchLoad(l_ConfigUI);
    if (rval != M64ERR_SUCCESS)
    {
        (*CoreDoCommand)(M64CMD_ROM_CLOSE, 0, NULL);
        (*CoreShutdown)();
        DetachCoreLib();
        return 12;
    }

    /* Parse and set plugin options. Doing this after loading the plugins
       allows the plugins to set up their own defaults first. */
    rval = ParseCommandLinePlugin(argc, (const char **) argv);
    if (rval != M64ERR_SUCCESS)
    {
        (*CoreDoCommand)(M64CMD_ROM_CLOSE, 0, NULL);
        (*CoreShutdown)();
        DetachCoreLib();
        return 5;
    }

    /* attach plugins to core */
    for (i = 0; i < 4; i++)
    {
        if ((*CoreAttachPlugin)(g_PluginMap[i].type, g_PluginMap[i].handle) != M64ERR_SUCCESS)
        {
            DebugMessage(M64MSG_ERROR, "core error while attaching %s plugin.", g_PluginMap[i].name);
            (*CoreDoCommand)(M64CMD_ROM_CLOSE, 0, NULL);
            (*CoreShutdown)();
            DetachCoreLib();
            return 13;
        }
    }

    /* set up Frame Callback if --testshots is enabled */
    if (l_TestShotList != NULL)
    {
        if ((*CoreDoCommand)(M64CMD_SET_FRAME_CALLBACK, 0, FrameCallback) != M64ERR_SUCCESS)
        {
            DebugMessage(M64MSG_WARNING, "couldn't set frame callback, --testshots will not work.");
        }
    }

    /* set gb cart loader */
    if ((*CoreDoCommand)(M64CMD_SET_MEDIA_LOADER, sizeof(l_media_loader), &l_media_loader) != M64ERR_SUCCESS)
    {
        DebugMessage(M64MSG_WARNING, "Couldn't set media loader, transferpak and GB carts will not work.");
    }

    /* load savestate at startup */
    if (l_SaveStatePath != NULL)
    {
        if ((*CoreDoCommand)(M64CMD_STATE_LOAD, 0, (void *) l_SaveStatePath) != M64ERR_SUCCESS)
        {
            DebugMessage(M64MSG_WARNING, "couldn't load state, rom will run normally.");
        }
    }

    /* Setup debugger */
    if (l_LaunchDebugger)
    {
        if (debugger_setup_callbacks())
        {
            DebugMessage(M64MSG_ERROR, "couldn't setup debugger callbacks.");
            (*CoreDoCommand)(M64CMD_ROM_CLOSE, 0, NULL);
            (*CoreShutdown)();
            DetachCoreLib();
            return 14;
        }
        /* Set Core config parameter to enable debugger */
        int bEnableDebugger = 1;
        (*ConfigSetParameter)(l_ConfigCore, "EnableDebugger", M64TYPE_BOOL, &bEnableDebugger);
        /* Fork the debugger input thread. */
#if SDL_VERSION_ATLEAST(2,0,0)
        SDL_CreateThread(debugger_loop, "DebugLoop", NULL);
#else
        SDL_CreateThread(debugger_loop, NULL);
#endif
    }
    else
    {
        /* Set Core config parameter to disable debugger */
        int bEnableDebugger = 0;
        (*ConfigSetParameter)(l_ConfigCore, "EnableDebugger", M64TYPE_BOOL, &bEnableDebugger);
    }

    /* Save the configuration file again, if necessary, to capture updated
       parameters from plugins. This is the last opportunity to save changes
       before the relatively long-running game. */
    if (l_SaveOptions && (*ConfigHasUnsavedChanges)(NULL))
        (*ConfigSaveFile)();

    analysis_load(CoreDoCommand);

    /* run the game */
    (*CoreDoCommand)(M64CMD_EXECUTE, 0, NULL);

    analysis_unload();

    /* detach plugins from core and unload them */
    for (i = 0; i < 4; i++)
        (*CoreDetachPlugin)(g_PluginMap[i].type);
    PluginUnload();

    /* close the ROM image */
    (*CoreDoCommand)(M64CMD_ROM_CLOSE, 0, NULL);

    /* save the configuration file again if --nosaveoptions was not specified, to keep any updated parameters from the core/plugins */
    if (l_SaveOptions && (*ConfigHasUnsavedChanges)(NULL))
        (*ConfigSaveFile)();

    /* Shut down and release the Core library */
    (*CoreShutdown)();
    DetachCoreLib();

    /* free allocated memory */
    if (l_TestShotList != NULL)
        free(l_TestShotList);

    return 0;
}

