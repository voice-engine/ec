/*-*- linux-c -*-*/


#include <stdio.h>

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
	int ret = 0;

	ret = system("systemctl is-active ec 1>/dev/null");
	if (ret != 0)
	{
		return 0;
	}

	*dst = NULL;
	ret = snd_config_hook_load(root, config, dst, private_data);


	return ret;
}

SND_DLSYM_BUILD_VERSION(conf_ec_hook_load_if_running,
			SND_CONFIG_DLSYM_VERSION_HOOK);

