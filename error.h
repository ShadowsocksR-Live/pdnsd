/* error.h - Error handling
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

/* $Id: error.h,v 1.7 2000/06/22 09:57:34 thomas Exp $ */

#ifndef _ERROR_H_
#define _ERROR_H_

#include "config.h"
#include <stdio.h>
#include <signal.h>

/* --- From main.c */
extern sigset_t sigs_msk;
extern int waiting;
/* --- */

#if TARGET==TARGET_LINUX
void fatal_sig(int sig);
#endif
void crash_msg(char *msg);
void init_log(void);
void log_error(char *s,...);
void log_warn(char *s, ...);
void log_info(int level, char *s, ...);

/* These are macros for setting up the signal handling of a new thread. They
 * are needed because the LinuxThreads implementation obviously has some
 * problems in signal handling, which makes the recommended solution (doing
 * sigwait() in one thread and blocking the signals in all threads) impossible.
 * So, for Linux, we have to install the fatal_sig handler. 
 * It seems to me that signal handlers in fact aren't shared between threads
 * under Linux. Also, sigwait() does not seem to work as indicated in the docs */
#if TARGET==TARGET_LINUX
#define THREAD_SIGINIT	pthread_sigmask(SIG_UNBLOCK,&sigs_msk,NULL); \
                        signal(SIGILL,fatal_sig); \
	                signal(SIGABRT,fatal_sig); \
	                signal(SIGFPE,fatal_sig); \
	                signal(SIGSEGV,fatal_sig); \
	                signal(SIGPIPE,fatal_sig);

#else
#define THREAD_SIGINIT pthread_sigmask(SIG_BLOCK,&sigs_msk,NULL)
#endif


/* Following are some ugly macros for debug messages that
 * should inhibit any code generation when DEBUG is not defined.
 * Of course, those messages could be done in a function, but I
 * want to save the overhead when DEBUG is not defined. 
 * debug_p needs to be defined (by including conff.h), or you
 * will get strange errors.
 * Dont forget your semicolon after the macro call, or you
 * will get other strange errors ;-)
 * The arguments are normal printfs, so you know how to use the args
 */
#if DEBUG>0
/* from main.c */
extern FILE *dbg;

#define DEBUG_MSG1(x)           if (debug_p) \
                                   fprintf(dbg,x)
#define DEBUG_MSG2(x,y)         if (debug_p) \
                                   fprintf(dbg,x,y)
#define DEBUG_MSG3(x,y,z)       if (debug_p) \
                                   fprintf(dbg,x,y,z)
#define DEBUG_MSG4(x,y,z,a)     if (debug_p) \
                                   fprintf(dbg,x,y,z,a)
#define DEBUG_MSG5(x,y,z,a,b)   if (debug_p) \
                                   fprintf(dbg,x,y,z,a,b)
#define DEBUG_MSG6(x,y,z,a,b,c) if (debug_p) \
                                   fprintf(dbg,x,y,z,a,b,c)
#else
#define DEBUG_MSG1(x) 
#define DEBUG_MSG2(x,y) 
#define DEBUG_MSG3(x,y,z) 
#define DEBUG_MSG4(x,y,z,a)
#define DEBUG_MSG5(x,y,z,a,b)
#define DEBUG_MSG6(x,y,z,a,b,c)
#endif

#endif