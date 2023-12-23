#include "debug.h"
#include <isa.h>

#include <stdlib.h>
#include <errno.h>

/* Temporarily enable the unit test in file */
// #define UNIT_TEST_EXPR

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
  TK_NONE, /*Representing the empty (NONE) operator */
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
#if 0
            /* Limit the number of digits to 32 at most to avoid buffer overflow */
            /* Simple solution: substr_len %= 33; */
            if (substr_len > 32) {
              Log("WARNING: token buffer overflow, actual number of decimal digits is %d", substr_len);
              substr_len = 32;
            }
#endif
            Assert(substr_len <= 32, "token buffer overflow, actual number of decimal digits is %d", substr_len);
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

static bool check_parentheses(uint32_t p, uint32_t q, bool *valid) {
  /* Assume parentheses in the given expression are matched by default */
  *valid = true;

  /* Check that parentheses are matched with each other at first */
  int parentheses[q - p + 1];
  /* Initialize the array with all 0 by default */
  memset(parentheses, 0, sizeof parentheses);
  int next_pos = 0;
  
  for (int i = p; i <= q; ++i) {
    Token curr_token = tokens[i];
    if (curr_token.type == TK_PARENTHESIS_LEFT) {
      parentheses[next_pos++] = TK_PARENTHESIS_LEFT;
    } else if (curr_token.type == TK_PARENTHESIS_RIGHT) {
      if (next_pos == 0) {
        return false;
      }

      if (parentheses[next_pos - 1] == TK_PARENTHESIS_LEFT) {
        --next_pos;
      } else {
        return false;
      }
    } else {
      ;
    }
  }

  if (next_pos > 0) {
    *valid = false;
  }

  return next_pos > 0 && (tokens[p].type!= TK_PARENTHESIS_LEFT || tokens[q].type!= TK_PARENTHESIS_RIGHT);
}

static uint32_t convert_to_uint32(const char *str) {
  if (NULL == str) {
    panic("Unexpected NULL pointer");
  }

  /* Convert the string to the number */
  char *end;
  uint32_t val;
  /* Ignore all errors happening before */
  errno = 0;
  val = strtoul(str, &end, 10);
  
  /*Sanity check: ensure the numeric operand is usable */
  Assert(end == str || *end != '\0' || ERANGE == errno, "strtoul failed: given numeric operand is invalid %s", str);

  return val;
}

/**
 * @brief Compare two operators by following the rules:
 *        1. supported operators: +, -, *, /
 *        2. + and - owns lower precedence than * and /
 *        3. + and - or * and / owns the same precedence, but considering as lower precedence because
 *           op2 is assumed to be the operand at a higher index
 * 
 * @param op1 
 * @param op2 
 * @return true 
 * @return false 
 */
static bool comp_operator_precedence(uint32_t op1, uint32_t op2) {
  bool is_lower_precedence = false;
  switch (op1) {
    case '+':
    case '-':
      is_lower_precedence = (op2 != '(' && op2 != ')') && !(op2 == '*' || op2 == '/');
      break;
    case '*':
    case '/':
      is_lower_precedence = (op2 != '(' && op2 != ')') && (op2 == '*' || op2 == '/');
      break;
    default: Assert(0, "Unsupported operator: %c", op1);
  }

  return is_lower_precedence;
}

/**
 * @brief Find the position of the main operator in the given expression
 * @note  main operator is the last operator to be processed in an expression
 * @param p 
 * @param q 
 * @return uint32_t 
 */
static uint32_t find_main_op(uint32_t p, uint32_t q) {
  int prev_pos = 0;
  int curr_pos = 0;
  int main_op_pos = 0;
  int main_op = TK_NONE;

  for (curr_pos = p; curr_pos <= q; ++curr_pos) {
    Token curr_token = tokens[curr_pos];
    if (curr_token.type == TK_NOTYPE || curr_token.type == TK_DECIMAL) {
      continue;
    }

    /* Operator adjacent to the left parentheses can never be the main operator */
    if (tokens[prev_pos].type == TK_PARENTHESIS_LEFT) {
      /* Update the previous position */
      prev_pos = curr_pos;
      continue;
    }

    /* Compare the current operator with the previously found one to determine the precedence */
    if (TK_NONE == main_op) {
      main_op = curr_token.type;
      main_op_pos = curr_pos;
    } else if (comp_operator_precedence(main_op, curr_pos)) {
        main_op = curr_token.type;
        main_op_pos = curr_pos;
    }

    prev_pos = curr_pos;
  }

  return main_op_pos;
}

/** To avoid unnecessary calculation when the expression if found to be invalid,
  * check the flag variable, i.e. success after each time of calling the function
  * the code looks a bit ugly though ...
  **/
static uint32_t eval(uint32_t p, uint32_t q, bool *success) {
  /* Indication of the validity of parentheses matching in the expression [p, q] */
  bool is_paren_valid = true;

  /* Sanity check before processing the expression */
  if (!(*success)) {
    return 0;
  }

  if (p > q) {
    /* Bad expression */
    *success = false;
    return 0;
  } else if (p == q) {
    /* Single token.
     * For now this token should be a number.
     * Return the value of the number.
     */
    return convert_to_uint32(tokens[p].str);
  } else if (check_parentheses(p, q, &is_paren_valid) == true) {
    /* The expression is surrounded by a matched pair of parentheses.
     * If that is the case, just throw away the parentheses.
     */
    if (!is_paren_valid) {
      *success = false;
      return 0;
    }

    /* Only return the value when the expression is invalid */
    uint32_t ret_val = eval(p + 1, q - 1, success);
    if (!(*success)) {
      return 0;
    }

    return ret_val;
  } else {
    uint32_t main_op = find_main_op(p, q);
    uint32_t val_left = eval(p, main_op - 1, success);
    uint32_t val_right = eval(main_op + 1, q, success);

    switch(tokens[main_op].type) {
      case '+': *success = true; return val_left + val_right;
      case '-': *success = true; return val_left - val_right;
      case '*': *success = true; return val_left * val_right;
      case '/': *success = true; return val_left / val_right;
      default: Assert(0, "Unsupported operator: %c", tokens[main_op].type);
    }
  }
}


word_t expr(char *e, bool *success) {
  if (!make_token(e)) {
    *success = false;
    return 0;
  }

  /* Evaluate the expression, note success indicates the validity of the result */
  return eval(0, nr_token - 1, success);
}

/*============================= Unit Tests =============================*/
#ifdef UNIT_TEST_EXPR
static void test_make_token(void) {

}


int main(void) {
  test_make_token();
  return 0;
}
#endif