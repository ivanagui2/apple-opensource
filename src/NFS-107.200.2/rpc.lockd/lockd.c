/*
 * Copyright (c) 2002-2010 Apple Inc.  All rights reserved.
 *
 * @APPLE_LICENSE_HEADER_START@
 * 
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this
 * file.
 * 
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 * 
 * @APPLE_LICENSE_HEADER_END@
 */
/*	$NetBSD: lockd.c,v 1.7 2000/08/12 18:08:44 thorpej Exp $	*/
/*	$FreeBSD: src/usr.sbin/rpc.lockd/lockd.c,v 1.13 2002/04/11 07:19:30 alfred Exp $ */

/*
 * Copyright (c) 1995
 *	A.R. Gordon (andrew.gordon@net-tel.co.uk).  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed for the FreeBSD project
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY ANDREW GORDON AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/cdefs.h>
#ifndef lint
__RCSID("$NetBSD: lockd.c,v 1.7 2000/08/12 18:08:44 thorpej Exp $");
#endif

/*
 * main() function for NFS lock daemon.  Most of the code in this
 * file was generated by running rpcgen /usr/include/rpcsvc/nlm_prot.x.
 *
 * The actual program logic is in the file lock_proc.c
 */

/* make sure we can handle lots of file descriptors */
#include <sys/syslimits.h>
#define _DARWIN_UNLIMITED_SELECT

#include <sys/types.h>
#include <sys/socket.h>

#include <err.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <syslog.h>
#include <signal.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include <sys/sysctl.h>
#include <sys/select.h>
#include <libutil.h>
#include <util.h>
#include <launch.h>

#include <oncrpc/rpc.h>
#include <oncrpc/rpcb.h>

#include "sm_inter.h"

#include "lockd.h"
#include "nlm_prot.h"
int bindresvport_sa(int sd, struct sockaddr *sa);

int		_rpcsvcdirty = 0;

struct pidfh *pfh = NULL;

const struct nfs_conf_lockd config_defaults =
{
	45,		/* grace_period */
	60,		/* host_monitor_cache_timeout */
	0,		/* port */
	0,		/* send_using_tcp */
	0,		/* send_using_mnt_transport */
	180,		/* shutdown_delay_client */
	180,		/* shutdown_delay_server */
	1,		/* tcp */
	1,		/* udp */
	0,		/* verbose */
};
struct nfs_conf_lockd config;

int lockudpsock, locktcpsock;
int lockudp6sock, locktcp6sock;
int udpport, tcpport;
int udp6port, tcp6port;
int grace_expired;
int nsm_state;
pid_t server_pid = -1;
struct mon mon_host;
time_t currsec;

void	init_nsm(void);
void	nlm_prog_0(struct svc_req *, SVCXPRT *);
void	nlm_prog_1(struct svc_req *, SVCXPRT *);
void	nlm_prog_3(struct svc_req *, SVCXPRT *);
void	nlm_prog_4(struct svc_req *, SVCXPRT *);
void	usage(void);

static int config_read(struct nfs_conf_lockd *);
static void sigalarm_grace_period_handler(void);
static void handle_sigchld(int sig);
void my_svc_run(void);

static int statd_is_loaded(void);
static int statd_load(void);
static int statd_service_start(void);


/*
 * This is the start up of lockd. Lockd has two functions one is to receive
 * request from the kernel and forward them to the appropriate servers, i.e.,
 * the client side. The other is to handle request from the remote clients, i.e.,
 * the server side. Note the client request use asynchronous rpc (_MSG procedures),
 * so the server side will get the reply and then use an nfs system call to return
 * the result to the kernel. We use two processes, one for the client and one for
 * the server. This is due to the fact that rpc library is mt hostile. At a later
 * date we should fix that. The client forks off the server since we're started by
 * lockd and the client gets its receive right from lockd. When nfsd starts up it
 * will send a "ping" request on a host port, launchd will then start lockd.
 *
 * This daemon is to be started by launchd, as such it follows the following
 * launchd rules:
 *	We don't:
 *		call daemon(3)
 *		call fork and having the parent process exit
 *		change uids or gids.
 *		set up the current working directory or chroot.
 *		set the session id
 * 		change stdio to /dev/null.
 *		call setrusage(2)
 *		call setpriority(2)
 *		Ignore SIGTERM.
 *	We are launched on demand
 *		and we catch SIGTERM to exit cleanly.
 *
 * In practice daemonizing in the classic unix sense would probably be ok
 * since we get invoke by traffic on a task_special_port, but we will play
 * by the rules, its even easier to boot.
 */

int
main(argc, argv)
	int argc;
	char **argv;
{
	SVCXPRT *transp;
	struct sockaddr_storage saddr;
	struct sockaddr_in *sin = (struct sockaddr_in*)&saddr;
	struct sockaddr_in6 *sin6 = (struct sockaddr_in6*)&saddr;
	socklen_t socklen;
	int ch, rv, on = 1, svcregcnt;
	struct sigaction sa;
	struct rlimit rlp;
	pid_t child, pid;
	sigset_t waitset, osigset;
	int server_sig;

	config = config_defaults;
	config_read(&config);

	while ((ch = getopt(argc, argv, "d:g:x:")) != (-1)) {
		switch (ch) {
		case 'd':
			config.verbose = atoi(optarg);
			break;
		case 'g':
			config.grace_period = atoi(optarg);
			break;
		case 'x':
			config.host_monitor_cache_timeout = atoi(optarg);
			break;
		default:
		case '?':
			usage();
			/* NOTREACHED */
		}
	}
	if (geteuid()) { /* This command allowed only to root */
		fprintf(stderr, "Sorry. You are not superuser\n");
		exit(1);
        }

	/* Install signal handler to do cleanup */
	signal(SIGINT, handle_sig_cleanup);
	signal(SIGTERM, handle_sig_cleanup);
	signal(SIGHUP, handle_sig_cleanup);
	signal(SIGQUIT, handle_sig_cleanup);

	openlog("rpc.lockd", LOG_CONS | LOG_PID | ((config.verbose == 99) ? LOG_PERROR : 0), LOG_DAEMON);

	/* claim PID file */
	pfh = pidfile_open(_PATH_LOCKD_PID, 0644, &pid);
	if (pfh == NULL) {
		syslog(LOG_ERR, "can't open lockd pidfile: %s (%d)", strerror(errno), errno);
		if (errno == EEXIST) {
			syslog(LOG_ERR, "lockd already running, pid: %d", pid);
			exit(0);
		}
		exit(2);
	}
	if (pidfile_write(pfh) == -1)
		syslog(LOG_WARNING, "can't write to lockd pidfile: %s (%d)", strerror(errno), errno);

	if (config.verbose)
		syslog(LOG_INFO, "lockd starting, debug level %d", config.verbose);
	else
		syslog(LOG_INFO, "lockd starting");

	/* make sure statd is running */
	if ((rv = statd_start()))
		syslog(LOG_WARNING, "unable to start statd %d", rv);

	/* Install signal handler to collect exit status of child processes */
	sa.sa_handler = handle_sigchld;
	sigemptyset(&sa.sa_mask);
	sigaddset(&sa.sa_mask, SIGCHLD);
	sa.sa_flags = SA_RESTART;
	sigaction(SIGCHLD, &sa, NULL);

	/*
	 * Create a separate process, the client code is really a separate
	 * daemon that shares a lot of code. We let the server be the child.
	 * The reason for this is that child will get the receive right from
	 * launchd and it is that process that launchd should be monitoring
	 * We mask SIGUSR1 and then pause in the parent. When the child is
	 * ready it will signal the parent to continue. In this way we know
	 * that if we send a request out to some server, we have a server
	 * process up to handle the reply.
	 */

	sigemptyset(&waitset);
	sigaddset(&waitset, SIGUSR1);
	sigaddset(&waitset, SIGCHLD);
	sigaddset(&waitset, SIGINT);
	sigaddset(&waitset, SIGTERM);
	sigprocmask(SIG_BLOCK, &waitset, &osigset);
	switch (child = fork()) {
	case -1:
		syslog(LOG_ERR, "Could not fork server");
		handle_sig_cleanup(SIGQUIT);
		/*NOTREACHED*/
	case 0:
		/* Server doesn't need to block */
		sigprocmask(SIG_SETMASK, &osigset, NULL);
		break;
	default:
		/* we're the parent and the client */
		server_pid = child;
		/* Wait for the server to be up */
		sigwait(&waitset, &server_sig);
		if (server_sig != SIGUSR1) {
			syslog(LOG_ERR, "Lockd got unexpected signal %d\n", server_sig);
			handle_sig_cleanup(SIGQUIT);
		}
		sigprocmask(SIG_SETMASK, &osigset, NULL); /* Reset signal mask */
		client_mach_request();
		/* We should never return, but if we do kill our other half */
		syslog(LOG_ERR,	"Lockd: client_mach_request(), returned unexpectedly");
		handle_sig_cleanup(SIGQUIT);
		/*NOTREACHED*/
	}

	/* We're the child and the server */
	lockudpsock = locktcpsock = -1;
	lockudp6sock = locktcp6sock = -1;

	rpcb_unset(NULL, NLM_PROG, NLM_SM);
	rpcb_unset(NULL, NLM_PROG, NLM_VERS);
	rpcb_unset(NULL, NLM_PROG, NLM_VERSX);
	rpcb_unset(NULL, NLM_PROG, NLM_VERS4);

	/* parent cleans up the pid file */
	pfh = NULL;

	if (config.udp) {

		/* IPv4 */
		if ((lockudpsock = socket(AF_INET, SOCK_DGRAM, 0)) < 0)
			syslog(LOG_ERR, "can't create UDP IPv4 socket: %s (%d)", strerror(errno), errno);
		if (lockudpsock >= 0) {
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr = INADDR_ANY;
			sin->sin_port = htons(config.port);
			sin->sin_len = sizeof(*sin);
			if (bindresvport_sa(lockudpsock, (struct sockaddr *)sin) < 0) {
				/* socket may still be lingering from previous incarnation */
				/* wait a few seconds and try again */
				sleep(6);
				if (bindresvport_sa(lockudpsock, (struct sockaddr *)sin) < 0) {
					syslog(LOG_ERR, "can't bind UDP IPv4 addr: %s (%d)", strerror(errno), errno);
					close(lockudpsock);
					lockudpsock = -1;
				}
			}
		}
		if (lockudpsock >= 0) {
			socklen = sizeof(*sin);
			if (getsockname(lockudpsock, (struct sockaddr *)sin, &socklen)) {
				syslog(LOG_ERR, "can't getsockname on UDP IPv4 socket: %s (%d)", strerror(errno), errno);
				close(lockudpsock);
				lockudpsock = -1;
			} else {
				udpport = ntohs(sin->sin_port);
			}
		}
		if ((lockudpsock >= 0) && ((transp = svcudp_create(lockudpsock)) == NULL)) {
			syslog(LOG_ERR, "cannot create UDP IPv4 service");
			close(lockudpsock);
			lockudpsock = -1;
		}
		if (lockudpsock >= 0) {
			svcregcnt = 0;
			if (!svc_register(transp, NLM_PROG, NLM_SM, nlm_prog_0, IPPROTO_UDP))
				syslog(LOG_ERR, "unable to register IPv4 (NLM_PROG, NLM_SM, udp)");
			else
				svcregcnt++;
			if (!svc_register(transp, NLM_PROG, NLM_VERS, nlm_prog_1, IPPROTO_UDP))
				syslog(LOG_ERR, "unable to register IPv4 (NLM_PROG, NLM_VERS, udp)");
			else
				svcregcnt++;
			if (!svc_register(transp, NLM_PROG, NLM_VERSX, nlm_prog_3, IPPROTO_UDP))
				syslog(LOG_ERR, "unable to register IPv4 (NLM_PROG, NLM_VERSX, udp)");
			else
				svcregcnt++;
			if (!svc_register(transp, NLM_PROG, NLM_VERS4, nlm_prog_4, IPPROTO_UDP))
				syslog(LOG_ERR, "unable to register IPv4 (NLM_PROG, NLM_VERS4, udp)");
			else
				svcregcnt++;
			if (!svcregcnt) {
				svc_destroy(transp);
				close(lockudpsock);
				lockudpsock = -1;
			}
		}

		/* IPv6 */
		if ((lockudp6sock = socket(AF_INET6, SOCK_DGRAM, 0)) < 0)
			syslog(LOG_ERR, "can't create UDP IPv6 socket: %s (%d)", strerror(errno), errno);
		if (lockudp6sock >= 0) {
			if (setsockopt(lockudp6sock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on)))
				syslog(LOG_WARNING, "can't set IPV6_V6ONLY on socket: %s (%d)", strerror(errno), errno);
			sin6->sin6_family = AF_INET6;
			sin6->sin6_addr = in6addr_any;
			sin6->sin6_port = htons(config.port);
			sin6->sin6_len = sizeof(*sin6);
			if (bindresvport_sa(lockudp6sock, (struct sockaddr *)sin6) < 0) {
				/* socket may still be lingering from previous incarnation */
				/* wait a few seconds and try again */
				sleep(6);
				if (bindresvport_sa(lockudp6sock, (struct sockaddr *)sin6) < 0) {
					syslog(LOG_ERR, "can't bind UDP IPv6 addr: %s (%d)", strerror(errno), errno);
					close(lockudp6sock);
					lockudp6sock = -1;
				}
			}
		}
		if (lockudp6sock >= 0) {
			socklen = sizeof(*sin6);
			if (getsockname(lockudp6sock, (struct sockaddr *)sin6, &socklen)) {
				syslog(LOG_ERR, "can't getsockname on UDP IPv6 socket: %s (%d)", strerror(errno), errno);
				close(lockudp6sock);
				lockudp6sock = -1;
			} else {
				udp6port = ntohs(sin6->sin6_port);
			}
		}
		if ((lockudp6sock >= 0) && ((transp = svcudp_create(lockudp6sock)) == NULL)) {
			syslog(LOG_ERR, "cannot create UDP IPv6 service");
			close(lockudp6sock);
			lockudp6sock = -1;
		}
		if (lockudp6sock >= 0) {
			svcregcnt = 0;
			if (!svc_register(transp, NLM_PROG, NLM_SM, nlm_prog_0, IPPROTO_UDP))
				syslog(LOG_ERR, "unable to register IPv6 (NLM_PROG, NLM_SM, udp)");
			else
				svcregcnt++;
			if (!svc_register(transp, NLM_PROG, NLM_VERS, nlm_prog_1, IPPROTO_UDP))
				syslog(LOG_ERR, "unable to register IPv6 (NLM_PROG, NLM_VERS, udp)");
			else
				svcregcnt++;
			if (!svc_register(transp, NLM_PROG, NLM_VERSX, nlm_prog_3, IPPROTO_UDP))
				syslog(LOG_ERR, "unable to register IPv6 (NLM_PROG, NLM_VERSX, udp)");
			else
				svcregcnt++;
			if (!svc_register(transp, NLM_PROG, NLM_VERS4, nlm_prog_4, IPPROTO_UDP))
				syslog(LOG_ERR, "unable to register IPv6 (NLM_PROG, NLM_VERS4, udp)");
			else
				svcregcnt++;
			if (!svcregcnt) {
				svc_destroy(transp);
				close(lockudp6sock);
				lockudp6sock = -1;
			}
		}

	}

	if (config.tcp) {

		/* IPv4 */
		if ((locktcpsock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
			syslog(LOG_ERR, "can't create TCP IPv4 socket: %s (%d)", strerror(errno), errno);
		if (locktcpsock >= 0) {
			if (setsockopt(locktcpsock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
				syslog(LOG_WARNING, "setsockopt TCP IPv4 SO_REUSEADDR: %s (%d)", strerror(errno), errno);
			sin->sin_family = AF_INET;
			sin->sin_addr.s_addr = INADDR_ANY;
			sin->sin_port = htons(config.port);
			sin->sin_len = sizeof(*sin);
			if (bindresvport_sa(locktcpsock, (struct sockaddr *)sin) < 0) {
				syslog(LOG_ERR, "can't bind TCP IPv4 addr: %s (%d)", strerror(errno), errno);
				close(locktcpsock);
				locktcpsock = -1;
			}
		}
		if (locktcpsock >= 0) {
			socklen = sizeof(*sin);
			if (getsockname(locktcpsock, (struct sockaddr *)sin, &socklen)) {
				syslog(LOG_ERR, "can't getsockname on TCP IPv4 socket: %s (%d)", strerror(errno), errno);
				close(locktcpsock);
				locktcpsock = -1;
			} else {
				tcpport = ntohs(sin->sin_port);
			}
		}
		if ((locktcpsock >= 0) && ((transp = svctcp_create(locktcpsock, 0, 0)) == NULL)) {
			syslog(LOG_ERR, "cannot create TCP IPv4 service");
			close(locktcpsock);
			locktcpsock = -1;
		}
		if (locktcpsock >= 0) {
			svcregcnt = 0;
			if (!svc_register(transp, NLM_PROG, NLM_SM, nlm_prog_0, IPPROTO_TCP))
				syslog(LOG_ERR, "unable to register IPv4 (NLM_PROG, NLM_SM, tcp)");
			else
				svcregcnt++;
			if (!svc_register(transp, NLM_PROG, NLM_VERS, nlm_prog_1, IPPROTO_TCP))
				syslog(LOG_ERR, "unable to register IPv4 (NLM_PROG, NLM_VERS, tcp)");
			else
				svcregcnt++;
			if (!svc_register(transp, NLM_PROG, NLM_VERSX, nlm_prog_3, IPPROTO_TCP))
				syslog(LOG_ERR, "unable to register IPv4 (NLM_PROG, NLM_VERSX, tcp)");
			else
				svcregcnt++;
			if (!svc_register(transp, NLM_PROG, NLM_VERS4, nlm_prog_4, IPPROTO_TCP))
				syslog(LOG_ERR, "unable to register IPv4 (NLM_PROG, NLM_VERS4, tcp)");
			else
				svcregcnt++;
			if (!svcregcnt) {
				svc_destroy(transp);
				close(locktcpsock);
				locktcpsock = -1;
			}
		}

		/* IPv6 */
		if ((locktcp6sock = socket(AF_INET6, SOCK_STREAM, 0)) < 0)
			syslog(LOG_ERR, "can't create TCP IPv6 socket: %s (%d)", strerror(errno), errno);
		if (locktcp6sock >= 0) {
			if (setsockopt(locktcp6sock, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on)) < 0)
				syslog(LOG_WARNING, "setsockopt TCP IPv6 SO_REUSEADDR: %s (%d)", strerror(errno), errno);
			setsockopt(locktcp6sock, IPPROTO_IPV6, IPV6_V6ONLY, &on, sizeof(on));
			sin6->sin6_family = AF_INET6;
			sin6->sin6_addr = in6addr_any;
			sin6->sin6_port = htons(config.port);
			sin6->sin6_len = sizeof(*sin6);
			if (bindresvport_sa(locktcp6sock, (struct sockaddr *)sin6) < 0) {
				syslog(LOG_ERR, "can't bind TCP IPv6 addr: %s (%d)", strerror(errno), errno);
				close(locktcp6sock);
				locktcp6sock = -1;
			}
		}
		if (locktcp6sock >= 0) {
			socklen = sizeof(*sin6);
			if (getsockname(locktcp6sock, (struct sockaddr *)sin6, &socklen)) {
				syslog(LOG_ERR, "can't getsockname on TCP IPv6 socket: %s (%d)", strerror(errno), errno);
				close(locktcp6sock);
				locktcp6sock = -1;
			} else {
				tcp6port = ntohs(sin6->sin6_port);
			}
		}
		if ((locktcp6sock >= 0) && ((transp = svctcp_create(locktcp6sock, 0, 0)) == NULL)) {
			syslog(LOG_ERR, "cannot create TCP IPv6 service");
			close(locktcp6sock);
			locktcp6sock = -1;
		}
		if (locktcp6sock >= 0) {
			svcregcnt = 0;
			if (!svc_register(transp, NLM_PROG, NLM_SM, nlm_prog_0, IPPROTO_TCP))
				syslog(LOG_ERR, "unable to register IPv6 (NLM_PROG, NLM_SM, tcp)");
			else
				svcregcnt++;
			if (!svc_register(transp, NLM_PROG, NLM_VERS, nlm_prog_1, IPPROTO_TCP))
				syslog(LOG_ERR, "unable to register IPv6 (NLM_PROG, NLM_VERS, tcp)");
			else
				svcregcnt++;
			if (!svc_register(transp, NLM_PROG, NLM_VERSX, nlm_prog_3, IPPROTO_TCP))
				syslog(LOG_ERR, "unable to register IPv6 (NLM_PROG, NLM_VERSX, tcp)");
			else
				svcregcnt++;
			if (!svc_register(transp, NLM_PROG, NLM_VERS4, nlm_prog_4, IPPROTO_TCP))
				syslog(LOG_ERR, "unable to register IPv6 (NLM_PROG, NLM_VERS4, tcp)");
			else
				svcregcnt++;
			if (!svcregcnt) {
				svc_destroy(transp);
				close(locktcp6sock);
				locktcp6sock = -1;
			}
		}

	}

	if ((lockudp6sock < 0) && (locktcp6sock < 0))
		syslog(LOG_WARNING, "Can't create NLM IPv6 sockets");
	if ((lockudpsock < 0) && (locktcpsock < 0))
		syslog(LOG_WARNING, "Can't create NLM IPv4 sockets");
	if ((lockudp6sock < 0) && (locktcp6sock < 0) &&
	    (lockudpsock < 0) && (locktcpsock < 0)) {
		syslog(LOG_ERR, "Can't create any NLM sockets!");
		exit(1);
	}

	init_nsm();

	/* raise our resource limits as far as they can go */
	if (getrlimit(RLIMIT_NOFILE, &rlp)) {
		syslog(LOG_WARNING, "getrlimit(RLIMIT_NOFILE) failed: %s",
			strerror(errno));
	} else {
		rlp.rlim_cur = MIN(rlp.rlim_max, 2*FD_SETSIZE);
		if (setrlimit(RLIMIT_NOFILE, &rlp)) {
			syslog(LOG_WARNING, "setrlimit(RLIMIT_NOFILE) failed: %s",
				strerror(errno));
		}
	}

	/* set up grace period alarm */
	sa.sa_handler = (sig_t) sigalarm_grace_period_handler;
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESETHAND; /* should only happen once */
	sa.sa_flags |= SA_RESTART;
	if (sigaction(SIGALRM, &sa, NULL) != 0) {
		syslog(LOG_WARNING, "sigaction(SIGALRM) failed: %s",
		    strerror(errno));
		exit(1);
	}
	if (config.grace_period > 0) {
		grace_expired = 0;
		alarm(config.grace_period);
	} else {
		grace_expired = 1;
	}

	/* Signal the parent (client process) that its ok to start */
	kill(getppid(), SIGUSR1);

	my_svc_run();		/* Should never return */
	exit(1);
}

void
sigalarm_grace_period_handler(void)
{
	grace_expired = 1;
}

void
usage()
{
	errx(1, "usage: rpc.lockd [-d <debuglevel>] [-g <grace period>] "
	    " [-x <host monitor cache timeout>] [-w]");
}

/*
 * init_nsm --
 *	Reset the NSM state-of-the-world and acquire its state.
 */
void
init_nsm(void)
{
	enum clnt_stat ret;
	sm_stat mstat;
	char localhost[] = "localhost";
	int attempt = 0;

	/* setup constant data for SM_MON calls */
	mon_host.mon_id.my_id.my_name = localhost;
	mon_host.mon_id.my_id.my_prog = NLM_PROG;
	mon_host.mon_id.my_id.my_vers = NLM_SM;
	mon_host.mon_id.my_id.my_proc = NLM_SM_NOTIFY;  /* bsdi addition */

	/*
	 * !!!
	 * The statd program must already be registered when lockd runs.
	 * If we have a problem contacting statd, pause and try again a
	 * number of times in case statd is just slow in coming up.
	 */
	do {
		ret = callrpc((char *) "localhost", SM_PROG, SM_VERS, SM_UNMON_ALL,
		    (xdrproc_t)xdr_my_id, &mon_host.mon_id.my_id, (xdrproc_t)xdr_sm_stat, &mstat);
		if (ret) {
			syslog(LOG_WARNING, "can't contact statd, %u %s", SM_PROG, clnt_sperrno(ret));
			if (++attempt < 20) {
				/* attempt to start it again */
				statd_start();
				sleep(attempt);
				continue;
			}
		}
		break;
	} while (1);

	if (ret != 0) {
		syslog(LOG_ERR, "server init_nsm failed! %u %s", SM_PROG, clnt_sperrno(ret));
		exit(1);
	}

	nsm_state = mstat.state;

}

/*
 * handle_sig_cleanup
 *
 * Purpose:	on signal, kill server child and do pid file cleanup
 * Returns:	Nothing
 */
void
handle_sig_cleanup(int sig)
{
	pid_t pid;
	int status;

	if (server_pid != -1) {
		pid = server_pid;
		server_pid = -1;
		kill(pid, SIGTERM);
		wait4(pid, &status, 0, NULL);
	} else {
		alarm(1); /* XXX 5028243 in case rpcb_unset() gets hung up during shutdown */
		rpcb_unset(NULL, NLM_PROG, NLM_SM);
		rpcb_unset(NULL, NLM_PROG, NLM_VERS);
		rpcb_unset(NULL, NLM_PROG, NLM_VERSX);
		rpcb_unset(NULL, NLM_PROG, NLM_VERS4);
	}
	if (pfh && !sig)
		pidfile_remove(pfh);
	exit(!sig ? 0 : 1);
}

/*
 * handle_sigchld
 *
 * Exit if we catch the lockd server child process dying.
 */
static void
handle_sigchld(int sig __unused)
{
	pid_t pid;
	int status;

	pid = wait4(-1, &status, WNOHANG, NULL);
	if ((server_pid != -1) && (pid == server_pid)) {
		if (status)
			syslog(LOG_ERR, "lockd server %d failed with status %d",
				pid, WEXITSTATUS(status));
		handle_sig_cleanup(SIGCHLD);
		/*NOTREACHED*/
	}
}

/*
 * The RPC service run loop
 */
void
my_svc_run(void)
{
        fd_set readfds;
        struct timeval timeout;
	struct timeval now;
	int error;
	int hashosts = 0;
	struct timeval *top;
	extern int svc_maxfd;


	for( ;; ) {
		timeout.tv_sec = config.host_monitor_cache_timeout + 1;
		timeout.tv_usec = 0;

		bcopy(&svc_fdset, &readfds, sizeof(svc_fdset));
		/*
		 * If there are any expired hosts then sleep with a
		 * timeout to expire them.
		 */
		if (hashosts && (timeout.tv_sec >= 0))
			top = &timeout;
		else
			top = NULL;
		error = select(svc_maxfd+1, &readfds, NULL, NULL, top);
		if (error == -1) {
			if (errno == EINTR)
				continue;
                        perror("rpc.lockd: my_svc_run: select failed");
                        return;
		}
		gettimeofday(&now, NULL);
		currsec = now.tv_sec;
		if (error > 0)
			svc_getreqset(&readfds);
		if ((config.verbose > 3) && (error == 0))
			fprintf(stderr, "my_svc_run: select timeout\n");
		hashosts = expire_lock_hosts();
	}
}

/*
 * read the lockd values from nfs.conf
 */
static int
config_read(struct nfs_conf_lockd *conf)
{
	FILE *f;
	size_t len, linenum = 0;
	char *line, *p, *key, *value;
	long val;

	if (!(f = fopen(_PATH_NFS_CONF, "r"))) {
		if (errno != ENOENT)
			syslog(LOG_WARNING, "%s", _PATH_NFS_CONF);
		return (1);
	}
	for (; (line = fparseln(f, &len, &linenum, NULL, 0)); free(line)) {
		if (len <= 0)
			continue;
		/* trim trailing whitespace */
		p = line + len - 1;
		while ((p > line) && isspace(*p))
			*p-- = '\0';
		/* find key start */
		key = line;
		while (isspace(*key))
			key++;
		/* find equals/value */
		value = p = strchr(line, '=');
		if (p) /* trim trailing whitespace on key */
			do { *p-- = '\0'; } while ((p > line) && isspace(*p));
		/* find value start */
		if (value)
			do { value++; } while (isspace(*value));

		/* all lockd keys start with "nfs.lockd." */
		if (strncmp(key, "nfs.lockd.", 10)) {
			if (config.verbose)
				syslog(LOG_DEBUG, "%4ld %s=%s\n", linenum, key, value ? value : "");
			continue;
		}
		val = !value ? 1 : strtol(value, NULL, 0);
		if (config.verbose)
			syslog(LOG_DEBUG, "%4ld %s=%s (%ld)\n", linenum, key, value ? value : "", val);

		if (!strcmp(key, "nfs.lockd.grace_period")) {
			if (value && val)
				conf->grace_period = val;
		} else if (!strcmp(key, "nfs.lockd.host_monitor_cache_timeout")) {
			if (value)
				conf->host_monitor_cache_timeout = val;
		} else if (!strcmp(key, "nfs.lockd.port")) {
			if (value && val)
				conf->port = val;
		} else if (!strcmp(key, "nfs.lockd.send_using_tcp")) {
			conf->send_using_tcp = val;
		} else if (!strcmp(key, "nfs.lockd.send_using_mnt_transport")) {
			conf->send_using_mnt_transport = val;
		} else if (!strcmp(key, "nfs.lockd.shutdown_delay_client")) {
			if (value && val)
				conf->shutdown_delay_client = val;
		} else if (!strcmp(key, "nfs.lockd.shutdown_delay_server")) {
			if (value && val)
				conf->shutdown_delay_server = val;
		} else if (!strcmp(key, "nfs.lockd.tcp")) {
			conf->tcp = val;
		} else if (!strcmp(key, "nfs.lockd.udp")) {
			conf->udp = val;
		} else if (!strcmp(key, "nfs.lockd.verbose")) {
			conf->verbose = val;
		} else {
			if (config.verbose)
				syslog(LOG_DEBUG, "ignoring unknown config value: %4ld %s=%s\n", linenum, key, value ? value : "");
		}

	}

	fclose(f);
	return (0);
}

/*
 * get the PID of the running statd
 */
static pid_t
get_statd_pid(void)
{
	char pidbuf[128], *pidend;
	int fd, len, rv;
	pid_t pid;
	struct flock lock;

	if ((fd = open(_PATH_STATD_PID, O_RDONLY)) < 0) {
		if (config.verbose)
			syslog(LOG_DEBUG, "%s: %s (%d)", _PATH_STATD_PID, strerror(errno), errno);
		return (0);
	}
	len = sizeof(pidbuf) - 1;
	if ((len = read(fd, pidbuf, len)) < 0) {
		if (config.verbose)
			syslog(LOG_DEBUG, "%s: %s (%d)", _PATH_STATD_PID, strerror(errno), errno);
		close(fd);
		return (0);
	}

	/* parse PID */
	pidbuf[len] = '\0';
	pid = strtol(pidbuf, &pidend, 10);
	if (!len || (pid < 1)) {
		if (config.verbose)
			syslog(LOG_DEBUG, "%s: bogus pid: %s", _PATH_STATD_PID, pidbuf);
		close(fd);
		return (0);
	}

	/* check for lock on file by PID */
	lock.l_type = F_RDLCK;
	lock.l_whence = SEEK_SET;
	lock.l_start = 0;
	lock.l_len = 0;
	rv = fcntl(fd, F_GETLK, &lock);
	close(fd);
	if (rv != 0) {
		if (config.verbose)
			syslog(LOG_DEBUG, "%s: fcntl: %s (%d)", _PATH_STATD_PID, strerror(errno), errno);
		return (0);
	} else if (lock.l_type == F_UNLCK) {
		if (config.verbose)
			syslog(LOG_DEBUG, "%s: not locked\n", _PATH_STATD_PID);
		return (0);
	}
	return (pid);
}

static int
statd_is_running(void)
{
	return (get_statd_pid() > 0);
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"

static int
statd_is_loaded(void)
{
	launch_data_t msg, resp;
	int rv = 0;

	msg = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	if (!msg)
		return (0);
	launch_data_dict_insert(msg, launch_data_new_string(_STATD_SERVICE_LABEL), LAUNCH_KEY_GETJOB);

	resp = launch_msg(msg);
	if (resp) {
		if (launch_data_get_type(resp) == LAUNCH_DATA_DICTIONARY)
			rv = 1;
		launch_data_free(resp);
	} else {
		syslog(LOG_ERR, "launch_msg(): %m");
	}

	launch_data_free(msg);
	return (rv);
}

static int
statd_load(void)
{
	launch_data_t msg, job, args, resp;
	int rv = 1;

	msg = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	job = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	args = launch_data_alloc(LAUNCH_DATA_ARRAY);
	if (!msg || !job || !args) {
		if (msg) launch_data_free(msg);
		if (job) launch_data_free(job);
		if (args) launch_data_free(args);
		return (1);
	}
	launch_data_array_set_index(args, launch_data_new_string(_PATH_STATD), 0);
	launch_data_dict_insert(job, launch_data_new_string(_STATD_SERVICE_LABEL), LAUNCH_JOBKEY_LABEL);
	launch_data_dict_insert(job, launch_data_new_bool(FALSE), LAUNCH_JOBKEY_ONDEMAND);
	launch_data_dict_insert(job, launch_data_new_bool(TRUE), LAUNCH_JOBKEY_HOPEFULLYEXITSLAST);
	launch_data_dict_insert(job, args, LAUNCH_JOBKEY_PROGRAMARGUMENTS);
	launch_data_dict_insert(msg, job, LAUNCH_KEY_SUBMITJOB);

	resp = launch_msg(msg);
	if (!resp) {
		rv = errno;
	} else {
		if (launch_data_get_type(resp) == LAUNCH_DATA_ERRNO)
			rv = launch_data_get_errno(resp);
		launch_data_free(resp);
	}

	launch_data_free(msg);
	return (rv);
}

static int
statd_service_start(void)
{
	launch_data_t msg, resp;
	int rv = 1;

	msg = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	if (!msg)
		return (1);
	launch_data_dict_insert(msg, launch_data_new_string(_STATD_SERVICE_LABEL), LAUNCH_KEY_STARTJOB);

	resp = launch_msg(msg);
	if (!resp) {
		rv = errno;
	} else {
		if (launch_data_get_type(resp) == LAUNCH_DATA_ERRNO)
			rv = launch_data_get_errno(resp);
		launch_data_free(resp);
	}

	launch_data_free(msg);
	return (rv);
}

int
statd_start(void)
{
	struct timespec ts;
	int rv;

	if (statd_is_running())
		rv = 0;
	else if (statd_is_loaded())
		rv = statd_service_start();
	else
		rv = statd_load();

	/* sleep a little to give statd/portmap a chance to start */
	/* (better to sleep 100ms than to timeout on portmap calls) */
	ts.tv_sec = 0;
	ts.tv_nsec = 100*1000*1000;
	nanosleep(&ts, NULL);

	return (rv);
}

int
statd_stop(void)
{
	launch_data_t msg, resp;
	int rv = 1;

	msg = launch_data_alloc(LAUNCH_DATA_DICTIONARY);
	if (!msg)
		return (1);
	launch_data_dict_insert(msg, launch_data_new_string(_STATD_SERVICE_LABEL), LAUNCH_KEY_REMOVEJOB);

	resp = launch_msg(msg);
	if (!resp) {
		rv = errno;
	} else {
		if (launch_data_get_type(resp) == LAUNCH_DATA_ERRNO)
			rv = launch_data_get_errno(resp);
		launch_data_free(resp);
	}

	launch_data_free(msg);
	return (rv);
}

#pragma clang diagnostic pop