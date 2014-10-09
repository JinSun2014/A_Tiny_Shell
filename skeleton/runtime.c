/***************************************************************************
* *  Title: Runtime environment 
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
char* fgCmd_first;

/* List of alias */
aliasL *aliasList = NULL;

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
/* run command which has redir in and out together  */
static void RedirInOut(commandT**);
/* set alias */
void setAlias(commandT*);
/* unset alias */
static void unsetAlias(commandT*);
/* print alias */
void printAlias();
/* parse alias */
static void parseAlias(char*, char**, char**);
/* check if valid Alias */
static bool isValidAlias(char*, char*);
/************External Declaration*****************************************/
EXTERN char* single_param(char*);

/**************Implementation***********************************************/
int total_task;
void RunCmd(commandT** cmd, int n)
{
  int i;
  total_task = n;
  if(n == 1)
  {
	if (cmd[0]->is_redirect_in && cmd[0]->is_redirect_out)
	{
		RedirInOut(cmd);
	}
	else if (cmd[0]->is_redirect_in)
	{
	  RunCmdRedirIn(cmd[0], cmd[0]->redirect_in);
    }
	else if (cmd[0]->is_redirect_out)
    {
	  RunCmdRedirOut(cmd[0], cmd[0]->redirect_out);
    }
	else
	{
    RunCmdFork(cmd[0], TRUE);
	}
  }
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
    RunBuiltInCmd(cmd);
  }
  else
  {
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

// 0: standard input
// 1: standard output
// 2: standard error
// ">"
void RunCmdRedirOut(commandT* cmd, char* file)
{
	int output = dup(1);
	int fileOut = open(file, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	if (fileOut == -1)
	{
		return;
	}
	dup2(fileOut, 1);
	RunCmdFork(cmd, TRUE);
	close(fileOut);
	dup2(output, 1);
}

// "<"
void RunCmdRedirIn(commandT* cmd, char* file)
{
	int input = dup(0);
	int fileIn = open(file, O_RDONLY);
	if (fileIn == -1)
	{
		return;
	}
	dup2(fileIn, 0);
	close(fileIn);
	RunCmdFork(cmd, TRUE);
	dup2(input, 0);
}

void RedirInOut(commandT** cmd)
{
	int input = dup(0);
	int output = dup(1);
	int fileIn = open(cmd[0]->redirect_in, O_RDONLY);
	int fileOut =  open(cmd[0]->redirect_out, O_WRONLY | O_CREAT, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
	dup2(fileIn, 0);
	dup2(fileOut, 1);
	RunCmdFork(cmd[0], TRUE);
	dup2(input, 0);
	dup2(output, 1);
	close(fileIn);
	close(fileOut);
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

/* check the bgjob list is empty or not*/
bool isEmpty()
{
	return (bgjobs == NULL ||bgjobs->next == NULL);
}

/* add one job into the bgjob list */
void AddBgJob(bgjobL* job)
{
//	printf("adding bg job to the list. \n ");
	if (bgjobs == NULL)
	{
		bgjobs = malloc(sizeof(bgjobL));
		bgjobs->next = job;
		job->jobId = 1;
	}
	else
	{
		int jobsCount = 0;
		bgjobL* cursor = bgjobs;
		while (cursor->next)
		{
			cursor = cursor->next;
			jobsCount++;
		}
		job->jobId = jobsCount + 1;
		cursor->next = job;
	}
}

/* print the bgjob list */
void printBgJobList()
{
	//printf("printing bg jobs. \n");
	if (bgjobs == NULL || bgjobs->next == NULL)
		//printf("no bg jobs in the list. \n");
		return;
	else
	{
		bgjobL* cursor = bgjobs->next;
		while (cursor != NULL)
		{
			//printf("bgjob pid: %s   ",cursor->pid);
			if (cursor->status == RUNNING)
			{
				printf("[%d]   %s              %s &\n", cursor->jobId, "Running", cursor->cmd_first);
			}
			else if (cursor->status == STOPPED)
			{
				printf("[%d]   %s              %s \n", cursor->jobId, "Stopped", cursor->cmd_first);
			}
			else if (cursor->status == DONE)
			{
				printf("[%d]   %s              %s \n", cursor->jobId, "Done", cursor->cmd_first);
			}
			//printf("bgjob cmd_first: %s   ", cursor->cmd_first);
			cursor = cursor->next;
		}
	}
	fflush(stdout);
}

// fg wait; bg not wait
static void Exec(commandT* cmd, bool forceFork)
{
	if (forceFork)
	{

	//printf("cmd[0]->argv %s \n" + cmd[0]->argv);
	//printf("cmdline: %s \n", cmd->cmdline);
	//printf("redirect_in: %s , redirect_out: %s \n", cmd->redirect_in, cmd->redirect_out);
	//printf("bg: %i  ", cmd->bg);
	//printf("argc: %i \n ", cmd->argc);
	pid_t pid;
	pid = fork();
	if (pid < 0)
	{
		printf("pid < 0 \n ");
		return;
	}
	if (pid == 0)
	{
		setpgid(0, 0);
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
			fgCmd_first = cmd->cmdline;
			//printf("this is not bg job.\n");
			//printf("pid: %d \n", fgChild);
		//	waitpid(pid, &status, 0);

			while(fgChild > 0)
			{
				sleep(1);
			}
		}
		else // bg job
		{
			//printf("this is bg job. \n");
			bgjobL* bgJob = (bgjobL*) malloc(sizeof(bgjobL));
			bgJob->pid = pid;
			bgJob->status = RUNNING;
			bgJob->cmd_first = cmd->cmdline;
			//printf("bg job pid: %d \n", bgJob->pid);
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
	if ((strcmp(cmd, "cd") == 0) || \
         strcmp(cmd, "bg") == 0 || \
         strcmp(cmd, "fg") == 0 || \
         strcmp(cmd, "jobs") == 0 || \
         strcmp(cmd, "alias") == 0 || \
         strcmp(cmd, "unalias") == 0 \
         )
	{
		// printf("Builtin function. \n");
		return TRUE;
	}
	else
	{
		// printf("NOT builtin function. \n");
		return FALSE;
	}
}

/* pop specific job out from the bg job list*/
bgjobL* popJob(int id)
{
	bgjobL* cursor = bgjobs->next;
	bgjobL* previous = bgjobs;
	while (cursor!= NULL)
	{
		if (cursor->jobId == id)
		{
			previous->next = cursor->next;
			//printf("cursor, finish pop \n");
			return cursor;
		}
		cursor = cursor->next;
		previous = previous->next;
	}
	//printf("null, finish pop \n");
	return NULL;
}

static void RunBuiltInCmd(commandT* cmd)
{
	char* cmd_first = cmd->argv[0];
	// cd command
	if (strcmp(cmd_first, "cd") == 0)
	{
		//printf("the command is cd. \n");
		int cd_result;
		char* homeDirectory = getenv("HOME");
		if (cmd->argc == 1)
		{
			cd_result = chdir(homeDirectory);
		}
		else
		{
			//printf("cd argv1: %s \n", cmd->argv[1]);
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
		int targetJobId;
		//printf("the command is fg. \n");
		if (isEmpty()) // no background jobs, do nothing
		{
			//printf("there is no jobs in background. \n");
			return;
		}
		else // has background jobs, pop it and exec in fg
		{
			// no argument after fg, choose the most recent one to the fg
			if (cmd->argc == 1)
			{
				//printf("no arg. \n");
				//printBgJobList();
				//printf(".....\n");
				bgjobL* cursor = bgjobs->next;
				while (cursor->next != NULL)
				{
					cursor = cursor->next;
				}
				targetJobId = cursor->jobId;
				//printf("no arg, find most recent one. jobId:%d \n", targetJobId);
			}
			else // find target job and put it to the fg
			{
				targetJobId = atoi(cmd->argv[1]);
				//printf("has arg, jobId: %d \n", targetJobId);
			}
			bgjobL* popedJob;
			popedJob = popJob(targetJobId);
			if (popedJob != NULL)
			{
				//printf("find target bgjob jobId:%d   pid:%d  cmd:%s", popedJob->jobId, popedJob->pid, popedJob->cmd_first);
				fgChild = popedJob->pid;
				fgCmd_first = popedJob->cmd_first;
				kill(-fgChild, SIGCONT);
				while (fgChild)
				{
					//printf("sleep: %d \n", fgChild);
					sleep(1);
				}
				free(popedJob);
				return;
			}
			else
			{
				printf("jobId not found! \n");
				return;
			}

		}
	}

	// bg command
	if (strcmp(cmd_first, "bg") == 0)
	{
		int targetJobId;
		//printf("the command is bg. \n");
		if (isEmpty()) // no background jobs, do nothing
		{
			//printf("there is no jobs in background. \n");
			return;
		}
		else // has background jobs
		{
			if (cmd->argc == 1) // no arg, find the most recent one 
			{
				//printf("no arg. \n");
				bgjobL* cursor = bgjobs->next;
				while (cursor->next != NULL)
				{
					cursor = cursor->next;
				}
				targetJobId = cursor->jobId;
				//printf("no arg, find most recent one. jobId: %d \n", targetJobId);
			}
			else // has arg, find it
			{
				targetJobId = atoi(cmd->argv[1]);
			}
			//printf("has arg, jobId: %d \n", targetJobId);
			bgjobL* cursor = bgjobs->next;
			while (cursor)
			{
				if (cursor->jobId == targetJobId)
				{
					cursor->status = STOPPED;
					break;
				}
				cursor = cursor->next;
			}
			if (cursor == NULL)
			{
				printf("job not found. \n");
				return;
			}
			kill(cursor->pid, SIGCONT);
			cursor->status = RUNNING;
			return;
		}
	}

	// jobs command
	if (strcmp(cmd_first, "jobs") == 0)
	{
		//printf("the command is jobs. \n");
		printBgJobList();
	}

    // alias command
    if (strcmp(cmd_first, "alias") == 0)
    {
        if (cmd->argc <= 1)
            printAlias();
        else
            setAlias(cmd);
    }
    else if (strcmp(cmd_first, "unalias") == 0)
    {
        unsetAlias(cmd);
    }
}

/* pop out all "DONE" jobs in the bgjob list */
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
			printf("[%d]    %s              %s \n", cursor->jobId, "Done", cursor->cmd_first);
			previous->next = cursor->next;
			free(cursor->cmd_first);
			free(cursor);
			cursor = previous->next;
		}
		else
		{
			cursor = cursor->next;
			previous = previous->next;
		}
	}
	fflush(stdout);
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

void setAlias(commandT* cmd){
    // printf("set Alias\n");
    char *aliasCmd = NULL;
    char *originCmd = NULL;

    parseAlias(cmd->argv[1], &aliasCmd, &originCmd);
    int i;
    /* get rid of trailing space */
    for (i = strlen(originCmd) - 1; originCmd[i] == ' '; --i){
        originCmd[i] = '\0';
    }
    for (i = strlen(aliasCmd) - 1; aliasCmd[i] == ' '; --i){
        originCmd[i] = '\0';
    }
    if (!isValidAlias(aliasCmd, originCmd)){
        return;
    }
    /* insert alias by alphabet order */
    if (aliasList == NULL){
        aliasList = (aliasL*) malloc(sizeof(aliasL));
        aliasList->aliasCmd = aliasCmd;
        aliasList->originCmd = originCmd;
        aliasList->next = NULL;
        return;
    } else {
        aliasL* curser = aliasList;
        if (strcmp(curser->aliasCmd, aliasCmd) == 0){
            free(curser->originCmd);
            curser->originCmd = originCmd;
            free(aliasCmd);
            return;
        }
        if (curser->aliasCmd[0] > aliasCmd[0]){
            aliasL* newAlias = (aliasL*) malloc(sizeof(aliasL));
            newAlias->aliasCmd = aliasCmd;
            newAlias->originCmd = originCmd;
            newAlias->next = curser;
            aliasList = newAlias;
            newAlias = NULL;
            return;
        }
        while (curser->next != NULL && strcmp(curser->next->aliasCmd, aliasCmd) != 0 && curser->next->aliasCmd[0] <= aliasCmd[0])
            curser = curser->next;
        if (curser->next != NULL){
            if (curser->next->aliasCmd[0] > aliasCmd[0]){
                aliasL* newAlias = (aliasL*) malloc(sizeof(aliasL));
                newAlias->next = curser->next;
                newAlias->aliasCmd = aliasCmd;
                newAlias->originCmd = originCmd;
                curser->next = newAlias;
                newAlias = NULL;
                return;
            }
            else{
                free(curser->next->originCmd);
                curser->next->originCmd = originCmd;
                free(aliasCmd);
                return;
            }
        }
        else{
            aliasL* newAlias = (aliasL*) malloc(sizeof(aliasL));
            newAlias->aliasCmd = aliasCmd;
            newAlias->originCmd = originCmd;
            newAlias->next = NULL;
            curser->next = newAlias;
            newAlias = NULL;
            return;
        }
    }
}

void unsetAlias(commandT* cmd){
    char *originCmd = malloc(sizeof(char) * 50);
    originCmd[0] = '\0';
    int counter = 0;
    int i;
    for (i = 1; i != cmd->argc; ++i){
        strcat(originCmd, cmd->argv[i]);
        strcat(originCmd, " ");
        counter += strlen(cmd->argv[i]) + 1;
    }
    originCmd[counter - 1] = '\0';
    // printf("unset origin: %s, length: %d\n", originCmd, strlen(originCmd));

    aliasL* curser = aliasList;
    if (curser){
        if (strcmp(curser->originCmd, originCmd) == 0){
            aliasL* tmp = aliasList;
            aliasList = aliasList->next;
            free(tmp->originCmd);
            free(tmp->aliasCmd);
            free(tmp);
            free(originCmd);
            tmp = NULL;
            return;
        }
        while (curser->next){
            if (strcmp(curser->next->originCmd, originCmd) == 0){
                aliasL* tmp = curser->next;
                curser->next = curser->next->next;
                free(tmp->originCmd);
                free(tmp->aliasCmd);
                free(tmp);
                free(originCmd);
                tmp = NULL;
                return;
            }
            curser = curser->next;
        }
    }
    printf("%s: command not found\n", originCmd);
}

void printAlias(){
    aliasL* curser = aliasList;
    while (curser){
        printf("alias %s='%s'\n", curser->aliasCmd, curser->originCmd);
        curser = curser->next;
    }
}

void parseAlias(char* cmdline, char** aliasCmd, char** originCmd){
    int i = 0;
    int quotation1 = -1;
    for (i = 0; i < strlen(cmdline); i++){
       if (cmdline[i] == '\''){
               quotation1 = i;
               break;
       }
    }
    (*aliasCmd) = substring(cmdline, 0, quotation1 - 1);
    (*originCmd) = substring(cmdline, quotation1 + 2, strlen(cmdline) - quotation1 - 1);
}

char* substring(char *string, int position, int length)
{
    char *pointer;
    int c;

    pointer = malloc(sizeof(char) * (length+1));
    if (pointer == NULL){
        printf("Unable to allocate memory.\n");
        exit(EXIT_FAILURE);
    }
    for (c = 0 ; c < position -1 ; c++)
        string++;
    for (c = 0 ; c < length ; c++){
        *(pointer+c) = *string;
        string++;
    }
    *(pointer+c) = '\0';
    return pointer;
}

bool isValidAlias(char* aliasCmd, char* originCmd){
    char* cpyOriginCmd = malloc(sizeof(char) * strlen(originCmd));
    strcpy(cpyOriginCmd, originCmd);
    char* first_world = single_param(cpyOriginCmd);
    if (strcmp(first_world, aliasCmd) == 0){
        // printf("Invliadi Alian!\n");
        free(cpyOriginCmd);
        return FALSE;
    } else{
        // printf("Valid alian!\n");
        free(cpyOriginCmd);
        return TRUE;
    }
}
