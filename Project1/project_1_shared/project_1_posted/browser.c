/* CSCI-4061 Fall 2022
 * Group Member #1: Ashwin Wariar - waria012
 * Group Member #2: Ryan Koo - koo00017
 * Group Member #3: Yong Hyeon Yi - yi000055
 */

#include "wrapper.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <errno.h>
#include <gtk/gtk.h>

/* === PROVIDED CODE === */
// Function Definitions
void new_tab_created_cb(GtkButton *button, gpointer data);
int run_control();
int on_blacklist(char *uri);
int bad_format (char *uri);
void uri_entered_cb(GtkWidget* entry, gpointer data);
void init_blacklist (char *fname);

/* === PROVIDED CODE === */
// Global Definitions
#define MAX_TAB 100                 //Maximum number of tabs allowed
#define MAX_BAD 1000                //Maximum number of URL's in blacklist allowed
#define MAX_URL 100                 //Maximum char length of a url allowed

/* === STUDENTS IMPLEMENT=== */
// HINT: What globals might you want to declare?

pid_t CHILDREN_PID[MAX_TAB];
char BLACKLIST[MAX_BAD * MAX_URL];
int TAB_NUM = 0;

/* === PROVIDED CODE === */
/*
 * Name:		          new_tab_created_cb
 *
 * Input arguments:	
 *      'button'      - whose click generated this callback
 *			'data'        - auxillary data passed along for handling
 *			                this event.
 *
 * Output arguments:   void
 * 
 * Description:        This is the callback function for the 'create_new_tab'
 *			               event which is generated when the user clicks the '+'
 *			               button in the controller-tab. The controller-tab
 *			               redirects the request to the parent (/router) process
 *			               which then creates a new child process for creating
 *			               and managing this new tab.
 */
// NO-OP for now
void new_tab_created_cb(GtkButton *button, gpointer data)
{}
 
/* === PROVIDED CODE === */
/*
 * Name:                run_control
 * Output arguments:    void
 * Function:            This function will make a CONTROLLER window and be blocked until the program terminates.
 */
int run_control()
{
  // (a) Init a browser_window object
	browser_window * b_window = NULL;

	// (b) Create controller window with callback function
	create_browser(CONTROLLER_TAB, 0, G_CALLBACK(new_tab_created_cb),
		       G_CALLBACK(uri_entered_cb), &b_window); 

	// (c) enter the GTK infinite loop
	show_browser();
	return 0;
}

/* === STUDENTS IMPLEMENT=== */
/* 
    Function: on_blacklist  --> "Check if the provided URI is in th blacklist"
    Input:    char* uri     --> "URI to check against the blacklist"
    Returns:  True  (1) if uri in blacklist
              False (0) if uri not in blacklist
    Hints:
            (a) File I/O
            (b) Handle case with and without "www." (see writeup for details)
            (c) How should you handle "http://" and "https://" ??
*/ 
int on_blacklist (char *uri) {
  // Need to deal with two versions of url: (i) with www. (ii) witout www.
  // If the entered URL includes www. , create a copy without www. to uri_new
  char uri_new[MAX_URL];
  char http[128] = "http://";
  char https[128] = "https://";
  char www[128] = "www."; // Initialize strings to deal with

  // memset(uri_new, '\0', sizeof(uri_new)); //clear the memory
  if (strstr(uri, http) != NULL){ // Checks to see if URI has 'http://'
    if (strstr(uri, www) != NULL) { // Checks to see if URI has 'www.'
      strcpy(uri_new, uri+strlen(http)+strlen(www)); // Turns the URI into the same URI without the 'http://www.'
    } else {
      strcpy(uri_new, uri+strlen(http)); // Turns the URI into the URI without 'http://'
    }
  }
  else if(strstr(uri, https) != NULL){ // Same as the if statment above except we are checking for 'https://'
      if (strstr(uri, www) != NULL) {
        strcpy(uri_new, uri+strlen(https)+strlen(www));
      } else {
        strcpy(uri_new, uri+strlen(https));
      }
  } else {
    strcpy(uri_new, uri);
  }

  if (strstr(BLACKLIST, uri_new) != NULL) { // Checks if the new URI is in the global blacklist array
     return 1;
  }


  return 0;
}

/* === STUDENTS IMPLEMENT=== */
/* 
    Function: bad_format    --> "Check for a badly formatted url string"
    Input:    char* uri     --> "URI to check if it is bad"
    Returns:  True  (1) if uri is badly formatted 
              False (0) if uri is well formatted
    Hints:
              (a) String checking for http:// or https://
*/
int bad_format (char *uri) {
  //STUDENTS IMPLEMENT
  char str1[128] = "http://";
  char str2[128] = "https://"; // Initialize string that contains http:// or https://
  if (strstr(uri, str1) != NULL || strstr(uri, str2) != NULL) { // Checks to see if the URI contains either http:// or https://
    return 0;
  }
  return 1;
}

/* === STUDENTS IMPLEMENT=== */
/*
 * Name:                uri_entered_cb
 *
 * Input arguments:     
 *                      'entry'-address bar where the url was entered
 *			                'data'-auxiliary data sent along with the event
 *
 * Output arguments:     void
 * 
 * Function:             When the user hits the enter after entering the url
 *			                 in the address bar, 'activate' event is generated
 *			                 for the Widget Entry, for which 'uri_entered_cb'
 *			                 callback is called. Controller-tab captures this event
 *			                 and sends the browsing request to the router(/parent)
 *			                 process.
 * Hints:
 *                       (a) What happens if data is empty? No Url passed in? Handle that
 *                       (b) Get the URL from the GtkWidget (hint: look at wrapper.h)
 *                       (c) Print the URL you got, this is the intermediate submission
 *                       (d) Check for a bad url format THEN check if it is in the blacklist
 *                       (e) Check for number of tabs! Look at constraints section in lab
 *                       (f) Open the URL, this will need some 'forking' some 'execing' etc. 
 */
void uri_entered_cb(GtkWidget* entry, gpointer data)
{
  //STUDENTS IMPLEMENT
  char* url = get_entered_uri(entry);
  char tab_num[20];

  if (data == NULL) { // If an empty tab is entered in, send a bad format popup
    bad_format(url); 
  } 
  else {
      printf("URL entered is %s\n", url);  // Print entered URL 
    
      if (bad_format(url)) { // Checks if the URL has a bad format
          alert("Alert: Bad Format!");
          return;
      } 
      else if (on_blacklist(url)) { // Checks if the URL is in the blacklist
          alert("Alert: On Blacklist!");
          return;
      } 
      else {
          if (TAB_NUM < MAX_TAB) {
              pid_t child = fork(); // Fork the process as long as # of tabs doesn't exceed limit
              if(child == -1){
                  perror("Failed to fork");
                  exit(1);
              }	  
              else if(child == 0) {
                CHILDREN_PID[TAB_NUM] = getpid();
                sprintf(tab_num, "%d", TAB_NUM); // Turns the tab_num into string and executes render program
                execl("./render", "render", tab_num, url, NULL);
              }
                TAB_NUM++;
        }
	  else
	      alert("Alert: Max Tab!"); // Alerts user that MAX_TAB limit has been reached
    	  
     } 
  }
      return;
}

/* === STUDENTS IMPLEMENT=== */
/* 
    Function: init_blacklist --> "Open a passed in filepath to a blacklist of url's, read and parse into an array"
    Input:    char* fname    --> "file path to the blacklist file"
    Returns:  void
    Hints:
            (a) File I/O (fopen, fgets, etc.)
            (b) If we want this list of url's to be accessible elsewhere, where do we put the array?
*/
void init_blacklist (char *fname) {
  char line[MAX_URL];
  FILE *f = fopen(fname, "rt"); // Opens blacklist
  if (f == NULL) { // Make sure file opens correctly
    perror("Failed to open");
    exit(1);
  }
 
  while(fgets(line, MAX_URL, f) != NULL) { // Reads and parses the blacklist into an array
      char str[MAX_URL];
      sscanf(line, "%s ", str); // Reads the input from 'line'
      strcat(BLACKLIST, line); // Appends the blacklist URL onto the array
  }
  //printf("%s", BLACKLIST); // Prints the blacklist, which is declared globally
  fclose(f); // Closes the blacklist
  return;
}

/* === STUDENTS IMPLEMENT=== */
/* 
    Function: main 
    Hints:
            (a) Check for a blacklist file as argument, fail if not there [PROVIDED]
            (b) Initialize the blacklist of url's
            (c) Create a controller process then run the controller
                (i)  What should controller do if it is exited? Look at writeup (KILL! WAIT!)
            (d) Parent should not exit until the controller process is done 
*/
int main(int argc, char **argv)
{
  // Check arguments for blacklist, error and warn if no blacklist
  if (argc != 2) {
    fprintf (stderr, "browser <blacklist_file>\n");
    exit (0);
  }
  // Initialize the blacklist of url's
  init_blacklist(argv[1]);

  int status = 0;
  pid_t browser_process = fork(); // Forks the broswer process

  // (1) Check for error of fork() (AND ALL SYSTEM CALLS)
  if (browser_process == -1) {
    perror("Failed to fork");
    exit(1);
  }
  if (browser_process == 0) { // Initializes a window to open
    run_control();

    // Loop through all PID's from (get_entered_uri) and KILL then wait
    for (int i = 0; i < TAB_NUM; i++) {
      kill(CHILDREN_PID[i], 9);
    }

    // waiting for all children of controller 
    while ((wait(&status)) > 0);

    
    // if (window == 0) {
    //   signal(SIGQUIT, SIG_IGN);
    //   kill(0, SIGQUIT); // Kill all the child processes before the parents   
    // }
  }

  while ((wait(&status)) > 0); // Waits for all of the child processes to exit

  return 0;
}
