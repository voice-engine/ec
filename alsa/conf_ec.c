/*-*- linux-c -*-*/


#include <stdio.h>
#include <unistd.h>
#include <alsa/asoundlib.h>


/* Not actually part of the alsa api....  */
extern int
snd_config_hook_load(snd_config_t * root, snd_config_t * config,
		     snd_config_t ** dst, snd_config_t * private_data);

int
conf_ec_hook_load_if_running(snd_config_t * root, snd_config_t * config,
				snd_config_t ** dst,
				snd_config_t * private_data)
{
	*dst = NULL;

	if (access("/var/tmp/ec/pid", F_OK) != 0)
	{
		return 0;
	}

	return  snd_config_hook_load(root, config, dst, private_data);
}

SND_DLSYM_BUILD_VERSION(conf_ec_hook_load_if_running,
			SND_CONFIG_DLSYM_VERSION_HOOK);

