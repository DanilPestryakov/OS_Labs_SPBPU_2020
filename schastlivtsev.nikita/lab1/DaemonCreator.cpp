#include "DaemonCreator.h"
#include <iostream> // for runtime error exceptions
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fstream>
#include <syslog.h>
#include <string>

#include "SignalHandler.h"

bool DaemonCreator::createDaemon(SettingsManager& settingsManager) {
    bool parent = forkWrapper(); // first fork
    if (parent) {
        return true; // can kill the parent process in main
    }

    if (setsid() < 0) // new session
        throw std::runtime_error("Session creation failed");

    // set signal handlers
    SignalHandler::setSettingsManager(&settingsManager);
    signal(SIGTERM, SignalHandler::getSigtermHandler()); // SIGTERM -- finish
    signal(SIGHUP, SignalHandler::getSighupHandler()); // SIGHUP -- read config file

    parent = forkWrapper(); // second fork
    if (parent) {
        return true; // can kill the parent process in main
    }

    umask(0); // set mask for the file permissions
    chdir("/"); // set current directory

    // Close all open file descriptors
    for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
        close(x);
    }

    std::string pidFilename(Settings::getPidPath());
    killBrother(pidFilename.c_str());

    pid_t pid = getpid();
    savePid(pidFilename.c_str(), pid); // save daemon' pid to a file

    openlog("logLoggerDaemon", LOG_PID, LOG_DAEMON); // open system log
    syslog(LOG_INFO, "Daemon created");
    return false; // it's the last fork, don't kill it
}


bool DaemonCreator::forkWrapper() {
    pid_t pid = fork(); // create new process

    if (pid < 0) // error in fork
        throw std::runtime_error("Fork has not been created (PID < 0)");
    else if (pid > 0) { // kill parent
        closelog();
        return true;  // it's a parent process
    }
    return false;  // it's a child process
}


void DaemonCreator::savePid(char const * const pidFile, const pid_t pid) {
    // Add check file existance
    std::ofstream outfile(pidFile, std::ofstream::out);
    outfile << pid;
    outfile.close();
}


void DaemonCreator::killBrother(char const * const pidFile) {
    std::ifstream infile(pidFile, std::ios::in);
    if (infile.good()) {  // kill bro only if it's exist
        pid_t pid;
        infile >> pid; // get PID of another process
        //std::string command("kill -SIGTERM ");
        //command += std::to_string(pid); // add process PID to command
        //system(command.c_str()); // kill brother with SIGTERM signal
        kill(pid, SIGTERM);
        infile.close();
    }
    else {
        syslog(LOG_ERR, "No PID file");
    }
}
