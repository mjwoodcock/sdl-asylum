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

static int get_file_length(const char* path);

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
    if (setregid(getgid(), getgid()) != 0)
    {
        fprintf(stderr, "setregid failed\n");
        exit(1);
    }
    if (setreuid(getuid(), getuid()) != 0)
    {
        fprintf(stderr, "setreuid failed\n");
        exit(1);
    }
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

int loadfile(char** spaceptr, char* filename, char* path)
{
    char fullname[PATH_MAX];
    char *buffer;
    int length;
    snprintf(fullname, sizeof(fullname), "%s%s", path, filename);
    FILE *f = fopen(fullname, "rb");
    if (!f)
        goto fullfail;
    fseek(f, 0, SEEK_END);
    length = ftell(f);
    rewind(f);
    buffer = (char *)malloc(length);
    if (!buffer)
        goto failf;
    if (fread(buffer, length, 1, f) != 1)
        goto failbuf;
    fclose(f);
    *spaceptr = buffer;
    return length;
failbuf:
    free(buffer);
failf:
    fclose(f);
fullfail:
    printf("Can't load file %s\n", fullname);
    exit(EXIT_FAILURE);
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
