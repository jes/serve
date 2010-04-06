/* Logging stuff for serve

	 By James Stanley

	 Public domain */

#include "serve.h"

FILE *out;
FILE *err;

/* check if pidfile exists, check if the process is running, save pid */
void save_pid(int pid) {
	int n;
	FILE *file;
	struct stat stat_buf;

	/* if pidfile exists, check if the process is running */
	if(stat(pidfile, &stat_buf) == 0) {
		file = fopen(pidfile, "r");
		if(!file) {
			log_text(err, "'%s' already exists, but can not be read. "
							 "Execution shall continue, but with no logging of the pid (%d).",
							 pidfile, pid);
			pidfile = NULL;
      fclose(file);
			return;
		} else {
			fscanf(file, "%d", &n);
      fclose(file);
			if(kill(n, 0) == -1) {
				log_text(err, "'%s' already exists, but no process with pid %d is "
								 "running. Perhaps serve crashed or was forcibly killed. "
                "Execution shall continue, but with no logging of the pid "
                "(%d).",
                pidfile, n, pid);
			} else {
				log_text(err, "'%s' already exists, and a process with pid %d is "
                 "running. Execution shall not continue.", pidfile, n);
				pidfile = NULL;
        exit(1);
			}
		}
	}

	/* now write the pid */
	file = fopen(pidfile, "w");
	if(file) {
		fprintf(file, "%d\n", pid);
		fclose(file);
		log_text(err, "Wrote pid %d to '%s'.", pid, pidfile);
	} else {
		log_text(err, "Unable to open '%s' for writing. Execution shall continue, "
						 "but with no logging of the pid (%d).", pidfile, pid);
    pidfile = NULL;
	}
}

/* log text to the given file, with date and time */
void log_text(FILE *file, const char *fmt, ...) {
  static time_t lasttim = -1;
	static char date[64];
	struct tm *tm_time;
	va_list args;
	time_t tim;
  int len;
  char *buf = malloc(1024);

	/* write date and time */
	time(&tim);
	if(tim != lasttim) {/* no point re-generating the text all the time */
    lasttim = tim;
    tm_time = localtime(&tim);
    strftime(date, 64, "[%a %d %b %Y %T]", tm_time);
  }

  /* write text to string */
  va_start(args, fmt);
  if((len = vsnprintf(buf, 1024, fmt, args)) > 1024) {
    buf = realloc(buf, len + 1);
    vsnprintf(buf, len + 1, fmt, args);
  }
  va_end(args);

  fprintf(file, "%s %s\n", date, buf);
  
  free(buf);
}

/* logs the request to the appropriate file */
void log_request(request *r) {
	FILE *file;

	/* write successful requests to the logfile, errors to the error file */
	if(r->status < 400) file = out;
  else file = err;

	log_text(file, "[%s {%s}%s %lld] %d %s %s", r->client, r->user_agent,
           r->encoding == GZIP ? " gz" : "", r->content_length, r->status,
           status_reason[r->status], r->req);
}
