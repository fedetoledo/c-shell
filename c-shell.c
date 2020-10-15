#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>

#define MAX_SWITCH_LENGTH 	50     
#define MAX_CMD_LENGTH     	512 
#define MAX_CMD_SWITCHES   	100
#define CHECK_FOR_ZOMBIES  	25

//Signal handlers
void zombie_killer(int);
void background_cmd_failed(int);

void prompt();
void print_help();
void cmd_input(char *);
void clear_spaces_end(char *);
void clear_spaces_start(char *);
void clear_control_chars(char *);
void clear_ampersand(char *);
void parse_cmd(char *, int, char *[]);
char *print_dir();
int cd(char *);
int background_cmd(char *);
int redirect_cmd(char *);

pid_t last_pid = 0;

int main(int argc, char **argv) {
    char command[MAX_CMD_LENGTH];
    char *command_argv[MAX_CMD_SWITCHES];
    int returning;

    signal(SIGALRM, zombie_killer);
    signal(SIGUSR1, background_cmd_failed);
    alarm(CHECK_FOR_ZOMBIES);

    do {
        cmd_input(command);
        // parse the command to split the switches
        parse_cmd(command, MAX_CMD_SWITCHES, command_argv);

        // exit the shell
        if (strncmp(command, "exit", 4) == 0) break;

        // help
        if (strncmp(command, "help", 4) == 0) {
            print_help();
            continue;
        }

        // change dir
        if (strncmp(command, "cd", 2) == 0) {
            if (command_argv[1] > 0 && cd(command_argv[1]) < 0) {
                perror(command_argv[1]);
            }
            continue; // skips the fork
        }

        pid_t pid = fork();

        if (pid == 0) { // child
            signal(SIGALRM, SIG_DFL); 
            signal(SIGUSR1, SIG_DFL);

            int back = background_cmd(command);

            if ( background_cmd(command) ) clear_ampersand(command);

            // parse the command to split the switches
            parse_cmd(command, MAX_CMD_SWITCHES, command_argv);
            int i = 0;

            // output redirection
            if (redirect_cmd(command)) {

                char *archivo;
                int lastIndex = 0;
                int i;
                
                // search for filename in command (last cmd vector position)
                for (i = 0; command_argv[i] != NULL; i++);
                lastIndex = i-1;
                archivo = command_argv[lastIndex];
                
                command_argv[lastIndex-1] = '\0'; // remove ">" symbol to execute the command
                command_argv[lastIndex] = '\0'; // remove filename to execute the command

                int outfile = open(archivo, O_CREAT | O_WRONLY, S_IRWXU);
                if (outfile == -1){
                      fprintf(stderr, "Error: failed to create the file %s\n", archivo);
                } else {
                      if(dup2(outfile, STDOUT_FILENO) != STDOUT_FILENO){
                        fprintf(stderr, "Error: failed stdout redirection\n");
                      }
                    execvp(command_argv[0], command_argv);
                    printf("Error: Command [%s] not found\n", command);
                }
            } else {
                execvp(command_argv[0], command_argv);
                printf("Error: Command [%s] not found\n", command);
            }

            if (back) kill(getppid(), SIGUSR1); // send signal to parent process to notify child's failure
            exit(127);
        } else { // parent process
            last_pid = pid;
            if ( !background_cmd(command) ) {
                pid = waitpid(pid, &returning, 0);
                if (WEXITSTATUS(returning) != 127) {
                    printf("Process %d exited with status: %d\n", pid, WEXITSTATUS(returning));
                }
            }
        }

    } while(strncmp(command, "exit", 4));
    // before exit, remove all posible zombie processes
    // orphan processes may still being executed in background
    zombie_killer(0);
    return 0;
}

// functions 

void cmd_input(char *cmd) {
    prompt();
    memset(cmd, 0, MAX_CMD_LENGTH);
    fgets(cmd, MAX_CMD_LENGTH, stdin);

    // change "\n" to "\0" at the end of command
    clear_control_chars(cmd);
    clear_spaces_end(cmd);
    clear_spaces_start(cmd);
}

int background_cmd(char *cmd) {
    int n = strlen(cmd)-1;
    return( n >= 0 && cmd[n] == '&');
}

void clear_spaces_end(char *cmd) {
    int n = strlen(cmd) - 1;
    while( n >= 0 && cmd[n] == ' ') {
        cmd[n] = '\0';
        n--;
    }
}

void clear_spaces_start(char *cmd) {
    char *p = cmd;
    while( *p && *p == ' ') p++;
    if (p == cmd) return;
    char tmp[MAX_CMD_LENGTH];
    strcpy(tmp,p);
    strcpy(cmd,tmp);
}

void clear_control_chars(char *cmd) {
    char *p = cmd;
    while(*p) {
        switch(*p) {
            case '\n':
            case '\r':
            case '\t':
            case '\b':
                *p = ' ';
                break;
        }
        p++;
    }
}

void clear_ampersand(char *cmd) {
    cmd[strlen(cmd)-1]= '\0';
    clear_spaces_end(cmd);
}

void parse_cmd(char *cmd, int argc, char *argv[]) {
    int i = 0;
    int pos1 = 0;
    int pos2 = 0;
    int nc = 0;
    int tope = strlen(cmd);
    do {
        pos1= i;
        while(i < tope && cmd[i] != ' ') i++;
        pos2= i;
        if ( pos1 == pos2 ) break;
        if ( (pos2 - pos1) > MAX_SWITCH_LENGTH) {
            printf("parseo_cmd(): argumento %d de command [%s] supera los %d caracteres\n",nc,cmd,MAX_SWITCH_LENGTH);
            break;    
        }
        argv[nc] = strndup(cmd+pos1,pos2-pos1);
        // advance to next switch
        while( i < tope && cmd[i] == ' ') i++;
        nc++;
    } while( nc < argc && i < tope);
    argv[nc] = NULL;
}

//SIGNAL HANDLERS --------------

void zombie_killer(int signo) {
    int returning = 0;
    pid_t pid = 0;
    while((pid = waitpid(0, &returning, WNOHANG)) > 0) {
        // kills a zombie
    }
    alarm(CHECK_FOR_ZOMBIES);
}

void background_cmd_failed(int signo) {
    int returning;
    waitpid(last_pid, &returning, 0);
}

//EXTRA ----------------
void prompt() {
    char *user = getenv("USER");
    char *desktop = getenv("DESKTOP_SESSION");
    printf("\033[0;32m");
    printf("%s@%s:", user, desktop);
    printf("\033[0;33m");
    printf("%s", print_dir());
    printf("\033[0m");
    printf("$ ");
}

char * print_dir() {
    char cwd[1024];
    return getcwd(cwd, sizeof(cwd));
}

int cd (char * path) {
    return chdir(path);
}

int redirect_cmd(char *cmd) {

    for (int i=0; i< strlen(cmd); i++) {
        if (cmd[i] == '>') {
            return 1;
        }

    }
    return 0;
}

void print_help() {
    printf("\n");
    printf("******* C-shell *******\n");
    printf("Available commands\n");
    printf("cd <path> - Changes current directory to path\n");
    printf("help - Show help\n");
    printf("exit - Exit C-shell\n");
    printf("Command > filename - redirect the output to the file\n");
}
