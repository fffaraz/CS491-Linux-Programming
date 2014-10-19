// mynl -- number lines of files
// Faraz Fallahi (faraz@siu.edu)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <regex.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

#define bool _Bool
#define true (1)
#define false (0)

// this macro saves the cost of a function call if the first letters differ
#define STREQ(a, b) (*(a) == *(b) && strcmp((a), (b)) == 0)

//----------------------------------------------------------------------

#define FILE_BUFFER_SIZE 512

#define BUFF_SIZE_INIT 50
#define BUFF_SIZE_INC  10

//----------------------------------------------------------------------

#define MY_FILE struct _MY_FILE

struct _MY_FILE
{
    int fd;
    int flags;
    char buf[FILE_BUFFER_SIZE];
    int size;
    int pos;
};

MY_FILE my_stdin[] = { { STDIN_FILENO, 0, {}, 0, 0 } };

int my_fgetc(MY_FILE* stream)
{
    if(stream->pos == stream->size)
    {
        stream->pos = 0;
        stream->size = read(stream->fd, stream->buf, FILE_BUFFER_SIZE);

        if(stream->size == -1) // error
        {
            stream->flags |= 2;
            return EOF;
        }

        if(stream->size == 0) // eof
        {
            stream->flags |= 1;
            return EOF;
        }
    }

    return stream->buf[stream->pos++];
}

int my_feof(MY_FILE* stream)
{
    return stream->flags & 1;
}

int my_ferror(MY_FILE* stream)
{
    return stream->flags & 2;
}

void my_clearerr(MY_FILE* stream)
{
    stream->flags = 0;
}

MY_FILE* my_fopen(const char *filename, const char *modes)
{
    (void)modes; // unused parameter

    int fd = open(filename, O_RDONLY);
    if(fd == -1) return NULL;

    MY_FILE *file = calloc(1, sizeof(MY_FILE));
    file->fd = fd;

    return file;
}

int my_fclose(MY_FILE* stream)
{
    int fd = stream->fd;
    free(stream);
    return close(fd) == 0 ? 0 : EOF;
}

//----------------------------------------------------------------------

// struct linebuffer holds a line of text.
struct LineBuffer
{
    size_t size;   // allocated
    size_t length; // used
    char *buffer;
};

/* Read an arbitrarily long line of text from STREAM into LINEBUFFER.
 * Consider lines to be terminated by DELIMITER.
 * Keep the delimiter.
 * Do not NUL-terminate; Therefore the stream can contain NUL bytes, and
 * the length (including the delimiter) is returned in linebuffer->length.
 * Return NULL when stream is empty or upon error.
 * Otherwise, return LINEBUFFER.
 */
struct LineBuffer *readlinebuffer(struct LineBuffer *linebuffer, MY_FILE *stream, char delimiter)
{
    int c;
    char *buffer = linebuffer->buffer;
    char *p = linebuffer->buffer;
    char *end = buffer + linebuffer->size;

    if(my_feof(stream)) return NULL;

    do
    {
        c = my_fgetc(stream);

        if(c == EOF)
        {
            // Return NULL when stream is empty or upon error
            if(p == buffer || my_ferror(stream)) return NULL;
            // append DELIMITER if it's the last line of a file that ends in a character other than DELIMITER.
            if(p[-1] != delimiter) c = delimiter;
        }

        // Check if we need to allocate more space
        if(p == end)
        {
            size_t oldsize = linebuffer->size;
            linebuffer->size += BUFF_SIZE_INC;
            buffer = realloc(buffer, linebuffer->size);
            p = buffer + oldsize;
            linebuffer->buffer = buffer;
            end = buffer + linebuffer->size;
        }

        *p++ = c;
    }
    while(c != delimiter);

    linebuffer->length = p - buffer;
    return linebuffer;
}

//----------------------------------------------------------------------

// Input line buffer.
static struct LineBuffer line_buf;

// printf format string for unnumbered lines.
static char *print_no_line_fmt = NULL;

// Number of blank lines to consider to be one line for numbering (-l).
static int blank_join = 1;

// Width of line numbers (-w).
static int lineno_width = 6;

// Current print line number.
static long int line_no = 1;

// True if we have ever read standard input.
static bool have_read_stdin = false;

// Separator string to print after line number (-s).
static char const *separator_str = "\t";

// Format of lines (-b).
static char const *line_type = "t";

// Regex for lines to number (-bp).
static regex_t line_regex;

//----------------------------------------------------------------------

// Print the line number and separator; increment the line number.
static void print_lineno(void)
{
    printf("%*ld%s", lineno_width, line_no, separator_str);
    line_no++;
}

// Process a text line in 'line_buf'.
static void proc_text(void)
{
    static int blank_lines = 0; // Consecutive blank lines so far.

    switch(line_type[0])
    {
    case 'a':
        if (blank_join > 1)
        {
            if (line_buf.length > 1 || ++blank_lines == blank_join)
            {
                print_lineno();
                blank_lines = 0;
            }
            else
                fputs(print_no_line_fmt, stdout);
        }
        else
            print_lineno();
        break;
    case 't':
        if(line_buf.length > 1)
            print_lineno();
        else
            fputs(print_no_line_fmt, stdout);
        break;
    case 'n':
        fputs(print_no_line_fmt, stdout);
        break;
    case 'p':
        switch(regexec(&line_regex, line_buf.buffer, 0, NULL, 0))
        {
        case 0:
            print_lineno();
            break;
        case REG_NOMATCH:
            fputs(print_no_line_fmt, stdout);
            break;
        default:
            //regerror(); //TODO
            //fprintf(stderr, "error in regular expression search.\n");
            perror("error in regular expression search.");
            exit(EXIT_FAILURE);
            break;
        }
    }

    //line_buf.buffer[line_buf.length] = '\0';
    //fputs(line_buf.buffer, stdout);
    fwrite(line_buf.buffer, sizeof(char), line_buf.length, stdout);
}

// Read and process the file pointed to by FP.
static void process_file(MY_FILE *fp)
{
    while(readlinebuffer(&line_buf, fp, '\n'))
        proc_text();
}

// Process file FILE to standard output. Return true if successful.
bool nl_file(char const *file)
{
    MY_FILE *stream;

    if(STREQ(file, "-"))
    {
        have_read_stdin = true;
        stream = my_stdin;
    }
    else
    {
        stream = my_fopen(file, "r");
        if(stream == NULL)
        {
            //fprintf(stderr, "can't open file %s\n", file);
            perror(strerror(errno));
            return false;
        }
    }

    process_file(stream);

    if(my_ferror(stream))
    {
        perror(strerror(errno)); //TODO
        return false;
    }

    if(STREQ(file, "-"))
        my_clearerr(stream);
    else
        if(my_fclose(stream) == EOF)
            return false;

    return true;
}

//----------------------------------------------------------------------

// Set the command line flag TYPEP and possibly the regex pointer REGEXP, according to 'optarg'.
static bool build_type_arg()
{
    bool rval = true;

    switch(optarg[0])
    {
    case 'a':
    case 't':
    case 'n':
        line_type = optarg;
        break;
    case 'p':
        line_type = optarg++;
        if(regcomp(&line_regex, optarg, 0)) // flags?!
        {
            //regerror(); //TODO
            fprintf(stderr, "error in regular expression compile. %s\n", optarg);
            //perror("error in regular expression compile.");
            exit(EXIT_FAILURE);
        }
        break;
    default:
        rval = false;
        fprintf(stderr, "Invalid -b option STYLE: %s\n", optarg);
        break;
    }
    return rval;
}

// Parse command arguments
static bool parse_args(int argc, char **argv)
{
    struct option longopts[] = {
        {"body-numbering", required_argument, NULL, 'b'},
        {"number-separator", required_argument, NULL, 's'},
        {"number-width", required_argument, NULL, 'w'},
        {"join-blank-lines", required_argument, NULL, 'l'},
        {NULL, 0, NULL, 0}
    };

    int c;
    while((c = getopt_long(argc, argv, "b:s:w:l:", longopts, NULL)) != -1)
    {
        switch(c)
        {
        case 'b':
            if(!build_type_arg())
            {
                fprintf(stderr, "invalid body numbering style: %s\n", optarg);
                return false;
            }
            break;
        case 's':
            separator_str = optarg;
            break;
        case 'w':
            lineno_width = atoi(optarg);
            break;
        case 'l':
            blank_join = atoi(optarg);
            break;
        case '?': //TODO
            fprintf (stderr, "Unknown option `-%c'.\n", optopt);
            return false;
            break;
        default:
            fprintf(stderr, "Usage: %s [-bSTYLE] [-sSTRING] [FILE...]\n", argv[0]);
            return false;
        }
    }
    return true;
}

//----------------------------------------------------------------------

int main(int argc, char **argv)
{
    bool ok = true;

    // Parse arguments
    ok = parse_args(argc, argv);

    if(!ok) exit(EXIT_FAILURE);

    // Initialize the input buffer.
    memset(&line_buf, 0, sizeof(struct LineBuffer));
    line_buf.size = BUFF_SIZE_INIT;
    line_buf.buffer = malloc(line_buf.size);

    // Initialize the printf format for unnumbered lines.
    size_t len;
    len = strlen(separator_str);
    print_no_line_fmt = malloc(lineno_width + len + 1);
    memset(print_no_line_fmt, ' ', lineno_width + len);
    print_no_line_fmt[lineno_width + len] = '\0';

    // Main processing.
    if(optind == argc)
        ok = nl_file("-"); // No input files. Read from stdin.
    else
        while(optind < argc)
            ok &= nl_file(argv[optind++]);

    // Cleaning
    free(line_buf.buffer);
    if(line_type[0] == 'p') regfree(&line_regex);
    if(have_read_stdin && my_fclose(my_stdin) == EOF) exit(EXIT_FAILURE);

    exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
    return EXIT_SUCCESS;
}
