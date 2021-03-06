/*  asylum.c */

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

#include <SDL/SDL.h>
#include <SDL/SDL_mixer.h>

#include "asylum_os.h"
#include "asylum.h"
#include "file.h"
#include "keyboard.h"
#include "sound.h"

#define _firstzone 0

#define _soundentlen 16


static char bank;
static char rate50;
static char cheatpermit;
static char charsok;

static char* backadr;
static board *brainadr;
static board *neuronadr;
static int currentzone;

static int plzone;

char masterplotal;
char frameinc = 1;
int framectr;
board *boardadr;
fastspr_sprite blokeadr[77];
fastspr_sprite blockadr[256];
fastspr_sprite charsadr[48];
fastspr_sprite exploadr[32];
fastspr_sprite alspradr[256];
int xpos, ypos;
char plscore[8];
asylum_options options;
extern char sound_available;

void init()
{
    setdefaults();
    loadconfig();
    vduread(options);
    swi_removecursors();
    bank = 1;
    switchbank(); //set up bank variables
    switchbank(); //set up bank variables
    if (getfiles()) abort_game();

    if (options.soundtype == 2)
        swi_sound_speaker(SPEAKER_ON);

    scorezero();
    cheatpermit = prelude();
    if (cheatpermit == 2)  abort_game();
    for ( ; ; )
    {
        clear_esc_key();
        if (options_menu(0)) // not in game
        {
            abort_game(); return;
        }
	currentzone = 0;
        if (getlevelfiles())
        {
            if (1) abort_game();
            // or, depending on what getlevelfiles() returned
            continue;
        }

        swi_bodgemusic_stop();
        if (game()) continue;
        if (gotallneurons())
        {
            if (options.idpermit != 1) permitid();
            if (options.soundtype == 2) swi_bodgemusic_start(1); // ?? (3,0) in original
        }
        else                            // was "else if overflow clear"
        {
            if (options.soundtype == 2) swi_bodgemusic_start(2);
            swi_sound_qtempo(0x980);
            swi_bodgemusic_volume(options.musicvol);
        }
        showhighscore();
    }
}

int abort_game()
{
    swi_bodgemusic_stop();
    losehandlers();
    clear_esc_key();
    SDL_Quit();
    exit(0);
}

int game()
{
    wipetexttab();
    setup();
    switchcolch(); //setup colch vars
    startmessage();
    startplayer();
    getarms();
    if (cheatpermit == 1)  zonecheatread(&plzone);
    do
    {
       zonerestart:
        if (options.mentalzone == 4)  loadgame();
        else  restartplayer();
        do
        {
            showgamescreen();
            if (options.soundtype == 2) swi_bodgemusic_start((plzone != 0));
            swi_bodgemusic_volume(options.musicvol);
            frameinc = 1;
            rate50 = 1;
            swi_blitz_wait(1);
            while (!swi_readescapestate())
            {
                if (plzone != currentzone)
                {
                    loadzone(); goto zonerestart;
                }
                if ((char)rate50 != 1)
                {
                    plmove();
                    bonuscheck();
                    fuelairproc();
                    switchcolch();
                    masterplotal = 0;
                    moval();
                    project();
                    bullets();
                    alfire();
                    wakeupal(xpos, ypos);
                }
                plmove();
                bonuscheck();
                fuelairproc();
                mazeplot(xpos, ypos);
                switchcolch();
                masterplotal = 1;
                playerplot(true);
                moval();
                project();
                bullets();
                alfire();
                playerplot(false);
                bonusplot();
                scoreadd();
                update_show_strength();
                texthandler(1);
                seeifdead();
                wakeupal(xpos, ypos);
                if (cheatpermit == 1) cheatread();
                scorewipe();
                plotscore();
                frameinc = ((options.gearchange == 0) ? 2 : 1);
                if ((rate50 != 1) && (frameinc < 2)) //rate 25 but one frame passed
                {
                    swi_blitz_wait(1);
                    frameinc = 2;
                    rate50 = 1;
                }
                else if (frameinc > 1) rate50 = 0;
		swi_next_frame(20);

                framectr += frameinc;

                switchbank();
                switch (player_dead())
                {
		   case 1:
                    goto zonerestart;
		   case 2:
		    return 0;  
		   case 0:
		    ;
                }
            }
            swi_bodgemusic_stop();
            redraw_bonus();
            if (escapehandler()) return 1;
	    backprep(backadr);
        }
        while (1);
    }
    while (1);
}

void showtext()
{
    texthandler(0);
    switchbank();
}

static uint8_t store_for_neuron[30+78*28];
static uint8_t store_for_savegame[30+78*28];
static uint8_t store_player_state[25];

void saveal(uint8_t store[30+78*28])
{
    save_player(store);
    save_alents(store+30);
}

void restoreal(uint8_t store[30+78*28])
{
    if (restore_player(store)) return;
    wipealtab();
    restore_alents(store+30);
}

void bonus1()
{
    bonusnumb(10); message(96, 224, 0, -2, "Bonus 10000"); addtoscore(10000);
}

static const int keydefs[] =
{ -SDLK_z, -SDLK_x, -SDLK_SEMICOLON, -SDLK_PERIOD, -SDLK_RETURN };

void setdefaults()
{
    options.soundtype = 2;
    options.soundquality = 1;
    options.soundvol = 0x7f;
    options.musicvol = 0x7f;
    options.speaker = SPEAKER_ON;
    options.leftkey = keydefs[0];
    options.rightkey = keydefs[1];
    options.upkey = keydefs[2];
    options.downkey = keydefs[3];
    options.firekey = keydefs[4];
    options.gearchange = 1;
    options.explospeed = 1;
    options.fullscreen = 0;
    options.opengl = 1;
    options.size = 1; // 640 x 512
    options.scale = 1;
    options.joyno = 0;
    options.mentalzone = 1;
    options.initials[0] = 'P';
    options.initials[1] = 'S';
    options.initials[2] = 'Y';
}

int checkifextend()
{
    FILE *fp;
    int r;

    fp = find_game(FIND_DATA_READ_ONLY);
    r = (fp != NULL);
    if (fp)
    {
        fclose(fp);
    }

    return r;
}


void exithandler()
{
    losehandlers();
    exit(0);
}

void losehandlers()
{
    SDL_Quit(); return;
}

void loadzone()
{
    int r1 = currentzone;

    if ((currentzone = plzone) == 0) exitneuron(r1);
    else enterneuron(r1);
}

void change_zone(int zone)
{
    plzone = zone; 
}

void enterneuron(int r1)
{
    if (r1 == 0) saveal(store_for_neuron);
    currentzone = plzone = getneuronfiles(plzone);
    if (!currentzone)
    {
        exitneuron(r1); return;
    }
    wipealtab();
    getarms();
    initweapon();
    initprojtab();
    initbultab();
    backprep(backadr);
    boardreg();
    prepfueltab();
    startplayer();
}

void exitneuron(int r1)
{
    boardadr = brainadr;
    restoreal(store_for_neuron);
    initweapon();
    initprojtab();
    initbultab();
    retrievebackdrop();
    backprep(backadr);
    boardreg();
    prepfueltab();
    wipesoundtab();
}


void setup()
{
    framectr = 0;
    plzone = _firstzone;
    wipesoundtab();
    initweapon();
    initprojtab();
    initbultab();
    initrockettab();
    getvars();
    prepstrength();
    scorezero();
    backprep(backadr);
    boardreg();
    wipealtab();
    prepfueltab();
}

void wipesoundtab()
{
    for (int r0 = 7; r0 >= 0; r0--)
        swi_stasis_volslide(r0, 0xfc00, 0);
}

void screensave()
{
    plotscore();
	/* XXX: Add code to save the screen here */
}

void c_array_initializers()
{
    init_projsplittab(); init_rocketbursttab(); init_alspintab(); init_rockettab();
    init_palette(); init_splittab();
    load_voices(0);
    init_keyboard();
}

int main(int argc, char** argv)
{
    find_resources();

    if ((argc > 2) && !strcmp(argv[1], "--dumpmusic"))
    {
        dropprivs();
        load_voices(1);
        dumpmusic(argc,argv);
        exit(0);
    }
    else if ((argc > 2) & !strcmp(argv[1], "--nosound"))
    {
        sound_available = 0;
    }
    
    open_scores();
    dropprivs();

    SDL_Init(SDL_INIT_VIDEO|SDL_INIT_AUDIO);
    SDL_WM_SetCaption("Asylum", "Asylum");
    SDL_EnableUNICODE(1);
#ifndef _NO_SOUND
    if (sound_available)
    {
        init_audio();
    }
#endif
    c_array_initializers();
    swi_stasis_control(8, 8);
    init(); // while (snuffctr>=300);
    SDL_Quit();
    exit(0);
    return 0;
}



static char gamescreenpath[] = "GameScreen";
static char chatscreenpath[] = "ChatScreen";
static char blokepath[] = "FSPBloke";
static char boardpath[] = "Brain.dat";
static char blockpath[] = "FSPBlocks.fsp";
static char backpath[] = "Backfile.dat";
static char neuronbackpath[] = "Neurons/Backfile.dat";
static char neuronpath[] = "Neurons/Cell1.dat";
static char* neuronnumber = neuronpath+12;
static char explopath[] = "FSPExplo";
static char charpath[] = "FSPChars";
static char alienpath[] = "FSPAliens.fsp";
static char resourcepath[] = "./Resources/";
static char idpath[] = "./Id/";
static char psychepath[] = "./Psyche/";
static char egopath[] = "./Ego/";
static char egomusic1path[] = "./Ego/Music1";
static char egomusic2path[] = "./Ego/Music2";
static char psychemusic1path[] = "./Psyche/Music1";
static char psychemusic2path[] = "./Psyche/Music2";
static char idmusic1path[] = "./Id/Music1";
static char idmusic2path[] = "./Id/Music2";
static char mainmusicpath[] = "./Resources/Music1";
static char deathmusicpath[] = "./Resources/Music2";

int getfiles()
{
    getvitalfiles();
    showloading();
    if (sound_available)
    {
        init_sounds();
        getmusicfiles();
        swi_bodgemusic_start(1);
    }
    getgamefiles();
    return 0;
}

void getvitalfiles()
{
    charsok = 0;
    char *chatscreenadr, *charsadr_load;
    loadfile(&chatscreenadr, chatscreenpath, resourcepath);
    int charslen = loadfile(&charsadr_load, charpath, resourcepath);
    initialize_sprites(charsadr_load, charsadr, 48, charsadr_load+charslen);
    initialize_chatscreen(chatscreenadr);
    charsok = 1;
}

void getmusicfiles()
{
    swi_bodgemusic_load(1, mainmusicpath);
    swi_bodgemusic_load(2, deathmusicpath);
}

static char* currentpath;

void getgamefiles()
{
    char *gamescreenadr, *blokeadr_load, *exploadr_load;

    loadfile(&gamescreenadr, gamescreenpath, resourcepath);
    initialize_gamescreen(gamescreenadr);
    int blokelen = loadfile(&blokeadr_load, blokepath, resourcepath);
    initialize_sprites(blokeadr_load, blokeadr, 77, blokeadr_load+blokelen);
    int explolen = loadfile(&exploadr_load, explopath, resourcepath);
    initialize_sprites(exploadr_load, exploadr, 32, exploadr_load+explolen);
}

void getlevelsprites()
{
    char *blockadr_load, *alienadr_load;
    switch (options.mentalzone)
    {
    case 2: currentpath = psychepath; break;
    case 3: currentpath = idpath; break;
    case 4: currentpath = psychepath /*XXX*/; break;
    default: currentpath = egopath;
    }
    int blocklen = loadfile(&blockadr_load, blockpath, currentpath);
    initialize_sprites(blockadr_load, blockadr, 256, blockadr_load+blocklen);

    int alienlen = loadfile(&alienadr_load, alienpath, currentpath);
    initialize_sprites(alienadr_load, alspradr, 256, alienadr_load+alienlen);
}

int getlevelfiles()
{
    showgamescreen();
    switch (options.mentalzone)
    {
    case 2: currentpath = psychepath; break;
    case 3: currentpath = idpath; break;
    case 4: currentpath = psychepath /*XXX*/; break;
    default: currentpath = egopath;
    }
    getlevelsprites();

    loadfile((char**)&brainadr, boardpath, currentpath);
    boardadr = brainadr;
// hack: fix endianness
    boardadr->width = read_littleendian((uint32_t*)&boardadr->width);
    boardadr->height = read_littleendian((uint32_t*)&boardadr->height);

    loadfile(&backadr, backpath, currentpath);

    char* r1;
    switch (options.mentalzone)
    {
    case 2: r1 = psychemusic1path; break;
    case 3: r1 = idmusic1path; break;
    default: r1 = egomusic1path;
    }

    swi_bodgemusic_load(0, r1);

    switch (options.mentalzone)
    {
    case 2: r1 = psychemusic2path; break;
    case 3: r1 = idmusic2path; break;
    default: r1 = egomusic2path;
    }
    swi_bodgemusic_load(1, r1);
    return 0;
}

int retrievebackdrop()
{
    loadfile(&backadr, backpath, currentpath);
    return 0;
}

int getneuronfiles(int plzone)
{
    loadfile(&backadr, neuronbackpath, currentpath);
    *neuronnumber = '0' + plzone;
    loadfile((char**)&neuronadr, neuronpath, currentpath);
    boardadr = neuronadr;
// hack: fix endianness
    boardadr->width = read_littleendian((uint32_t*)&boardadr->width);
    boardadr->height = read_littleendian((uint32_t*)&boardadr->height);
    return plzone;
}

static char config_keywords[16][12] =
{ "LeftKeysym",    "RightKeysym", "UpKeysym",   "DownKeysym", "FireKeysym",
  "SoundType",   "SoundQ",      "FullScreen",
  "OpenGL", "ScreenSize", "ScreenScale",
  "SoundVolume", "MusicVolume", "MentalZone", "Initials",   "You" };

static char idpermitstring[] = "You are now permitted to play the ID!!!\n";

void loadconfig()
{
    char keyword[13];

    FILE* r0 = find_config(FIND_DATA_READ_ONLY);
    if (r0 != NULL)
    {
        while (fscanf(r0, " %12s", keyword) != EOF)
        {
            int i;
            for (i = 0; i < 16; i++)
                if (!strncmp(keyword, config_keywords[i], 12)) break;
            if (i == 14)
            {
                fscanf(r0, " %3c", options.initials); continue;
            }
            if (i == 15)
            {
                options.idpermit = 1; break;
            }                       // end of file
            if (i == 16) break;     // parsing failed
            int temp;
            fscanf(r0, " %i", &temp);
            switch (i)
            {
            case 0: options.leftkey = -temp; break;
            case 1: options.rightkey = -temp; break;
            case 2: options.upkey = -temp; break;
            case 3: options.downkey = -temp; break;
            case 4: options.firekey = -temp; break;
            case 5: options.soundtype = temp; break;
                //case 6: options.soundquality=temp; break;
            case 7: options.fullscreen = temp; break;
            case 8: options.opengl = temp; break;
            case 9: options.size = temp; break;
            case 10: options.scale = temp; break;
            case 11: options.soundvol = temp; break;
            case 12: options.musicvol = temp; break;
            case 13: options.mentalzone = temp; break;
            }
        }
        fclose(r0);
    }
    if (!options.idpermit) if (options.mentalzone > 2) options.mentalzone = 2;
    //options.idpermit=(idp==1)?1:0;
}

void saveconfig()
{
    FILE* r0 = find_config(FIND_DATA_READ_WRITE);
    if (r0 == NULL) return;
    fprintf(r0, "%s %i\n%s %i\n%s %i\n%s %i\n%s %i\n%s %i\n%s %i\n%s %i\n%s %i\n%s %i\n%s %i\n%s %i\n%s %i\n%s %c%c%c\n%s",
            config_keywords[0], -options.leftkey,
            config_keywords[1], -options.rightkey,
            config_keywords[2], -options.upkey,
            config_keywords[3], -options.downkey,
            config_keywords[4], -options.firekey,
            config_keywords[5], options.soundtype,
            //config_keywords[6], options.soundquality,
            config_keywords[7], options.fullscreen,
            config_keywords[8], options.opengl,
            config_keywords[9], options.size,
            config_keywords[10], options.scale,
            config_keywords[11], options.soundvol,
            config_keywords[12], options.musicvol,
            config_keywords[13], options.mentalzone,
            config_keywords[14], options.initials[0],
	                         options.initials[1],
	                         options.initials[2],
            ((options.idpermit == 1) ? idpermitstring : ""));
    fclose(r0);
}

void loadgame()
{
    FILE* r0 = find_game(FIND_DATA_READ_ONLY);
    if (r0 == NULL) /* XXX failing silently is bad */ return;
    fread(&options.mentalzone, 1, 1, r0);
    fread(&plzone, 1, 1, r0);  currentzone = plzone;
    fread(store_player_state, 1, 25, r0);
    restore_player_state(store_player_state);
    getlevelfiles();
    fread(store_for_savegame, 1, 30+78*28, r0);
    fread(store_for_neuron, 1, 30+78*28, r0);
    restoreal(store_for_savegame);
    fread(brainadr->contents, brainadr->width, brainadr->height, r0);
    if (plzone)
    {
        getneuronfiles(plzone); /* HACK determine dimensions and allocate neuronadr */
        fread(neuronadr->contents, neuronadr->width, neuronadr->height, r0);
    }
    fclose(r0);
    backprep(backadr);
    boardreg();
    reinitplayer();
}

void savegame()
{
    FILE* r0 = find_game(FIND_DATA_READ_WRITE);
    if (r0 == NULL) /* XXX failing silently is bad */ return;
    fwrite(&options.mentalzone, 1, 1, r0);
    fwrite(&plzone, 1, 1, r0);
    save_player_state(store_player_state);
    fwrite(store_player_state, 1, 25, r0);
    saveal(store_for_savegame);
    fwrite(store_for_savegame, 1, 30+78*28, r0);
    fwrite(store_for_neuron, 1, 30+78*28, r0);
    fwrite(brainadr->contents, brainadr->width, brainadr->height, r0);
    if (plzone)
    {
        fwrite(neuronadr->contents, neuronadr->width, neuronadr->height, r0);
    }
    fclose(r0);
}

void permitid()
{
    FILE* r0 = find_config(FIND_DATA_APPEND);
    if (r0 != NULL)
    {
        fprintf(r0, "%s", idpermitstring);
        fclose(r0);
    }
    options.idpermit = 1;
}

