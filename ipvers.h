/* ipvers.h - definitions for IPv4 and IPv6
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

/* $Id: ipvers.h,v 1.5 2000/06/13 12:12:27 thomas Exp $ */

#ifndef _IPVERS_H_
#define _IPVERS_H_

#include "config.h"
#include "a-conf.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#if !defined(ENABLE_IPV4) && !defined(ENABLE_IPV6)
# error "You need to define either or both of ENABLE_IPV4 and ENABLE_IPV6. Look into your config.h.templ"
#endif

/* From main.c */
#ifdef ENABLE_IPV4
extern int run_ipv4;
#endif
#ifdef ENABLE_IPV6
extern int run_ipv6;
#endif

#if defined(ENABLE_IPV4) && TARGET==LINUX && defined(NO_IN_PKTINFO)
struct in_pktinfo
{
	int		ipi_ifindex;
	struct in_addr	ipi_spec_dst;
	struct in_addr	ipi_addr;
};
#endif

#if defined(ENABLE_IPV4) && !defined(SIN_LEN) && (TARGET==TARGET_BSD)
# define SIN_LEN
#endif 

#if defined(ENABLE_IPV6) && TARGET==LINUX

/* Some glibc versions (I know of 2.1.2) get this wrong, so we define out own. To be exact, this is fixed
 * glibc code. */
#ifdef IN6_ARE_ADDR_EQUAL
# undef IN6_ARE_ADDR_EQUAL
#endif
#define IN6_ARE_ADDR_EQUAL(a,b) \
	((((uint32_t *) (a))[0] == ((uint32_t *) (b))[0]) && \
	 (((uint32_t *) (a))[1] == ((uint32_t *) (b))[1]) && \
	 (((uint32_t *) (a))[2] == ((uint32_t *) (b))[2]) && \
	 (((uint32_t *) (a))[3] == ((uint32_t *) (b))[3]))

#endif

/* This is the IPv6 flowid that we pass on to the IPv6 protocol stack. This value was not currently defined
 * at the time of writing. Should this change, define a appropriate flowinfo here. */
#define IPV6_FLOWINFO 0

/* There does not seem to be a function/macro to generate IPv6-mapped IPv4-Adresses. So here comes mine. 
 * Pass a in_addr* and a in6_addr* */
#define IPV6_MAPIPV4(a,b) ((uint32_t *)(b))[3]=(a)->s_addr;((uint32_t *)(b))[2]=htonl(0xffff);((uint32_t *)(b))[1]=((uint32_t *)(b))[0]=0

/* A macro to extract the pointer to the address of a struct sockaddr (_in or _in6) */

#define SOCKA_A4(a) ((void *)&((struct sockaddr_in *)(a))->sin_addr)
#define SOCKA_A6(a) ((void *)&((struct sockaddr_in6 *)(a))->sin6_addr)

#ifdef ENABLE_IPV4
# ifdef ENABLE_IPV6
#  define SOCKA_A(a) (run_ipv4?SOCKA_A4(a):SOCKA_A6(a))
# else
#  define SOCKA_A(a) SOCKA_A4(a)
# endif
#else
# define SOCKA_A(a) SOCKA_A6(a)
#endif

/* This is to compare two addresses. This is a macro because it may change due to the more complex IPv6 adressing architecture
 * (there are, for example, two equivalent addresses of the loopback device) 
 * Pass this two addresses as in_addr or in6_addr. pdnsd_a is ok (it is a union) */

#define ADDR_EQUIV4(a,b) (((struct in_addr *)(a))->s_addr==((struct in_addr *)(b))->s_addr)
#define ADDR_EQUIV6(a,b) IN6_ARE_ADDR_EQUAL(((struct in6_addr *)(a)),((struct in6_addr *)(b)))

#ifdef ENABLE_IPV4
# ifdef ENABLE_IPV6
#  define ADDR_EQUIV(a,b) ((run_ipv4 && ADDR_EQUIV4(a,b)) || (run_ipv6 && ADDR_EQUIV6(a,b)))
# else
#  define ADDR_EQUIV(a,b) ADDR_EQUIV4(a,b)
# endif
#else
# define ADDR_EQUIV(a,b) ADDR_EQUIV6(a,b)
#endif

/* See if we need 4.4BSD style sockaddr_* structures and define some macros that set the length field. 
 * The non-4.4BSD behaviour is the only one that is POSIX-conformant.*/
#if defined(SIN6_LEN) || defined(SIN_LEN)
# define BSD44_SOCKA
# define SET_SOCKA_LEN4(socka) (socka.sin_len=sizeof(struct sockaddr_in))
# define SET_SOCKA_LEN6(socka) (socka.sin6_len=sizeof(struct sockaddr_in6))
#else
# define SET_SOCKA_LEN4(socka)
# define SET_SOCKA_LEN6(socka)
#endif

#ifndef ENABLE_IPV6
# define ADDRSTR_MAXLEN INET6_ADDRSTRLEN
#else
# ifdef INET_ADDRSTRLEN
#  define ADDRSTR_MAXLEN INET_ADDRSTRLEN
# else
#  define ADDRSTR_MAXLEN 16
# endif
#endif

typedef union {
#ifdef ENABLE_IPV4
	struct in_addr   ipv4;
#endif
#ifdef ENABLE_IPV6
	struct in6_addr  ipv6;
#endif
} pdnsd_a;


#endif