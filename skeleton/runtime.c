/***************************************************************************
 *  Title: Runtime environment 
 * -------------------------------------------------------------------------
 *    Purpose: Runs commands
 *    Author: Stefan Birrer
 *    Version: $Revision: 1.1 $
 *    Last Modification: $Date: 2005/10/13 05:24:59 $
 *    File: $RCSfile: runtime.c,v $
 *    Copyright: (C) 2002 by Stefan Birrer
 ***************************************************************************/
/***************************************************************************
 *  ChangeLog:
 * -------------------------------------------------------------------------
 *    $Log: runtime.c,v $
 *    Revision 1.1  2005/10/13 05:24:59  sbirrer
 *    - added the skeleton files
 *
 *    Revision 1.6  2002/10/24 21:32:47  sempi
 *    final release
 *
 *    Revision 1.5  2002/10/23 21:54:27  sempi
 *    beta release
 *
 *    Revision 1.4  2002/10/21 04:49:35  sempi
 *    minor correction
 *
 *    Revision 1.3  2002/10/21 04:47:05  sempi
 *    Milestone 2 beta
 *
 *    Revision 1.2  2002/10/15 20:37:26  sempi
 *    Comments updated
 *
 *    Revision 1.1  2002/10/15 20:20:56  sempi
 *    Milestone 1
 *
 ***************************************************************************/
#define __RUNTIME_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

/************Private include**********************************************/
#include "runtime.h"
#include "io.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */

/************Global Variables*********************************************/

#define NBUILTINCOMMANDS (sizeof BuiltInCommands / sizeof(char*))

/* the pids of the background processes */
bgjobL *bgjobs = NULL;
int fgChild;
/************Function Prototypes******************************************/
/* run command */
static void RunCmdFork(commandT*, bool);
/* runs an external program command after some checks */
static void RunExternalCmd(commandT*, bool);
/* resolves the path and checks for exutable flag */
static bool ResolveExternalCmd(commandT*);
/* forks and runs a external program */
static void Exec(commandT*, bool);
/* runs a builtin command */
static void RunBuiltInCmd(commandT*);
/* checks whether a command is a builtin command */
static bool IsBuiltIn(char*);
/************External Declaration*****************************************/

/**************Implementation***********************************************/
int total_task;
void RunCmd(commandT** cmd, int n)
{
	//char * path;
	//path = getCurrentWorkingDir();
	//printf("current working dir: %s \n", path);
  int i;
  total_task = n;
  printf("total_task: %d \n", n);
  if(n == 1)
    RunCmdFork(cmd[0], TRUE);
  else{
    RunCmdPipe(cmd[0], cmd[1]);
    for(i = 0; i < n; i++)
      ReleaseCmdT(&cmd[i]);
  }
}

void RunCmdFork(commandT* cmd, bool fork)
{
  if (cmd->argc<=0)
    return;
  //printf("argv[0]: %s \n", cmd->argv[0]);
  // no built in functions
  if (IsBuiltIn(cmd->argv[0]))
  {
  	printf("running build in cmd.\n");
    RunBuiltInCmd(cmd);
  }
  else
  {
  	printf("running external cmd.\n");
    RunExternalCmd(cmd, fork);
  }
}

void RunCmdBg(commandT* cmd)
{
  // TODO
}

void RunCmdPipe(commandT* cmd1, commandT* cmd2)
{
}

void RunCmdRedirOut(commandT* cmd, char* file)
{
}

void RunCmdRedirIn(commandT* cmd, char* file)
{
}


/*Try to run an external command*/
static void RunExternalCmd(commandT* cmd, bool fork)
{
  // if the cmd can be located in path
  if (ResolveExternalCmd(cmd)){
  	//printf("exec.\n");
    Exec(cmd, fork);
  }
  else {
    printf("%s: command not found\n", cmd->argv[0]);
    fflush(stdout);
    ReleaseCmdT(&cmd);
  }
}

/*Find the executable based on search list provided by environment variable PATH*/
static bool ResolveExternalCmd(commandT* cmd)
{
  char *pathlist, *c;
  char buf[1024];
  int i, j;
  struct stat fs;

  if(strchr(cmd->argv[0],'/') != NULL){
    if(stat(cmd->argv[0], &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(cmd->argv[0],X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(cmd->argv[0]);
          return TRUE;
        }
    }
    return FALSE;
  }
  pathlist = getenv("PATH");
  //printf("path: %s \n", pathlist);
  if(pathlist == NULL) return FALSE;
  i = 0;
  while(i<strlen(pathlist)){
    c = strchr(&(pathlist[i]),':');
    if(c != NULL){
      for(j = 0; c != &(pathlist[i]); i++, j++)
        buf[j] = pathlist[i];
      i++;
    }
    else{
      for(j = 0; i < strlen(pathlist); i++, j++)
        buf[j] = pathlist[i];
    }
    buf[j] = '\0';
    strcat(buf, "/");
    strcat(buf,cmd->argv[0]);
    if(stat(buf, &fs) >= 0){
      if(S_ISDIR(fs.st_mode) == 0)
        if(access(buf,X_OK) == 0){/*Whether it's an executable or the user has required permisson to run it*/
          cmd->name = strdup(buf); 
          return TRUE;
        }
    }
  }
  return FALSE; /*The command is not found or the user don't have enough priority to run.*/
}

void AddBgJob(bgjobL* job)
{
//	printf("adding bg job to the list. \n ");
	if (bgjobs == NULL)
	{
		bgjobs = malloc(sizeof(bgjobL));
		bgjobs->next = job;
	}
	else
	{
		job->next = bgjobs->next;
		bgjobs->next = job;
	}
}

void printBgJobList()
{
	printf("printing bg jobs. \n");
	if (bgjobs == NULL || bgjobs->next == NULL)
		printf("no bg jobs in the list. \n");
	else
	{
		bgjobL* cursor = bgjobs->next;
		while (cursor != NULL)
		{
			//printf("bgjob pid: %s   ",cursor->pid);
			if (cursor->status == RUNNING)
			{
				printf("[%d]   %s              %s &\n", cursor->jobId, "RUNNING", cursor->cmd_first);
			}
			else if (cursor->status == STOPPED)
			{
				printf("[%d]   %s              %s \n", cursor->jobId, "STOPPED", cursor->cmd_first);
			}
			else if (cursor->status == DONE)
			{
				printf("[%d]   %s              %s \n", cursor->jobId, "DONE", cursor->cmd_first);
			}
			printf("bgjob cmd_first: %s   ", cursor->cmd_first);
			cursor = cursor->next;
		}
	}	
}

// fg wait; bg not wait
static void Exec(commandT* cmd, bool forceFork)
{
	if (forceFork)
	{

	//printf("cmd[0]->argv %s \n" + cmd[0]->argv);
	//printf("name: %s \n", cmd->name);
	//printf("cmdline: %s \n", cmd->cmdline);
	//printf("redirect_in: %s , redirect_out: %s \n", cmd->redirect_in, cmd->redirect_out);
	//printf("bg: %i  ", cmd->bg);
	//printf("argc: %i \n ", cmd->argc);
	pid_t pid;
	pid = fork();
	int * status;
	if (pid < 0)
	{
		printf("pid < 0 \n ");
		return;
	}
	if (pid == 0)
	{
		//setpgid(0, 0);
		//printf("i am child. \n");
		execv(cmd->name, cmd->argv);
		exit(0);
	}
	else // prent process
	{
		//printf("i am parent. \n");
		// fg job
		if (cmd->bg == 0)
		{
			fgChild = pid;
			printf("this is not bg job.\n");
			printf("pid: %d \n", fgChild);
			waitpid(pid, &status, 0);

			//while(fgChild > 0)
			//{
			//	sleep(1);
			//}
		}
		else // bg job
		{
			printf("this is bg job. \n");
			bgjobL* bgJob = (bgjobL*) malloc(sizeof(bgjobL));
			bgJob->pid = pid;
			bgJob->status = RUNNING;
			bgJob->cmd_first = cmd->argv[0];
			printf("bg job pid: %d \n", bgJob->pid);
			bgJob->next = NULL;
			AddBgJob(bgJob);
			//printBgJobList();
			return;
		}
	}
	}
	//printBgJobList();
}

static bool IsBuiltIn(char* cmd)
{
	if ((strcmp(cmd, "cd") == 0) || strcmp(cmd, "bg") == 0 || strcmp(cmd, "fg") == 0 || strcmp(cmd, "jobs") == 0)
	{
		printf("build in function. \n");
		return TRUE;
	}
	else
	{
		printf("not build in function. \n");
		return FALSE;
	}
}


static void RunBuiltInCmd(commandT* cmd)
{
	char* cmd_first = cmd->argv[0];
	// cd command
	if (strcmp(cmd_first, "cd") == 0)
	{
		printf("the command is cd. \n");
		int cd_result;
		char* homeDirectory = "/home/";
		if (cmd->argc == 1)
		{
			cd_result = chdir(homeDirectory);
		}
		else
		{
			printf("cd argv1: %s \n", cmd->argv[1]);
			cd_result = chdir(cmd->argv[1]);
		}
		if (cd_result < 0)
		{
			printf("error in cd.\n");
		}
	}	
	
	// fg command
	if (strcmp(cmd_first, "fg") == 0)
	{
		printf("the command is fg. \n");
	}

	// bg command
	if (strcmp(cmd_first, "bg") == 0)
	{
		printf("the command is bg. \n");
	}

	// jobs command
	if (strcmp(cmd_first, "jobs") == 0)
	{
		printf("the command is jobs. \n");
		printBgJobList();
	}
}		

void CheckJobs()
{
	if (bgjobs == NULL || bgjobs->next == NULL)
	{
		return;
	}
	bgjobL* previous = bgjobs;
	bgjobL* cursor = bgjobs->next;
	while (cursor)
	{
		if (cursor->status == DONE)
		{
			printf("[%d]   %s              %s \n", cursor->jobId, "DONE", cursor->cmd_first);
			previous->next = cursor->next;
			free(cursor->cmd_first);
			free(cursor);
		}
		cursor = previous->next;
		previous = previous->next;
	}
}


commandT* CreateCmdT(int n)
{
  int i;
  commandT * cd = malloc(sizeof(commandT) + sizeof(char *) * (n + 1));
  cd -> name = NULL;
  cd -> cmdline = NULL;
  cd -> is_redirect_in = cd -> is_redirect_out = 0;
  cd -> redirect_in = cd -> redirect_out = NULL;
  cd -> argc = n;
  for(i = 0; i <=n; i++)
    cd -> argv[i] = NULL;
  return cd;
}

/*Release and collect the space of a commandT struct*/
void ReleaseCmdT(commandT **cmd){
  int i;
  if((*cmd)->name != NULL) free((*cmd)->name);
  if((*cmd)->cmdline != NULL) free((*cmd)->cmdline);
  if((*cmd)->redirect_in != NULL) free((*cmd)->redirect_in);
  if((*cmd)->redirect_out != NULL) free((*cmd)->redirect_out);
  for(i = 0; i < (*cmd)->argc; i++)
    if((*cmd)->argv[i] != NULL) free((*cmd)->argv[i]);
  free(*cmd);
}
