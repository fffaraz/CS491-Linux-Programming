// mynl -- number lines of files
// Faraz Fallahi (faraz@siu.edu)

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <getopt.h>
#include <regex.h>

#define bool _Bool
#define true (1)
#define false (0)

// this macro saves the cost of a function call if the first letters differ
#define STREQ(a, b) (*(a) == *(b) && strcmp((a), (b)) == 0)

//----------------------------------------------------------------------

// struct linebuffer holds a line of text.
struct linebuffer
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
struct linebuffer *readlinebuffer(struct linebuffer *linebuffer, FILE *stream, char delimiter)
{
    int c;
    char *buffer = linebuffer->buffer;
    char *p = linebuffer->buffer;
    char *end = buffer + linebuffer->size;

    if(feof(stream)) return NULL;

    do
    {
        c = fgetc(stream);

        if(c == EOF)
        {
            // Return NULL when stream is empty or upon error
            if(p == buffer || ferror(stream)) return NULL;
            // append DELIMITER if it's the last line of a file that ends in a character other than DELIMITER.
            if(p[-1] != delimiter) c = delimiter;
        }

        // Check if it's needed to allocate more space
        if(p == end)
        {
            size_t oldsize = linebuffer->size;
            linebuffer->size += 1024;
            buffer = realloc(buffer, linebuffer->size); //FIXME
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
static struct linebuffer line_buf;

// printf format string for unnumbered lines.
static char *print_no_line_fmt = NULL;

// Width of line numbers.
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
    switch(line_type[0])
    {
    case 'a':
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
        switch(regexec(&line_regex, line_buf.buffer, 0, NULL, 0)) //FIXME
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

    fwrite(line_buf.buffer, sizeof(char), line_buf.length, stdout);
}

// Read and process the file pointed to by FP.
static void process_file(FILE *fp)
{
    while(readlinebuffer(&line_buf, fp, '\n'))
        proc_text();
}

// Process file FILE to standard output. Return true if successful.
static bool nl_file(char const *file)
{
    FILE *stream;

    if(STREQ(file, "-"))
    {
        have_read_stdin = true;
        stream = stdin;
    }
    else
    {
        stream = fopen(file, "r");
        if(stream == NULL)
        {
            fprintf(stderr, "can't open file %s\n", file);
            return false;
        }
    }

    process_file(stream);

    if(ferror(stream)) return false;

    if(STREQ(file, "-"))
        clearerr(stream);
    else
        if(fclose(stream) == EOF)
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
        break;
    }
    return rval;
}

static bool parse_args(int argc, char **argv)
{
    int c;
    while((c = getopt(argc, argv, "b:s:")) != -1)
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
    memset(&line_buf, 0, sizeof(struct linebuffer));

    // Initialize the printf format for unnumbered lines.
    size_t len;
    len = strlen(separator_str);
    print_no_line_fmt = malloc(lineno_width + len + 1);
    memset(print_no_line_fmt, ' ', lineno_width + len);
    print_no_line_fmt[lineno_width + len] = '\0';

    // Main processing.
    if (optind == argc)
        ok = nl_file("-"); // No input files. Read from stdin.
    else
        for (; optind < argc; optind++)
            ok &= nl_file(argv[optind]);

    // Cleaning
    free(line_buf.buffer);
    if(line_type[0] == 'p') regfree(&line_regex);
    if(have_read_stdin && fclose (stdin) == EOF) exit(EXIT_FAILURE);

    exit(ok ? EXIT_SUCCESS : EXIT_FAILURE);
    return EXIT_SUCCESS;
}
