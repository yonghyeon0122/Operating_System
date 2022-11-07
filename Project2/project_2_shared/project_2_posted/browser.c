#include "wrapper.h"
#include "util.h"
#include <sys/types.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <gtk/gtk.h>
#include <signal.h>

#define MAX_TABS 100  // this gives us 99 tabs, 0 is reserved for the controller
#define MAX_BAD 1000
#define MAX_URL 100
#define MAX_FAV 100
#define MAX_LABELS 100 


comm_channel comm[MAX_TABS];         // Communication pipes 
char favorites[MAX_FAV][MAX_URL];    // Maximum char length of a url allowed
int num_fav = 0;                     // # favorites

typedef struct tab_list {
  int free;
  int pid; // may or may not be useful
} tab_list;

// Tab bookkeeping
tab_list TABS[MAX_TABS];  


/************************/
/* Simple tab functions */
/************************/

// return total number of tabs
int get_num_tabs () {
  int count = 0;
  int i;
  for (i=1; i<MAX_TABS; i++) {
    if (TABS[i].free == 0) { // Checks for the total number of tabs and increments the count by 1
      count++;
    }
  }

  return count;
}

// get next free tab index
int get_free_tab () {
  int i;
  for (i=1; i<MAX_TABS; i++) {
    if (TABS[i].free > 0) { // Checks if the tab is free or not and returns the index of the next tab that is free
      // printf("NEXT FREE TAB: %d \n", i);
      return i;
    }
  }

  return MAX_TABS - 1;
}

// init TABS data structure
void init_tabs () {
  int i;

  for (i=1; i<MAX_TABS; i++) { // initializes all tabs except the first one to be free
    TABS[i].free = 1;
    TABS[0].free = 0;
  }
}

/***********************************/
/* Favorite manipulation functions */
/***********************************/

// return 0 if favorite is ok, -1 otherwise
// both max limit, already a favorite (Hint: see util.h) return -1
int fav_ok (char *uri) {
  if (num_fav >= MAX_FAV || on_favorites(uri)) { // Checks if the uri is already a favorite, or the maximum number of favorites have already been declared
    return -1;
  }

  return 0;
}


// Add uri to favorites file and update favorites array with the new favorite
void update_favorites_file (char *uri) {
  
  // First, check if the added uri is already in the favorite lists
  // Add uri to favorites file
  FILE *f = fopen(".favorites", "a");
  if (f == NULL) { // Make sure file opens correctly
    perror("Failed to open\n");
    exit(1);
  }
  
  fwrite(uri, 1, strlen(uri), f); // Writes uri to favorites array

  // Update favorites array with the new favorite
  strcat(favorites[num_fav], uri); // Appends the favorites URL onto the array
  num_fav++;

  fclose(f);  
}

// Set up favorites array
void init_favorites (char *fname) {
  char line[MAX_URL];
  FILE *f = fopen(fname, "rt"); // Opens favorites
  if (f == NULL) { // Make sure file opens correctly
    perror("Failed to open");
    exit(1);
  }
  while(fgets(line, MAX_URL, f) != NULL) { // Reads and parses the favorites into an array
      char str[MAX_URL];
      sscanf(line, "%s ", str); // Reads the input from 'line'
      strcat(favorites[num_fav], line); // Appends the favorites URL onto the array
      num_fav++;
  }
  fclose(f); // Closes the favorites array
  return;
}

// Make fd non-blocking just as in class!
// Return 0 if ok, -1 otherwise
// Really a util but I want you to do it :-)
int non_block_pipe (int fd) {
  int nFlags;
  
  if ((nFlags = fcntl(fd, F_GETFL, 0)) < 0)
    return -1;
  if ((fcntl(fd, F_SETFL, nFlags | O_NONBLOCK)) < 0)
    return -1;
  return 0;
}

/***********************************/
/* Functions involving commands    */
/***********************************/

// Checks if tab is bad and url violates constraints; if so, return.
// Otherwise, send NEW_URI_ENTERED command to the tab on inbound pipe
void handle_uri (char *uri, int tab_index) { 
  if (bad_format(uri) == 1) { // URL format check
    alert("Bad Format!");
    return;
  } 
  else if(TABS[tab_index].free == 1 || tab_index < 1){ // Alert if the controller requests the tab that doesn't exist
    alert("Bad Tab!");
    return;
  }
  else {
    req_t command; // Initialize new command/message struct
    command.type = NEW_URI_ENTERED; // Set type to NEW_URI_ENTERED
    command.tab_index = tab_index; // Set tab index
    strcpy(command.uri, uri);
    write(comm[tab_index].inbound[1], &command, sizeof(command)); // Write the command/message to the inbound pipe
  }
}


// A URI has been typed in, and the associated tab index is determined
// If everything checks out, a NEW_URI_ENTERED command is sent (see Hint)
// Short function
void uri_entered_cb(GtkWidget* entry, gpointer data) {
  
  if(data == NULL) {	
    return;
  }

  // Get the tab (hint: wrapper.h)
  int tab_idx = query_tab_id_for_request(entry, data);
  
  // Get the URL (hint: wrapper.h)
  char* url = get_entered_uri(entry);

  // Hint: now you are ready to handle_the_uri
  handle_uri(url, tab_idx);
}
  

// Called when + tab is hit
// Check tab limit ... if ok then do some heavy lifting (see comments)
// Create new tab process with pipes
// Long function
void new_tab_created_cb (GtkButton *button, gpointer data) {
  
  if (data == NULL) { 
    return;
  }

  // at tab limit?
  if (get_num_tabs() == MAX_TABS-1) {
    alert("Max Tabs!");
    return;
  }

  // Get a free tab
  int free_tab = get_free_tab();
  TABS[free_tab].free = 0;

  // Create communication pipes for this tab  
  pipe(comm[free_tab].inbound);
  pipe(comm[free_tab].outbound);

  // Make the read ends non-blocking 
  non_block_pipe(comm[free_tab].inbound[0]);
  non_block_pipe(comm[free_tab].outbound[0]);

  // fork and create new render tab
  // Note: render has different arguments now: tab_index, both pairs of pipe fd's
  // (inbound then outbound) -- this last argument will be 4 integers "a b c d"
  // Hint: stringify args
  pid_t child = fork();

  if (child == -1){
    perror("Fork Failed\n");
    exit(1);
  } else if (child == 0) { // inside the child process
    TABS[free_tab].pid = getpid(); // assign child pid to this tab
    char pipe_str[20];  // create buffer for the inbound and outbound pipe
    char new_tab[20];   // create buffer for the tab name

    sprintf(pipe_str, "%d %d %d %d", comm[free_tab].inbound[0], comm[free_tab].inbound[1], comm[free_tab].outbound[0], comm[free_tab].outbound[1]); // turn the inbound and outbound pipe into a string
    sprintf(new_tab, "%d", free_tab); // turn the tab num into a string

    int ret = execl("./render", "render", new_tab, pipe_str, NULL); // execute render with the tab and pipe FD arguments
    if (ret == 0) { // check if render failed
      perror("Error exec failed\n");
      exit(-1);
    }
  }
  // Controller parent just does some TABS bookkeeping
}

// This is called when a favorite is selected for rendering in a tab
// Hint: you will use handle_uri ...
// However: you will need to first add "https://" to the uri so it can be rendered
// as favorites strip this off for a nicer looking menu
// Short
void menu_item_selected_cb (GtkWidget *menu_item, gpointer data) {

  if (data == NULL) {
    return;
  }
  
  // Note: For simplicity, currently we assume that the label of the menu_item is a valid url
  // get basic uri
  char *basic_uri = (char *)gtk_menu_item_get_label(GTK_MENU_ITEM(menu_item));

  // append "https://" for rendering
  char uri[MAX_URL];
  sprintf(uri, "https://%s", basic_uri);

  // Get the tab (hint: wrapper.h)
  int tab_idx = query_tab_id_for_request(menu_item, data);
  // Hint: now you are ready to handle_the_uri
  handle_uri(uri, tab_idx);

  return;
}


// BIG CHANGE: the controller now runs an loop so it can check all pipes
// Long function
int run_control() {
  browser_window * b_window = NULL;
  int i, nRead;
  req_t req;

  //Create controller window
  create_browser(CONTROLLER_TAB, 0, G_CALLBACK(new_tab_created_cb),
		 G_CALLBACK(uri_entered_cb), &b_window, comm[0]);

  // Create favorites menu
  create_browser_menu(&b_window, &favorites, num_fav);
  
  while (1) {
    process_single_gtk_event();

    // Read from all tab pipes including private pipe (index 0)
    // to handle commands:
    // PLEASE_DIE (controller should die, self-sent): send PLEASE_DIE to all tabs
    // From any tab:
    //    IS_FAV: add uri to favorite menu (Hint: see wrapper.h), and update .favorites
    //    TAB_IS_DEAD: tab has exited, what should you do?

    // Loop across all pipes from VALID tabs -- starting from 0
    for (i=0; i<MAX_TABS; i++) {
      if (TABS[i].free) continue;
      nRead = read(comm[i].outbound[0], &req, sizeof(req_t)); // pipe from tabs

      // Check that nRead returned something before handling cases
      if (nRead == -1) {
        continue;
      }

      // Case 1: PLEASE_DIE
      if(req.type == PLEASE_DIE){
        // When the request is PLEASE_DIE, get the number of the existing tabs at the point
        // Send PLEASE_DIE to existing tabs
        // Wait until all the tabs terminte
        // Then exit
        int tab_num = get_num_tabs();
        for (int j=1; j<MAX_TABS; j++){ // Loop across pipes except the controller
          if(TABS[j].free == 0) { 
            write(comm[j].inbound[1], &req, sizeof(req_t)); // Send the PLESE_DIE command to each tab
          }
        }
                
        for (int j=1; j< tab_num; j++)
          wait(NULL);
        
        exit(1);
      }
      // Case 2: TAB_IS_DEAD
	    if (req.type == TAB_IS_DEAD){
        TABS[i].free = 1; // Need to free 
      }
      // Case 3: IS_FAV
      if (req.type == IS_FAV) {
        char *uri = req.uri;
        strcat(uri, "\n"); // Need to concat the new line for a proper comparison
        if(fav_ok(uri) == -1){ // if favorite already exists, print alert
          alert("FAV EXISTS!");
        }
        else{
          update_favorites_file(uri); // update favorites file and add to favorite menu
          add_uri_to_favorite_menu(b_window, uri);
        }
      }
    }
    usleep(1000);
  }
  return 0;
}


int main(int argc, char **argv)
{

  if (argc != 1) {
    fprintf (stderr, "browser <no_args>\n");
    exit (0);
  }

  init_tabs ();
  init_blacklist(".blacklist");
  init_favorites(".favorites"); // init blacklist (see util.h), and favorites (write this, see above)

  // Fork controller
  pid_t controller = fork();
  int status = 0;

  if(controller == -1){
    perror("Fork Failed\n");
    exit(-1);
  }
  else if (controller == 0) { // Child
    // Create two pipes and make the read end non-blocking
    TABS[0].pid = getpid(); // The first TABS element is for the controller process
    pipe(comm[0].inbound);
    pipe(comm[0].outbound);
    
    non_block_pipe(comm[0].inbound[0]); // Makes the pipes non-blocking
    non_block_pipe(comm[0].outbound[0]);

    run_control();
  } 

  // Child creates a pipe for itself comm[0]
  // then calls run_control ()
  // Parent waits ...
  while((wait(&status)) > 0); // wait for all children processes

  return 0;
}
