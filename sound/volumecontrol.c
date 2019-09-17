// gcc volumecontrol.c -o volumecontrol -lasound
#include <string.h>
#include <alsa/asoundlib.h>
#include <alsa/mixer.h>

int main(int argc, char **argv)
{
    snd_mixer_t *handle;
    snd_mixer_selem_id_t *sid;
    snd_mixer_elem_t *elem;
    int res = 0;
    long min, max, cur_val, one_percent, volume;
    const char *action, *card = "default", *selem_name = "Master";

    if (argc != 2) {
        printf("Usage: %s +/-\n", argv[0]);
        return -1;
    }

    action = argv[1];

    if (strcmp(action, "+") && strcmp(action, "-")) {
        printf("Usage: %s +/-\n", argv[0]);
        return -1;
    }

    res = snd_mixer_open(&handle, 0);

    if (res < 0) {
        printf("error: %s\n", snd_strerror(res));
        goto end;
    }

    res = snd_mixer_attach(handle, card);

    if (res < 0) {
        printf("error: %s\n", snd_strerror(res));
        goto end;
    }

    res = snd_mixer_selem_register(handle, NULL, NULL);

    if (res < 0) {
        printf("error: %s\n", snd_strerror(res));
        goto end;
    }

    res = snd_mixer_load(handle);

    if (res < 0) {
        printf("error: %s\n", snd_strerror(res));
        goto end;
    }

    snd_mixer_selem_id_alloca(&sid);
    snd_mixer_selem_id_set_index(sid, 0);
    snd_mixer_selem_id_set_name(sid, selem_name);

    elem = snd_mixer_find_selem(handle, sid);

    if (!elem) {
        printf("error: %s\n", snd_strerror(res));
        goto end;
    }

    res = snd_mixer_selem_get_playback_volume(elem, SND_MIXER_SCHN_MONO, &cur_val);

    if (res < 0) {
        printf("error: %s\n", snd_strerror(res));
        goto end;
    }

    snd_mixer_selem_get_playback_volume_range(elem, &min, &max);

    one_percent = max / 100;
    volume = !strcmp(action, "+") ? cur_val + (5 * one_percent) : cur_val - (5 * one_percent);

    if (volume > min && volume < max)
        snd_mixer_selem_set_playback_volume_all(elem, volume);

end:
    snd_mixer_close(handle);
    return res;
}