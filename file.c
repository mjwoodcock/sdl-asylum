/*  file.c */

/*  Copyright Hugh Robinson 2006-2008.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <limits.h>

#include "asylum.h"
#include "file.h"

static int read_file(const char* path, char* start, char* end);
static int write_file(const char* path, char* start, char* end);
static int get_file_length(const char* path);
static int does_file_exist(const char* path);

static char resource_path[PATH_MAX];
static char score_path[PATH_MAX];

static char configname[] = "/.asylum";
static char savegamename[] = "/.asylum_game";

static const char* score_name[4] = {
    "/EgoHighScores", "/PsycheHighScores", "/IdHighScores", "/ExtendedHighScores"
};

static FILE* score_file[4];


FILE* find_game(int op)
{
    char fullname[PATH_MAX] = "";

    char* home = getenv("HOME");
    if (home)
	strcat(fullname, home);
    else
	return NULL;
    strcat(fullname, savegamename);
    switch (op)
    {
    case FIND_DATA_READ_ONLY:
        return fopen(fullname, "rb");
    case FIND_DATA_READ_WRITE:
        return fopen(fullname, "wb");
    default:
	return NULL;
    }
}

FILE* find_config(int op)
{
    char fullname[PATH_MAX] = "";

    char* home = getenv("HOME");
    if (home)
	strcat(fullname, home);
    else
	strcat(fullname, resource_path);
    strcat(fullname, configname);
    switch (op)
    {
    case FIND_DATA_READ_ONLY:
        return fopen(fullname, "rb");
    case FIND_DATA_READ_WRITE:
        return fopen(fullname, "wb");
    case FIND_DATA_APPEND:
        return fopen(fullname, "ab");
    default:
        return NULL;
    }
}

void dropprivs()
{
#ifndef _WIN32
    setregid(getgid(), getgid());
    setreuid(getuid(), getuid());
#endif
}

uint32_t read_littleendian(uint8_t* bytes)
{
    return (*bytes)|(bytes[1]<<8)|(bytes[2]<<16)|(bytes[3]<<24);
}

uint32_t read_littleendian(uint32_t* word)
{
    return read_littleendian((uint8_t*)word);
}

void write_littleendian(uint8_t* bytes, uint32_t word)
{
    *bytes = word & 0xff;
    bytes[1] = (word>>8) & 0xff;
    bytes[2] = (word>>16) & 0xff;
    bytes[3] = (word>>24) & 0xff;
}

int load_data(char** spaceptr, char* filename, char* path)
{
    int reload = 0;
    char fullname[PATH_MAX];
    int length;

    snprintf(fullname, sizeof(fullname), "%s%s", path, filename);
    do
    {
        length = get_file_length(fullname);
        *spaceptr = (char *)malloc(length);
        if (read_file(fullname, *spaceptr, *spaceptr + length))
        {
            reload = badlevelload();
        }
    } while(reload == 1);

    return reload;
}

int loadvitalfile(char** spaceptr, char* r1, char* path)
{
// if VS or if r0==1
    char fullname[PATH_MAX] = "";

    strcat(fullname, path);
    strcat(fullname, r1);
    int r4 = get_file_length(fullname);
    if (r4 <= 0) fatalfile();
    *spaceptr = (char*)malloc(r4);
    if (read_file(fullname, *spaceptr, (*spaceptr) + r4)) fatalfile();
    return r4;
}

int loadfile(char** spaceptr, char* r1, char* path)
{
    int r4;
    char fullname[PATH_MAX] = "";

    snprintf(fullname, sizeof(fullname), "%s%s", path, r1);
    r4 = get_file_length(fullname);

    // hack: +4 as feof doesn't trigger until we've passed the end
    *spaceptr = (char*)malloc(r4+4);

    if (r4 == -1)
    {
        filenotthere(); return 1;
    }
    if (*spaceptr == NULL)
    {
        nomemory(); return 1;
    }
    if (read_file(fullname, *spaceptr, (*spaceptr) + r4 + 4))
    {
        filesyserror(); return 1;
    }
    return r4;
}

void set_paths()
{
#ifdef RESOURCEPATH
    if (chdir(RESOURCEPATH) == 0)
    {
        strcpy(resource_path, RESOURCEPATH);
        strcpy(score_path, SCOREPATH);
        /* We could fall back to ~/.asylum/ if SCOREPATH is not writable.
           However just assuming the current directory is ok is not cool. */
        return;
    }
#endif

    fprintf(stderr, "Running as uninstalled, looking for files in local directory.\n");

#ifdef HAVE_GET_EXE_PATH
    char exe_path[PATH_MAX];
    if (get_exe_path(exe_path, sizeof(exe_path)))
    {
        strcpy(resource_path, exe_path);
        strncat(resource_path, "/data");

        strcpy(score_path, exe_path);
        strcat(score_path, "/hiscores");
        return;
    }
#endif

    strcpy(resource_path, "data");
    strcpy(score_path, "../hiscores"); /* relative to resource_path */
}

void open_scores()
{
    char filename[PATH_MAX];

    for (int i = 0; i < 4; ++i)
    {
        strcpy(filename, score_path);
        strcat(filename, score_name[i]);
        score_file[i] = fopen(filename, "r+b");
        if (score_file[i] == NULL)
        {
            // Perhaps the file didn't exist yet
            score_file[i] = fopen(filename, "w+b");
            if (score_file[i] == NULL)
            {
                // Perhaps we don't have write permissions :(
                score_file[i] = fopen(filename, "rb");
                if (score_file[i] == NULL)
                    fprintf(stderr, "Couldn't open %s, check if the directory exists\n", filename);
                else
                    fprintf(stderr, "Opening %s read-only, high scores will not be saved\n", filename);
            }
        }
    }
}

void find_resources()
{
    set_paths();
    if (chdir(resource_path) != 0)
    {
        fprintf(stderr, "Couldn't find resources directory %s\n", resource_path);
        exit(1);
    }
}

void savescores(char* highscorearea, int mentalzone)
{
    highscorearea[13*5] = swi_oscrc(0, highscorearea, highscorearea+13*5, 1);
    if (mentalzone >= 1 && mentalzone <= 4 && score_file[mentalzone - 1] != NULL)
    {
        FILE * f = score_file[mentalzone - 1];
        fseek(f, 0, SEEK_SET);
        fwrite(highscorearea, 1, 13*5+1, f);
        fflush(f);
    }
}

void loadscores(char* highscorearea, int mentalzone)
{
    if (mentalzone >= 1 && mentalzone <= 4 && score_file[mentalzone - 1] != NULL)
    {
        FILE * f = score_file[mentalzone - 1];
        fseek(f, 0, SEEK_SET);
        if (fread(highscorearea, 1, 13*5+1, f) == 13*5+1 &&
            swi_oscrc(0, highscorearea, highscorearea+13*5, 1) == highscorearea[13*5])
        {
            return;
        }
    }
    setdefaultscores();
}

int filelength(const char* name, const char* path)
{
    char fullname[PATH_MAX] = "";

    snprintf(fullname, sizeof(fullname), "%s%s", path, name);
    int r4 = get_file_length(fullname);
    if (r4 == -1)
    {
        filesyserror(); return 0;
    }
    return r4;
}

static int write_file(const char* path, char* start, char* end)
{
    FILE *f;

    f = fopen(path, "wb");
    for (char* i = start; i < end; i++) fputc(*i, f);
    fclose(f);
    return 0;
}

static int does_file_exist(const char* path)
{
    FILE *f;

    f = fopen(path, "rb");
    if (f == NULL) return 0;
    fclose(f);
    return 1;
}

static int get_file_length(const char* path)
{
    FILE *f;
    int x;

    f = fopen(path, "rb");
    if (f == NULL) return -1;
    fseek(f, 0, SEEK_END);
    x = ftell(f);
    fclose(f);
    return x;
}

static int read_file(const char* path, char* start, char* end)
{
    FILE *f;

    f = fopen(path, "rb");
    if (f == NULL) return -1;
    for (char* i = start; i < end && !feof(f); i++) *i = fgetc(f);
    fclose(f);
    return 0;
}

