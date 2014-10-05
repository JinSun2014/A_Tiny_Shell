/***************************************************************************
 *  Title: MySimpleShell 
 * -------------------------------------------------------------------------
 *    Purpose: A simple shell implementation 
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2005/10/13 05:24:59 $
 *    File: $RCSfile: tsh.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: tsh.c,v $
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.4  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.3  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __MYSS_IMPL__

/************System include***********************************************/
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>

/************Private include**********************************************/
#include "tsh.h"
#include "io.h"
#include "interpreter.h"
#include "runtime.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

#define BUFSIZE 80
#define MAXPATH 80
/************Global Variables*********************************************/

/************Function Prototypes******************************************/
/* handles SIGINT and SIGSTOP signals */
extern int fgChild;
static void sig(int);
static void sigHandler(int);
void printPrompt();
extern pid_t waitpid(pid_t pid, int* status, int options);
extern bgjobL* bgjobs;
void changeStatus(pid_t id, state newStatus);
/************External Declaration*****************************************/

/**************Implementation***********************************************/

int main (int argc, char *argv[])
{
  /* Initialize command buffer */
  char* cmdLine = malloc(sizeof(char*)*BUFSIZE);

  /* shell initialization */
  if (signal(SIGINT, sig) == SIG_ERR) PrintPError("SIGINT");
  if (signal(SIGTSTP, sig) == SIG_ERR) PrintPError("SIGTSTP");
  if (signal(SIGCONT, sig) == SIG_ERR) PrintPError("SIGCONT");
  if (signal(SIGCHLD, sigHandler) == SIG_ERR) PrintPError("SIGCHLD");


  while (!forceExit) /* repeat forever */
  {
	printPrompt();
    /* read command line */
    getCommandLine(&cmdLine, BUFSIZE);
//	printf("cmdLine: %s", cmdLine);

	// strcmp: string compare
    if(strcmp(cmdLine, "exit") == 0)
    {
      forceExit=TRUE;
      continue;
    }

    /* checks the status of background jobs */
    CheckJobs();

    /* interpret command and line
     * includes executing of commands */
    Interpret(cmdLine);

  }

  /* shell termination */
  free(cmdLine);
  return 0;
} /* end main */

static void sig(int signo)
{
	switch (signo)
	{
		case SIGINT:
			printf("SIGINT \n");
			break;
		case SIGTSTP:
			printf("SIGTSTP \n");
			break;
		case SIGCONT:
			printf("SIGCONT \n");
			break;
	}
}

// used to handler terminated process in background
static void sigHandler(int signo)
{
	printf("SIGCHLD \n");
	pid_t pid;
	int status;
	pid = waitpid(-1, &status, WNOHANG|WUNTRACED);
	printf("status: %d \n", status);
	printf("sigHandler pid: %d \n", pid);
	// handle fg jobs
	if (pid == fgChild)
	{
		fgChild = 0;
		return;
	}
	changeStatus(pid, DONE);
}

void printPrompt()
{
	// get current directory path
	char buffer[MAXPATH];
	char* result = getcwd(buffer, MAXPATH);
	//substr(result, buffer, 13, strlen(buffer));
	printf("%s$> ", result);
}

void changeStatus(pid_t id, state newStatus)
{
	printf("changeStatus id: %d \n", id);
	if (bgjobs == NULL || bgjobs->next == NULL)
	{
		return;
	}
	bgjobL* cursor = bgjobs->next;
	while (cursor)
	{
		if (cursor->pid == id)
		{
			//cursor->status = DONE;
			cursor->status = newStatus;
			return;
		}
		cursor = cursor->next;
	}
}
