/* CSCI-4061 Fall 2022 - Project 3
 * Group Member #1 - Ashwin Wariar waria012
 * Group Member #2 - Ryan Koo koo3017
 * Group Member #3 - Yong Hyeon Yi yi000055 */


Everyone worked on each of the methods and implementation over various synchronous meetings through
zoom and VSCode LiveShare. 

Everyone worked on each of the methods together such that everyone understand the functionality of each component
that made up the web server. 

For our cache structure, we went with the simplest approach and utilized a FIFO structure, where the first entry
indicates the oldest entry and thus once we reached the end of the cache we evicted entries starting from the beginning again (following the circular buffer pattern).