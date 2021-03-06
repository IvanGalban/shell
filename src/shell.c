#include <stdio.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include "../include/parser.h"

#define register_size 51
#define buffer_size 1024

char register_buffer[register_size][buffer_size];

int register_buffer_size[register_size];

int start_register=0;

int end_register=0;

const char *endl="\n";

char *defaul_path;

void print_history(int fd)
{
	int i;
	for (i = start_register; i != end_register; i=(i+1)%register_size)
	{
		write(fd,register_buffer[i],register_buffer_size[i]);
	}	
}

int write_prompt()
{
		char path[1024],*logname,user[64],*p1="@",*p2=":",*p3="$ ",*p4="~",*p5=">";
		getcwd(path,1024);
		logname = getenv("LOGNAME");
		gethostname(user,64);
		write(STDIN_FILENO,p5,strlen(p5));
		write(STDIN_FILENO,logname,strlen(logname));
		write(STDIN_FILENO,p1,strlen(p1));
		write(STDIN_FILENO,user,strlen(user));
		write(STDIN_FILENO,p2,strlen(p2));

		if(strncmp(defaul_path,path,strlen(defaul_path))==0)
		{
			write(STDIN_FILENO,p4,strlen(p4));
			strcpy(path,path+strlen(defaul_path));
		}

		write(STDIN_FILENO,path,strlen(path));
		write(STDIN_FILENO,p3,strlen(p3));
}


void read_command(command *cmd)
{
	 	char buffer[buffer_size];
		int size=read(STDIN_FILENO,buffer, buffer_size);
		

		if(buffer[0]!=' ')
		{
			register_buffer_size[end_register]=size;
			strncpy(register_buffer[end_register],buffer,size);
			end_register=(end_register+1)%register_size;
		}

		if(end_register==start_register)
			start_register=(start_register+1)%register_size;	
		
		parse_command(buffer, size, cmd);
		
}

int get_outfile(simple_command *cmd)
{
	int i;
	int outfile=STDOUT_FILENO;

	int size_outfile=cmd->_no_outfiles;

	if(size_outfile > 0)
	{
		for (i = 0; i < cmd->_no_outfiles; ++i)
		{
	        if(i > 0)
	        {
				close (outfile);
	        }
			if(cmd->_outfiles[i]._type == t_output_write)
			{
				outfile=open(cmd->_outfiles[i]._file, O_WRONLY | O_CREAT | O_TRUNC,S_IRWXU);
			}
			else
			{
				outfile=open(cmd->_outfiles[i]._file, O_APPEND| O_CREAT|O_WRONLY,S_IRWXU );
			}
		}
	}
	return outfile;
}

int get_infile(simple_command *cmd)
{
	int infile=STDIN_FILENO;
	
	int size_infile=cmd->_no_infiles;
	
	if(size_infile>0)
		infile = open(cmd->_infiles[size_infile-1],O_RDONLY,S_IRWXU);

	return infile;
}


int exec_command(simple_command* cmd,int infile,int outfile)
{
	if (infile != STDIN_FILENO)
	{
		dup2 (infile, STDIN_FILENO);
		close (infile);
	}
	if (outfile != STDOUT_FILENO)
	{
		dup2 (outfile, STDOUT_FILENO);
		close (outfile);
	}


	execvp(cmd->_tokens[0],cmd->_tokens);
	perror ("pm_sh");
	exit (1);
}

int builtin_command(simple_command *cmd,int fd)
{
	char *tmp=cmd->_tokens[0];
	if(strcmp("cd",tmp)==0)
	{
		
		if(cmd->_tokens[1]==NULL || strcmp("~",cmd->_tokens[1])==0)
		{
			chdir(defaul_path);
		}
		else if (chdir(cmd->_tokens[1]) != 0) 
		{
  			perror("pm_sh");
		}
  		return 1;
	}
	if(strcmp("history",tmp)==0)
	{
			print_history(fd);
			return 1;
	}
	if(strcmp("exit",tmp)==0)
		return 2;
    return 0;
}

int execute_process(command *cmd)
{
	int i;
	pid_t pid;
	int fdpipe[2], infile = get_infile(&cmd->_simple_commands[0]), outfile;
	
	for(i=0;i<cmd->_no_simple_commands;i++)
	{
			if(infile<0)
			{
				perror("pm_sh");
				return -1;
			}

			outfile=get_outfile(&cmd->_simple_commands[i]);
			
			if(i!=cmd->_no_simple_commands-1)
			{
				pipe(fdpipe);
				
				if(outfile==STDOUT_FILENO)
					outfile=fdpipe[1];
				else
					close(fdpipe[1]);
			}

			int built = builtin_command(&cmd->_simple_commands[i],outfile);
			if(built==2)
				return 0;

			if(!built)
			{
				pid=fork();
				int status;
				if(pid==0)
				{
					exec_command(&cmd->_simple_commands[i],infile,outfile);
				}
			}
								
			if(infile!=get_infile(&cmd->_simple_commands[i]))
				close(infile);
			
			if(outfile!=get_outfile(&cmd->_simple_commands[i]))
				close(outfile);
			
			if(i<cmd->_no_simple_commands-1)
				infile=get_infile(&cmd->_simple_commands[i+1]);
			

			if(infile==STDIN_FILENO)
				infile=fdpipe[0];
			else 
				close(fdpipe[0]);
		}
	
	
	if(!cmd->_background)
	{
		int status;
		waitpid(pid,&status,0);
	}
}


int main()
{
	char *logname = getenv("LOGNAME");
	char home[] = "/home/";
	defaul_path=strcat(home, logname);
	chdir(defaul_path);
	
	while(1)
	{
		write_prompt();
		command cmd;
		read_command(&cmd);

		int exit = execute_process(&cmd);

		if(exit==2)
			return 0; 
	}

	return 0;
}