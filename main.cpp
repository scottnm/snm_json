#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "char_vec.h"
#include "helpers.h"
#include "json.h"

// MACROS
#define NO_CONT assert("no return" && false);
#define ERRORBUF_SIZE 200

// STATIC FUNC DECLARATIONS
char getnc(FILE* stream);
void ungetnc(char c, FILE* stream);
void logerror_and_quit(FILE* stream, const char* err_str);
void flush_whitespace(FILE* stream);
char* parse_str(FILE* stream);
char* parse_key(FILE* stream);
double parse_num(FILE* stream);
value parse_val(FILE* stream);
void parse_obj(FILE* stream);

// STATIC VALUES
static char_vec error_buf;

// DEFINITIONS
int main(int argc, char** argv)
{
    // check for help/usage
    if (argc >= 2)
    {
        for(int i = 1; i < argc; ++i)
        {
            if (strcmp(argv[i], "-h") == 0)
            {
                printf("Usage: ./main <json_file>\n"
                       "Usage: <json_output> | ./main\n");
                return 0;
            }
        }
    }

    error_buf = new_char_vec(ERRORBUF_SIZE);
    parse_obj(argc < 2 ? stdin : fopen(argv[1], "r"));
    destroy_char_vec(&error_buf);
    return 0;
}

void parse_obj(FILE* stream)
{
    // assert that the object begins with the { char
    flush_whitespace(stream);
    if (getnc(stream) != '{')
    {
        logerror_and_quit(stream, "Error, bad obj syntax");
    }

    auto parsing = true;
    while (parsing)
    {
        auto key = parse_key(stream);
        printf("next key: %s\n", key);
        free(key);
        value v = parse_val(stream);
        switch(v.type)
        {
            case vtype::NUM:
                printf("next val (num): %f\n", v.num);
                break;
            case vtype::STR:
                printf("next val (str): %s\n", v.str);
                break;
            default:
                logerror_and_quit(stream, "invalid value type");
        }
        flush_whitespace(stream);
        auto end_of_kv = getnc(stream);
        if (end_of_kv == '}')
        {
            parsing = false;
        }
        else if (end_of_kv != ',')
        {
            if (end_of_kv == EOF)
            {
                ungetnc(EOF, stream);
                logerror_and_quit(stream, "Unexpected EOF");
            }
            logerror_and_quit(stream, "End of keyvalue pair was not followed by a comma or a closing brace");
        }
    }
}

char* parse_key(FILE* stream)
{
    auto key = parse_str(stream);
    if (getnc(stream) != ':')
    {
        logerror_and_quit(stream, "Missing ':' after key");
    }

    return key;
}

char* parse_str(FILE* stream)
{
    flush_whitespace(stream);
    auto c = getnc(stream);
    if (c != '"')
    {
        logerror_and_quit(stream, "When trying to parse a string, a value other than an opening quote was found");
    }

    auto str_buf = new_char_vec(15);
    for(char next = getnc(stream); next != '"'; next = getnc(stream))
    {
        push_char(&str_buf, next);
    }
    return detach_char_vec(&str_buf);
}

double parse_num(FILE* stream)
{
    auto numbuf = new_char_vec(15);

    {
        char nc;
        for(nc = getnc(stream); nc != ',' && nc != '}' && !isspace(nc); nc = getnc(stream))
        {
            push_char(&numbuf, nc);
        }
        ungetnc(nc, stream); // put back final , or } for the obj_parser to interpret
    }

    auto end_of_numstr = numbuf.top;
    auto numstr = detach_char_vec(&numbuf);
    char* endptr;
    auto res = strtod(numstr, &endptr);
    // TODO: handle the case where the number is too large or small (errno case)
    if (endptr != end_of_numstr) // str couldn't be converted because it wasn't a number
    {
        logerror_and_quit(stream, "Invalid number could not be parsed");
    }
    free(numstr);
    flush_whitespace(stream);
    return res;
}

value parse_val(FILE* stream)
{
    flush_whitespace(stream);

    auto lookahead = getnc(stream);
    ungetnc(lookahead, stream);

    value parsed_val;

    switch (lookahead)
    {
        case '"': // str val
            parsed_val.type = vtype::STR;
            parsed_val.str = parse_str(stream);
            break;
        case '{': // obj
            logerror_and_quit(stream, "cannot parse objects yet");
            break;
        case '[': // arr
            logerror_and_quit(stream, "cannot parse arrays yet");
            break;
        default:  // number
            if (isdigit(lookahead) || lookahead == '+' || lookahead == '-')
            {
                parsed_val.type = vtype::NUM;
                parsed_val.num = parse_num(stream);
                break;
            }
            logerror_and_quit(stream, "invalid character parsed");
            NO_CONT
    }

    return parsed_val;
}

char getnc(FILE* stream)
{
    auto c = getc(stream);
    push_char(&error_buf, c);
    return c;
}

void ungetnc(char c, FILE* stream)
{
    ungetc(c, stream);
    --error_buf.top;
}

void logerror_and_quit(FILE* stream, const char* err_str)
{
    auto outloc = stderr;

    fprintf(outloc, "errbuf: ");
    print_char_vec(outloc, &error_buf, false);
    fprintf(outloc, "\x1b[33m(*)\x1b[0m");
    // print the next 10 chars in the file stream
    if (stream != NULL)
    {
        char nc = getnc(stream);
        for (int i = 0; i < 10 && nc != EOF; ++i)
        {
            fprintf(outloc, "%c", nc);
            nc = getc(stream); // use stream because we just don't need to backup to the error buf
        }
        fprintf(outloc, "\n");
    }
    error(err_str);
}

void flush_whitespace(FILE* stream)
{
    char c;
    auto reading_whitespace = true;
    while (reading_whitespace)
    {
        c = getnc(stream);
        reading_whitespace = isspace(c);
    }
    ungetnc(c, stream);
}