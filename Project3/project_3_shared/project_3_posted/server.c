/* CSCI-4061 Fall 2022 - Project 3
 * Group Member #1 - Ashwin Wariar waria012
 * Group Member #2 - Ryan Koo koo3017
 * Group Member #3 - Yong Hyeon Yi yi000055
*/

#include "server.h"
#define PERM 0644

//Global Variables [Values Set in main()]
int queue_len           = INVALID;                              //Global integer to indicate the length of the queue
int cache_len           = INVALID;                              //Global integer to indicate the length or # of entries in the cache        
int num_worker          = INVALID;                              //Global integer to indicate the number of worker threads
int num_dispatcher      = INVALID;                              //Global integer to indicate the number of dispatcher threads      
FILE *logfile;                                                  //Global file pointer for writing to log file in worker

/* ************************ Global Hints **********************************/
int queue_entry_w         = -1;                 // Worker entry (Rear queue empty state) [worker()] --> How will you track which index in the request queue to remove next?
int queue_entry_d         = -1;                 // Dispatcher entry (Front queue empty state) [dispatcher()] --> How will you know where to insert the next request received into the request queue?
int in_cache      = 0;                          // [Cache]           --> When using cache, how will you track which cache entry to evict from array?
int curequest = 0;                              // [multiple funct]  --> How will you update and utilize the current number of requests in the request queue?

pthread_t worker_thread[MAX_THREADS];           // [multiple funct]  --> How will you track the p_thread's that you create for workers?
pthread_t dispatcher_thread[MAX_THREADS];       // [multiple funct]  --> How will you track the p_thread's that you create for dispatchers?
int threadID[MAX_THREADS];                      // [multiple funct]  --> Might be helpful to track the ID's of your threads in a global array


pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;        //What kind of locks will you need to make everything thread safe? [Hint you need multiple]
pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;

pthread_cond_t free_space = PTHREAD_COND_INITIALIZER;

pthread_cond_t queue_notempty = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_notfull = PTHREAD_COND_INITIALIZER;
pthread_cond_t queue_access = PTHREAD_MUTEX_INITIALIZER;

request_t req_entries[MAX_QUEUE_LEN];                    // How will you track the requests globally between threads? How will you ensure this is thread safe?
cache_entry_t* cache_arr;                                // [Cache]  --> How will you read from, add to, etc. the cache? Likely want this to be global
/**********************************************************************************/


/*
  THE CODE STRUCTURE GIVEN BELOW IS JUST A SUGGESTION. FEEL FREE TO MODIFY AS NEEDED
*/


/* ******************************** Cache Code  ***********************************/

// Function to check whether the given request is present in cache
int getCacheIndex(char *request){
  /* TODO (GET CACHE INDEX)
  *    Description:      return the index if the request is present in the cache otherwise return INVALID
  */
  for (int i = 0; i < in_cache; i++) { // Loops through cache to see if request is present in cache
    char *req = cache_arr[i].request;
    if (strcmp(req, request) == 0) {
      return i;
    } 
  }
  
  return INVALID;
}

// Function to add the request and its file content into the cache
void addIntoCache(char *mybuf, char *memory, int memory_size){
  /* TODO (ADD CACHE)
  *    Description:      It should add the request at an index according to the cache replacement policy
  *                      Make sure to allocate/free memory when adding or replacing cache entries
  */
  cache_arr[in_cache].content = (char*) malloc(memory_size); // Allocates memory 

  if (cache_arr[in_cache].content == NULL) {
    perror("MALLOC FAIL CACHE CONTENT");
    exit(-1);
  }

  memcpy(cache_arr[in_cache].content, memory, memory_size); // Copies content into memory
  cache_arr[in_cache].request = (char*) malloc(strlen(mybuf));
  if (cache_arr[in_cache].request == NULL) {
    perror("MALLOC FAIL CACHE REQUEST");
    exit(-1)
  }
  strcpy(cache_arr[in_cache].request, mybuf);
  cache_arr[in_cache].len = memory_size;

  in_cache = (in_cache + 1) % cache_len; // Does the cache replacement FIFO style
}

// Function to clear the memory allocated to the cache
void deleteCache(){
  /* TODO (CACHE)
  *    Description:      De-allocate/free the cache memory
  */
  // Not needed
}

// Function to initialize the cache
void initCache(){
  /* TODO (CACHE)
  *    Description:      Allocate and initialize an array of cache entries of length cache size
  */
  
  cache_arr = malloc(sizeof(cache_entry_t) * cache_len); // Initializes cache
  if (cache_arr == NULL) {
    perror("CACHE INIT MALLOC FAIL");
    exit(-1);
  }
}

/**********************************************************************************/

/* ************************************ Utilities ********************************/
// Function to get the content type from the request
char* getContentType(char *mybuf) {
  /* TODO (Get Content Type)
  *    Description:      Should return the content type based on the file type in the request
  *                      (See Section 5 in Project description for more details)
  *    Hint:             Need to check the end of the string passed in to check for .html, .jpg, .gif, etc.
  */

    const char *dot = strrchr(mybuf, '.');
    char *content_type = malloc(128); // Allocates memory for content type string
    if (content_type == NULL) {
      perror("CONTENT TYPE MALLOC FAIL");
      exit(-1);
    }

    if (strcmp(dot, ".html") == 0 || strcmp(dot, ".htm") == 0) { // Checks what type of file based on string passed in
      strcpy(content_type, "text/html");
    } else if (strcmp(dot, ".jpg") == 0) {
      strcpy(content_type, "image/jpg");
    } else if (strcmp(dot, ".gif") == 0) {
      strcpy(content_type,"image/gif");
    } else {
      strcpy(content_type, "text/plain");
    }
    return content_type;
}

// Function to open and read the file from the disk into the memory. Add necessary arguments as needed
// Hint: caller must malloc the memory space
int readFromDisk(int fd, char *mybuf, void **memory) {
  //    Description: Try and open requested file, return INVALID if you cannot meaning error
  FILE *fp;
  if((fp = fopen(mybuf, "r")) == NULL){ // Opens the file
     fprintf (stderr,  "ERROR: Fail to open the file: [%s]\n", mybuf);
    return INVALID;
  }
  
  /* TODO 
  *    Description:      Find the size of the file you need to read, read all of the contents into a memory location and return the file size
  *    Hint:             Using fstat or fseek could be helpful here
  *                      What do we do with files after we open them?
  */
  struct stat info;
  stat(mybuf, &info); // Finds the size of the file we want to read
  int file_size = (int) info.st_size;
  fprintf(stderr, "Size of the file [%d] bytes\n", file_size);

  *memory = malloc(file_size);  // Allocates memory for the size of the file
  if (memory == NULL) {
    perror("MALLOC ERROR FILE SIZE");
    exit(-1);
  }

  fseek(fp, 0, SEEK_SET); // Seek to the beginning of the file
  size_t mem_size = fread(*memory, 1, file_size, fp); // Returns the size of the memory (byte)
  if (mem_size < file_size) { // Prints error if the memory size after reading is different from the file size
    fprintf(stderr, "ERROR: Failed to read file\n");
    return INVALID;
  }  

  fclose(fp); // Closes the file
  return mem_size;
}

/**********************************************************************************/

// Function to receive the path)request from the client and add to the queue
void * dispatch(void *arg) {

  /********************* DO NOT REMOVE SECTION - TOP     *********************

  /* TODO (B.I)
  *    Description:      Get the id as an input argument from arg, set it to ID
  */

  // ID is from globally define threadID[] array
  int ID = *(int *) arg;

  while (1) {

    /* TODO (FOR INTERMEDIATE SUBMISSION)
    *    Description:      Receive a single request and print the conents of that request
    *                      The TODO's below are for the full submission, you do not have to use a 
    *                      buffer to receive a single request 
    *    Hint:             Helpful Functions: int accept_connection(void) | int get_request(int fd, char *filename
    *                      Recommend using the request_t structure from server.h to store the request. (Refer section 15 on the project write up)
    */

    /* TODO (B.II)
    *    Description:      Accept client connection
    *    Utility Function: int accept_connection(void) //utils.h => Line 24
    */
    request_t tempreq;

    int fd = accept_connection(); // Accepts client connection

    if (fd < 0) { // Error check for accept_connection
      return;
    }

    tempreq.fd = fd;
    /* TODO (B.III)
    *    Description:      Get request from the client
    *    Utility Function: int get_request(int fd, char *filename); //utils.h => Line 41
    */    
    char filename[BUFF_SIZE];
    while(get_request(tempreq.fd, filename)) {
      if ((get_request(tempreq.fd, filename)) != 0) { // Error check for get_request
      return;
      }
    }

    /* TODO (B.IV)
    *    Description:      Add the request into the queue
    */

        //(1) Copy the filename from get_request into allocated memory to put on request queue
        tempreq.request = (char*) malloc(strlen(filename) * sizeof(char));
        if (tempreq.request == NULL) {
          perror("REQUEST MALLOC FAIL");
          exit(-1);
        }
        
        memcpy(tempreq.request, filename, strlen(filename)+1);

        //(2) Request thread safe access to the request queue         
        pthread_mutex_lock(&queue_access);
        
        //(3) Check for a full queue... wait for an empty one which is signaled from req_queue_notfull         
        while (curequest == queue_len) {
           pthread_cond_wait(&queue_notfull, &queue_access);
        }      

        //(4) Insert the request into the queue   
        // The very first enqueue should set queue_entery_d = 0 and queue_entry_w = 1
        if((queue_entry_d == -1) && (queue_entry_w == -1)){
          queue_entry_d = 0;
          queue_entry_w = 0;
        }

        req_entries[queue_entry_d] = tempreq;
              
        //(5) Update the queue index in a circular fashion    
        queue_entry_d = (queue_entry_d + 1) % queue_len;
        curequest += 1;

        //(6) Release the lock on the request queue and signal that the queue is not empty anymore         
        pthread_cond_signal(&queue_notempty);
		    pthread_mutex_unlock(&queue_access);
 }

  return NULL;
}

/**********************************************************************************/
// Function to retrieve the request from the queue, process it and then return a result to the client
void * worker(void *arg) {
  /********************* DO NOT REMOVE SECTION - BOTTOM      *********************/

  // Helpful/Suggested Declarations
  int num_request = 0;                                    //Integer for tracking each request for printing into the log
  bool cache_hit  = false;                                //Boolean flag for tracking cache hits or misses if doing 
  int filesize    = 0;                                    //Integer for holding the file size returned from readFromDisk or the cache
  void *memory    = NULL;                                 //memory pointer where contents being requested are read and stored
  int fd          = INVALID;                              //Integer to hold the file descriptor of incoming request
  char mybuf[BUFF_SIZE];                                  //String to hold the file path from the request
  char mybuf_w_dot[BUFF_SIZE];
 
  /* TODO (C.I)
  *    Description:      Get the id as an input argument from arg, set it to ID
  */
  int ID = *(int *) arg;

  while (1) {
    /* TODO (C.II)
    *    Description:      Get the request from the queue and do as follows
    */
          //(1) Request thread safe access to the request queue by getting the req_queue_mutex lock
          pthread_mutex_lock(&queue_access);        

          //(2) While the request queue is empty conditionally wait for the request queue lock once the not empty signal is raised
          while (curequest == 0) {
            pthread_cond_wait(&queue_notempty, &queue_access);
          }
          
          //(3) Now that you have the lock AND the queue is not empty, read from the request queue
          request_t retrieved = req_entries[queue_entry_w];
          memset(mybuf_w_dot, '\0', BUFF_SIZE); // clear the buffer before its usage 
          memset(mybuf, '\0', BUFF_SIZE); // clear the buffer before its usage
          mybuf_w_dot[0] = '.';

          fd = retrieved.fd; // copy the retrieved fd to fd
          strcpy(mybuf, retrieved.request); // copy the retrieved request to mybuf
          
          //(4) Update the request queue remove index in a circular fashion
          // Free the retrieved request for a future queue usage
          free(req_entries[queue_entry_w].request);
          req_entries[queue_entry_w].fd = 0; 

          queue_entry_w = (queue_entry_w + 1) % queue_len; // Worker have to increase the queue entry
          curequest -= 1;
          
          // If the dequeue is done and the queue is empty, set the queue_entry_d and queue_entry_w to the "empty state"     
          if(queue_entry_d == queue_entry_w){
            queue_entry_d = -1;
            queue_entry_w = -1;
          }

          //(5) Check for a path with only a "/" if that is the case add index.html to it
          if (strlen(mybuf) == 1 && strcmp("/", mybuf) == 0) {
            strcat(mybuf, "index.html");
          }

          //(6) Fire the request queue not full signal to indicate the queue has a slot opened up and release the request queue lock
          pthread_cond_signal(&queue_notfull);
          pthread_mutex_unlock(&queue_access);        


    /* TODO (C.III)
    *    Description:      Get the data from the disk or the cache 
    *    Local Function:   int readFromDisk(//necessary arguments//);
    *                      int getCacheIndex(char *request);  
    *                      void addIntoCache(char *mybuf, char *memory , int memory_size);  
    */

    // Before readFromDisk, check from cache first using getCacheIndex()
    // If it is a cache hit, get the content from the cache
    // If it is a cache miss, readFromDisk(), store the request and the content to the cache
    int cache_hit_index = getCacheIndex(mybuf);
    
    if(cache_hit_index != INVALID){
      cache_hit = true;
      printf("=========== Cache Hit! ========== From Cache index [%d]\n", cache_hit_index); 
      memory = (void*)cache_arr[cache_hit_index].content; 
      filesize = cache_arr[cache_hit_index].len;      
      printf("Memory size returned from the Cache is [%d]bytes\n", filesize); 
    }
    else {
      cache_hit = false;     
      printf("=========== Cache Miss! =========== Goes to Cache index [%d]\n", in_cache); 
      strcat(mybuf_w_dot, mybuf);
      filesize = readFromDisk(fd, mybuf_w_dot, &memory);
      printf("Memory size returned from readFromDisk is [%d]bytes\n", filesize); 
   
      // Add the request, file content, and the filesize into the Cache
      // Only done when the data from readFromDisk() is valid
      if(filesize != INVALID){
        if (cache_arr[in_cache].request != NULL) 
          free(cache_arr[in_cache].request);  // Only frees the contents of in_cache if there is something inside of it
        if (cache_arr[in_cache].content != NULL) 
          free(cache_arr[in_cache].content);
      
        addIntoCache(mybuf, (char*) memory, filesize);
      }
    }

    /* TODO (C.IV)
    *    Description:      Log the request into the file and terminal
    *    Utility Function: LogPrettyPrint(FILE* to_write, int threadId, int requestNumber, int file_descriptor, char* request_str, int num_bytes_or_error, bool cache_hit);
    *    Hint:             Call LogPrettyPrint with to_write = NULL which will print to the terminal
    *                      You will need to lock and unlock the logfile to write to it in a thread safe manor
    */
    LogPrettyPrint(NULL, ID, num_request, fd, mybuf, filesize, cache_hit);
    num_request++;
   

    /* TODO (C.V)
    *    Description:      Get the content type and return the result or error
    *    Utility Function: (1) int return_result(int fd, char *content_type, char *buf, int numbytes); //look in utils.h 
    *                      (2) int return_error(int fd, char *buf); //look in utils.h 
    */
    char *content_type; 
    char buff_error[BUFF_SIZE];
    
    // Determine which functions to run based on the filesize value returned from readFromDisk()
    if(filesize == INVALID){
      if(return_error(fd, buff_error)!=0) {
        fprintf(stderr, "Failed to return_error()\n"); // error check
      } else {
        fprintf(stderr, "Invalid Request \n"); // print the error message
      }
    }
    else{
      content_type = getContentType(mybuf);
      printf("%s\n", content_type);
      if(return_result(fd, content_type, memory, filesize)!=0)
        fprintf(stderr, "Failed to return_result()\n"); // error check
    }
  }
}

/**********************************************************************************/

int main(int argc, char **argv) {

  /********************* DO NOT REMOVE SECTION - TOP     *********************/
  // Error check on number of arguments
  if(argc != 7){
    printf("usage: %s port path num_dispatcher num_workers queue_length cache_size\n", argv[0]);
    return -1;
  }

  int port            = -1;
  char path[PATH_MAX] = "no path set\0";
  num_dispatcher      = -1;                               //global variable
  num_worker          = -1;                               //global variable
  queue_len           = -1;                               //global variable
  cache_len           = -1;                               //global variable


  /********************* DO NOT REMOVE SECTION - BOTTOM  *********************/
  /* TODO (A.I)
  *    Description:      Get the input args --> (1) port (2) path (3) num_dispatcher (4) num_workers  (5) queue_length (6) cache_size
  */
  port = atoi(argv[1]);
  strncpy(path, argv[2], sizeof(path));
  num_dispatcher = atoi(argv[3]);
  num_worker = atoi(argv[4]);
  queue_len = atoi(argv[5]);
  cache_len = atoi(argv[6]);


  /* TODO (A.II)
  *    Description:     Perform error checks on the input arguments
  *    Hints:           (1) port: {Should be >= MIN_PORT and <= MAX_PORT} | (2) path: {Consider checking if path exists (or will be caught later)}
  *                     (3) num_dispatcher: {Should be >= 1 and <= MAX_THREADS} | (4) num_workers: {Should be >= 1 and <= MAX_THREADS}
  *                     (5) queue_length: {Should be >= 1 and <= MAX_QUEUE_LEN} | (6) cache_size: {Should be >= 1 and <= MAX_CE}
  */
  if(!(port >= MIN_PORT && port <= MAX_PORT)){
    perror("PORT NUMBER ERROR\n");
    exit(-1);
  }
  if(path == NULL){ // Only checking emptiness at this point
    perror("PATH DOESN'T EXIST\n");
    exit(-1);
  }
  if(!(num_dispatcher >= 1 && num_dispatcher <= MAX_THREADS)){
    perror("DISPATCHER NUMBER ERROR\n");
    exit(-1);
  }
  if(!(num_worker >= 1 && num_worker <= MAX_THREADS)){
    perror("WORKER NUMBER ERROR\n");
    exit(-1);
  }
  if(!(queue_len >= 1 && queue_len <= MAX_QUEUE_LEN)){
    perror("QUEUE LENGTH ERROR\n");
    exit(-1);
  }
  if(!(cache_len >= 1 && cache_len <= MAX_CE)){
    perror("CACHE SIZSE ERROR\n");
    exit(-1);
  }

  /********************* DO NOT REMOVE SECTION - TOP    *********************/
  printf("Arguments Verified:\n\
    Port:           [%d]\n\
    Path:           [%s]\n\
    num_dispatcher: [%d]\n\
    num_workers:    [%d]\n\
    queue_length:   [%d]\n\
    cache_size:     [%d]\n\n", port, path, num_dispatcher, num_worker, queue_len, cache_len);
  /********************* DO NOT REMOVE SECTION - BOTTOM  *********************/


  /* TODO (A.III)
  *    Description:      Open log file
  *    Hint:             Use Global "File* logfile", use "web_server_log" as the name, what open flags do you want?
  */
  if((logfile = fopen("web_server_log", "a")) == NULL){ // File open with append mode
    perror("Web Server File Opening Failed\n");
    exit(-1);
  }
  /* TODO (A.IV)
  *    Description:      Change the current working directory to server root directory
  *    Hint:             Check for error!
  */
  if(chdir(path) != 0){
    perror("chdir() to ./testing failed\n");
    exit(-1);
  }
  
  /* TODO (A.V)
  *    Description:      Initialize cache  
  *    Local Function:   void    initCache();
  */
  initCache();


  /* TODO (A.VI)
  *    Description:      Start the server
  *    Utility Function: void init(int port); //look in utils.h 
  */
  init(port);

  /* TODO (A.VII)
  *    Description:      Create dispatcher and worker threads 
  *    Hints:            Use pthread_create, you will want to store pthread's globally
  *                      You will want to initialize some kind of global array to pass in thread ID's
  *                      How should you track this p_thread so you can terminate it later? [global]
  */

  //INTERMEDIATE SUBMISSION
  // 1. Dispatcher thread creation
  // Create num_dispatcher threads and store the tid as a global array, dispatcher_thread
  // function: disptach(void* arg)
  // arguments: Integer thread ID array???
  for(int i = 0; i < num_dispatcher; i++) {
      threadID[i] = i; // ThreadID for argument pass to dispatch()
      if(pthread_create(&(dispatcher_thread[i]), NULL, dispatch, (void *) &threadID[i]) != 0) {
            printf("Thread %d failed to create\n", i);
      }
      printf("Dispatcher           [%3d] Started\n", i);
  }

  // 2. Worker thread creation
  // Create num_workers threads and store the tid as a global array, dispatcher_thread
  // function: worker(void* arg)
  // arguments: Integer thread ID array???
  for(int i = 0; i < num_worker; i++) {
      threadID[i] = i; // ThreadID for argument pass to dispatch()
      if(pthread_create(&(worker_thread[i]), NULL, worker, (void *) &threadID[i]) != 0) {
            printf("Thread %d failed to create\n", i);
      }
      printf("Worker               [%3d] Started\n", i);
  }


  // Wait for each of the threads to complete their work
  // Threads (if created) will not exit (see while loop), but this keeps main from exiting
  int i;  
  for(i = 0; i < num_worker; i++){
    fprintf(stderr, "JOINING WORKER %d \n",i);
    if((pthread_join(worker_thread[i], NULL)) != 0){
      printf("ERROR : Fail to join worker thread %d.\n", i);
    }
  }
  
  for(i = 0; i < num_dispatcher; i++){
    fprintf(stderr, "JOINING DISPATCHER %d \n",i);
    if((pthread_join(dispatcher_thread[i], NULL)) != 0){
      printf("ERROR : Fail to join dispatcher thread %d.\n", i);
    }
  }
  fprintf(stderr, "SERVER DONE \n");  // will never be reached in SOLUTION
}

