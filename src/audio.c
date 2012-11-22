#include <alsa/asoundlib.h>

snd_pcm_t *g_pcm_handle = NULL;
snd_pcm_hw_params_t *g_hwparams = NULL;

int audio_init(void)
{
	snd_pcm_t *pcm_handle;
	snd_pcm_stream_t stream = SND_PCM_STREAM_PLAYBACK;
	snd_pcm_hw_params_t *hwparams;

	char *pcm_name;

	pcm_name = strdup("plughw:0,0");
	snd_pcm_hw_params_alloca(&hwparams);

	if (snd_pcm_open(&pcm_handle, pcm_name, stream, 0) < 0) {
		fprintf(stderr, "Unable to open PCM device %s\n", pcm_name);
		goto error;
	}

	if (snd_pcm_hw_params_set_access(pcm_handle, hwparams, SND_PCM_ACCESS_RW_INTERLEAVED) < 0) {
		fprintf(stderr, "Unable to set interleaved format\n");
		goto error;
	}

	g_pcm_handle = pcm_handle;
	g_hwparams = hwparams;

	return 0;
error:
	snd_pcm_close(g_pcm_handle);
	free(pcm_name);
	return -1;
}

void audio_destroy(void)
{
	snd_pcm_close(g_pcm_handle);
}

int audio_deliver(int rate, int channels, const void *frames, int num_frames)
{
	int exact_rate;
	int dir;
	int periods = 2;
	snd_pcm_uframes_t periodsize = channels * num_frames;

	if (snd_pcm_hw_params_set_format(g_pcm_handle, g_hwparams, SND_PCM_FORMAT_S16_LE) < 0) {
		fprintf(stderr, "Failed to set format\n");
		return -1;
	}

	exact_rate = rate;
	if (snd_pcm_hw_params_set_rate_near(g_pcm_handle, g_hwparams, &exact_rate, 0) < 0) {
		fprintf(stderr, "Failed to set rate\n");
		return -1;
	}

	if (rate != exact_rate) {
		fprintf(stderr, "Failed to set rate %d Hz, using %d Hz instead\n", rate, exact_rate);
		return -1;
	}

	if (snd_pcm_hw_params_set_channels(g_pcm_handle, g_hwparams, channels) < 0) {
		fprintf(stderr, "Failed to set channels\n");
		return -1;
	}
	
	/*
	if (snd_pcm_hw_params_set_periods(g_pcm_handle, g_hwparams, num_frames, 0) < 0) {
		fprintf(stderr, "Failed to set periods\n");
		return -1;
	}
	*/


	if (snd_pcm_hw_params_set_period_size(g_pcm_handle, g_hwparams, periodsize, 0) < 0) {
		fprintf(stderr, "Failed to set period size\n");
		return -1;
	}

	if (snd_pcm_hw_params_set_buffer_size(g_pcm_handle, g_hwparams, periodsize * 4) < 0) {
		fprintf(stderr, "Failed to set buffer size\n");
		return -1;
	}
	return 0;
	
}
