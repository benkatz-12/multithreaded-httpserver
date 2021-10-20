Multithreaded http based web-server:
    www/httpserv.c

    Usage:
        compilation (gcc 9.3.0): gcc -pthread httpserv.c -o server
        run server: ./server <open port number>

    Synopsis:
        Given web server files, we were tasked to build a http-based server that is able to handle multiple connecctions and 
            respond to GET requests and POST requests
        Used the p_thread C library to handle multiple connections
    Flow:
        Accept connection
        Read in the http request and parse
        Craft http header using parsed information
        Craft relative file path using parsed information
        Read server file onto TCP connection with http header prepended
        If POST, alter server file in buffer before sending
        Exit connection after done serving, error, or TCP timeout
