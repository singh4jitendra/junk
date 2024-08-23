/*
 ******************************************************************************
 *
 * filetimecheck.c - This program checks the time associated with a file and
 *                   checks to see if the time stamp is older than a
 *                   specified time.
 *
 * Command-line options:
 *                   a  - Compare time last accessed; requires time in minutes
 *                   c  - Compare time last changed; requires time in minutes
 *                   m  - Compare time last modified; requires time in minutes
 *
 * Return codes:
 *                   0  - File is younger than time specified
 *                   1  - File is older than or equal to time specified
 *                   2  - Syntax error on command line
 *                   3  - File error occurred
 *
 ******************************************************************************
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#ifndef unix
#error This program requires a UNIX compiler.
#endif

#define ACCESS_TIME    1
#define MODIFY_TIME    2
#define CHANGE_TIME    3
#define DEFAULT_TIME   15
#define DEFAULT_FLAG   MODIFY_TIME

/*
 ******************************************************************************
 *
 * Function:    main
 *
 * Description: This function processes the command line arguments and
 *              checks the file times. It is called automatically at
 *              startup.
 *
 * Parameters:  argc - int; number of command line arguments
 *              argv - char *[]; array containing the arguments
 *
 * Returns:     Returns an integer to the operating system.
 *                  0  - File is younger than time specified
 *                  1  - File is older than or equal to time specified
 *                  2  - Syntax error on command line
 *                  3  - File error occurred
 *
 ******************************************************************************
*/
main(int argc, char *argv[])
{
   int option;
   int flag = 0;
   int index;
   time_t max_time_difference = (time_t)(DEFAULT_TIME * 60);
   time_t current_time;
   time_t delta_time;
   struct stat fs;
   extern char *optarg;
   extern int optind;

   if (argc < 2) {
      fprintf(stderr,
         "Usage: ttime [-a minutes | -m minutes | -c minutes] file\n");
      exit(2);
   }

   while ((option = getopt(argc, argv, "a:c:m:")) != EOF) {
      switch (option) {
         case 'a':    /* use time last accessed */
            if (flag) {
               fprintf(stderr, "only one option allowed\n");
               exit(2);
            }

            flag = ACCESS_TIME;
            max_time_difference = (time_t)(atol(optarg) * 60);
            break;
         case 'c':    /* use time last changed */
            if (flag) {
               fprintf(stderr, "only one option allowed\n");
               exit(2);
            }

            flag = CHANGE_TIME;
            max_time_difference = (time_t)(atol(optarg) * 60);
            break;
         case 'm':    /* use time last modified */
            if (flag) {
               fprintf(stderr, "only one option allowed\n");
               exit(2);
            }

            flag = MODIFY_TIME;
            max_time_difference = (time_t)(atol(optarg) * 60);
            break;
         default:
            exit(2);
      }
   }

   if (!flag)
      flag = DEFAULT_FLAG;

   index = optind;
   if (!strcmp(argv[index], "")) {
      fprintf(stderr,
         "Usage: ttime [-a minutes | -m minutes | -c minutes] file\n");
      exit(2);
   }

   if (stat(argv[index], &fs) < 0)
      fprintf(stderr, "%s: %s\n", argv[index], strerror(errno));
   else {
      current_time = time(NULL);

      if (flag == ACCESS_TIME)
         delta_time = current_time - fs.st_atime;
      else if (flag == MODIFY_TIME)
         delta_time = current_time - fs.st_mtime;
      else if (flag == CHANGE_TIME)
         delta_time = current_time - fs.st_ctime;

      return (delta_time >= max_time_difference);
   }

   return 3;
}
