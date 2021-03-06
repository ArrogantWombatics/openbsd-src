/*	$OpenBSD: log.c,v 1.28 2016/07/01 23:36:38 renato Exp $ */

/*
 * Copyright (c) 2003, 2004 Henning Brauer <henning@openbsd.org>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>
#include <netdb.h>

#include "ldpd.h"
#include "ldpe.h"
#include "lde.h"
#include "log.h"

static const char * const procnames[] = {
	"parent",
	"ldpe",
	"lde"
};

static void	 vlog(int, const char *, va_list);

static int	 debug;
static int	 verbose;

void
log_init(int n_debug)
{
	extern char	*__progname;

	debug = n_debug;

	if (!debug)
		openlog(__progname, LOG_PID | LOG_NDELAY, LOG_DAEMON);

	tzset();
}

void
log_verbose(int v)
{
	verbose = v;
}

void
logit(int pri, const char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	vlog(pri, fmt, ap);
	va_end(ap);
}

static void
vlog(int pri, const char *fmt, va_list ap)
{
	char	*nfmt;

	if (debug) {
		/* best effort in out of mem situations */
		if (asprintf(&nfmt, "%s\n", fmt) == -1) {
			vfprintf(stderr, fmt, ap);
			fprintf(stderr, "\n");
		} else {
			vfprintf(stderr, nfmt, ap);
			free(nfmt);
		}
		fflush(stderr);
	} else
		vsyslog(pri, fmt, ap);
}

void
log_warn(const char *emsg, ...)
{
	char	*nfmt;
	va_list	 ap;

	/* best effort to even work in out of memory situations */
	if (emsg == NULL)
		logit(LOG_CRIT, "%s", strerror(errno));
	else {
		va_start(ap, emsg);

		if (asprintf(&nfmt, "%s: %s", emsg, strerror(errno)) == -1) {
			/* we tried it... */
			vlog(LOG_CRIT, emsg, ap);
			logit(LOG_CRIT, "%s", strerror(errno));
		} else {
			vlog(LOG_CRIT, nfmt, ap);
			free(nfmt);
		}
		va_end(ap);
	}
}

void
log_warnx(const char *emsg, ...)
{
	va_list	 ap;

	va_start(ap, emsg);
	vlog(LOG_CRIT, emsg, ap);
	va_end(ap);
}

void
log_info(const char *emsg, ...)
{
	va_list	 ap;

	va_start(ap, emsg);
	vlog(LOG_INFO, emsg, ap);
	va_end(ap);
}

void
log_debug(const char *emsg, ...)
{
	va_list	 ap;

	if (verbose & LDPD_OPT_VERBOSE) {
		va_start(ap, emsg);
		vlog(LOG_DEBUG, emsg, ap);
		va_end(ap);
	}
}

void
fatal(const char *emsg)
{
	if (emsg == NULL)
		logit(LOG_CRIT, "fatal in %s: %s", procnames[ldpd_process],
		    strerror(errno));
	else
		if (errno)
			logit(LOG_CRIT, "fatal in %s: %s: %s",
			    procnames[ldpd_process], emsg, strerror(errno));
		else
			logit(LOG_CRIT, "fatal in %s: %s",
			    procnames[ldpd_process], emsg);

	if (ldpd_process == PROC_MAIN)
		exit(1);
	else				/* parent copes via SIGCHLD */
		_exit(1);
}

void
fatalx(const char *emsg)
{
	errno = 0;
	fatal(emsg);
}

#define NUM_LOGS	4
const char *
log_sockaddr(void *vp)
{
	static char	 buf[NUM_LOGS][NI_MAXHOST];
	static int	 round = 0;
	struct sockaddr	*sa = vp;

	round = (round + 1) % NUM_LOGS;

	if (getnameinfo(sa, sa->sa_len, buf[round], NI_MAXHOST, NULL, 0,
	    NI_NUMERICHOST))
		return ("(unknown)");
	else
		return (buf[round]);
}

const char *
log_in6addr(const struct in6_addr *addr)
{
	struct sockaddr_in6	sa_in6;

	memset(&sa_in6, 0, sizeof(sa_in6));
	sa_in6.sin6_len = sizeof(sa_in6);
	sa_in6.sin6_family = AF_INET6;
	sa_in6.sin6_addr = *addr;

	recoverscope(&sa_in6);

	return (log_sockaddr(&sa_in6));
}

const char *
log_in6addr_scope(const struct in6_addr *addr, unsigned int ifindex)
{
	struct sockaddr_in6	sa_in6;

	memset(&sa_in6, 0, sizeof(sa_in6));
	sa_in6.sin6_len = sizeof(sa_in6);
	sa_in6.sin6_family = AF_INET6;
	sa_in6.sin6_addr = *addr;

	addscope(&sa_in6, ifindex);

	return (log_sockaddr(&sa_in6));
}

const char *
log_addr(int af, const union ldpd_addr *addr)
{
	static char	 buf[NUM_LOGS][INET6_ADDRSTRLEN];
	static int	 round = 0;

	switch (af) {
	case AF_INET:
		round = (round + 1) % NUM_LOGS;
		if (inet_ntop(AF_INET, &addr->v4, buf[round],
		    sizeof(buf[round])) == NULL)
			return ("???");
		return (buf[round]);
	case AF_INET6:
		return (log_in6addr(&addr->v6));
	default:
		break;
	}

	return ("???");
}

/* names */
const char *
af_name(int af)
{
	switch (af) {
	case AF_INET:
		return ("ipv4");
	case AF_INET6:
		return ("ipv6");
	case AF_MPLS:
		return ("mpls");
	default:
		return ("UNKNOWN");
	}
}

const char *
socket_name(int type)
{
	switch (type) {
	case LDP_SOCKET_DISC:
		return ("discovery");
	case LDP_SOCKET_EDISC:
		return ("extended discovery");
	case LDP_SOCKET_SESSION:
		return ("session");
	default:
		return ("UNKNOWN");
	}
}

const char *
nbr_state_name(int state)
{
	switch (state) {
	case NBR_STA_PRESENT:
		return ("PRESENT");
	case NBR_STA_INITIAL:
		return ("INITIALIZED");
	case NBR_STA_OPENREC:
		return ("OPENREC");
	case NBR_STA_OPENSENT:
		return ("OPENSENT");
	case NBR_STA_OPER:
		return ("OPERATIONAL");
	default:
		return ("UNKNOWN");
	}
}

const char *
if_state_name(int state)
{
	switch (state) {
	case IF_STA_DOWN:
		return ("DOWN");
	case IF_STA_ACTIVE:
		return ("ACTIVE");
	default:
		return ("UNKNOWN");
	}
}

const char *
if_type_name(enum iface_type type)
{
	switch (type) {
	case IF_TYPE_POINTOPOINT:
		return ("POINTOPOINT");
	case IF_TYPE_BROADCAST:
		return ("BROADCAST");
	}
	/* NOTREACHED */
	return ("UNKNOWN");
}

const char *
status_code_name(uint32_t status)
{
	static char buf[16];

	switch (status) {
	case S_SUCCESS:
		return ("Success");
	case S_BAD_LDP_ID:
		return ("Bad LDP Identifier");
	case S_BAD_PROTO_VER:
		return ("Bad Protocol Version");
	case S_BAD_PDU_LEN:
		return ("Bad PDU Length");
	case S_UNKNOWN_MSG:
		return ("Unknown Message Type");
	case S_BAD_MSG_LEN:
		return ("Bad Message Length");
	case S_UNKNOWN_TLV:
		return ("Unknown TLV");
	case S_BAD_TLV_LEN:
		return ("Bad TLV Length");
	case S_BAD_TLV_VAL:
		return ("Malformed TLV Value");
	case S_HOLDTIME_EXP:
		return ("Hold Timer Expired");
	case S_SHUTDOWN:
		return ("Shutdown");
	case S_LOOP_DETECTED:
		return ("Loop Detected");
	case S_UNKNOWN_FEC:
		return ("Unknown FEC");
	case S_NO_ROUTE:
		return ("No Route");
	case S_NO_LABEL_RES:
		return ("No Label Resources");
	case S_AVAILABLE:
		return ("Label Resources Available");
	case S_NO_HELLO:
		return ("Session Rejected, No Hello");
	case S_PARM_ADV_MODE:
		return ("Rejected Advertisement Mode Parameter");
	case S_MAX_PDU_LEN:
		return ("Rejected Max PDU Length Parameter");
	case S_PARM_L_RANGE:
		return ("Rejected Label Range Parameter");
	case S_KEEPALIVE_TMR:
		return ("KeepAlive Timer Expired");
	case S_LAB_REQ_ABRT:
		return ("Label Request Aborted");
	case S_MISS_MSG:
		return ("Missing Message Parameters");
	case S_UNSUP_ADDR:
		return ("Unsupported Address Family");
	case S_KEEPALIVE_BAD:
		return ("Bad KeepAlive Time");
	case S_INTERN_ERR:
		return ("Internal Error");
	case S_ILLEGAL_CBIT:
		return ("Illegal C-Bit");
	case S_WRONG_CBIT:
		return ("Wrong C-Bit");
	case S_INCPT_BITRATE:
		return ("Incompatible bit-rate");
	case S_CEP_MISCONF:
		return ("CEP-TDM mis-configuration");
	case S_PW_STATUS:
		return ("PW Status");
	case S_UNASSIGN_TAI:
		return ("Unassigned/Unrecognized TAI");
	case S_MISCONF_ERR:
		return ("Generic Misconfiguration Error");
	case S_WITHDRAW_MTHD:
		return ("Label Withdraw PW Status Method");
	case S_TRANS_MISMTCH:
		return ("Transport Connection Mismatch");
	case S_DS_NONCMPLNCE:
		return ("Dual-Stack Noncompliance");
	default:
		snprintf(buf, sizeof(buf), "[%08x]", status);
		return (buf);
	}
}

const char *
pw_type_name(uint16_t pw_type)
{
	static char buf[64];

	switch (pw_type) {
	case PW_TYPE_ETHERNET_TAGGED:
		return ("Eth Tagged");
	case PW_TYPE_ETHERNET:
		return ("Ethernet");
	default:
		snprintf(buf, sizeof(buf), "[%0x]", pw_type);
		return (buf);
	}
}

char *
log_hello_src(const struct hello_source *src)
{
	static char buffer[64];

	switch (src->type) {
	case HELLO_LINK:
		snprintf(buffer, sizeof(buffer), "iface %s",
		    src->link.ia->iface->name);
		break;
	case HELLO_TARGETED:
		snprintf(buffer, sizeof(buffer), "source %s",
		    log_addr(src->target->af, &src->target->addr));
		break;
	}

	return (buffer);
}

const char *
log_map(const struct map *map)
{
	static char	buf[64];
	int		af;

	switch (map->type) {
	case MAP_TYPE_WILDCARD:
		if (snprintf(buf, sizeof(buf), "wildcard") < 0)
			return ("???");
		break;
	case MAP_TYPE_PREFIX:
		switch (map->fec.prefix.af) {
		case AF_IPV4:
			af = AF_INET;
			break;
		case AF_IPV6:
			af = AF_INET6;
			break;
		default:
			return ("???");
		}

		if (snprintf(buf, sizeof(buf), "%s/%u",
		    log_addr(af, &map->fec.prefix.prefix),
		    map->fec.prefix.prefixlen) == -1)
			return ("???");
		break;
	case MAP_TYPE_PWID:
		if (snprintf(buf, sizeof(buf), "pwid %u (%s)",
		    map->fec.pwid.pwid,
		    pw_type_name(map->fec.pwid.type)) == -1)
			return ("???");
		break;
	default:
		return ("???");
	}

	return (buf);
}

const char *
log_fec(const struct fec *fec)
{
	static char	buf[64];
	union ldpd_addr	addr;

	switch (fec->type) {
	case FEC_TYPE_IPV4:
		addr.v4 = fec->u.ipv4.prefix;
		if (snprintf(buf, sizeof(buf), "ipv4 %s/%u",
		    log_addr(AF_INET, &addr), fec->u.ipv4.prefixlen) == -1)
			return ("???");
		break;
	case FEC_TYPE_IPV6:
		addr.v6 = fec->u.ipv6.prefix;
		if (snprintf(buf, sizeof(buf), "ipv6 %s/%u",
		    log_addr(AF_INET6, &addr), fec->u.ipv6.prefixlen) == -1)
			return ("???");
		break;
	case FEC_TYPE_PWID:
		if (snprintf(buf, sizeof(buf),
		    "pwid %u (%s) - %s",
		    fec->u.pwid.pwid, pw_type_name(fec->u.pwid.type),
		    inet_ntoa(fec->u.pwid.lsr_id)) == -1)
			return ("???");
		break;
	default:
		return ("???");
	}

	return (buf);
}

static char *msgtypes[] = {
	"",
	"RTM_ADD: Add Route",
	"RTM_DELETE: Delete Route",
	"RTM_CHANGE: Change Metrics or flags",
	"RTM_GET: Report Metrics",
	"RTM_LOSING: Kernel Suspects Partitioning",
	"RTM_REDIRECT: Told to use different route",
	"RTM_MISS: Lookup failed on this address",
	"RTM_LOCK: fix specified metrics",
	"RTM_OLDADD: caused by SIOCADDRT",
	"RTM_OLDDEL: caused by SIOCDELRT",
	"RTM_RESOLVE: Route created by cloning",
	"RTM_NEWADDR: address being added to iface",
	"RTM_DELADDR: address being removed from iface",
	"RTM_IFINFO: iface status change",
	"RTM_IFANNOUNCE: iface arrival/departure",
	"RTM_DESYNC: route socket overflow",
};

void
log_rtmsg(unsigned char rtm_type)
{
	if (!(verbose & LDPD_OPT_VERBOSE2))
		return;

	if (rtm_type > 0 &&
	    rtm_type < sizeof(msgtypes)/sizeof(msgtypes[0]))
		log_debug("kernel message: %s", msgtypes[rtm_type]);
	else
		log_debug("kernel message: rtm_type %d out of range",
		    rtm_type);
}
