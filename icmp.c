/* icmp.c - Server response tests using ICMP echo requests
   Copyright (C) 2000 Thomas Moestl

This file is part of the pdnsd package.

pdnsd is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2, or (at your option)
any later version.

pdnsd is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with pdsnd; see the file COPYING.  If not, write to
the Free Software Foundation, 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

/*
 * This should now work on both Linux and FreeBSD. If anyone
 * with experience in other Unix flavors wants to contribute platform-specific
 * code, he is very welcome. 
 */

#include "config.h"
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <string.h>
#include "ipvers.h"
#if TARGET==TARGET_BSD
# include <netinet/in_systm.h>
#endif
#include <netinet/ip.h>
#if TARGET==TARGET_LINUX
# include <linux/types.h>
# include <linux/icmp.h>
#else
# include <netinet/ip_icmp.h>
#endif
#ifdef ENABLE_IPV6
# include <netinet/ip6.h>
# include <netinet/icmp6.h>
#endif
#include <netinet/ip.h>
#include <netdb.h>
#include "icmp.h"
#include "error.h"

#if !defined(lint) && !defined(NO_RCSIDS)
static char rcsid[]="$Id: icmp.c,v 1.8 2000/06/06 21:20:52 thomas Exp $";
#endif

#define ICMP_MAX_ERRS 5
int icmp_errs=0; /* This is only here to minimize log output. Since the 
		    consequences of a race is only one log message more/less
		    (out of ICMP_MAX_ERRS), no lock is required. */

#if TARGET==TARGET_BSD
# define icmphdr   icmp
# define iphdr     ip
# define ip_ihl    ip_hl
# define ip_saddr  ip_src.s_addr
#else
# define ip_saddr  saddr
# define ip_ihl    ihl
#endif

#if TARGET==TARGET_LINUX
# define icmp_type  type
# define icmp_code  code
# define icmp_cksum checksum
# define icmp_id un.echo.id
# define icmp_seq un.echo.sequence
#endif

#if (TARGET==TARGET_LINUX) || (TARGET==TARGET_BSD)
/*
 * These are the ping implementations for Linux in ther IPv4/ICMPv4 and IPv6/ICMPv6 versions.
 * I know they share some code, but I'd rather keep them separated in some parts, as some
 * things might go in different directions there.
 * Btw., the Linux version of ping6 should be fairly portable, according to rfc2292.
 * The ping4 might be, but well, try it.
 */

/* glibc2.0 versions don't have some Linux 2.2 Kernel #defines */
#ifndef MSG_ERRQUEUE
# define MSG_ERRQUEUE 0x2000
#endif


/* glibc2.0 versions don't have some Linux 2.2 Kernel #defines */
#ifndef IP_RECVERR
# define IP_RECVERR 11
#endif

/* IPv4/ICMPv4 ping. Called from ping (see below) */
static int ping4(struct in_addr addr, int timeout, int rep)
{
	char buf[1024];
	int i,tm;
	int rve=1;
	int len;
	int isock,osock;
#if TARGET==TARGET_LINUX
	struct icmp_filter f;
#else
	struct protoent *pe;
	int SOL_IP;
#endif
	struct sockaddr_in from;
	struct icmphdr icmpd;
	struct icmphdr *icmpp;
	struct msghdr msg;
	unsigned long sum;
	unsigned short *ptr;
	unsigned short id=(unsigned short)(rand()&0xffff); /* randomize a ping id */
	socklen_t sl;

#if TARGET!=TARGET_LINUX	
	if (!(pe=getprotobyname("ip"))) {
		if (icmp_errs<ICMP_MAX_ERRS) {
			icmp_errs++;
			log_warn("icmp ping: getprotobyname() failed: %s",strerror(errno));
		}
		return -1;
	}
	SOL_IP=pe->p_proto;
#endif

	for (i=0;i<rep;i++) {
		/* Open a raw socket for replies */
		isock=socket(PF_INET, SOCK_RAW, IPPROTO_ICMP);
		if (isock==-1) {
			if (icmp_errs<ICMP_MAX_ERRS) {
				icmp_errs++;
				log_warn("icmp ping: socket() failed: %s",strerror(errno));
			}
			return -1;
		}
#if TARGET==TARGET_LINUX
		/* Fancy ICMP filering -- only on Linux */
		
		/* In fact, there should be macros for treating icmp_filter, but I haven't found them in Linux 2.2.15.
		 * So, set it manually and unportable ;-) */
		f.data=0xfffffffe;
		if (setsockopt(isock,SOL_RAW,ICMP_FILTER,&f,sizeof(f))==-1) {
			if (icmp_errs<ICMP_MAX_ERRS) {
				icmp_errs++;
				log_warn("icmp ping: setsockopt() failed: %s", strerror(errno));
			}
			close(isock);
			return -1;
		}
#endif
		fcntl(isock,F_SETFL,O_NONBLOCK);
		/* send icmp_echo_request */
		osock=socket(PF_INET,SOCK_RAW,IPPROTO_ICMP);
		if (osock==-1) {
			if (icmp_errs<ICMP_MAX_ERRS) {
				icmp_errs++;
				log_warn("icmp ping: socket() failed.");
			}
			close(isock);
			return -1;
		}
		if (setsockopt(osock,SOL_IP,IP_RECVERR,&rve,sizeof(rve))==-1) {
			if (icmp_errs<ICMP_MAX_ERRS) {
				icmp_errs++;
				log_warn("icmp ping: setsockopt() failed: %s",strerror(errno));
			}
			close(osock);
			close(isock);
			return -1;
		}
		icmpd.icmp_type=ICMP_ECHO;
		icmpd.icmp_code=0;
		icmpd.icmp_cksum=0;
		icmpd.icmp_id=htons((short)id);
		icmpd.icmp_seq=htons((short)i);

		/* Checksumming - Algorithm taken from nmap. Thanks... */

		ptr=(unsigned short *)&icmpd;
		sum=0;

		for (len=0;len<4;len++) {
			sum+=*ptr++;
		}
		sum = (sum >> 16) + (sum & 0xffff);
		sum += (sum >> 16);
		icmpd.icmp_cksum=~sum;


		from.sin_family=AF_INET;
		from.sin_port=0;
		from.sin_addr=addr;
		SET_SOCKA_LEN4(from);
		if (sendto(osock,&icmpd,8,0,(struct sockaddr *)&from,sizeof(from))==-1) {
			if (icmp_errs<ICMP_MAX_ERRS) {
				icmp_errs++;
				log_warn("icmp ping: sendto() failed: %s.",strerror(errno));

			}
			close(osock);
			close(isock);
			return -1;
		}
		fcntl(osock,F_SETFL,O_NONBLOCK);
		/* listen for reply. */
		tm=0;
		do {
			memset(&msg,0,sizeof(msg));
			msg.msg_control=buf;
			msg.msg_controllen=1024;
			if (recvmsg(osock,&msg,MSG_ERRQUEUE)!=-1) {
				if (*((unsigned int *)buf)!=0) {
					close(osock);
					close(isock);
					return -1;  /* error in sending (e.g. no route to host) */
				}
			}
			sl=sizeof(from);
			if ((len=recvfrom(isock,&buf,sizeof(buf),0,(struct sockaddr *)&from,&sl))!=-1) {
				if (len>20 && len-((struct iphdr *)buf)->ip_ihl*4>=8) {
					icmpp=(struct icmphdr *)(((unsigned long int *)buf)+((struct iphdr *)buf)->ip_ihl);
					if (((struct iphdr *)buf)->ip_saddr==addr.s_addr &&
					     icmpp->icmp_type==ICMP_ECHOREPLY && ntohs(icmpp->icmp_id)==id && ntohs(icmpp->icmp_seq)<=i) {
						close(osock);
						close(isock);
						return (i-ntohs(icmpp->icmp_seq))*timeout+tm; /* return the number of ticks */
					}
				}
			} else {
				if (errno!=EAGAIN)
				{
					close(osock);
					close(isock);
					return -1; /* error */
				}
			}
			usleep(100000);
			tm++;
		} while (tm<timeout);
		close(osock);
		close(isock);
	}
	return -1; /* no answer */
}


#ifdef ENABLE_IPV6

/* glibc2.0 versions don't have some Linux 2.2 Kernel #defines */
#ifndef IPV6_RECVERR
# define IPV6_RECVERR  25
#endif
#ifndef IPV6_CHECKSUM
# define IPV6_CHECKSUM  7
#endif

/* IPv6/ICMPv6 ping. Called from ping (see below) */
static int ping6(struct in6_addr a, int timeout, int rep)
{
	char buf[1024];
	int i,tm;
	int rve=1;
/*	int ck_offs=2;*/
	int len;
	int isock,osock;
	struct icmp6_filter f;
	struct sockaddr_in6 from;
	struct icmp6_hdr icmpd;
	struct icmp6_hdr *icmpp;
	struct msghdr msg;
	unsigned short id=(unsigned short)(rand()&0xffff); /* randomize a ping id */
	socklen_t sl;
#if TARGET!=TARGET_LINUX
	int SOL_IPV6;
	struct protoent *pe;

	if (!(pe=getprotobyname("ipv6"))) {
		if (icmp_errs<ICMP_MAX_ERRS) {
			icmp_errs++;
			log_warn("icmp ping: getprotobyname() failed: %s",strerror(errno));
		}
		return -1;
	}
	SOL_IPV6=pe->p_proto;
#endif


	ICMP6_FILTER_SETBLOCKALL(&f);
	ICMP6_FILTER_SETPASS(ICMP6_ECHO_REQUEST,&f);

	for (i=0;i<rep;i++) {
		/* Open a raw socket for replies */
		isock=socket(PF_INET6, SOCK_RAW, IPPROTO_ICMPV6);
		if (isock==-1) {
			if (icmp_errs<ICMP_MAX_ERRS) {
				icmp_errs++;
				log_warn("icmpv6 ping: socket() failed: %s",strerror(errno));
			}
			return -1;
		}
		if (setsockopt(isock,IPPROTO_ICMPV6,ICMP6_FILTER,&f,sizeof(f))==-1) {
			if (icmp_errs<ICMP_MAX_ERRS) {
				icmp_errs++;
				log_warn("icmpv6 ping: setsockopt() for is failed: %s", strerror(errno));
			}
			close(isock);
			return -1;
		}
		fcntl(isock,F_SETFL,O_NONBLOCK);
		/* send icmp_echo_request */
		osock=socket(PF_INET6,SOCK_RAW,IPPROTO_ICMPV6);
		if (osock==-1) {
			if (icmp_errs<ICMP_MAX_ERRS) {
				icmp_errs++;
				log_warn("icmpv6 ping: socket() failed.");
			}
			close(isock);
			return -1;
		}

		/* enable error queueing and checksumming. --checksumming should be on by default.*/
		if (setsockopt(osock,SOL_IPV6,IPV6_RECVERR,&rve,sizeof(rve))==-1 /*|| 
 		    setsockopt(osock,IPPROTO_ICMPV6,IPV6_CHECKSUM,&ck_offs,sizeof(ck_offs))==-1*/) {
			if (icmp_errs<ICMP_MAX_ERRS) {
				icmp_errs++;
				log_warn("icmpv6 ping: setsockopt() for os failed: %s",strerror(errno));
			}
			close(osock);
			close(isock);
			return -1;
		}
		icmpd.icmp6_type=ICMP6_ECHO_REQUEST;
		icmpd.icmp6_code=0;
		icmpd.icmp6_cksum=0; /* The friently kernel does fill that in for us. */
		icmpd.icmp6_id=htons((short)id);
		icmpd.icmp6_seq=htons((short)i);
		
		from.sin6_family=AF_INET6;
		from.sin6_flowinfo=IPV6_FLOWINFO;
		from.sin6_port=0;
		from.sin6_addr=a;
		SET_SOCKA_LEN6(from);
/*		printf("to: %s.\n",inet_ntop(AF_INET6,&from.sin6_addr,buf,1024));*/
		if (sendto(osock,&icmpd,sizeof(icmpd),0,(struct sockaddr *)&from,sizeof(from))==-1) {
			if (icmp_errs<ICMP_MAX_ERRS) {
				icmp_errs++;
				log_warn("icmpv6 ping: sendto() failed: %s.",strerror(errno));

			}
			close(osock);
			close(isock);
			return -1;
		}
		fcntl(osock,F_SETFL,O_NONBLOCK);
		/* listen for reply. */
		tm=0;
		do {
			memset(&msg,0,sizeof(msg));
			msg.msg_control=buf;
			msg.msg_controllen=1024;
			if (recvmsg(osock,&msg,MSG_ERRQUEUE)!=-1) {
				if (*((unsigned int *)buf)!=0) {
					close(osock);
					close(isock);
					return -1;  /* error in sending (e.g. no route to host) */
				}
			}
			sl=sizeof(from);
/*			printf("before: %s.\n",inet_ntop(AF_INET6,&from.sin6_addr,buf,1024));*/
			if ((len=recvfrom(isock,&buf,sizeof(buf),0,(struct sockaddr *)&from,&sl))!=-1) {
				if (len>=sizeof(struct icmp6_hdr)) {
/*	   			        printf("reply: %s.\n",inet_ntop(AF_INET6,&from.sin6_addr,buf,1024));*/
				        /* we get packets without IPv6 header, luckily */
					icmpp=(struct icmp6_hdr *)buf;
				        /* The address comparation was diked out because some linux versions
				         * seem to have problems with it. */
					if (/*IN6_ARE_ADDR_EQUAL(&from.sin6_addr,&a) &&*/
						ntohs(icmpp->icmp6_id)==id && ntohs(icmpp->icmp6_seq)<=i) {
						close(osock);
						close(isock);
						return (i-ntohs(icmpp->icmp6_seq))*timeout+tm; /* return the number of ticks */
					}
				}
			} else {
				if (errno!=EAGAIN)
				{
					close(osock);
					close(isock);
					return -1; /* error */
				}
			}
			usleep(100000);
			tm++;
		} while (tm<timeout);
		close(osock);
		close(isock);
	}
	return -1; /* no answer */
}
#endif /* ENABLE_IPV6*/


/* Perform an icmp ping on a host, returning -1 on timeout or 
 * "host unreachable" or the ping time in 10ths of secs.
 * timeout in 10ths of seconds, rep is the repetition count
 */
int ping(pdnsd_a *addr, int timeout, int rep)
{
#ifdef ENABLE_IPV6
	struct in_addr v4;
#endif

#ifdef ENABLE_IPV4
	if (run_ipv4)
		return ping4(addr->ipv4,timeout,rep);
#endif
#ifdef ENABLE_IPV6
	if (run_ipv6) {
		/* If it is a IPv4 mapped IPv6 address, we prefer ICMPv4. */
		if (IN6_IS_ADDR_V4MAPPED(&addr->ipv6)) {
			v4.s_addr=((long *)&addr->ipv6)[3];
			return ping4(v4,timeout,rep);
		} else 
			return ping6(addr->ipv6,timeout,rep);
	}
#endif
	return -1;
}

#else
# error "No OS macro defined. Please look into config.h.templ."
#endif /*TARGET==TARGET_LINUX || TARGET==TARGET_BSD*/