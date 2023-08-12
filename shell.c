/**
 * File: stsh.cc
 * -------------
 * Defines the entry point of the stsh executable.
 */

#include <iostream>
#include <fcntl.h>
#include <unistd.h>   // for fork
#include <sys/wait.h> // for waitpid
#include "stsh-parser/stsh-parse.h"
#include "stsh-parser/stsh-readline.h"
#include "stsh-exception.h"
#include "fork-utils.h" // this needs to be the last #include in the list

using namespace std;
void singleProcess(const pipeline& p) {
  const command& command = p.commands[0];

  // will return 0 if child
  pid_t pidOrZero = fork();
  
  if (pidOrZero == 0) {
   
    const char *input = p.input.c_str();
    const char *output = p.output.c_str();

     if (!p.output.empty()) {
      int fd_o = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd_o == -1) {
        throw STSHException("Could not open \"" + p.output + "\".");
      }
      dup2(fd_o, STDOUT_FILENO);
      close(fd_o);
    }

      if (!p.input.empty()) {
      int fd_i = open(input, O_RDONLY , 0644);
      if (fd_i == -1) {
        throw STSHException("Could not open \"" + p.input + "\".");
      }
      dup2(fd_i, STDIN_FILENO);
      close(fd_i);
     }

    // if we are child execute the command
    execvp(command.argv[0],command.argv);


    // if child gets here, there was an error
    throw STSHException(string(command.argv[0]) + ": Command not found.");

  }
  // if we are parent, wait for the child to finish
  waitpid(pidOrZero, NULL, 0);
}

void runTwoProcessPipeline(const pipeline& p) {

  const command& cmd1 = p.commands[0];
  const command& cmd2 = p.commands[1];
  pid_t pids[2];

  // Create a pipe where its FDs will automatically be closed on execvp
  int fds[2];
  pipe2(fds, O_CLOEXEC);

  // Spawn the first child
  pids[0] = fork();
  if (pids[0] == 0) {
    const char *input = p.input.c_str();
    if (!p.input.empty()) {
    int fd_i = open(input, O_RDONLY , 0644);
    if (fd_i == -1) {
       throw STSHException("Could not open \"" + p.input + "\".");
    }
    dup2(fd_i, STDIN_FILENO);
    close(fd_i);
    }

    // The first child's STDOUT should be the write end of the pipe
    dup2(fds[1], STDOUT_FILENO);
    execvp(cmd1.argv[0], cmd1.argv);
    throw STSHException(string(cmd1.argv[0]) + ": Command not found.");
  }

  // We no longer need the write end of the pipe
  close(fds[1]);

  // Spawn the second child
  pids[1] = fork();
  if (pids[1] == 0) {
    const char *output = p.output.c_str();
    if (!p.output.empty()) {
      int fd_o = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
      if (fd_o == -1) {
        throw STSHException("Could not open \"" + p.output + "\".");
      }
      dup2(fd_o, STDOUT_FILENO);
      close(fd_o);
    }
    // The second child's STDIN should be the read end of the pipe
    dup2(fds[0], STDIN_FILENO);
    execvp(cmd2.argv[0], cmd2.argv);
    throw STSHException(string(cmd2.argv[0]) + ": Command not found.");
  }

  // We no longer need the read end of the pipe
  close(fds[0]);
  
  
  // Wait for both children to finish
  waitpid(pids[0], NULL, 0);
  waitpid(pids[1], NULL, 0);
}

void runArbitraryProcessPipeline(const pipeline& p) {
  
  pid_t pids[p.commands.size()];

  // Case 1: First Process -- only redirecting the STDOUT
      const command& cmd1 = p.commands[0];

      int fds[2];
      pipe2(fds, O_CLOEXEC);

      // Spawn the first child
      pids[0] = fork();
      if (pids[0] == 0) {

        const char *input = p.input.c_str();
        if (!p.input.empty()) {
          int fd_i = open(input, O_RDONLY , 0644);
          if (fd_i == -1) {
             throw STSHException("Could not open \"" + p.input + "\".");
          }
          dup2(fd_i, STDIN_FILENO);
          close(fd_i);
        }

        // The first child's STDOUT should be the write end of the pipe
        dup2(fds[1], STDOUT_FILENO);
        execvp(cmd1.argv[0], cmd1.argv);
        throw STSHException(string(cmd1.argv[0]) + ": Command not found.");
      }
  // We no longer need the write end of the pipe
  close(fds[1]);

  // Case 2: Middle Child
      
      int middle_cases_amt = (p.commands.size() - 2);
      for (int i = 0; i < middle_cases_amt; i++) {

          int temp = fds[0];
          pipe2(fds, O_CLOEXEC);

          // Spawn the nth child
          pids[i + 1] = fork();

          if (pids[i + 1] == 0) {
            const command& general_cmd = p.commands[i + 1];
            // The first child's STDOUT should be the write end of the pipe
            dup2(fds[1], STDOUT_FILENO);
            dup2(temp, STDIN_FILENO); 
            execvp(general_cmd.argv[0], general_cmd.argv);
            throw STSHException(string(general_cmd.argv[0]) + ": Command not found.");
          
      }
          
     // We no longer need the write end of the pipe
     close(fds[1]);
     close(temp);
    }
  // Case 3: Last Child -- only redirecting the STDIN
      int last_child_index = (p.commands.size() - 1);
      const command& last_cmd = p.commands[last_child_index];
  
      // Spawn the last child      
        pids[last_child_index] = fork();
        if (pids[last_child_index] == 0) {
          // The second child's STDIN should be the read end of the pipe
          const char *output = p.output.c_str();
          if (!p.output.empty()) {
            int fd_o = open(output, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd_o == -1) {
              throw STSHException("Could not open \"" + p.output + "\".");
            }
            dup2(fd_o, STDOUT_FILENO);
            close(fd_o);
         }
          dup2(fds[0], STDIN_FILENO);
          execvp(last_cmd.argv[0], last_cmd.argv);
          throw STSHException(string(last_cmd.argv[0]) + ": Command not found.");
        }

    close(fds[0]);

// loop through and execvp all pids

for (int m = 0; m < p.commands.size(); m++) {
   waitpid(pids[m], NULL, 0);
}

}

/**
 * Create new process(es) for the provided pipeline. Spawns child processes with
 * input/output redirected to the appropriate pipes and/or files.
 */
void runPipeline(const pipeline& p) {
  
  if (p.commands.size() == 1) {
    singleProcess(p);
  } else if (p.commands.size() >= 2) {
  //   //throw STSHException(string("You enetered this block!"));
  //   runTwoProcessPipeline(p);
  // } else {
    runArbitraryProcessPipeline(p);
  }
}


/**
 * Define the entry point for a process running stsh.
 */
int main(int argc, char *argv[]) {
  pid_t stshpid = getpid();
  rlinit(argc, argv);
  while (true) {
    string line;
    if (!readline(line) || line == "quit" || line == "exit") break;
    if (line.empty()) continue;
    try {
      pipeline p(line);
      runPipeline(p);
    } catch (const STSHException& e) {
      cerr << e.what() << endl;
      if (getpid() != stshpid) exit(0); // if exception is thrown from child process, kill it
    }
  }
  return 0;
}
