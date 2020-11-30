#include <stdio.h>
#include <sys/types.h>
#include <unistd.h>
#include <pwd.h>
#include <sys/stat.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/param.h>
#include <sys/wait.h>

#include "cmd_parse.h"

unsigned short isVerbose = 0;

int process_user_input_simple(void){
    char str[MAX_STR_LEN];
    char *ret_val;
    char *raw_cmd;
    cmd_list_t *cmd_list = NULL;
    int cmd_count = 0;
    char prompt[30];

    // Set up a cool user prompt.
    sprintf(prompt, PROMPT_STR " %s :-) ", getenv("LOGNAME"));
    for ( ; ; ) {
        fputs(prompt, stdout);
        memset(str, 0, MAX_STR_LEN);
        ret_val = fgets(str, MAX_STR_LEN, stdin);

        if (NULL == ret_val) {
            // end of input, a control-D was pressed.
            // Bust out of the input loop and go home.
            break;
        }

        // STOMP on the trailing newline returned from fgets(). I should be
        // more careful about STOMPing on the last character, just in case
        // it is something other than a newline, but I'm being bold and STOMPing.
        str[strlen(str) - 1] = 0;
        if (strlen(str) == 0) {
            // An empty command line.
            // Just jump back to the promt and fgets().
            // Don't start telling me I'm going to get cooties by
            // using continue.
            continue;
        }

        if (strcmp(str, EXIT_CMD) == 0) {
            // Pickup your toys and go home. I just hope there are not
            // any memory leaks.
            break;
        }
        // Basic commands are pipe delimited.
        // This is really for Stage 2.
        raw_cmd = strtok(str, PIPE_DELIM);

        cmd_list = (cmd_list_t *) calloc(1, sizeof(cmd_list_t));

        // This block should probably be put into its own function.
        cmd_count = 0;
        while (raw_cmd != NULL ) {
            cmd_t *cmd = (cmd_t *) calloc(1, sizeof(cmd_t));

            cmd->raw_cmd = strdup(raw_cmd);
            cmd->list_location = cmd_count++;

            if (cmd_list->head == NULL) {
                // An empty list.
                cmd_list->tail = cmd_list->head = cmd;
            }
            else {
                // Make this the last in the list of cmds
                cmd_list->tail->next = cmd;
                cmd_list->tail = cmd;
            }
            cmd_list->count++;

            // Get the next raw command.
            raw_cmd = strtok(NULL, PIPE_DELIM);
        }
        // Now that I have a linked list of the pipe delimited commands,
        // go through each individual command.
        parse_commands(cmd_list);

        // This is a really good place to call a function to exec the
        // the commands just parsed from the user's command line.
        exec_commands(cmd_list);

        // We (that includes you) need to free up all the stuff we just
        // allocated from the heap. That linked list is linked lists looks
        // like it will be nasty to free up, but just follow the memory.
        free_list(cmd_list);
        free(cmd_list);
        cmd_list = NULL;
    }

    return(EXIT_SUCCESS);
}

void simple_argv(int argc, char *argv[] ){
    int opt;

    while ((opt = getopt(argc, argv, "hv")) != -1) {
        switch (opt) {
        case 'h':
            // help
            // Show something helpful
            fprintf(stdout, "You must be out of your Vulcan mind if you think\n"
                    "I'm going to put helpful things in here.\n\n");
            exit(EXIT_SUCCESS);
            break;
        case 'v':
            // verbose option to anything
            // I have this such that I can have -v on the command line multiple
            // time to increase the verbosity of the output.
            isVerbose++;
            if (isVerbose) {
                fprintf(stderr, "verbose: verbose option selected: %d\n"
                        , isVerbose);
            }
            break;
        case '?':
            fprintf(stderr, "*** Unknown option used, ignoring. ***\n");
            break;
        default:
            fprintf(stderr, "*** Oops, something strange happened <%c> ... ignoring ...***\n", opt);
            break;
        }
    }
}

void exec_commands( cmd_list_t *cmds ) {
    cmd_t *cmd = cmds->head;

    if (1 == cmds->count) {
        if (!cmd->cmd) {
            return;
        }
        if (0 == strcmp(cmd->cmd, CD_CMD)) {
            if (0 == cmd->param_count) {
                // Just a "cd" on the command line without a target directory
                // need to cd to the HOME directory.

            	chdir(getenv("HOME"));

            }
            else {

                if (0 == chdir(cmd->param_list->param)) {
                    // a happy chdir!  ;-)
                }
                else {
                    // a sad chdir.  :-(
                }
            }
        }
        else if (0 == strcmp(cmd->cmd, PWD_CMD)) {
            char str[MAXPATHLEN];

            getcwd(str, MAXPATHLEN); 
            printf(" " PWD_CMD ": %s\n", str);
        }
        else if (0 == strcmp(cmd->cmd, ECHO_CMD)) {

            param_t *curr = cmd->param_list;
            while(curr != NULL){
            	fprintf(stdout, "%s ", curr->param);
            	curr = curr->next;
            }
            fprintf(stdout, "\n");
        }
        else {
        	pid_t cpid = -1;
        	cpid=fork();
        	switch(cpid){
        		case 0:
        			{
        			int count = cmd->param_count;
        			char *args[count+2];
        			param_t *curr = cmd->param_list;
        			int i = 0;
        			args[0] = cmd->cmd;

                    if(cmd->input_src == REDIRECT_FILE){
                        int fd = open(cmd->input_file_name, O_RDONLY);
                        if (fd < 0){
                            perror("cant open input file");
                            exit(1);
                        }
                        dup2(fd,STDIN_FILENO);
                        close(fd);
                    }
                    if(cmd->output_dest == REDIRECT_FILE){
                        int fd = open(cmd->output_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0660);
                        if (fd < 0){
                            perror("can't open output file");
                            exit(1);
                        }
                        dup2(fd,STDOUT_FILENO);
                        close(fd);
                    }
                    i=1;
                    while(curr){
                        args[i] = curr->param;
                        curr = curr->next;
                        i++;
                    }
        			args[count+1] = NULL;
        			execvp(args[0],args);
        			perror("execvp failed\n");
        			_exit(5);
        		}
        		default:
        			wait(NULL);
        			break;
        	}
        }
    }

    else {
        int p_trail = -1;
        int pipes[2] = {-1,-1};
        // STAGE 2 CODE!!!
        //cmds == cmd_list_t passed in (this data type just points to the head and tail cmd_t)
        //cmd_t cmd = cmds->head
        cmd_t *curr = cmds->head;       //Curr == current cmd_t    Currs w/ 1 s == current param_t

        while(curr != NULL){
            pid_t cpid = -1;

            

            if(curr != cmds->tail){
                //not last
                pipe(pipes);
            }

            cpid = fork();
            //fprintf(stderr, "%d, %d, command: %s\n",getpid(),cpid,curr->cmd);
            switch(cpid){
                case 0:
                    {
                    //child
                    //int count = curr->param_count;
                    char **args = calloc((curr->param_count+2),sizeof(char*));
                    param_t *currs = curr->param_list;
                    int i = 0;
                    args[0] = curr->cmd;

                    if(curr != cmds->head){
                        //not first
                    
                        dup2(p_trail,STDIN_FILENO);
                    } 

                    if(curr != cmds->tail){
                        //not last
                    
                        dup2(pipes[STDOUT_FILENO],STDOUT_FILENO);
                        close(pipes[STDOUT_FILENO]);
                        close(pipes[STDIN_FILENO]);
                    }

                    //fprintf(stderr, "%d first test: %s\n",getpid(),curr->cmd);

                    if(curr->input_src == REDIRECT_FILE){
                        int fdi = open(curr->input_file_name, O_RDONLY);
                        if (fdi < 0){
                            perror("cant open input file");
                            exit(1);
                        }
                        dup2(fdi,STDIN_FILENO);
                        close(fdi);
                    }

                    //fprintf(stderr, "%d second test: %s\n",getpid(),curr->cmd);

                    if(curr->output_dest == REDIRECT_FILE){
                        int fdo = open(curr->output_file_name, O_WRONLY | O_CREAT | O_TRUNC, 0660);
                        if (fdo < 0){
                            perror("can't open output file");
                            exit(1);
                        }
                        dup2(fdo,STDOUT_FILENO);
                        close(fdo);
                    }

                    i=1;

                    //fprintf(stderr, "%d third test: %s\n",getpid(),curr->cmd);

                    while(currs){
                        args[i] = currs->param;
                        currs = currs->next;
                        i++;
                    }

                    //fprintf(stderr, "%d forth test: %s\n",getpid(),curr->cmd);

                    //args[count+1] = NULL;

                    //fprintf(stderr, "execvp-ing: %s\n",curr->cmd);

                    execvp(args[0],args);
                    perror("execvp failed\n");
                    _exit(5);  
                    //exec above

                    }
                default:
                    //parent
                    if(curr != cmds->head){
                        //not first
                    
                        close(p_trail);
                    }
                    if(curr != cmds->tail){
                        //not last
                    
                        close(pipes[STDOUT_FILENO]);
                        p_trail = pipes[STDIN_FILENO];
                    }
                    break; 
            }
        curr = curr->next;
        }
        while(wait(NULL) > 0);
    }
}

void free_list(cmd_list_t *cmd_list){

    cmd_t *cmd = cmd_list->head;

    if(cmd->next == NULL){
    	free(cmd->raw_cmd);
    	free(cmd->cmd);
    	free(cmd->param_list);
    	free(cmd->input_file_name);
    	free(cmd->output_file_name);
    	free(cmd);
    } else {
    	free_cmd(cmd->next);
    	free(cmd->raw_cmd);
    	free(cmd->param_list);
    	free(cmd->input_file_name);
    	free(cmd->output_file_name);
    	free(cmd);
    }
    //free(cmd);
}

void print_list(cmd_list_t *cmd_list){
    cmd_t *cmd = cmd_list->head;

    while (NULL != cmd) {
        print_cmd(cmd);
        cmd = cmd->next;
    }

}

void free_cmd (cmd_t *cmd){

    cmd_t *curr = cmd;

    if(curr->next == NULL){
    	free(curr->raw_cmd);
    	free(curr->cmd);
    	free(curr->input_file_name);
    	free(curr->output_file_name);
    	free(curr->param_list);
    	free(curr);
    } else {
    	free_cmd(curr->next);
    	free(curr->raw_cmd);
    	free(curr->cmd);
    	free(curr->input_file_name);
    	free(curr->output_file_name);
    	free(curr->param_list);
    	free(curr);
    }
}

void print_cmd(cmd_t *cmd){
    param_t *param = NULL;
    int pcount = 1;

    fprintf(stderr,"raw text: +%s+\n", cmd->raw_cmd);
    fprintf(stderr,"\tbase command: +%s+\n", cmd->cmd);
    fprintf(stderr,"\tparam count: %d\n", cmd->param_count);
    param = cmd->param_list;

    while (NULL != param) {
        fprintf(stderr,"\t\tparam %d: %s\n", pcount, param->param);
        param = param->next;
        pcount++;
    }

    fprintf(stderr,"\tinput source: %s\n"
            , (cmd->input_src == REDIRECT_FILE ? "redirect file" :
               (cmd->input_src == REDIRECT_PIPE ? "redirect pipe" : "redirect none")));
    fprintf(stderr,"\toutput dest:  %s\n"
            , (cmd->output_dest == REDIRECT_FILE ? "redirect file" :
               (cmd->output_dest == REDIRECT_PIPE ? "redirect pipe" : "redirect none")));
    fprintf(stderr,"\tinput file name:  %s\n"
            , (NULL == cmd->input_file_name ? "<na>" : cmd->input_file_name));
    fprintf(stderr,"\toutput file name: %s\n"
            , (NULL == cmd->output_file_name ? "<na>" : cmd->output_file_name));
    fprintf(stderr,"\tlocation in list of commands: %d\n", cmd->list_location);
    fprintf(stderr,"\n");
}

#define stralloca(_R,_S) {(_R) = alloca(strlen(_S) + 1); strcpy(_R,_S);}

void parse_commands(cmd_list_t *cmd_list){
    cmd_t *cmd = cmd_list->head;
    char *arg;
    char *raw;

    while (cmd) {
        // Because I'm going to be calling strtok() on the string, which does
        // alter the string, I want to make a copy of it. That's why I strdup()
        // it.
        // Given that command lines should not be tooooo long, this might
        // be a reasonable place to try out alloca(), to replace the strdup()
        // used below. It would reduce heap fragmentation.
        //raw = strdup(cmd->raw_cmd);

        // Following my comments and trying out alloca() in here. I feel the rush
        // of excitement from the pending doom of alloca(), from a macro even.
        // It's like double exciting.
        stralloca(raw, cmd->raw_cmd);

        arg = strtok(raw, SPACE_DELIM);
        if (NULL == arg) {
            // The way I've done this is like ya'know way UGLY.
            // Please, look away.
            // If the first command from the command line is empty,
            // ignore it and move to the next command.
            // No need free with alloca memory.
            //free(raw);
            cmd = cmd->next;
            // I guess I could put everything below in an else block.
            continue;
        }
        // I put something in here to strip out the single quotes if
        // they are the first/last characters in arg.
        if (arg[0] == '\'') {
            arg++;
        }
        if (arg[strlen(arg) - 1] == '\'') {
            arg[strlen(arg) - 1] = '\0';
        }
        cmd->cmd = strdup(arg);
        // Initialize these to the default values.
        cmd->input_src = REDIRECT_NONE;
        cmd->output_dest = REDIRECT_NONE;

        while ((arg = strtok(NULL, SPACE_DELIM)) != NULL) {
            if (strcmp(arg, REDIR_IN) == 0) {
                // redirect stdin

                //
                // If the input_src is something other than REDIRECT_NONE, then
                // this is an improper command.
                //

                // If this is anything other than the FIRST cmd in the list,
                // then this is an error.

                cmd->input_file_name = strdup(strtok(NULL, SPACE_DELIM));
                cmd->input_src = REDIRECT_FILE;
            }
            else if (strcmp(arg, REDIR_OUT) == 0) {
                // redirect stdout
                       
                //
                // If the output_dest is something other than REDIRECT_NONE, then
                // this is an improper command.
                //

                // If this is anything other than the LAST cmd in the list,
                // then this is an error.

                cmd->output_file_name = strdup(strtok(NULL, SPACE_DELIM));
                cmd->output_dest = REDIRECT_FILE;
            }
            else {
                // add next param
                param_t *param = (param_t *) calloc(1, sizeof(param_t));
                param_t *cparam = cmd->param_list;

                cmd->param_count++;
                // Put something in here to strip out the single quotes if
                // they are the first/last characters in arg.
                if (arg[0] == '\'') {
                    arg++;
                }
                if (arg[strlen(arg) - 1] == '\'') {
                    arg[strlen(arg) - 1] = '\0';
                }
                param->param = strdup(arg);
                if (NULL == cparam) {
                    cmd->param_list = param;
                }
                else {
                    // I should put a tail pointer on this.
                    while (cparam->next != NULL) {
                        cparam = cparam->next;
                    }
                    cparam->next = param;
                }
            }
        }
        // This could overwite some bogus file redirection.
        if (cmd->list_location > 0) {
            cmd->input_src = REDIRECT_PIPE;
        }
        if (cmd->list_location < (cmd_list->count - 1)) {
            cmd->output_dest = REDIRECT_PIPE;
        }

        // No need free with alloca memory.
        //free(raw);
        cmd = cmd->next;
    }

    if (isVerbose > 0) {
        print_list(cmd_list);
    }
}

int main(void){
	
	process_user_input_simple();

    return 0;
}  
















//