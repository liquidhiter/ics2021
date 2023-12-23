#include <isa.h>

/* We use the POSIX regex functions to process regular expressions.
 * Type 'man regex' for more information about POSIX regex functions.
 */
#include <regex.h>

enum {
  TK_NOTYPE = 256, /* start from 256: char is used as the token_type */
  TK_EQ,
  TK_DECIMAL,
  TK_PARENTHESIS_LEFT,
  TK_PARENTHESIS_RIGHT,
  
  /* TODO: Add more token types */

};


/* The characters (,),[,],.,*,?,+,|,^ and $ are special symbols 
 * and have to be escaped with a backslash symbol in order to be treated as literal characters
 * @source: https://en.wikibooks.org/wiki/Regular_Expressions/POSIX-Extended_Regular_Expressions
 */

static struct rule {
  const char *regex;
  int token_type;
} rules[] = {

  /* TODO: Add more rules.
   * Pay attention to the precedence level of different rules.
   */

  {" +", TK_NOTYPE},    // spaces
  {"\\+", '+'},         // plus
  {"==", TK_EQ},        // equal
  {"-", '-'},           // minus
  {"\\*", '*'},         // multiply
  {"/", '/'},         // divide
  {"[:digit:]", TK_DECIMAL},   // decimal digit
  {"\\(", TK_PARENTHESIS_LEFT},  // left parenthesis
  {"\\)", TK_PARENTHESIS_RIGHT}, // right parenthesis

};

#define NR_REGEX ARRLEN(rules)

static regex_t re[NR_REGEX] = {};

/* Rules are used for many times.
 * Therefore we compile them only once before any usage.
 */
void init_regex() {
  int i;
  char error_msg[128];
  int ret;

  for (i = 0; i < NR_REGEX; i ++) {
    ret = regcomp(&re[i], rules[i].regex, REG_EXTENDED);
    if (ret != 0) {
      regerror(ret, &re[i], error_msg, 128);
      panic("regex compilation failed: %s\n%s", error_msg, rules[i].regex);
    }
  }
}

typedef struct token {
  int type;
  char str[32];
} Token;

static Token tokens[32] __attribute__((used)) = {};
static int nr_token __attribute__((used))  = 0;

static bool make_token(char *e) {
  int position = 0;
  int i;
  regmatch_t pmatch;

  nr_token = 0;

  while (e[position] != '\0') {
    /* Try all rules one by one. */
    for (i = 0; i < NR_REGEX; i ++) {
      if (regexec(&re[i], e + position, 1, &pmatch, 0) == 0 && pmatch.rm_so == 0) {
        char *substr_start = e + position;
        int substr_len = pmatch.rm_eo;

        Log("match rules[%d] = \"%s\" at position %d with len %d: %.*s",
            i, rules[i].regex, position, substr_len, substr_len, substr_start);

        position += substr_len;

        /* TODO: Now a new token is recognized with rules[i]. Add code
         * to record the token in the array `tokens'. For certain types
         * of tokens, some extra actions should be performed.
         */

        Token newToken = {};
        switch (rules[i].token_type) {
          case TK_NOTYPE:
            /* No need to record spaces */
            break;
          case TK_EQ:
          case TK_PARENTHESIS_LEFT:
          case TK_PARENTHESIS_RIGHT:
            newToken.type = rules[i].token_type;
            tokens[nr_token++] = newToken;
            break;
          case TK_DECIMAL:
            newToken.type = TK_DECIMAL;
            /* Limit the number of digits to 32 at most to avoid buffer overflow */
            /* Simple solution: substr_len %= 33; */
            if (substr_len > 32) {
              Log("WARNING: token buffer overflow, actual number of decimal digits is %d", substr_len);
              substr_len = 32;
            }
            strncpy(newToken.str, substr_start, substr_len);
            break;
          default:
            break;
        }
      }
    }

    if (i == NR_REGEX) {
      printf("no match at position %d\n%s\n%*.s^\n", position, e, position, "");
      return false;
    }
  }

  return true;
}


word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* TODO: Insert codes to evaluate the expression. */
  TODO();

  return 0;
}
