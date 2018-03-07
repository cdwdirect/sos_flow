/*
 *
 * Mini regex-module inspired by Rob Pike's regex code described in:
 *
 * http://www.cs.princeton.edu/courses/archive/spr09/cos333/beautiful.html
 *
 *
 *
 * Supports:
 * ---------
 *   '.'        Dot, matches any character
 *   '^'        Start anchor, matches beginning of string
 *   '$'        End anchor, matches end of string
 *   '*'        Asterisk, match zero or more (greedy)
 *   '+'        Plus, match one or more (greedy)
 *   '?'        Question, match zero or one (non-greedy)
 *   '[abc]'    Character class, match if one of {'a', 'b', 'c'}
 *   '[^abc]'   Inverted class, match if NOT one of {'a', 'b', 'c'} -- NOTE: feature is currently broken!
 *   '[a-zA-Z]' Character ranges, the character set of the ranges { a-z | A-Z }
 *   '\s'       Whitespace, \t \f \r \n \v and spaces
 *   '\S'       Non-whitespace
 *   '\w'       Alphanumeric, [a-zA-Z0-9_]
 *   '\W'       Non-alphanumeric
 *   '\d'       Digits, [0-9]
 *   '\D'       Non-digits
 *
 *
 */



#include <stdio.h>

#include "sos.h"
#include "sos_types.h"
#include "sos_re.h"


/* Definitions: */

#define MAX_REGEXP_OBJECTS      30    /* Max number of regex symbols in expression. */
#define MAX_CHAR_CLASS_LEN      40    /* Max length of character-class buffer in.   */


typedef struct SOS_regex_t
{
  unsigned char  type;   /* CHAR, STAR, etc.                      */
  union
  {
    unsigned char  ch;   /*      the character itself             */
    unsigned char* ccl;  /*  OR  a pointer to characters in class */
  };
} SOS_regex_t;



/* Private function declarations: */
static int SOS_matchpattern(SOS_regex_t* pattern, const char* text);
static int SOS_matchcharclass(char c, const char* str);
static int SOS_matchstar(SOS_regex_t p, SOS_regex_t* pattern, const char* text);
static int SOS_matchplus(SOS_regex_t p, SOS_regex_t* pattern, const char* text);
static int SOS_matchone(SOS_regex_t p, char c);
static int SOS_matchdigit(char c);
static int SOS_matchalpha(char c);
static int SOS_matchwhitespace(char c);
static int SOS_matchmetachar(char c, const char* str);
static int SOS_matchrange(char c, const char* str);
static int SOS_ismetachar(char c);


/* Public functions: */
int SOS_re_match(const char* pattern, const char* text)
{
  return SOS_re_matchp(SOS_re_compile(pattern), text);
}

int SOS_re_matchp(SOS_re_t pattern, const char* text)
{
  if (pattern != 0)
  {
    if (pattern[0].type == SOS_REGEX_BEGIN)
    {
      return ((SOS_matchpattern(&pattern[1], text)) ? 0 : -1);
    }
    else
    {
      int idx = -1;

      do
      {
        idx += 1;
        if (SOS_matchpattern(pattern, text))
        {
          return idx;
        }
      }
      while (*text++ != '\0');
    }
  }
  return -1;
}

SOS_re_t SOS_re_compile(const char* pattern)
{
  /* The sizes of the two static arrays below substantiates the static RAM usage of this module.
     MAX_REGEXP_OBJECTS is the max number of symbols in the expression.
     MAX_CHAR_CLASS_LEN determines the size of buffer for chars in all char-classes in the expression. */
  static SOS_regex_t re_compiled[MAX_REGEXP_OBJECTS];
  static unsigned char ccl_buf[MAX_CHAR_CLASS_LEN];
  int ccl_bufidx = 1;

  char c;     /* current char in pattern   */
  int i = 0;  /* index into pattern        */
  int j = 0;  /* index into re_compiled    */

  while (pattern[i] != '\0' && (j+1 < MAX_REGEXP_OBJECTS))
  {
    c = pattern[i];

    switch (c)
    {
      /* Meta-characters: */
      case '^': {    re_compiled[j].type = SOS_REGEX_BEGIN;           } break;
      case '$': {    re_compiled[j].type = SOS_REGEX_END;             } break;
      case '.': {    re_compiled[j].type = SOS_REGEX_DOT;             } break;
      case '*': {    re_compiled[j].type = SOS_REGEX_STAR;            } break;
      case '+': {    re_compiled[j].type = SOS_REGEX_PLUS;            } break;
      case '?': {    re_compiled[j].type = SOS_REGEX_QUESTIONMARK;    } break;
/*    case '|': {    re_compiled[j].type = SOS_REGEX_BRANCH;          } break; <-- not working properly */

      /* Escaped character-classes (\s \w ...): */
      case '\\':
      {
        if (pattern[i+1] != '\0')
        {
          /* Skip the escape-char '\\' */
          i += 1;
          /* ... and check the next */
          switch (pattern[i])
          {
            /* Meta-character: */
            case 'd': {    re_compiled[j].type = SOS_REGEX_DIGIT;            } break;
            case 'D': {    re_compiled[j].type = SOS_REGEX_NOT_DIGIT;        } break;
            case 'w': {    re_compiled[j].type = SOS_REGEX_ALPHA;            } break;
            case 'W': {    re_compiled[j].type = SOS_REGEX_NOT_ALPHA;        } break;
            case 's': {    re_compiled[j].type = SOS_REGEX_WHITESPACE;       } break;
            case 'S': {    re_compiled[j].type = SOS_REGEX_NOT_WHITESPACE;   } break;

            /* Escaped character, e.g. '.' or '$' */ 
            default:  
            {
              re_compiled[j].type = SOS_REGEX_CHAR;
              re_compiled[j].ch = pattern[i];
            } break;
          }
        }
        /* '\\' as last char in pattern -> invalid regular expression. */
/*
        else
        { 
          re_compiled[j].type = SOS_REGEX_CHAR;
          re_compiled[j].ch = pattern[i];
        }
*/
      } break;

      /* Character class: */
      case '[':
      {
        /* Remember where the char-buffer starts. */
        int buf_begin = ccl_bufidx;

        /* Look-ahead to determine if negated */
        if (pattern[i+1] == '^')
        {
          re_compiled[j].type = SOS_REGEX_INV_CHAR_CLASS;
          i += 1; /* Increment i to avoid including '^' in the char-buffer */
        }  
        else
        {
          re_compiled[j].type = SOS_REGEX_CHAR_CLASS;
        }

        /* Copy characters inside [..] to buffer */
        while (    (pattern[++i] != ']')
                && (pattern[i]   != '\0')) /* Missing ] */
        {
          if (ccl_bufidx >= MAX_CHAR_CLASS_LEN) {
              //fputs("exceeded internal buffer!\n", stderr);
              return 0;
          }
          ccl_buf[ccl_bufidx++] = pattern[i];
        }
        if (ccl_bufidx >= MAX_CHAR_CLASS_LEN)
        {
            /* Catches cases such as [00000000000000000000000000000000000000][ */
            //fputs("exceeded internal buffer!\n", stderr);
            return 0;
        }
        /* Null-terminate string end */
        ccl_buf[ccl_bufidx++] = 0;
        re_compiled[j].ccl = &ccl_buf[buf_begin];
      } break;

      /* Other characters: */
      default:
      {
        re_compiled[j].type = SOS_REGEX_CHAR;
        re_compiled[j].ch = c;
      } break;
    }
    i += 1;
    j += 1;
  }
  /* 'UNUSED' is a sentinel used to indicate end-of-pattern */
  re_compiled[j].type = SOS_REGEX_UNUSED;

  return (SOS_re_t) re_compiled;
}

void SOS_re_print(SOS_regex_t* pattern)
{
  char* types = (char *)SOS_REGEX_str; 
  
  int i;
  for (i = 0; i < MAX_REGEXP_OBJECTS; ++i)
  {
    if (pattern[i].type == SOS_REGEX_UNUSED)
    {
      break;
    }

    printf("type: %s", types[pattern[i].type]);
    if (pattern[i].type == SOS_REGEX_CHAR_CLASS || pattern[i].type == SOS_REGEX_INV_CHAR_CLASS)
    {
      printf(" [");
      int j;
      char c;
      for (j = 0; j < MAX_CHAR_CLASS_LEN; ++j)
      {
        c = pattern[i].ccl[j];
        if ((c == '\0') || (c == ']'))
        {
          break;
        }
        printf("%c", c);
      }
      printf("]");
    }
    else if (pattern[i].type == SOS_REGEX_CHAR)
    {
      printf(" '%c'", pattern[i].ch);
    }
    printf("\n");
  }
}



/* Private functions: */
static int SOS_matchdigit(char c)
{
  return ((c >= '0') && (c <= '9'));
}
static int SOS_matchalpha(char c)
{
  return ((c >= 'a') && (c <= 'z')) || ((c >= 'A') && (c <= 'Z'));
}
static int SOS_matchwhitespace(char c)
{
  return ((c == ' ') || (c == '\t') || (c == '\n') || (c == '\r') || (c == '\f') || (c == '\v'));
}
static int SOS_matchalphanum(char c)
{
  return ((c == '_') || SOS_matchalpha(c) || SOS_matchdigit(c));
}
static int SOS_matchrange(char c, const char* str)
{
  return ((c != '-') && (str[0] != '\0') && (str[0] != '-') &&
         (str[1] == '-') && (str[1] != '\0') &&
         (str[2] != '\0') && ((c >= str[0]) && (c <= str[2])));
}
static int SOS_ismetachar(char c)
{
  return ((c == 's') || (c == 'S') == (c == 'w') || (c == 'W') || (c == 'd') || (c == 'D'));
}

static int SOS_matchmetachar(char c, const char* str)
{
  switch (str[0])
  {
    case 'd': return  SOS_matchdigit(c);
    case 'D': return !SOS_matchdigit(c);
    case 'w': return  SOS_matchalphanum(c);
    case 'W': return !SOS_matchalphanum(c);
    case 's': return  SOS_matchwhitespace(c);
    case 'S': return !SOS_matchwhitespace(c);
    default:  return (c == str[0]);
  }
}

static int SOS_matchcharclass(char c, const char* str)
{
  do
  {
    if (SOS_matchrange(c, str))
    {
      return 1;
    }
    else if (str[0] == '\\')
    {
      /* Escape-char: increment str-ptr and match on next char */
      str += 1;
      if (SOS_matchmetachar(c, str))
      {
        return 1;
      } 
      else if ((c == str[0]) && !SOS_ismetachar(c))
      {
        return 1;
      }
    }
    else if (c == str[0])
    {
      if (c == '-')
      {
        return ((str[-1] == '\0') || (str[1] == '\0'));
      }
      else
      {
        return 1;
      }
    }
  }
  while (*str++ != '\0');

  return 0;
}

static int SOS_matchone(SOS_regex_t p, char c)
{
  switch (p.type)
  {
    case SOS_REGEX_DOT:            return 1;
    case SOS_REGEX_CHAR_CLASS:     return  SOS_matchcharclass(c, (const char*)p.ccl);
    case SOS_REGEX_INV_CHAR_CLASS: return !SOS_matchcharclass(c, (const char*)p.ccl);
    case SOS_REGEX_DIGIT:          return  SOS_matchdigit(c);
    case SOS_REGEX_NOT_DIGIT:      return !SOS_matchdigit(c);
    case SOS_REGEX_ALPHA:          return  SOS_matchalphanum(c);
    case SOS_REGEX_NOT_ALPHA:      return !SOS_matchalphanum(c);
    case SOS_REGEX_WHITESPACE:     return  SOS_matchwhitespace(c);
    case SOS_REGEX_NOT_WHITESPACE: return !SOS_matchwhitespace(c);
    default:                       return  (p.ch == c);
  }
}

static int SOS_matchstar(SOS_regex_t p, SOS_regex_t* pattern, const char* text)
{
  do
  {
    if (SOS_matchpattern(pattern, text))
      return 1;
  }
  while ((text[0] != '\0') && SOS_matchone(p, *text++));

  return 0;
}

static int SOS_matchplus(SOS_regex_t p, SOS_regex_t* pattern, const char* text)
{
  while ((text[0] != '\0') && SOS_matchone(p, *text++))
  {
    if (SOS_matchpattern(pattern, text))
      return 1;
  }
  return 0;
}


#if 0

/* Recursive matching */
static int SOS_matchpattern(SOS_regex_t* pattern, const char* text)
{
  if ((pattern[0].type == SOS_REGEX_UNUSED) || (pattern[1].type == SOS_REGEX_QUESTIONMARK))
  {
    return 1;
  }
  else if (pattern[1].type == SOS_REGEX_STAR)
  {
    return SOS_matchstar(pattern[0], &pattern[2], text);
  }
  else if (pattern[1].type == SOS_REGEX_PLUS)
  {
    return SOS_matchplus(pattern[0], &pattern[2], text);
  }
  else if ((pattern[0].type == SOS_REGEX_END) && pattern[1].type == SOS_REGEX_UNUSED)
  {
    return text[0] == '\0';
  }
  else if ((text[0] != '\0') && SOS_matchone(pattern[0], text[0]))
  {
    return SOS_matchpattern(&pattern[1], text+1);
  }
  else
  {
    return 0;
  }
}

#else

/* Iterative matching */
static int SOS_matchpattern(SOS_regex_t* pattern, const char* text)
{
  do
  {
    if ((pattern[0].type == SOS_REGEX_UNUSED) || (pattern[1].type == SOS_REGEX_QUESTIONMARK))
    {
      return 1;
    }
    else if (pattern[1].type == SOS_REGEX_STAR)
    {
      return SOS_matchstar(pattern[0], &pattern[2], text);
    }
    else if (pattern[1].type == SOS_REGEX_PLUS)
    {
      return SOS_matchplus(pattern[0], &pattern[2], text);
    }
    else if ((pattern[0].type == SOS_REGEX_END) && pattern[1].type == SOS_REGEX_UNUSED)
    {
      return (text[0] == '\0');
    }
/*  Branching is not working properly
    else if (pattern[1].type == SOS_REGEX_BRANCH)
    {
      return (SOS_matchpattern(pattern, text) || SOS_matchpattern(&pattern[2], text));
    }
*/
  }
  while ((text[0] != '\0') && SOS_matchone(*pattern++, *text++));

  return 0;
}

#endif




