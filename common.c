/*
   Common helper and utility functions

   Copyright (C) 2018        Sebastian Parschauer  <s.parschauer(a)gmx.de>

   This file is part of libscanmem.

   This library is free software: you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published
   by the Free Software Foundation; either version 3 of the License, or
   (at your option) any later version.

   This library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this library.  If not, see <http://www.gnu.org/licenses/>.
   */

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>

#include "common.h"

/* states returned by check_process() */
enum pstate {
	PROC_RUNNING,
	PROC_ERR,  /* error during detection */
	PROC_DEAD,
	PROC_ZOMBIE
};

/*
 * We check if a process is running in /proc directly.
 * Also zombies are detected.
 *
 * Requirements: Linux kernel, mounted /proc
 * Assumption: (pid > 0)  --> Please check your PID before!
 */
static enum pstate check_process(pid_t pid){
	FILE *fp = NULL;
	char *line = NULL;
	size_t alloc_len = 0;
	char status = '\0';
	char path_str[128] = "/proc/";
	int pr_len, path_len = sizeof("/proc/") - 1;

	/* append $pid/status and check if file exists */
	pr_len = sprintf((path_str + path_len), "%d/status", pid);
	if (pr_len <= 0)
		goto err;
	path_len += pr_len;

	fp = fopen(path_str, "r");
	if (!fp) {
		if (errno != ENOENT)
			goto err;
		else
			return PROC_DEAD;
	}

	/* read process status */
	while (getline(&line, &alloc_len, fp) != -1) {
		if (alloc_len <= sizeof("State:\t"))
			continue;
		if (strncmp(line, "State:\t", sizeof("State:\t") - 1) == 0) {
			status = line[sizeof("State:\t") - 1];
			break;
		}
	}
	if (line)
		free(line);
	fclose(fp);

	if (status < 'A' || status > 'Z')
		goto err;

	/* zombies are not running - parent doesn't wait */
	if (status == 'Z' || status == 'X')
		return PROC_ZOMBIE;
	return PROC_RUNNING;
err:
	return PROC_ERR;
}

bool sm_process_is_dead(pid_t pid){
	return (check_process(pid) != PROC_RUNNING);
}

bool sm_add_current_match_to_history(){
	matches_and_old_values_array* matches = sm_globals.matches;
	matches_and_old_values_array* match_copy = malloc(matches->bytes_allocated);

	if (match_copy != NULL) {
		struct history_entry_t* entry = malloc(sizeof(struct history_entry_t));
		memcpy(match_copy, matches, matches->bytes_allocated);
		entry->num_matches = sm_globals.num_matches;
		entry->matches = match_copy;

		if (!TAILQ_EMPTY(&sm_globals.match_history)) {
			/* remove all the ones that would be overwritten first */
			struct history_entry_t* next = TAILQ_NEXT(sm_globals.current, list);
			struct history_entry_t* tmp;

			while(next != NULL) {
				tmp = TAILQ_NEXT(next, list);
				free(next->matches);
				TAILQ_REMOVE(&sm_globals.match_history, next, list);
				free(next);
				next = tmp;
			}
			TAILQ_INSERT_AFTER(&sm_globals.match_history, sm_globals.current, entry, list);
		} else {
			TAILQ_INSERT_TAIL(&sm_globals.match_history, entry, list);
		}

		sm_globals.current = entry;
		sm_globals.history_length++;

		return true;
	}

	return false;
}

