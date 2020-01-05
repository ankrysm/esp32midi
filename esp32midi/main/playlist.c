/*
 * playlist.c
 *
 *  Created on: 1 Jan 2020
 *      Author: ankrysm
 */

#include "local.h"

static const char *TAG = "playlist";

static T_PLAYLIST_ENTRY *playlist = NULL;
int playlist_flags = flags_none;

void freeplaylist() {
	if ( ! playlist) {
		return;
	}

	T_PLAYLIST_ENTRY *d, *dnxt;
	for ( d = playlist; d; d = dnxt)  {
		dnxt = d->nxt;
		free(d->path);
	}
	playlist = NULL;
}

void restart_playlist() {
	if ( ! playlist) {
		return;
	}

	for ( T_PLAYLIST_ENTRY *d = playlist; d; d = d->nxt)  {
		d->play_status = play_status_new;
	}

}


void dump_playlist() {
	if ( !playlist) {
		ESP_LOGI(TAG, "%s: no_playlist", __func__);
		return;
	}

	ESP_LOGI(TAG, "%s: flags: %s%s%s", __func__,
			(playlist_flags & flags_repeat ? "REPEAT ":""),
			(playlist_flags & flags_shuffle ? "SHUFFLE ":""),
			(playlist_flags & flags_play_all ? "PLAY_ALL ":"")
	);

	for ( T_PLAYLIST_ENTRY *d=playlist; d; d=d->nxt) {
		ESP_LOGI(TAG, "%s: %s %ld %d", __func__,
				d->path, d->sortkey, d->play_status);
	}

	ESP_LOGI(TAG, "%s: end of playlist", __func__);

}


/**
 * build a new playlist, old playlist will be free'd
 */
esp_err_t build_playlist(const char *dirpath) {
	freeplaylist();

	char entrypath[256+1];

    struct dirent *entry;
    struct stat entry_stat;

    int  shuffle = playlist_flags & flags_shuffle ? 1 : 0;

    int midifilecnt = 0;
    DIR *dir = opendir(dirpath);
    if (!dir) {
        ESP_LOGE(TAG, "%s: handle_play_random_midifile; Failed to stat dir : %s", __func__, dirpath);
        return ESP_FAIL;
    }

    // first reading of dir - discover number of midi files
    while ((entry = readdir(dir)) != NULL) {
    	if (entry->d_type == DT_DIR)
    		continue;
        snprintf(entrypath, sizeof(entrypath),"%s/%s", dirpath, entry->d_name);
        if (stat(entrypath, &entry_stat) == -1) {
            ESP_LOGE(TAG, "%s: %s: Failed to stat %s", __func__, entrypath, entry->d_name);
            continue;
        }
        if (! IS_FILE_EXT(entrypath, ".mid")) {
    		//ESP_LOGI(TAG, "%s: %s is not a midifile", __func__, entrypath);
    		continue;
        }
		ESP_LOGI(TAG, "%s[%d]: %s added", __func__, ++midifilecnt, entrypath);

		T_PLAYLIST_ENTRY *d = calloc(1, sizeof(T_PLAYLIST_ENTRY));
		d->path = strdup(entrypath);
		d->sortkey = shuffle ? random() : midifilecnt;
		d->play_status = play_status_new;

		if ( playlist) {
			T_PLAYLIST_ENTRY *dd;
			for (dd= playlist; dd->nxt; dd=dd->nxt) {}
			dd->nxt = d;
		} else {
			playlist = d;
		}
    }
    closedir(dir);
    return ESP_OK;
}

/**
 * switch between shuffle and linear
 * play-status will be kept
 */
void update_playlist() {
	if (!playlist) {
		return;
	}

	int  shuffle = playlist_flags & flags_shuffle ? 1 : 0;
	int cnt = 0;
	for ( T_PLAYLIST_ENTRY *d=playlist; d; d=d->nxt) {
		d->sortkey = shuffle ? random() : cnt++;
	}


}
T_PLAYLIST_ENTRY *nxtentry() {
	if ( !playlist) {
		return NULL;
	}

	T_PLAYLIST_ENTRY *nxt = NULL;
	long sortkey_min = LONG_MAX;
	for ( T_PLAYLIST_ENTRY *d=playlist; d; d=d->nxt) {
		if ( d->play_status == play_status_actual) {
			// mark actual as played
			d->play_status = play_status_played;
			continue;
		}
		if ( d->play_status != play_status_new ) {
			continue; // actual or already played
		}
		if ( d->sortkey < sortkey_min) {
			nxt = d;
			sortkey_min=d->sortkey;
		}
	}

	if ( nxt ) {
		nxt->play_status = play_status_actual; // marked as actual played
	}
	return nxt;
}


T_PLAYLIST_ENTRY *actualplayedentry() {
	if ( !playlist)
		return NULL;

	for ( T_PLAYLIST_ENTRY *d=playlist; d; d=d->nxt) {
		if ( d->play_status == play_status_actual) {
			return d;
		}
	}
	return NULL;
}

/**
 * central function:
 * get the next filename from the list
 */
char *nxtplaylistentry(int force_repeat) {
	if ( !playlist) {
		return NULL;
	}

	T_PLAYLIST_ENTRY *nxt = nxtentry();
	if ( !nxt ) {
		// all played
		if ( (playlist_flags & flags_repeat) || force_repeat) {
			restart_playlist();
			if ( playlist_flags & flags_shuffle) {
				update_playlist();
			}
			nxt = nxtentry();
			return nxt ? nxt->path : NULL;
		}
		return NULL;
	}
	return nxt->path;
}


int setplaylistposition(const char *filename) {
	if ( !playlist || !filename) {
		return 0;
	}
	T_PLAYLIST_ENTRY *found = NULL;
	for ( T_PLAYLIST_ENTRY *d=playlist; d; d=d->nxt) {
		if ( !strcmp(d->path, filename)) {
			// matches
			found = d;
			break;
		}
	}

	if ( !found) {
		ESP_LOGE(TAG, "%s: %s missing in playlist", __func__, filename);
		return -1;
	}

	T_PLAYLIST_ENTRY *actualplayed = actualplayedentry();
	if ( actualplayed) {
		actualplayed->play_status = play_status_played;
	}

	found->play_status = play_status_actual;
	ESP_LOGI(TAG, "%s: %s is actual played", __func__, filename);

	return 0;
}
