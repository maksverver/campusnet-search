#include "fcgi_stdio.h"
#include <stdlib.h>
#include <string.h>

#define MAX_QUERY_LENGTH 2000

extern char **environ;

int handle_request()
{
    char query[MAX_QUERY_LENGTH];
    int content_length;
    FILE *pipe;
    char buffer[1024];
    int read;
    const char *str;

    str = getenv("CONTENT_LENGTH");
    if(str == NULL)
    {
        puts("Status: 411 Length Required\n\n");
        return 1;
    }

    content_length = strtol(str, NULL, 0);

    if( content_length > sizeof(query) - 1 ||
        fread(query, 1, content_length, stdin) != (size_t)content_length )
    {
        puts("Status: 400 Bad Request\n\n");
        return 1;
    }

    str = getenv("query_command");
    if(str == NULL)
    {
        fprintf(stderr, "Environmental variable 'query_command' not set!\n");
        puts("Status: 500 Internal Server Error\n\n");
        return 1;
    }

    query[content_length] = '\0';
    setenv("query", query, 1);
    pipe = popen(str, "r");
    if(pipe == NULL)
    {
        fprintf(stderr, "Unable to open pipe!\n");
        puts("Status: 500 Internal Server Error\n\n");
        return 1;
    }

    puts("Content-type: application/xml\n");
    while((read = fread(buffer, 1, sizeof(buffer), pipe)) > 0)
    {
        fwrite(buffer, 1, read, stdout);
    }

    pclose(pipe);

    return 0;
}

int main()
{
    while(FCGI_Accept() >= 0)
        handle_request();

    return 0;
}
