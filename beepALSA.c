/*
 * Copyright (C) 2014 Yukimasa Sugizaki (derived from alsa-utils 1.0.25)
 */

/*
 * Copyright (C) 2000-2004 James Courtier-Dutton
 * Copyright (C) 2005 Nathan Hurst
 *
 * This file is part of the speaker-test tool.
 *
 * This small program sends a simple sinusoidal wave to your speakers.
 *
 * speaker-test is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * speaker-test is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA
 *
 *
 * Main program by James Courtier-Dutton (including some source code fragments from the alsa project.)
 * Some cleanup from Daniel Caujolle-Bert <segfault@club-internet.fr>
 * Pink noise option added Nathan Hurst, 
 *   based on generator by Phil Burk (pink.c)
 *
 * Changelog:
 *   0.0.8 Added support for pink noise output.
 * Changelog:
 *   0.0.7 Added support for more than 6 channels.
 * Changelog:
 *   0.0.6 Added support for different sample formats.
 *
 * $Id: speaker_test.c,v 1.00 2003/11/26 19:43:38 jcdutton Exp $
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sched.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <ctype.h>
#include <byteswap.h>

#define ALSA_PCM_NEW_HW_PARAMS_API
#define ALSA_PCM_NEW_SW_PARAMS_API
#include <alsa/asoundlib.h>
#include <sys/time.h>
#include <math.h>
#include "aconfig.h"
#include "gettext.h"
#include "version.h"

#ifdef ENABLE_NLS
#include <locale.h>
#endif

#define MAX_CHANNELS	16

#if __BYTE_ORDER == __LITTLE_ENDIAN
#define COMPOSE_ID(a,b,c,d)	((a) | ((b)<<8) | ((c)<<16) | ((d)<<24))
#define LE_SHORT(v)		(v)
#define LE_INT(v)		(v)
#define BE_SHORT(v)		bswap_16(v)
#define BE_INT(v)		bswap_32(v)
#else /* __BIG_ENDIAN */
#define COMPOSE_ID(a,b,c,d)	((d) | ((c)<<8) | ((b)<<16) | ((a)<<24))
#define LE_SHORT(v)		bswap_16(v)
#define LE_INT(v)		bswap_32(v)
#define BE_SHORT(v)		(v)
#define BE_INT(v)		(v)
#endif

static char              *device      = "default";       /* playback device */
static snd_pcm_format_t   format      = SND_PCM_FORMAT_S16; /* sample format */
static unsigned int       rate        = 48000;	            /* stream rate */
static unsigned int       channels    = 1;	            /* count of channels */
static unsigned int       speaker     = 0;	            /* count of channels */
static unsigned int       buffer_time = 0;	            /* ring buffer length in us */
static unsigned int       period_time = 0;	            /* period time in us */
static unsigned int       nperiods    = 4;                  /* number of periods */
static double             freq        = 440.0;              /* sinusoidal wave frequency in Hz */
static snd_pcm_uframes_t  buffer_size;
static snd_pcm_uframes_t  period_size;
static int debug = 0;

static const int	supported_formats[] = {
  SND_PCM_FORMAT_S8,
  SND_PCM_FORMAT_S16_LE,
  SND_PCM_FORMAT_S16_BE,
  SND_PCM_FORMAT_FLOAT_LE,
  SND_PCM_FORMAT_S32_LE,
  SND_PCM_FORMAT_S32_BE,
  -1
};

static void generate_sine(uint8_t *frames, int count, double *_phase) {
  double phase = *_phase;
  double max_phase = 1.0 / freq;
  double step = 1.0 / (double)rate;
  double res;
  float fres;
  int    chn;
  int32_t  ires;
  int8_t *samp8 = (int8_t*) frames;
  int16_t *samp16 = (int16_t*) frames;
  int32_t *samp32 = (int32_t*) frames;
  float   *samp_f = (float*) frames;

  while (count-- > 0) {
    for(chn=0;chn<(int)channels;chn++) {
      switch (format) {
      case SND_PCM_FORMAT_S8:
				res = (sin((phase * 2 * M_PI) / max_phase - M_PI)) * 0x03fffffff; /* Don't use MAX volume */
				ires = res;
				*samp8++ = ires >> 24;
        break;
      case SND_PCM_FORMAT_S16_LE:
				res = (sin((phase * 2 * M_PI) / max_phase - M_PI)) * 0x03fffffff; /* Don't use MAX volume */
				ires = res;
				*samp16++ = LE_SHORT(ires >> 16);
        break;
      case SND_PCM_FORMAT_S16_BE:
				res = (sin((phase * 2 * M_PI) / max_phase - M_PI)) * 0x03fffffff; /* Don't use MAX volume */
				ires = res;
				*samp16++ = BE_SHORT(ires >> 16);
        break;
      case SND_PCM_FORMAT_FLOAT_LE:
				res = (sin((phase * 2 * M_PI) / max_phase - M_PI)) * 0.75 ; /* Don't use MAX volume */
				fres = res;
				*samp_f++ = fres;
        break;
      case SND_PCM_FORMAT_S32_LE:
				res = (sin((phase * 2 * M_PI) / max_phase - M_PI)) * 0x03fffffff; /* Don't use MAX volume */
				ires = res;
				*samp32++ = LE_INT(ires);
        break;
      case SND_PCM_FORMAT_S32_BE:
				res = (sin((phase * 2 * M_PI) / max_phase - M_PI)) * 0x03fffffff; /* Don't use MAX volume */
				ires = res;
				*samp32++ = BE_INT(ires);
        break;
      default:
        ;
      }
    }

    phase += step;
    if (phase >= max_phase)
      phase -= max_phase;
  }

  *_phase = phase;
}

static int set_hwparams(snd_pcm_t *handle, snd_pcm_hw_params_t *params, snd_pcm_access_t access) {
  unsigned int rrate;
  int          err;
  snd_pcm_uframes_t     period_size_min;
  snd_pcm_uframes_t     period_size_max;
  snd_pcm_uframes_t     buffer_size_min;
  snd_pcm_uframes_t     buffer_size_max;

  /* choose all parameters */
  err = snd_pcm_hw_params_any(handle, params);
  if (err < 0) {
    fprintf(stderr, _("Broken configuration for playback: no configurations available: %s\n"), snd_strerror(err));
    return err;
  }

  /* set the interleaved read/write format */
  err = snd_pcm_hw_params_set_access(handle, params, access);
  if (err < 0) {
    fprintf(stderr, _("Access type not available for playback: %s\n"), snd_strerror(err));
    return err;
  }

  /* set the sample format */
  err = snd_pcm_hw_params_set_format(handle, params, format);
  if (err < 0) {
    fprintf(stderr, _("Sample format not available for playback: %s\n"), snd_strerror(err));
    return err;
  }

  /* set the count of channels */
  err = snd_pcm_hw_params_set_channels(handle, params, channels);
  if (err < 0) {
    fprintf(stderr, _("Channels count (%i) not available for playbacks: %s\n"), channels, snd_strerror(err));
    return err;
  }

  /* set the stream rate */
  rrate = rate;
  err = snd_pcm_hw_params_set_rate(handle, params, rate, 0);
  if (err < 0) {
    fprintf(stderr, _("Rate %iHz not available for playback: %s\n"), rate, snd_strerror(err));
    return err;
  }

  if (rrate != rate) {
    fprintf(stderr, _("Rate doesn't match (requested %iHz, get %iHz, err %d)\n"), rate, rrate, err);
    return -EINVAL;
  }

	if (debug)
		printf(_("Rate set to %iHz (requested %iHz)\n"), rrate, rate);
  /* set the buffer time */
  err = snd_pcm_hw_params_get_buffer_size_min(params, &buffer_size_min);
  err = snd_pcm_hw_params_get_buffer_size_max(params, &buffer_size_max);
  err = snd_pcm_hw_params_get_period_size_min(params, &period_size_min, NULL);
  err = snd_pcm_hw_params_get_period_size_max(params, &period_size_max, NULL);
	if (debug) {
		printf(_("Buffer size range from %lu to %lu\n"),buffer_size_min, buffer_size_max);
		printf(_("Period size range from %lu to %lu\n"),period_size_min, period_size_max);
	}
  if (period_time > 0) {
		if (debug)
			printf(_("Requested period time %u us\n"), period_time);
    err = snd_pcm_hw_params_set_period_time_near(handle, params, &period_time, NULL);
    if (err < 0) {
      fprintf(stderr, _("Unable to set period time %u us for playback: %s\n"),
	     period_time, snd_strerror(err));
      return err;
    }
  }
  if (buffer_time > 0) {
		if (debug)
			printf(_("Requested buffer time %u us\n"), buffer_time);
    err = snd_pcm_hw_params_set_buffer_time_near(handle, params, &buffer_time, NULL);
    if (err < 0) {
      fprintf(stderr, _("Unable to set buffer time %u us for playback: %s\n"),
	     buffer_time, snd_strerror(err));
      return err;
    }
  }
  if (! buffer_time && ! period_time) {
    buffer_size = buffer_size_max;
    if (! period_time)
      buffer_size = (buffer_size / nperiods) * nperiods;
		if (debug)
			printf(_("Using max buffer size %lu\n"), buffer_size);
    err = snd_pcm_hw_params_set_buffer_size_near(handle, params, &buffer_size);
    if (err < 0) {
      fprintf(stderr, _("Unable to set buffer size %lu for playback: %s\n"),
	     buffer_size, snd_strerror(err));
      return err;
    }
  }
  if (! buffer_time || ! period_time) {
		if (debug)
			printf(_("Periods = %u\n"), nperiods);
    err = snd_pcm_hw_params_set_periods_near(handle, params, &nperiods, NULL);
    if (err < 0) {
      fprintf(stderr, _("Unable to set nperiods %u for playback: %s\n"),
	     nperiods, snd_strerror(err));
      return err;
    }
  }

  /* write the parameters to device */
  err = snd_pcm_hw_params(handle, params);
  if (err < 0) {
    fprintf(stderr, _("Unable to set hw params for playback: %s\n"), snd_strerror(err));
    return err;
  }

  snd_pcm_hw_params_get_buffer_size(params, &buffer_size);
  snd_pcm_hw_params_get_period_size(params, &period_size, NULL);
	if (debug) {
		printf(_("was set period_size = %lu\n"),period_size);
		printf(_("was set buffer_size = %lu\n"),buffer_size);
	}
  if (2*period_size > buffer_size) {
    fprintf(stderr, _("buffer to small, could not use\n"));
    return -EINVAL;
  }

  return 0;
}

static int set_swparams(snd_pcm_t *handle, snd_pcm_sw_params_t *swparams) {
  int err;

  /* get the current swparams */
  err = snd_pcm_sw_params_current(handle, swparams);
  if (err < 0) {
    fprintf(stderr, _("Unable to determine current swparams for playback: %s\n"), snd_strerror(err));
    return err;
  }

  /* start the transfer when a buffer is full */
  err = snd_pcm_sw_params_set_start_threshold(handle, swparams, buffer_size);
  if (err < 0) {
    fprintf(stderr, _("Unable to set start threshold mode for playback: %s\n"), snd_strerror(err));
    return err;
  }

  /* allow the transfer when at least period_size frames can be processed */
  err = snd_pcm_sw_params_set_avail_min(handle, swparams, period_size);
  if (err < 0) {
    fprintf(stderr, _("Unable to set avail min for playback: %s\n"), snd_strerror(err));
    return err;
  }

  /* write the parameters to the playback device */
  err = snd_pcm_sw_params(handle, swparams);
  if (err < 0) {
    fprintf(stderr, _("Unable to set sw params for playback: %s\n"), snd_strerror(err));
    return err;
  }

  return 0;
}

/*
 *   Underrun and suspend recovery
 */

static int xrun_recovery(snd_pcm_t *handle, int err) {
  if (err == -EPIPE) {	/* under-run */
    err = snd_pcm_prepare(handle);
    if (err < 0)
      fprintf(stderr, _("Can't recovery from underrun, prepare failed: %s\n"), snd_strerror(err));
    return 0;
  } 
  else if (err == -ESTRPIPE) {

    while ((err = snd_pcm_resume(handle)) == -EAGAIN)
      sleep(1);	/* wait until the suspend flag is released */

    if (err < 0) {
      err = snd_pcm_prepare(handle);
      if (err < 0)
        fprintf(stderr, _("Can't recovery from suspend, prepare failed: %s\n"), snd_strerror(err));
    }

    return 0;
  }

  return err;
}

/*
 *   Transfer method - write only
 */

static int write_buffer(snd_pcm_t *handle, uint8_t *ptr, int cptr)
{
  int err;

  while (cptr > 0) {

    err = snd_pcm_writei(handle, ptr, cptr);

    if (err == -EAGAIN)
      continue;

    if (err < 0) {
      fprintf(stderr, _("Write error: %d,%s\n"), err, snd_strerror(err));
      if (xrun_recovery(handle, err) < 0) {
	fprintf(stderr, _("xrun_recovery failed: %d,%s\n"), err, snd_strerror(err));
	return -1;
      }
      break;	/* skip one period */
    }

    ptr += snd_pcm_frames_to_bytes(handle, err);
    cptr -= err;
  }
  return 0;
}

static int write_loop(snd_pcm_t *handle, int periods, uint8_t *frames)
{
  double phase = 0;
  int    err, n;

  fflush(stdout);

	if (debug)
		printf(_("write_loop: periods = %d\n"), periods);

  if (periods <= 0)
    periods = 1;

  for(n = 0; n < periods; n++) {
		generate_sine(frames, period_size, &phase);

    if ((err = write_buffer(handle, frames, period_size)) < 0)
      return err;
  }
  if (buffer_size > n * period_size) {
    snd_pcm_drain(handle);
    snd_pcm_prepare(handle);
  }
  return 0;
}

static void help(void)
{
  const int *fmt;

  printf(
	 _("Usage: speaker-test [OPTION]... \n"
	   "-h,--help	help\n"
	   "-e,--device	playback device\n"
	   "-r,--rate	stream rate in Hz\n"
	   "-c,--channels	count of channels in stream\n"
	   "-f,--frequency	sine wave frequency in Hz\n"
	   "-F,--format	sample format\n"
	   "-b,--buffer	ring buffer size in us\n"
	   "-p,--period	period size in us\n"
	   "-P,--nperiods	number of periods\n"
		 "-l,--length	beep for N milliseconds\n"
	   "-s,--speaker	single speaker test. Values 1=Left, 2=right, etc\n"
		 "-d,--debug	enable debug mode\n"
	   "\n"));
  printf(_("Recognized sample formats are:"));
  for (fmt = supported_formats; *fmt >= 0; fmt++) {
    const char *s = snd_pcm_format_name(*fmt);
    if (s)
      printf(" %s", s);
  }

  printf("\n\n");
}

int main(int argc, char *argv[]) {
  snd_pcm_t            *handle;
  int                   err, morehelp;
  snd_pcm_hw_params_t  *hwparams;
  snd_pcm_sw_params_t  *swparams;
  uint8_t              *frames;
  const int	       *fmt;
	int format_nbytes=0;
	int beep_length=1000;

  static const struct option long_option[] = {
    {"help",      0, NULL, 'h'},
    {"device",    1, NULL, 'e'},
    {"rate",      1, NULL, 'r'},
    {"channels",  1, NULL, 'c'},
    {"frequency", 1, NULL, 'f'},
    {"format",    1, NULL, 'F'},
    {"buffer",    1, NULL, 'b'},
    {"period",    1, NULL, 'p'},
    {"nperiods",  1, NULL, 'P'},
		{"length",    1, NULL, 'l'},
    {"speaker",   1, NULL, 's'},
    {"debug",	    0, NULL, 'd'},
    {NULL,        0, NULL, 0  },
  };

#ifdef ENABLE_NLS
  setlocale(LC_ALL, "");
  textdomain(PACKAGE);
#endif

  snd_pcm_hw_params_alloca(&hwparams);
  snd_pcm_sw_params_alloca(&swparams);
 
  morehelp = 0;

	if (debug)
		printf("\nspeaker-test %s\n\n", SND_UTIL_VERSION_STR);
  while (1) {
    int c;
    
    if ((c = getopt_long(argc, argv, "he:r:c:f:F:b:p:P:l:t:s:d", long_option, NULL)) < 0)
      break;
    
    switch (c) {
    case 'h':
      morehelp++;
      break;
    case 'e':
      device = strdup(optarg);
      break;
    case 'F':
      format = snd_pcm_format_value(optarg);
      for (fmt = supported_formats; *fmt >= 0; fmt++)
        if (*fmt == format)
          break;
      if (*fmt < 0) {
        fprintf(stderr, "Format %s is not supported...\n", snd_pcm_format_name(format));
        exit(EXIT_FAILURE);
      }
      break;
    case 'r':
      rate = atoi(optarg);
      rate = rate < 4000 ? 4000 : rate;
      rate = rate > 196000 ? 196000 : rate;
      break;
    case 'c':
      channels = atoi(optarg);
      channels = channels < 1 ? 1 : channels;
      channels = channels > 1024 ? 1024 : channels;
      break;
    case 'f':
      freq = atof(optarg);
      freq = freq < 30.0 ? 30.0 : freq;
      freq = freq > 5000.0 ? 5000.0 : freq;
      break;
    case 'b':
      buffer_time = atoi(optarg);
      buffer_time = buffer_time > 1000000 ? 1000000 : buffer_time;
      break;
    case 'p':
      period_time = atoi(optarg);
      period_time = period_time > 1000000 ? 1000000 : period_time;
      break;
    case 'P':
      nperiods = atoi(optarg);
      if (nperiods < 2 || nperiods > 1024) {
	fprintf(stderr, _("Invalid number of periods %d\n"), nperiods);
	exit(1);
      }
      break;
		case 'l':
			beep_length = atoi(optarg);
			break;
    case 's':
      speaker = atoi(optarg);
      speaker = speaker < 1 ? 0 : speaker;
      speaker = speaker > channels ? 0 : speaker;
      if (speaker==0) {
        fprintf(stderr, _("Invalid parameter for -s option.\n"));
        exit(EXIT_FAILURE);
      }  
      break;
    case 'd':
      debug = 1;
      break;
    default:
      fprintf(stderr, _("Unknown option '%c'\n"), c);
      exit(EXIT_FAILURE);
      break;
    }
  }

  if (morehelp) {
    help();
    exit(EXIT_SUCCESS);
  }

	if (debug) {
		printf(_("Playback device is %s\n"), device);
		printf(_("Stream parameters are %iHz, %s, %i channels\n"), rate, snd_pcm_format_name(format), channels);
		printf(_("Sine wave rate is %.4fHz\n"), freq);
	}

  if ((err = snd_pcm_open(&handle, device, SND_PCM_STREAM_PLAYBACK, 0)) < 0) {
    printf(_("Playback open error: %d,%s\n"), err,snd_strerror(err));
    exit(EXIT_FAILURE);
  }

  if ((err = set_hwparams(handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED)) < 0) {
    printf(_("Setting of hwparams failed: %s\n"), snd_strerror(err));
    snd_pcm_close(handle);
    exit(EXIT_FAILURE);
  }
  if ((err = set_swparams(handle, swparams)) < 0) {
    printf(_("Setting of swparams failed: %s\n"), snd_strerror(err));
    snd_pcm_close(handle);
    exit(EXIT_FAILURE);
  }
  if (debug) {
    snd_output_t *log;
    err = snd_output_stdio_attach(&log, stderr, 0);
    if (err >= 0) {
      snd_pcm_dump(handle, log);
      snd_output_close(log);
    }
  }

  frames = malloc(snd_pcm_frames_to_bytes(handle, period_size));
  
  if (frames == NULL) {
    fprintf(stderr, _("No enough memory\n"));
    exit(EXIT_FAILURE);
  }

	switch (format) {
		case SND_PCM_FORMAT_S8:
			format_nbytes = 1;
			break;
		case SND_PCM_FORMAT_S16_LE:
		case SND_PCM_FORMAT_S16_BE:
			format_nbytes = 2;
			break;
		case SND_PCM_FORMAT_FLOAT_LE:
			format_nbytes = sizeof(float);
			break;
		case SND_PCM_FORMAT_S32_LE:
		case SND_PCM_FORMAT_S32_BE:
			format_nbytes = 4;
			break;
		default:
			;
	}

	if (debug) {
		printf(_("format_nbytes = %d\n"), format_nbytes);
		printf(_("beep_length = %d\n"), beep_length);
	}

	err = write_loop(handle, rate/period_size*format_nbytes*(beep_length/1000.0), frames);

	if (err < 0) {
		fprintf(stderr, _("Transfer failed: %s\n"), snd_strerror(err));
	}


  free(frames);
  snd_pcm_close(handle);

  exit(EXIT_SUCCESS);
}
