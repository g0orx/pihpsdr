/*
 * File MacOS.c
 *
 * This file need only be compiled on MacOS.
 * It contains some functions only needed there:
 *
 * MaOSstartup(char *path)      : create working dir in "$HOME/Library/Application Support" etc.
 *
 * where *path is argv[0], the file name of the running executable
 *
 */

#ifdef __APPLE__

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include <pwd.h>
#include <uuid/uuid.h>

void MacOSstartup(char *path) {
//
// We used to do this from a wrapper shell script.
// However, this confuses MacOS when it comes to
// granting access to the local microphone therefore
// it is now built into the binary
//
// We have to distinguish two basic situations:
//
// a) piHPSDR is called from a command line
// b) piHPSDR is within an app bundle
//
// In case a) nothing has to be done here (standard UNIX),
// but in case b) we do the following:
//
// - if not yet existing, create the directory "$HOME/Library/Application Support/piHPSDR"
// - copy the icon to that directory
// - chdir to that directory
//
// Case b) is present if the current working directory is "/".
//
// If "$HOME/Library/Application Support" does not exist, fall back to case a)
//
    char *c;
    char source[1024];
    char workdir[1024];
    char *homedir;
    const char *AppSupport ="/Library/Application Support/piHPSDR";
    const char *IconInApp  ="/../Resources/hpsdr.png";
    struct stat st;
    int fdin, fdout;
    int rc;
//
//  If the current work dir is NOT "/", just do nothing
//    
    *workdir=0;
    if (getcwd(workdir,sizeof(workdir)) == NULL) return;
    if (strlen(workdir) != 1 || *workdir != '/') return;

//
//  Determine working directory,
//  "$HOME/Library/Application Support/piHPSDR"
//  and create it if it does not exist
//  take care to enclose name with quotation marks
//
    if ((homedir = getenv("HOME")) == NULL) {
      homedir = getpwuid(getuid())->pw_dir;
    }
    if (strlen(homedir) +strlen(AppSupport) > 1020) return;
    strcpy(workdir,homedir);
    strcat(workdir, AppSupport);

//
//  Check if working dir exists, otherwise try to create it
//
    if (stat(workdir, &st) < 0) {
      mkdir(workdir, 0755);
    }
//
//  Is it there? If not, give up
//
    if (stat(workdir, &st) < 0) {
      return;
    }
//
//  Is it a directory? If not, give up
//
    if ((st.st_mode & S_IFDIR) == 0) {
      return;
    }
//
//  chdir to the work dir
//
    chdir(workdir);
//
//  Make two local files for stdout and stderr, to allow
//  post-mortem debugging
//
    (void) freopen("pihpsdr.stdout", "w", stdout);
    (void) freopen("pihpsdr.stderr", "w", stderr);
//
//  Copy icon from app bundle to the work dir
//
    if (getcwd(workdir,sizeof(workdir)) != NULL && strlen(path) < 1024) {
      //
      // source = basename of the executable
      //
      strcpy(source, path);
      c=rindex(source,'/');
      if (c) {
        *c=0;
        if ((strlen(source) + strlen(IconInApp) < 1024)) {
          strcat(source,  IconInApp);
          //
          // Now copy the file from "source" to "workdir"
          //
          fdin=open(source, O_RDONLY);
          fdout=open("hpsdr.png", O_WRONLY | O_CREAT | O_TRUNC, (mode_t) 0400);
          if (fdin >= 0 && fdout >= 0) {
            //
            // Now do the copy, use "source" as I/O buffer
            //
            while ((rc=read(fdin, source, 1024)) > 0) {
              write (fdout, source, rc);
            }
            close(fdin);
            close(fdout);
          }
        }
      }
    }
}

#endif
