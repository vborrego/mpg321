/*
    mpg321 - a fully free clone of mpg123.
    Copyright (C) 2001 Joe Drew
    
    Originally based heavily upon:
    plaympeg - Sample MPEG player using the SMPEG library
    Copyright (C) 1999 Loki Entertainment Software
    
    Also uses some code from
    mad - MPEG audio decoder
    Copyright (C) 2000-2001 Robert Leslie
    
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#define _LARGEFILE_SOURCE 1

#include "mpg321.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <libgen.h>
#include <signal.h>
#include <limits.h>
#include <sys/time.h>
#include <assert.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>

#include "getopt.h" /* GNU getopt is needed, so I included it */

#include <id3tag.h>

int shuffle_play;
int stop_playing_file = 0;
int quit_now = 0;
char *playlist_file;
ao_device *playdevice=NULL;
mad_timer_t current_time;
mpg321_options options = { 0, NULL, NULL, 0 , 0, 0};
int status = MPG321_STOPPED;
int file_change = 0;

char *id3_get_tag (struct id3_tag const *tag, char const *what, unsigned int maxlen);

void mpg123_boilerplate()
{
    fprintf(stderr,"High Performance MPEG 1.0/2.0/2.5 Audio Player for Layer 1, 2, and 3.\n"
        "Version " FAKEVERSION " (" VERSIONDATE "). Written and copyrights by Joe Drew.\n"
        "Uses code from various people. See 'README' for more!\n"
        "THIS SOFTWARE COMES WITH ABSOLUTELY NO WARRANTY! USE AT YOUR OWN RISK!\n");
}

/* print the error in errno */
void mpg321_error(char *file)
{
    char *error = strerror(errno);
    fprintf(stderr, "%s: %s\n",file, error);
}

void usage(char *argv0)
{
    mpg123_boilerplate();
    fprintf(stderr,
        "\nUsage: %s [options] file(s) | URL(s) | -\n\n"
        "Options supported:\n"
        "   --verbose or -v          Increase verbosity\n"
        "   --quiet or -q            Quiet mode (no title or boilerplate)\n"
        "   --gain N or -g N         Set gain (audio volume) to N (0-100)\n"
        "   --skip N or -k N         Skip N frames into the file\n"
        "   --verbose or -v          Be more verbose in playing files\n"
        "   -o dt                    Set output devicetype to dt\n" 
    "                                [esd,alsa(09),arts,sun,oss]\n"
        "   --audiodevice N or -a N  Use N for audio-out\n"
        "   --stdout or -s           Use stdout for audio-out\n"
        "   --au N                   Use au file N for output\n"
        "   --cdr N                  Use wave file N for output\n"
        "   --wav N or -w N          Use wave file N for output\n"
        "   --test or -t             Test only; do no audio output\n"
        "   --list N or -@ N         Use playlist N as list of MP3 files\n"
        "   --random or -Z           Play files randomly until interrupted\n"
        "   --shuffle or -z          Shuffle list of files before playing\n"
        "   --fft                    Turn on FFT analysis of PCM data\n"
        "   -R                       Use remote control interface\n"
        "   --aggressive             Try to get higher priority\n"
        "   --help or --longhelp     Print this help screen\n"
        "   --version or -V          Print version information\n"
        "\n"
        "This version of mpg321 has been configured with " AUDIO_DEFAULT " as its default\n"
        "libao output device.\n" , argv0);
}

/* retsigtype is defined by configure;
   pass handle_signals -1 to initialize it */
RETSIGTYPE handle_signals(int sig) 
{
    static struct timeval last_signal = { 0, 0 };
    struct timeval curr_tv;
    
    gettimeofday(&curr_tv, NULL);
    
    if (sig == SIGINT)
    {
        if (((curr_tv.tv_sec - last_signal.tv_sec) * 1000000 +  
            (curr_tv.tv_usec - last_signal.tv_usec)) < 1000000) /* Allow a short delay to
                                                                  kill mpg321 */
        {
            quit_now = 1;
        }
            
        last_signal.tv_sec = curr_tv.tv_sec;
        last_signal.tv_usec = curr_tv.tv_usec;
        
        stop_playing_file = 1;
   }
   else if (sig == -1) /* initialize */
   {
        last_signal.tv_sec = curr_tv.tv_sec;
        last_signal.tv_usec = curr_tv.tv_usec;
   }
}

/*
    Only shows ID3 tags if at least one of
    TITLE, ARTIST, ALBUM, YEAR, COMMENT, GENRE
    is present.
*/
static int show_id3(struct id3_tag const *tag)
{
    unsigned int i;
    int print = 0;
    char emptystring[31];
    char *names[6];
    struct {
        int index;
        char const *id;
        char const *name;
    } const info[] = {
        { 0,    ID3_FRAME_TITLE,  "Title  : "   },
        { 1,    ID3_FRAME_ARTIST, "  Artist: "  },
        { 2,    ID3_FRAME_ALBUM,  "Album  : "   },
        { 3,    ID3_FRAME_YEAR,   "  Year  : "  },
        { 4,    ID3_FRAME_COMMENT,"Comment: "   },
        { 5,    ID3_FRAME_GENRE,  "  Genre : "  }
    };

    memset (emptystring, ' ', 30);
    emptystring[30] = '\0';
    /*  Get ID3 tag if available, 30 chars except 4 for year  */
    for (i=0; i<=5; i++)    {
        names[i] = NULL;
        names[i] = id3_get_tag(tag, info[i].id, (i==3) ? 4 : 30);
    }
    for (i=0; i<=5; i++)    {
        if (names[i])   {
            print = 1;
            break;
        }
    }
    if (!print) {
        return 0;
    }

    if (options.opt & MPG321_REMOTE_PLAY)
    {
        printf("@I ID3:");

        for (i=0; i<=5; i++)
        {
            if(!names[i])
            {
                printf(emptystring);
            }
            
            else
            {
                printf("%s", names[i]);
                free(names[i]);
            }
        }
        printf("\n");
    }
    
    else
    {
        /* Emulate mpg123 original behaviour  */
        for (i=0; i<=5; i++)    {
            fprintf (stderr, "%s", info[i].name);
            if (!names[i])  {
                fprintf (stderr, emptystring);
            }   else    {
                fprintf (stderr, "%s", names[i]);
                free (names[i]);
            }
            if (i%2) fprintf (stderr, "\n");
        }
    }

    return 1;
}
/*
 Shows loaded playlist
 */
void show_playlist(playlist* plist){
    //printf("Number of files in playlist: %d \n" , pl->numfiles );
    printf("## Playlist ## \n"  );    
    int idx=0;
    for(idx=0;idx<plist->numfiles;idx++){
        printf("\tFile: %s \n" , plist->files[idx] );    
    }
    printf("\n\n\n"  );       
}

int main(int argc, char *argv[])
{
    int fd = 0;
    char *currentfile, old_dir[PATH_MAX];
    playlist *pl = NULL;
    struct id3_file *id3struct = NULL;
    struct id3_tag *id3tag = NULL;

    buffer playbuf;
    
    struct mad_decoder decoder;

    old_dir[0] = '\0';

    playbuf.pl = pl = new_playlist();

    if (!pl)
    {
        fprintf(stderr, "malloc failed at startup!\n");
        exit(1);
    }

    options.volume = MAD_F_ONE;

    status = MPG321_PLAYING;
    
    /* Get the command line options */
    parse_options(argc, argv, pl);

    /* If there were no files and no playlist specified just print the usage */
    if (!playlist_file && optind == argc)
    {
        usage(argv[0]);
        exit(0);
    }

    if (playlist_file)
        load_playlist(pl, playlist_file);
        
    add_cmdline_files(pl, argv);

    if (shuffle_play)
        shuffle_files(pl);

    ao_initialize();

    check_default_play_device();
    
    if (!(options.opt & MPG321_REMOTE_PLAY))
    {
        handle_signals(-1); /* initialize signal handler */
     
        remote_input_buf[0] = '\0';
    }
    
    if (!(options.opt & MPG321_QUIET_PLAY)) 
        mpg123_boilerplate();
    
    if (options.opt & MPG321_REMOTE_PLAY)
    {
        printf ("@R MPG123\n");
    }

    show_playlist(pl);
    
    /* Play the mpeg files or zip it! */
    while((currentfile = get_next_file(pl, &playbuf)))
    {
        if (quit_now) 
            break;
        
        signal(SIGINT, SIG_DFL);
        
        playbuf.buf = NULL;
        playbuf.fd = -1;
        playbuf.length = 0;
        playbuf.done = 0;
        playbuf.num_frames = 0;
        playbuf.max_frames = -1;
        strncpy(playbuf.filename,currentfile, PATH_MAX);
        playbuf.filename[PATH_MAX-1] = '\0';
        
        if (status == MPG321_PLAYING) 
            file_change = 1;

        mad_timer_reset(&playbuf.duration);
        
        mad_timer_reset(&current_time);

        if (!(options.opt & MPG321_QUIET_PLAY) && file_change)
        {
            id3struct = id3_file_open (currentfile, ID3_FILE_MODE_READONLY);

            if (id3struct)
            {
                id3tag = id3_file_tag (id3struct);
            
                if (id3tag)
                {
                    show_id3 (id3tag);
                }

                id3_file_close (id3struct);
            }
        }

        if (options.opt & MPG321_REMOTE_PLAY && file_change)
        {
            id3struct = id3_file_open (currentfile, ID3_FILE_MODE_READONLY);

            if (id3struct)
            {
                id3tag = id3_file_tag (id3struct);
            
                if (id3tag)
                {
                    if (!show_id3(id3tag))
                    {
                        /* This shouldn't be necessary, but it appears that
                           libid3tag doesn't necessarily know if there are no
                           id3 tags on a given mp3 */
                        char * basec = strdup(currentfile);
                        char * basen = basename(basec);
                        
                        char * dot = strrchr(basen, '.');
                        
                        if (dot)
                            *dot = '\0';
                        
                        printf("@I %s\n", basen);

                        free(basec);
                    }
                }
                
                else
                {
                    fprintf(stderr, "Allocation error");
                    exit(1);
                }

                id3_file_close (id3struct);
            }
            
            else
            {
                char * basec = strdup(currentfile);
                char * basen = basename(basec);
                
                char * dot = strrchr(basen, '.');
                
                if (dot)
                    *dot = '\0';
                
                printf("@I %s\n", basen);

                free(basec);
            }
        }

        /* Create the MPEG stream */
        /* Check if source is on the network */
        if((fd = raw_open(currentfile)) != 0 || (fd = http_open(currentfile)) != 0
            || (fd = ftp_open(currentfile)) != 0)
        {
            playbuf.fd = fd;
            playbuf.buf = malloc(BUF_SIZE);
            playbuf.length = BUF_SIZE;
            
            mad_decoder_init(&decoder, &playbuf, read_from_fd, read_header, /*filter*/0,
                            output, /*error*/0, /* message */ 0);
        }

        /* Check if we are to use stdin for input */
        else if(strcmp(currentfile, "-") == 0)
        {
            playbuf.fd = fileno(stdin);
            playbuf.buf = malloc(BUF_SIZE);
            playbuf.length = BUF_SIZE;

            mad_decoder_init(&decoder, &playbuf, read_from_fd, read_header, /*filter*/0,
                            output, /*error*/0, /* message */ 0);
        }
            
        /* currentfile is a local file (presumably.) mmap() it */
        else
        {
            struct stat stat;
            
            if((fd = open(currentfile, O_RDONLY)) == -1)
            {
                mpg321_error(currentfile);

                /* mpg123 stops immediately if it can't open a file */
                break;
            }
            
            if(fstat(fd, &stat) == -1)
            {
                close(fd);
                mpg321_error(currentfile);
                continue;
            }
            
            if (!S_ISREG(stat.st_mode))
            {
                close(fd);
                continue;
            }
            
            calc_length(currentfile, &playbuf);

            if ((options.maxframes != -1) && (options.maxframes <= playbuf.num_frames))
            { 
                playbuf.max_frames = options.maxframes;
            }
            
            playbuf.frames = malloc((playbuf.num_frames + 1) * sizeof(void*));
            playbuf.times = malloc((playbuf.num_frames + 1) * sizeof(mad_timer_t));
    
            if((playbuf.buf = mmap(0, playbuf.length, PROT_READ, MAP_SHARED, fd, 0))
                                == MAP_FAILED)
            {
                close(fd);
                mpg321_error(currentfile);
                continue;
            }
            
            close(fd);
            playbuf.frames[0] = playbuf.buf;
            
            mad_decoder_init(&decoder, &playbuf, read_from_mmap, read_header, /*filter*/0,
                            output, /*error*/0, /* message */ 0);
        }

        if(!(options.opt & MPG321_QUIET_PLAY))/*zip it!!!*/
        {
            /* Because dirname might modify the argument */
            char * dirc = strdup(currentfile);
            char * basec = strdup(currentfile);

            char * basen = basename(basec);
            char * dirn = dirname(dirc);
            
            /* make sure that the file has a pathname; otherwise don't print out
               a Directory: listing */
            if(strchr(currentfile, '/') && strncmp(old_dir, dirn, PATH_MAX) != 0)
            {
                /* Print information about the file */
                fprintf(stderr, "\n");
                fprintf(stderr,"Directory: %s/\n", dirn);
                
                strncpy(old_dir, dirn, PATH_MAX);
                old_dir[PATH_MAX-1] = '\0';
            }
            
            /* print a newline between different songs only, not after
               Directory: listing */
            else
            {
                fprintf(stderr, "\n");
            }

            fprintf(stderr,"Playing MPEG stream from %s ...\n", basen);
    
            free(dirc);
            free(basec);
        }    

        signal(SIGINT, handle_signals);

        /* Every time the user gets us to rewind, we exit decoding,
           reinitialize it, and re-start it */
        while (1)
        {
            mad_decoder_run(&decoder, MAD_DECODER_MODE_SYNC);
            
            /* if we're rewinding on an mmap()ed stream */
            if(status == MPG321_REWINDING && playbuf.fd == -1) 
            {
                mad_decoder_init(&decoder, &playbuf, read_from_mmap, read_header, /*filter*/0,
                    output, /*error*/0, /* message */ 0);
            }    
            else
                break;
        } 

        if (!(options.opt & MPG321_QUIET_PLAY))
        {
            char time_formatted[11];
            mad_timer_string(current_time, time_formatted, "%.1u:%.2u", MAD_UNITS_MINUTES,
                       MAD_UNITS_SECONDS, 0);
            fprintf(stderr, "\n[%s] Decoding of %s finished.\n",time_formatted, basename(currentfile));
        }
        
        if (options.opt & MPG321_REMOTE_PLAY && status == MPG321_STOPPED)
        {
            clear_remote_file(pl);
        }

        mad_decoder_finish(&decoder);

        if (quit_now)
            break;

        if (playbuf.frames)
             free(playbuf.frames);

        if (playbuf.times)
            free(playbuf.times);
            
        if (playbuf.fd == -1)
        {
            munmap(playbuf.buf, playbuf.length);
        }

        else
        {
            free(playbuf.buf);
            if (playbuf.fd != fileno(stdin)) 
                close(playbuf.fd);
        }
    }

    if(playdevice)
        ao_close(playdevice);

    ao_shutdown();

#if defined(RAW_SUPPORT) || defined(HTTP_SUPPORT) || defined(FTP_SUPPORT) 
    if(fd) close(fd);
#endif

    return(0);
}


/* Convenience for retrieving already formatted id3 data
 * what parameter is one of
 *  ID3_FRAME_TITLE
 *  ID3_FRAME_ARTIST
 *  ID3_FRAME_ALBUM
 *  ID3_FRAME_YEAR
 *  ID3_FRAME_COMMENT
 *  ID3_FRAME_GENRE
 * It allocates a new string. Free it later.
 * NULL if no tag or error.
 */
char *id3_get_tag (struct id3_tag const *tag, char const *what, unsigned int maxlen)
{
    struct id3_frame const *frame = NULL;
    union id3_field const *field = NULL;
    int nstrings;
    int avail;
    int j;
    int tocopy;
    int len;
    char printable [1024];
    char *retval = NULL;
    id3_ucs4_t const *ucs4 = NULL;
    id3_latin1_t *latin1 = NULL;

    memset (printable, '\0', 1024);
    avail = 1024;
    if (strcmp (what, ID3_FRAME_COMMENT) == 0)
    {
        /*There may be sth wrong. I did not fully understand how to use
            libid3tag for retrieving comments  */
        j=0;
        frame = id3_tag_findframe(tag, ID3_FRAME_COMMENT, j++);
        if (!frame) return (NULL);
        ucs4 = id3_field_getfullstring (&frame->fields[3]);
        if (!ucs4) return (NULL);
        latin1 = id3_ucs4_latin1duplicate (ucs4);
        if (!latin1 || strlen(latin1) == 0) return (NULL);
        len = strlen(latin1);
        if (avail > len)
            tocopy = len;
        else
            tocopy = 0;
        if (!tocopy) return (NULL);
        avail-=tocopy;
        strncat (printable, latin1, tocopy);
        free (latin1);
    }
    
    else
    {
        frame = id3_tag_findframe (tag, what, 0);
        if (!frame) return (NULL);
        field = &frame->fields[1];
        nstrings = id3_field_getnstrings(field);
        for (j=0; j<nstrings; ++j)
        {
            ucs4 = id3_field_getstrings(field, j);
            if (!ucs4) return (NULL);
            if (strcmp (what, ID3_FRAME_GENRE) == 0)
                ucs4 = id3_genre_name(ucs4);
            latin1 = id3_ucs4_latin1duplicate(ucs4);
            if (!latin1) break;
            len = strlen(latin1);
            if (avail > len)
                tocopy = len;
            else
                tocopy = 0;
            if (!tocopy) break;
            avail-=tocopy;
            strncat (printable, latin1, tocopy);
            free (latin1);
        }
    }
    retval = malloc (maxlen + 1);
    if (!retval) return (NULL);

    strncpy (retval, printable, maxlen);
    retval[maxlen] = '\0';

    len = strlen(printable);
    if (maxlen > len)
    {
        memset (retval + len, ' ', maxlen - len);
    }

    return (retval);
}
