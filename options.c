/*
    mpg321 - a fully free clone of mpg123.
    options.c: Copyright (C) 2001, 2002 Joe Drew

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

#include "getopt.h"

#include <sys/time.h>
#include <sys/resource.h>
#include <unistd.h>
#include <string.h>

void parse_options(int argc, char *argv[], playlist *pl)
{
    struct option long_options[] =
    {
        /* NO-OPS. Implement these if you need them, please. */

        /* these take no parameter and have no short equiv */
        { "headphones", 0, 0, 'O' }, /* or -o h */
        { "speaker", 0, 0, 'P' }, /* or -o s */
        { "lineout", 0, 0, 'L' }, /* or -o l */
        { "reopen", 0, 0, 'N' },
        { "equalizer", 0, 0, 'E' },
        { "aggressive", 0, 0, 'I' },
        { "8bit", 0, 0, '8' },

        /* these take no parameter and have short equiv */
        { "2to1", 0, 0, '2' },
        { "4to1", 0, 0, '4' },
        { "check", 0, 0, 'c' },
        { "resync", 0, 0, 'y' },
        { "left", 0, 0, '0' },
        { "single0", 0, 0, '0' },
        { "right", 0, 0, '1' },
        { "single1", 0, 0, '1' },
        { "mono", 0, 0, 'm' },
        { "mix", 0, 0, 'm' },
        { "control", 0, 0, 'C' },
        { "auth", 0, 0, 'u' },

        /* these take a parameter and have short equiv */
        { "doublespeed", 1, 0, 'd' },
        { "halfspeed", 1, 0, 'h' },
        { "scale", 1, 0, 'f' },
        { "buffer", 1, 0, 'b' },
        { "proxy", 1, 0, 'p' },
        { "rate", 1, 0, 'r' },

        /* The following are all implemented. */

        /* These take a parameter and have no short equiv */
        { "au", 1, 0, 'A' },
        { "cdr", 1, 0, 'D' },

        /* Takes no parameters */
        { "verbose", 0, 0, 'v' },
        { "quiet", 0, 0, 'q' },
        { "test", 0, 0, 't' },
        { "stdout", 0, 0, 's' },
        { "STDOUT", 0, 0, 's' },
        { "version", 0, 0, 'V' },
        { "help", 0, 0, 'H' },
        { "longhelp", 0, 0, 'H' },
        { "shuffle", 0, 0, 'z' },
        { "random", 0, 0, 'Z' },
        { "remote", 0, 0, 'R' },
        { "stereo", 0, 0, 'T' },
        { "fft", 0, 0, 'F'},
        { "repeat", 0, 0, 'e'},

        /* takes parameters */
        { "frames", 1, 0, 'n' },
        { "skip-printing-frames", 1, 0, 'G' },
        { "output", 1, 0, 'o' },
        { "list", 1, 0, '@' },
        { "skip", 1, 0, 'k' },
        { "wav", 1, 0, 'w' },
        { "audiodevice", 1, 0, 'a' },
        { "gain", 1, 0, 'g' },
        { 0, 0, 0, 0 }
    };
    int option_index = 0, c;

    options.maxframes=-1;

    while ((c = getopt_long(argc, argv,
                                "OPLTNEI824cy01mCu:d:h:f:b:p:r:G:" /* unimplemented */
                                "A:D:vqtsVFHzZRo:n:@:k:w:a:g:",    /* implemented */
                        long_options, &option_index)) != -1)
    {
        switch(c)
        {
            case 'O': case 'P': case 'L': case 'N': case 'E': case '8':
            case '2': case '4': case 'c': case 'y': case '0': case '1': case 'm': case 'C':
            case 'u':
            case 'U': case 'd': case 'h': case 'f': case 'b': case 'p':
                break;
            case 'n':
                options.maxframes = atol(optarg);
                break;
            case 'r':
                break;

            case 'z':
                shuffle_play = 1;
                break;

            case 'Z':
                set_random_play(pl);
                break;
                
            case 'e':
                set_repeat_play(pl);
                break;
                
            case '@':
                playlist_file = strdup(optarg);
                break;

            case 'v':
                options.opt |= MPG321_VERBOSE_PLAY;
                setvbuf(stdout, NULL, _IONBF, 0);
                break;

            case 'F':
                options.opt |= MPG321_PRINT_FFT;
                break;

            case 'q':
                options.opt |= MPG321_QUIET_PLAY;
                break;

            case 'R':
                options.opt |= MPG321_REMOTE_PLAY;
                options.opt |= MPG321_QUIET_PLAY; /* surpress other output */
                setvbuf(stdout, NULL, _IONBF, 0);
                break;

            case 'k':
                options.seek = atol(optarg);
                status = MPG321_SEEKING;
                if (options.seek < 0)
                {
                    fprintf(stderr, "Number of frames to skip must be positive!\n");
                    exit(1);
                }
                break;

            case 't':
                options.opt = MPG321_USE_NULL;
                break;

            case 'w':
                options.opt |= MPG321_USE_WAV;
                options.device = strdup(optarg);
                break;

            case 'A':
                options.opt |= MPG321_USE_AU;
                options.device = strdup(optarg);
                break;

            case 'D':
                options.opt |= MPG321_USE_CDR;
                options.device = strdup(optarg);
                break;

            case 'a':
                /* use this device or file or whatever for output, with the
                   default output device, or whatever device is specified with -o */
                options.device = strdup(optarg);
                break;

            case 's':
                options.opt |= MPG321_USE_STDOUT;
                break;

            case 'o':
                if (strcmp(optarg, "alsa") == 0)
                {
                    options.opt |= MPG321_USE_ALSA;
                }

                else if (strcmp(optarg, "alsa09") == 0)
                {
                    options.opt |= MPG321_USE_ALSA09;
                }

                else if (strcmp(optarg, "esd") == 0)
                {
                    options.opt |= MPG321_USE_ESD;
                }

                else if (strcmp(optarg, "arts") == 0)
                {
                    options.opt |= MPG321_USE_ARTS;
                }

                else if (strcmp(optarg, "oss") == 0)
                {
                    options.opt |= MPG321_USE_OSS;
                }

                else if (strcmp(optarg, "sun") == 0)
                {
                    options.opt |= MPG321_USE_SUN;
                }

                else if (strcmp(optarg, "h") == 0 || strcmp(optarg, "s") == 0
                            || strcmp(optarg, "l") == 0)
                {
                    /* for now, we don't support these */
                }

                else /* Just pass on what the user gave to libao */
                {
                    options.opt |= MPG321_USE_USERDEF;
                    options.devicetype = strdup(optarg);
                }

                break;

            case 'g':
                options.volume = atoi(optarg);
                options.volume = (options.volume/100.0) * MAD_F_ONE;
                break;

            case 'I':
                /* We only try to get high priority. If it fails (just as
                   mpg123 does when run as a user), that's fine */
                setpriority(PRIO_PROCESS, getpid(), -20);
                break;

            case 'T':
                options.opt |= MPG321_FORCE_STEREO;
                break;

            case 'G':
                options.skip_printing_frames = atoi(optarg);
                break;

            case 'V':
                printf("mpg321 version " VERSION ". Copyright (C) 2001, 2002 Joe Drew.\n\n"
                       "This program is free software; you can redistribute it and/or modify\n"
                       "it under the terms of the GNU General Public License as published by\n"
                       "the Free Software Foundation; either version 2 of the License, or\n"
                       "(at your option) any later version.\n" );
                exit(0);

            case 'H':
                usage(argv[0]);
                exit(0);

            case ':':
                fprintf(stderr, "Missing argument to %s\n", argv[optind]);
                break;
        }
    }
}
