/* Initialisation functions for serve

	 By James Stanley

	 Public domain */

#include "serve.h"

char *signame[32];
char *status_reason[600];
char *page_text[600];

/* Initialises networking for serve. If it returns, it was successful.
	 service should be the port to use
	  *** Using service names instead of ports is not supported and may break CGI
	 Returns the file descriptor for the server */
int init_net(const char *service) {
	struct addrinfo hints, *addr, *ptr;
	int fd, yes = 1;

	/* we need to set up the options for getaddrinfo */
	memset(&hints, '\0', sizeof(struct addrinfo));
	hints.ai_family   = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags    = AI_PASSIVE; /* any interface */
	hints.ai_protocol = 0;          /* any protocol */

	/* firstly we get a list of addresses for the given service */
	if(getaddrinfo(listen_addr, service, &hints, &addr) != 0) {
		log_text(err, "Unable to get address list for service/port %s.", service);
		exit(1);
	}

	/* now go through the list of addresses and use the first available */
	for(ptr = addr; ptr; ptr = ptr->ai_next) {
		/* try to make a socket with this address */
		if((fd = socket(ptr->ai_family, SOCK_STREAM, ptr->ai_protocol)) == -1)
			continue;

		/* try to allow this socket to be reuseable by a subsequent process */
		if(setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1) {
			close(fd);
			continue;
		}

		/* and now bind to this socket */
		if(bind(fd, ptr->ai_addr, ptr->ai_addrlen) == -1) {
			close(fd);
			continue;
		}

		/* bind succeeded, let's roll */
		break;
	}

	/* we no longer care about our address */
	freeaddrinfo(addr);

	/* if ptr is NULL, bind never succeeded */
	if(!ptr) {
		log_text(err, "Unable to bind to an address.");
		exit(1);
	}

  /* now let's get listening */
	if(listen(fd, 10) == -1) {
		log_text(err, "Unable to listen on socket.");
		exit(1);
	}

	log_text(out, "Listening on port '%s'", service);

	/* our work here is done... */
	return fd;
}

/* signal handler for SIGCHLD
	 prevents ghosted processes from surviving */
void ghost_buster(int sig) {
	/* keep allowing children to exit until there is an error */
	while(waitpid(-1, NULL, WNOHANG) > 0);
}

/* signal handler for SIGINT, SIGTERM, etc.
	 prevents stale pidfile from happening */
void clean_quit(int sig) {
	log_text(err, "%s process received signal %d (%s), %s.",
           is_handler ? "Handler" : "Main", sig, signame[sig],
           is_handler ? "exiting" : "server terminating");

  if(!is_handler) {/* kill children */
    killpg(0, sig);
    if(pidfile) unlink(pidfile);
  }

	fclose(out);
	fclose(err);

	exit(0);
}

/* sets up signal handlers */
void init_sighandlers(void) {
	struct sigaction sa;

	/* ignore SIGPIPE (NOTE: This doesn't work) */
	signal(SIGPIPE, SIG_IGN);

	/* Fill in this list at the same time as signals are added below */
	signame[SIGINT]  = "SIGINT";
	signame[SIGQUIT] = "SIGQUIT";
  signame[SIGABRT] = "SIGABRT";
	signame[SIGSEGV] = "SIGSEGV";
  signame[SIGPIPE] = "SIGPIPE";
	signame[SIGTERM] = "SIGTERM";

	memset(&sa, '\0', sizeof(sa));

	/* signal handler for exit; fill in list above */
	sa.sa_handler = clean_quit;
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);
	sigaction(SIGQUIT, &sa, NULL);
  sigaction(SIGABRT, &sa, NULL);
	sigaction(SIGSEGV, &sa, NULL);
  sigaction(SIGPIPE, &sa, NULL);
	sigaction(SIGTERM, &sa, NULL);

	/* clean up ghosted children */
	sa.sa_handler = ghost_buster;
	sigaction(SIGCHLD, &sa, NULL);
}

/* Sets up the HTTP status reasons
   They all need to be here in case a CGI script uses them */
void init_status_reason(void) {
	int i;

	for(i = 0; i < 600; i++) {
		status_reason[i] = "(Unknown)";
    page_text[i] = NULL;
	}

	status_reason[100] = "Continue";
	status_reason[101] = "Switching Protocols";

	status_reason[200] = "OK";
	status_reason[201] = "Created";
	status_reason[202] = "Accepted";
	status_reason[203] = "Non-Authoritative Information";
	status_reason[204] = "No Content";
	status_reason[205] = "Reset Content";
	status_reason[206] = "Partial Content";

	status_reason[300] = "Multiple Choices";
	status_reason[301] = "Moved Permanently";
	status_reason[302] = "Found";
	status_reason[303] = "See Other";
	status_reason[304] = "Not Modified";
	status_reason[305] = "Use Proxy";
	status_reason[306] = "(Unused)";
	status_reason[307] = "Temporary Redirect";

	status_reason[400] = "Bad Request";
  page_text[400] = "The client issued a request that the server is unable to "
    "respond to.";
	status_reason[401] = "Unauthorized";
	status_reason[402] = "Payment Required";
	status_reason[403] = "Forbidden";
  page_text[403] = "You are not allowed to visit this page.";
	status_reason[404] = "Not Found";
  page_text[404] = "The file could not be located on the server.";
	status_reason[405] = "Method Not Allowed";
	status_reason[406] = "Not Acceptable";
	status_reason[407] = "Proxy Authentication Required";
	status_reason[408] = "Request Timeout";
	status_reason[409] = "Conflict";
	status_reason[410] = "Gone";
	status_reason[411] = "Length Required";
	status_reason[412] = "Precondition Failed";
	status_reason[413] = "Request Entity Too Large";
	status_reason[414] = "Request-URI Too Long";
	status_reason[415] = "Unsupported Media Type";
	status_reason[416] = "Requested Range Not Satisfiable";
	status_reason[417] = "Expectation Failed";
	status_reason[418] = "I'm a teapot";/* RFC2324 (HTCPCP) only */

	status_reason[500] = "Internal Server Error";
  page_text[500] = "The server is experiencing abnormal circumstances and was "
    "unable to respond to your request.";
	status_reason[501] = "Not Implemented";
	status_reason[502] = "Bad Gateway";
	status_reason[503] = "Service Unavailable";
	status_reason[504] = "Gateway Timeout";
	status_reason[505] = "HTTP Version Unsupported";
}
