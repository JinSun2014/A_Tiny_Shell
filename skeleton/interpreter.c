/* -*-C-*-
 *******************************************************************************
 *
 * File:         interpreter.c
 * RCS:          $Id: interpreter.c,v 1.1 2009/10/09 04:38:08 npb853 Exp $
 * Description:  Handles the input from stdin
 * Author:       Stefan Birrer
 *               AquaLab Research Group
 *               EECS
 *               Northwestern University
 * Created:      Fri Oct 06, 2006 at 13:33:58
 * Modified:     Fri Oct 06, 2006 at 13:39:14 fabianb@cs.northwestern.edu
 * Language:     C
 * Package:      N/A
 * Status:       Experimental (Do Not Distribute)
 *
 * (C) Copyright 2006, Northwestern University, all rights reserved.
 *
 *******************************************************************************
 */
#define __INTERPRETER_IMPL__

/************System include***********************************************/
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
/************Private include**********************************************/
#include "interpreter.h"
#include "io.h"
#include "runtime.h"

/************Defines and Typedefs*****************************************/
/*  #defines and typedefs should have their names in all caps.
 *  Global variables begin with g. Global constants with k. Local
 *  variables should be in all lower case. When initializing
 *  structures and arrays, line everything up in neat columns.
 */
typedef struct string_l {
  char* s;
  struct string_l* next;
} stringL;


EXTERN aliasL* aliasList;
static char** str_split(char*, const char);
static char* replaceAlias(char*);
static char* replaceHomeDir(char*);

/****************Function Implementation**************/

/*Parse a single word from the param. Get rid of '"' or '''*/
char* single_param(char *st)
{
  int quot1 = 0,quot2 = 0, start = 0;
  char *t = st;
  static int idx;

  idx = 0;
  while(1){
    if(start == 1){
      if(st[idx] == '\0') return t;
      if(st[idx] == '<' || st[idx] == '>') {st[idx] = '\0'; return t;}
      if(st[idx] == ' ' && quot1 == 0 && quot2 == 0) {st[idx] = '\0';return t;}
      if(st[idx] == '\'' && quot1 == 1) {st[idx] = '\0';return t;}
      if(st[idx] == '\'' && quot1 == 0) {quot1 = 1;}
      if(st[idx] == '"' && quot2 == 1) {st[idx] = '\0';return t;}
    }
    else{
      if(st[idx] == ' ' || st[idx] =='\0');
      else if(st[idx] == '"') {quot2 = 1; start = 1; t = &(st[idx+1]);}
      else if(st[idx] == '\'') {quot1 = 1; start = 1; t = &(st[idx+1]);}
      else {start = 1; t = &(st[idx]);}
    }
    idx++;
  }
}

/*Parse the single command and call single_param to parse each word in the command*/
void parser_single(char *c, int sz, commandT** cd, int bg)
{
  int i, task_argc = 0, quot1 = 0, quot2 = 0;
  char *in = NULL, * out = NULL, *tmp;
  int cmd_length;
  c[sz] = '\0';
  for(i = 0; i < sz; i++)
    if(c[i] != ' ') break;
  c = &(c[i]);
  sz = sz - i;
  cmd_length = sz;
  for(i = 0; i < sz; i++){
    if(c[i] == '\''){
      if(quot2) continue;
      else if(quot1){
        quot1 = 0;continue;
      }
      else quot1 = 1;
    }
    if(c[i] == '"'){
      if(quot1) continue;
      else if(quot2){
        quot2 = 0;continue;
      }
      else quot2 = 1;
    }
    if(c[i] == '<' && quot1 != 1 && quot2 != 1){
      if(cmd_length == sz) cmd_length = i; 
      while(i < (sz - 1) && c[i + 1] == ' ') i++;
      in = &(c[i+1]);
    }
    if(c[i] == '>' && quot1 != 1 && quot2 != 1){
      if(cmd_length == sz) cmd_length = i;
      while(i < (sz - 1) && c[i+1] == ' ') i++;
      out = &(c[i+1]);
    }
    if(c[i] == ' ' && quot1 != 1 && quot2 != 1){
      if(cmd_length == sz) task_argc++;
      while(i < (sz - 1) && c[i+1] == ' ') i++;
    }
  }
  if(c[cmd_length - 1] != ' ') task_argc++;
  (*cd) = CreateCmdT(task_argc);
  (*cd) -> bg = bg;
  (*cd)->cmdline = strdup(c);
  tmp = c;
  for(i = 0; i < task_argc; i++){
    (*cd) -> argv[i] = strdup(single_param(tmp));
    while ((*tmp) != '\0') tmp++;
    tmp ++;
  }
  if(in){
    (*cd) -> is_redirect_in = 1;
    (*cd) -> redirect_in = strdup(single_param(in));
  }
  if(out){
    (*cd) -> is_redirect_out = 1;
    (*cd) -> redirect_out = strdup(single_param(out));
  }
}


/**************Implementation***********************************************/
/*Parse the whole command line and split commands if a piped command is sent.*/
void Interpret(char* cmdLine)
{
  int task = 1;
  int bg = 0, i,k,j = 0, quotation1 = 0, quotation2 = 0;
  commandT **command;

  if(cmdLine[0] == '\0') return;
  // printf("cmdline: %s\n", cmdLine);
  char* newCmd = NULL;
  newCmd = replaceAlias(cmdLine);
  // printf("newCmd: %s\ncmdline: %s\n", newCmd, cmdLine);
  if (newCmd){
      // free(cmdLine);
      cmdLine = newCmd;
  }
  // printf("--After:cmdline: %s\n", cmdLine);
  newCmd = replaceHomeDir(cmdLine);
  if (newCmd){
      free(cmdLine);
      cmdLine = newCmd;
  }

  for(i = 0; i < strlen(cmdLine); i++){
    if(cmdLine[i] == '\''){
      if(quotation2) continue;
      else if(quotation1){
        quotation1 = 0;continue;
      }
      else quotation1 = 1;
    }
    if(cmdLine[i] == '"'){
      if(quotation1) continue;
      else if(quotation2){
        quotation2 = 0;continue;
      }
      else quotation2 = 1;
    }

    if(cmdLine[i] == '|' && quotation1 != 1 && quotation2 != 1){
      task ++;
    }
  }

  command = (commandT **) malloc(sizeof(commandT *) * task);
  i = strlen(cmdLine) - 1;
  while(i >= 0 && cmdLine[i] == ' ') i--;
  if(cmdLine[i] == '&'){
    if(i == 0) return;
    bg = 1;
    cmdLine[i] = '\0';
  }

  quotation1 = quotation2 = 0;
  task = 0;
  k = strlen(cmdLine);
  for(i = 0; i < k; i++, j++){
    if(cmdLine[i] == '\''){
      if(quotation2) continue;
      else if(quotation1){
        quotation1 = 0;continue;
      }
      else quotation1 = 1;
    }
    if(cmdLine[i] == '"'){
      if(quotation1) continue;
      else if(quotation2){
        quotation2 = 0;continue;
      }
      else quotation2 = 1;
    }
    if(cmdLine[i] == '|' && quotation1 != 1 && quotation2 != 1){
      parser_single(&(cmdLine[i-j]), j, &(command[task]),bg);
      task++;
      j = -1;
    }
  }
  parser_single(&(cmdLine[i-j]), j, &(command[task]),bg);

  // printf("Your command is: ");
  // int curser = 0;
  // char** cmds = command[0]->argv;
  // for (curser = 0; curser < command[0]->argc; ++curser){
      // printf("%i ", curser);
      // printf("%s ", cmds[curser]);
  // }
  // printf("\n");


  RunCmd(command, task+1);
  free(command);
}

char** str_split(char* a_str, const char a_delim)
{
    char** result    = 0;
    size_t count     = 0;
    char* tmp        = a_str;
    char* last_comma = 0;
    char delim[2];
    delim[0] = a_delim;
    delim[1] = 0;

    /* Count how many elements will be extracted. */
    while (*tmp)
    {
        if (a_delim == *tmp)
        {
            count++;
            last_comma = tmp;
        }
        tmp++;
    }

    /* Add space for trailing token. */
    count += last_comma < (a_str + strlen(a_str) - 1);

    /* Add space for terminating null string so caller
       knows where the list of returned strings ends. */
    count++;

    result = malloc(sizeof(char*) * count);

    if (result)
    {
        size_t idx  = 0;
        char* token = strtok(a_str, delim);

        while (token)
        {
            // assert(idx < count);
            *(result + idx++) = strdup(token);
            token = strtok(0, delim);
        }
        // assert(idx == count - 1);
        *(result + idx) = 0;
    }

    return result;
}

char* replaceAlias(char* cmdline){
    int oldLength = strlen(cmdline);
    char* oldCmd = malloc(sizeof(char) * oldLength + 1);
    strcpy(oldCmd, cmdline);

    char** tokens = str_split(cmdline, ' ');
    /* check and replace all alias */
    oldCmd[oldLength] = '\0';
    int newLength = 0;
    bool hasAlias = FALSE;
    if (tokens){
        int i = 0;
        aliasL* curser = aliasList;
        for (; *(tokens + i); i++){
            curser = aliasList;
            while (curser){
                if (strcmp(curser->aliasCmd, *(tokens + i)) == 0){
                    free(*(tokens + i));
                    *(tokens + i) = curser->originCmd;
                    // printf("*****Changed: %s\n", *(tokens + i));
                    hasAlias = TRUE;
                }
                newLength += strlen(*(tokens + i));
                newLength++;
                curser = curser->next;
            }
        }
    }

    char* newCmd;
    if(hasAlias){
        // printf("newLength: %d\n", newLength);
        newCmd = malloc(sizeof(char) * (newLength + 1));

        newCmd[0] = '\0';
        int i = 0;
        // printf("***Scan Tokens:\n");
        // for (; *(tokens + i); i++){
            // printf("%s %d\n", *(tokens + i), i);
        // }

        i = 0;
        for (; *(tokens + i); i++){
            strcat(newCmd, *(tokens + i));
            strcat(newCmd, " ");
        }
        newCmd[newLength] = '\0';

        for (; *(tokens + i); i++){
            free(*(tokens + i));
        }
        // printf("In function: newCmd: %s\n", newCmd);
        return newCmd;
    }
    else{
        int i = 0;
        for (; *(tokens + i); i++){
            free(*(tokens + i));
        }
        return oldCmd;
    }
}

char* replaceHomeDir(char* cmdline){
    int i = 0;
    // struct passwd *pw = getpwuid(getuid());
    const char *homedir = getenv("HOME");
    bool hasTelta = FALSE;
    for (i = 0; i != strlen(cmdline); ++i){
        if (cmdline[i] == '~'){
            hasTelta = TRUE;
        }
    }
    if (hasTelta){
        char* newCmd = malloc(sizeof(char) * 50);
        newCmd[0] = '\0';
        char** tokens = str_split(cmdline, '~');
        i = 0;
        for (; *(tokens + i); ++i){
            strcat(newCmd, *(tokens + i));
            if (!*(tokens + i + 1))
                strcat(newCmd, homedir);
        }
        return newCmd;
    }
    else{
        return NULL;
    }
}
