# Assignment 4 - Atomicity and Coherency

This C program creates an HTTP server and it runs forever. Client can communicate with the server using a specific port that the server opened. It will run until the user types ctrl-C. The server should be able to handle GET, PUT, and APPEND requests. This also adds an additional feature where you can create a log file to keep track of reuqest ids, status code, and method type. This server can also now handle multiple threads to handle multiple requests at the same time. This server now supports atomicity and coherency to ensure the right output.

## Building

Build the program with the make file or with the following:
```
$ make all
```

## Running

Run the program with:
```
$ ./httpserver <-t threads> <-l logfile> <port number>
```
Then, the client can test the program using any network related program. I mainly used cURL and netcat. A cURL GET request is shown below.
```
$ ./curl -v localhost:<port number>/<file name>
```
The above cURL command contains the verbose flag, which prints out the whole HTTP request. It is very useful for debugging.
```
$ ./curl -T <file1> -H 'Request-Id: <num>' localhost:<port number>/<file2>
```
The above cURL command contains the PUT command.


## Thought Process

My initial approach is to use a hash table to know if the file is being used or not. I take the URI and mod it by 4096 and times it by 4096. I then got very lost at how to keep the operations atomic. 

I implemented a mutex lock for logging the files and using flock with special permissions to lock the files. GET uses a shared lock and PUT/APPEND uses an exclusive lock. The log file writing operation doesn't really support coherency but since you're writing such a small buffer it should be fine. Other than that, I only lock when I am enqueueing and dequeueing. I don't want there to be too many critical sections in the program because then that will be a problem. 

## Errors encountered
1. I fixed a bug where I would check if a file is invalid before reading in the headers. This way it will update the request ID before exiting out of the function.
