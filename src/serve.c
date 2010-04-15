/* serve - Simple HTTP server

   By James Stanley

   Public domain */

#include "serve.h"

char *port = "8080";
char *pidfile;
int daemonize = 0;
char *user, *group;
char *server_name = "localhost";
char *listen_addr = NULL;

/* Makes a duplicate of the first n bytes of s. Will always copy n bytes and
   add a NUL-terminator regardless of the length of s */
char *strdup2(const char *s, size_t n) {
  char *p;

  if((p = malloc(n + 1)) == NULL) return NULL;
  memcpy(p, s, n);
  p[n] = '\0';

  return p;
}

void show_help(void) {
  printf(
         SERVER " by James Stanley.\n"
         "Light, config-less, HTTP server.\n"
         "\n"
         "  -d         Daemonize\n"
         "  -g GROUP   After initialising, setgid to GROUP (see -u)\n"
         "  -h         Show this text\n"
         "  -l ADDR    Listen on the given address\n"
         "  -m FILE    Prefer MIME types from the given file (also loads definitions "
         "from the default files). There can be several of this option.\n"
         "  -p PORT    Listen on the given port\n"
         "  -P PIDFILE Write the PID to the given file\n"
         "  -s HOSTNAME Name to use as host name in HTTP 1.0 requests\n"
         "  -u USER    After initialising, setuid to USER (see -g)\n"
         "\n"
         "Defaults are:\n"
         " -p8080 -slocalhost\n"
         "and, if daemonized:\n"
         " -p8080 -slocalhost -P/var/run/serve.pid\n"
         "Note that when running as a daemon, you should redirect stdout and stderr "
         "to log files.\n"
         );
}

/* Thanks Beej :) */
void *get_in_addr(struct sockaddr *sa) {
  if(sa->sa_family == AF_INET) {
    return &(((struct sockaddr_in*)sa)->sin_addr);
  }

  return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main(int argc, char **argv) {
  struct sockaddr_storage clientaddr;
  char addr[INET6_ADDRSTRLEN];
  char *ptr;
  socklen_t size;
  int servfd, fd;
  int n, opt;
  struct passwd *pw;
  struct group *gr;

  is_handler = 0;

  /* set up file pointers */
  out = stdout;
  err = stderr;

  load_mimetypes();
  init_builtin_files();

  /* get command line options */
  opterr = 1;
  while((opt = getopt(argc, argv, "dg:hl:m:p:P:s:u:")) != -1) {
    switch(opt) {
    case 'd':
      daemonize = 1;
      if(!pidfile) pidfile = "/var/run/serve.pid";
      break;
    case 'g':
      group = optarg;
      break;
    case 'h':
      show_help();
      return 0;
    case 'l':
      listen_addr = optarg;
      break;
    case 'm':
      load_mimetypes_from(optarg);
      break;
    case 'p':
      port = optarg;
      break;
    case 'P':
      pidfile = optarg;
      break;
    case 's':
      server_name = optarg;
      break;
    case 'u':
      user = optarg;
      break;
    default:
      log_text(err, "Bad argument passed, exiting.");
      return 1;
    }
  }

  /* initialise server */
  servfd = init_net(port);
  init_sighandlers();
  init_status_reason();

  /* now let's daemonize */
  if(daemonize) {
    /* flush output streams so they don't get flushed once for the parent and
       once for the child... */
    fflush(out);
    fflush(err);

    n = fork();
  
    if(n < 0) {
      log_text(err, "Unable to daemonize. Exiting.");
      return 1;
    }
  
    if(n) {/* parent process - log pid and quit */
      save_pid(n);
      return 0;
    }

    setsid();
    umask(0);

    close(STDIN_FILENO);
    /* stdout and stderr should be redirected to logs by operator */
  } else if(pidfile) {/* not a daemon, but write pid anyway */
    save_pid(getpid());
  }

  /* initialization done, switch group and user */
  if(group) {
    gr = getgrnam(group);
    if(!gr) {
      log_text(err, "Unable to set group to %s; group does not exist.", group);
    } else {
      if(setgid(gr->gr_gid) == -1)
        log_text(err, "Unable to set real group ID.");
      if(setegid(gr->gr_gid) == -1)
        log_text(err, "Unable to set effective group ID.");
    }
  }
  if(user) {
    pw = getpwnam(user);
    if(!pw) {
      log_text(err, "Unable to switch user to %s; user does not exist.", user);
    } else {
      if(setuid(pw->pw_uid) == -1)
        log_text(err, "Unable to set real user ID.");
      if(seteuid(pw->pw_uid) == -1)
        log_text(err, "Unable to set effective user ID.");
    }
  }

  /* set up a process group to avoid zombified processes */
  setpgid(0, 0);
	
  /* accept and handle connections forever */
  while(1) {
    size = sizeof(struct sockaddr_storage);

    fd = accept(servfd, (struct sockaddr*)&clientaddr, &size);

    if(fd == -1) {
      if(errno != EINTR)
        log_text(err, "accept returned -1 and it wasn't EINTR.");
      continue;
    }

    /* flush output streams so they don't get flushed once for the parent and
       once for the child... */
    fflush(out);
    fflush(err);

    /* now fork a handler process */
    if((n = fork()) == -1) {
      emergency_500(fd, "unable to fork handler process");
    }

    /* parent process continues accepting */
    if(n) {
      close(fd);
      continue;
    }

    /* child is a handler process */
    is_handler = 1;

    /* child process doesn't need this */
    close(servfd);
		
    /* now get the textual IP address */
    inet_ntop(clientaddr.ss_family, get_in_addr((struct sockaddr*)&clientaddr),
              addr, INET6_ADDRSTRLEN);
    ptr = addr;

    /* HACK: get nice-looking IPv4 addresses even if we're on IPv6 */
    if(strncmp(ptr, "::ffff:", 7) == 0) ptr += 7;
	 
    /* and handle the request */
    handle(fd, ptr);
  }

  return 0;
}
