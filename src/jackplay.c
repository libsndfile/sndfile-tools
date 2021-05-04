/*
** Copyright (C) 2007-2016 Erik de Castro Lopo <erikd@mega-nerd.com>
** Copyright (C) 2014 Alexander Regueiro <alex@noldorin.com>
** Copyright (C) 2013 elboulangero <elboulangero@gmail.com>
** Copyright (C) 2007 Jonatan Liljedahl <lijon@kymatica.com>
**
** This program is free software ; you can redistribute it and/or modify
** it under the terms of the GNU General Public License as published by
** the Free Software Foundation ; either version 2 of the License, or
** (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY ; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program ; if not, write to the Free Software
** Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

#include "src/config.h"

#include <stdbool.h>
#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <getopt.h>
#include <libgen.h>
#include <stdlib.h>
#include <string.h>

#include <math.h>
#include <pthread.h>
#include <signal.h>

#include <jack/jack.h>
#include <jack/ringbuffer.h>

#include <sndfile.h>

#include <src/common.h>

#define RB_SIZE		(1 << 16)
#define SAMPLE_SIZE (sizeof (jack_default_audio_sample_t))

#define	NOT(x)	(! (x))

typedef struct
{	jack_client_t *client ;
	jack_ringbuffer_t *ringbuf ;
	jack_nframes_t pos ;
	jack_default_audio_sample_t ** outs ;
	jack_port_t ** output_port ;

	SNDFILE *sndfile ;

	unsigned int channels ;
	unsigned int samplerate ;

	volatile int can_process ;
	volatile int read_done ;

	volatile unsigned int loop_count ;
	volatile unsigned int current_loop ;
} thread_info_t ;

static pthread_mutex_t disk_thread_lock = PTHREAD_MUTEX_INITIALIZER ;
static pthread_cond_t data_ready = PTHREAD_COND_INITIALIZER ;
static volatile int play_done = 0 ;

static void
close_signal_handler (int sig)
{	(void) sig ;
	play_done = 1 ;
}

static int
process_callback (jack_nframes_t nframes, void * arg)
{
	thread_info_t *info = (thread_info_t *) arg ;
	jack_default_audio_sample_t buf [info->channels] ;
	unsigned i, n ;

	for (n = 0 ; n < info->channels ; n++)
		info->outs [n] = jack_port_get_buffer (info->output_port [n], nframes) ;

	if (play_done || NOT (info->can_process))
	{	/* output silence */
		for (n = 0 ; n < info->channels ; n++)
			memset (info->outs [n], 0, sizeof (float) * nframes) ;
		return 0 ;
		} ;

	for (i = 0 ; i < nframes ; i++)
	{	size_t read_count ;

		/* Read one frame of audio. */
		read_count = jack_ringbuffer_read (info->ringbuf, (void *) buf, SAMPLE_SIZE * info->channels) ;
		if (read_count == 0 && info->read_done)
		{	/* File is done, so stop the main loop. */
			play_done = 1 ;
			/* Silence the rest of the audio buffer. */
			for (n = 0 ; n < info->channels ; n++)
				info->outs [n][i] = 0.0f ;
			return 0 ;
			} ;

		/* Update play-position counter. */
		info->pos += read_count / (SAMPLE_SIZE * info->channels) ;

		/* Output each channel of the frame. */
		for (n = 0 ; n < info->channels ; n++)
			info->outs [n][i] = buf [n] ;
		} ;

	/* Wake up the disk thread to read more data. */
	if (pthread_mutex_trylock (&disk_thread_lock) == 0)
	{	pthread_cond_signal (&data_ready) ;
		pthread_mutex_unlock (&disk_thread_lock) ;
		} ;

	return 0 ;
} /* process_callback */


/*
** This seemingly needless memcpy-ing is needed because the `buf` field of
** `jack_ringbuffer_data_t` is a `char*` and may not have the correct alignment
** for the `float` data read from the file.
*/
static sf_count_t
fill_jack_buffer (thread_info_t *info, jack_ringbuffer_data_t * vec)
{	sf_count_t frame_count = vec->len / sizeof (float) / info->channels ;
	sf_count_t buffer_frames ;
	static float buf [1 << 16] ;

	buffer_frames = ARRAY_LEN (buf) / info->channels ;
	frame_count = frame_count < buffer_frames ? frame_count : buffer_frames ;

	frame_count = sf_readf_float (info->sndfile, buf, frame_count) ;
	memcpy (vec->buf, buf, frame_count * info->channels * sizeof (buf [0])) ;

	return frame_count ;
} /* fill_jack_buffer */

static void *
disk_thread (void *arg)
{	thread_info_t *info = (thread_info_t *) arg ;
	sf_count_t read_frames ;
	jack_ringbuffer_data_t vec [2] ;
	size_t bytes_per_frame = SAMPLE_SIZE * info->channels ;

	pthread_setcanceltype (PTHREAD_CANCEL_ASYNCHRONOUS, NULL) ;
	pthread_mutex_lock (&disk_thread_lock) ;

	while (NOT (play_done))
	{	/* `vec` is *always* a two element array. See:
		** http://jackaudio.org/files/docs/html/ringbuffer_8h.html
		*/
		jack_ringbuffer_get_write_vector (info->ringbuf, vec) ;

		read_frames = 0 ;

		if (vec [0].len)
		{	/* Fill the first part of the ringbuffer. */
			read_frames = fill_jack_buffer (info, vec) ;

			if (vec [1].len)
			{	/* Fill the second part of the ringbuffer? */
				read_frames += fill_jack_buffer (info, vec + 1) ;
				} ;
			} ;

		if (read_frames == 0)
		{	info->current_loop ++ ;

			if (info->loop_count >= 1 && info->current_loop >= info->loop_count)
				break ; /* end of file? */

			sf_seek (info->sndfile, 0, SEEK_SET) ;
			continue ;
			}

		jack_ringbuffer_write_advance (info->ringbuf, read_frames * bytes_per_frame) ;

		/* Tell process_callback that we've filled the ringbuffer. */
		info->can_process = 1 ;

		/* Wait for the process_callback thread to wake us up. */
		pthread_cond_wait (&data_ready, &disk_thread_lock) ;
		} ;

	/* Tell that we're done reading the file. */
	info->read_done = 1 ;
	pthread_mutex_unlock (&disk_thread_lock) ;

	return NULL ;
} /* disk_thread */

static void
jack_shutdown (void *arg)
{	(void) arg ;
	exit (1) ;
} /* jack_shutdown */

static inline void
print_time (jack_nframes_t pos, int jack_sr)
{	float sec = pos / (1.0 * jack_sr) ;
	int min = sec / 60.0 ;
	fprintf (stderr, "%02d:%05.2f", min, fmod (sec, 60.0)) ;
} /* print_time */

static inline void
print_status (const thread_info_t * info)
{
	if (info->loop_count == 0)
		fprintf (stderr, "\r-> %6d     ", info->current_loop) ;
	else if (info->loop_count > 1)
		fprintf (stderr, "\r-> %6d/%d     ", info->current_loop, info->loop_count) ;
	else
		fprintf (stderr, "\r->     ") ;

	print_time (info->pos, info->samplerate) ;
	fflush (stdout) ;
} /* print_status */

static void
usage_exit (char * argv0, int status)
{
	printf ("\n"
		"Usage : %s [options] <input sound file>\n"
		"\n"
		"  Where [options] is one of:\n"
		"\n"
		" -w   --wait[=<port>]      : Wait for input before starting playback; optionally auto-connect to <port> using Jack.\n"
		" -a   --autoconnect=<port> : Auto-connect to <port> using Jack.\n"
		" -l   --loop=<count>       : Loop the file <count> times (0 for infinite).\n"
		" -h   --help               : Show this help message.\n"
		"\n"
		"Using %s.\n"
		"\n",
		basename (argv0), sf_version_string ()) ;
	exit (status) ;
} /* usage_exit */

static struct option const long_options [] =
{
	{ "wait", optional_argument, NULL, 'w' } ,
	{ "autoconnect", required_argument, NULL, 'a' } ,
	{ "loop", required_argument, NULL, 'l' } ,
	{ "help", no_argument, NULL, 'h' } ,
	{ NULL, 0, NULL, 0 }
} ;

int
main (int argc, char * argv [])
{	pthread_t thread_id ;
	SNDFILE *sndfile ;
	SF_INFO sfinfo = { } ;
	const char * filename ;
	jack_client_t *client ;
	jack_status_t status = 0 ;
	char * auto_connect_str = "system:playback_%d" ;
	bool wait_before_play = false ;
	int i, jack_sr, loop_count = 1 ;
	int c ;

	/* Parse options */
	while ((c = getopt_long (argc, argv,
				"w::"	/* --wait        */
				"a::"	/* --autoconnect */
				"l:"	/* --loop        */
				"h",	/* --help        */
				long_options, NULL)) != EOF)
	{	if (optarg != NULL && optarg [0] == '=')
		{	optarg++ ;
			}
		switch (c)
		{	case 'w' :
				wait_before_play = true ;
				auto_connect_str = optarg ;
				break ;
			case 'a' :
				auto_connect_str = optarg ;
				break ;
			case 'l' :
				loop_count = strtol (optarg, NULL, 10) ;
				break ;
			case 'h' :
				usage_exit (argv [0], EXIT_SUCCESS) ;
				break ;
			default :
				usage_exit (argv [0], EXIT_FAILURE) ;
			} ;
		}

	if (argc - optind != 1)
		usage_exit (argv [0], EXIT_FAILURE) ;

	filename = argv [optind] ;

	/* Create jack client */
	if ((client = jack_client_open ("jackplay", JackNullOption | JackNoStartServer, &status)) == 0)
	{	if (status & JackServerFailed)
			fprintf (stderr, "Unable to connect to JACK server\n") ;
		else
			fprintf (stderr, "jack_client_open () failed, status = 0x%2.0x\n", status) ;

		exit (1) ;
		} ;

	if (status & JackServerStarted)
		fprintf (stderr, "JACK server started\n") ;

	if (status & JackNameNotUnique)
	{	const char * client_name = jack_get_client_name (client) ;
		fprintf (stderr, "Unique name `%s' assigned\n", client_name) ;
		} ;

	/* Open the soundfile. */
	sndfile = sf_open (filename, SFM_READ, &sfinfo) ;
	if (sndfile == NULL)
	{	fprintf (stderr, "Could not open soundfile '%s'\n", filename) ;
		return 1 ;
		} ;

	fprintf (stderr, "Channels    : %d\nSample rate : %d Hz\nDuration    : ", sfinfo.channels, sfinfo.samplerate) ;
	print_time (loop_count * sfinfo.frames, sfinfo.samplerate) ;
	fprintf (stderr, "\n") ;

	if (loop_count < 1)
		fprintf (stderr, "Loop count  : infinite\n") ;
	else if (loop_count > 1)
		fprintf (stderr, "Loop count  : %d\n", loop_count) ;

	jack_sr = jack_get_sample_rate (client) ;

	if (sfinfo.samplerate != jack_sr)
		fprintf (stderr, "Warning: samplerate of soundfile (%d Hz) does not match jack server (%d Hz).\n", sfinfo.samplerate, jack_sr) ;

	struct sigaction sig = {
		.sa_handler = close_signal_handler ,
		.sa_flags = SA_RESTART } ;

	sigemptyset (&sig.sa_mask) ;
	sigaction (SIGINT, &sig, NULL) ;
	sigaction (SIGTERM, &sig, NULL) ;

	thread_info_t info = {
		.sndfile = sndfile ,
		.channels = sfinfo.channels ,
		.samplerate = jack_sr ,
		.client = client ,
		.loop_count = loop_count ,
		/* Allocate output ports. */
		.output_port = calloc (sfinfo.channels, sizeof (jack_port_t *)) ,
		.outs = calloc (sfinfo.channels, sizeof (jack_default_audio_sample_t *)) } ;

	for (i = 0 ; i < sfinfo.channels ; i++)
	{	char name [16] ;

		snprintf (name, sizeof (name), "out_%d", i + 1) ;
		info.output_port [i] = jack_port_register (client, name, JACK_DEFAULT_AUDIO_TYPE, JackPortIsOutput, 0) ;
		} ;

	/* Allocate and clear ringbuffer. */
	info.ringbuf = jack_ringbuffer_create (sizeof (jack_default_audio_sample_t) * RB_SIZE) ;
	memset (info.ringbuf->buf, 0, info.ringbuf->size) ;

	/* Set up callbacks. */
	jack_set_process_callback (client, process_callback, &info) ;
	jack_on_shutdown (client, jack_shutdown, 0) ;

	/* Activate client. */
	if (jack_activate (client))
	{	fprintf (stderr, "Cannot activate client.\n") ;
		return 1 ;
		} ;

	if (auto_connect_str != NULL)
	{	/* Auto-connect all channels. */
		for (i = 0 ; i < sfinfo.channels ; i++)
		{	char name [64] ;

			snprintf (name, sizeof (name), auto_connect_str, i + 1) ;

			if (jack_connect (client, jack_port_name (info.output_port [i]), name))
				fprintf (stderr, "Cannot connect output port %d (%s).\n", i, name) ;
			} ;
		}

	if (wait_before_play)
	{	/* Wait for key press before playing. */
		printf ("Press <ENTER> key to start playing...") ;
		getchar () ;
		}

	/* Start the disk thread. */
	pthread_create (&thread_id, NULL, disk_thread, &info) ;

	/* Sit in a loop, displaying the current play position. */
	while (NOT (play_done))
	{	print_status (&info) ;
		usleep (10000) ;
		} ;

	jack_deactivate (client) ;

	pthread_cond_signal (&data_ready) ;
	pthread_join (thread_id, NULL) ;

	print_status (&info) ;

	/* Clean up. */
	for (i = 0 ; i < sfinfo.channels ; i++)
		jack_port_unregister (client, info.output_port [i]) ;

	jack_ringbuffer_free (info.ringbuf) ;
	jack_client_close (client) ;

	free (info.output_port) ;
	free (info.outs) ;

	sf_close (sndfile) ;

	puts ("") ;

	return 0 ;
} /* main */
