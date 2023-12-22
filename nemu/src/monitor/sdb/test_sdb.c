#include <assert.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <readline/readline.h>
#include <readline/history.h>
#include <stdlib.h>
#include <errno.h>

#define DEV_LOG 1

// #include "sdb.h"

#define ASSERT_EQUAL(a, b) \
do { \
    if (a != b) { \
        fprintf(stderr, "%s:%d: %s!= %s\n", __FILE__, __LINE__, #a, #b); \
    } \
} while (0)\


#define ASSERT_NOT_EQUAL(a, b) \
do { \
    if (a == b) { \
        fprintf(stderr, "%s:%d: %s == %s\n", __FILE__, __LINE__, #a, #b); \
    } \
} while (0)\

#if 0
#define _Log(...) \
  do { \
    printf(__VA_ARGS__); \
  } while (0)



#define Log(format, ...) \
    _Log(ASNI_FMT("[%s:%d %s] " format, ASNI_FG_BLUE) "\n", \
        __FILE__, __LINE__, __func__, ## __VA_ARGS__)
#endif

#define Log(...)


/*===========================================================*/

/**
 * @brief  
 * @note   
 */
enum {SI_STEP_VALID = 0x01, SI_STEP_INVALID = 0x10,};

/**
 * @brief  
 * @note   
 * @param  args: 
 * @param  step: 
 * @retval 
 */
static int parse_si_arg(char* args, uint64_t* step) {

  /* CASE 1: no argument is found after parsing the command line argument by strtok */
  if (NULL == args) {
#ifdef DEV_LOG
    Log("DEV LOG: no argument is found after parsing the command line argument by strtok\n");
#endif /*DEV_LOG*/
    *step = 1U;
    return SI_STEP_VALID;
  }

  uint64_t next_pos;
  uint8_t next_char;

  /* Read out the leading whitespaces */
  for (next_pos = 0; (next_char = *(args + next_pos)) == ' '; next_pos++);

  /* Check the first non-whitespace character */
  if (next_char == '0') {
#ifdef DEV_LOG
    Log("DEV LOG: argument only contains whitespaces\n");
#endif /*DEV_LOG*/
    *step = 1U;
    return SI_STEP_VALID;
  }

  /* CASE 3: non-whitespace argument(s) is found, which can be invalid though */
  if (next_char != '[') {
#ifdef DEV_LOG
    Log("DEV LOG: invalid argument is found: %c", next_char);
#endif /*DEV_LOG*/
    return SI_STEP_INVALID;
  }
  /* Points to the next character */
  next_pos++;

  /* Read out the leading spaces before the digit if exists */
  for (; (next_char = *(args + next_pos)) == ' '; next_pos++);

  /* Check the first non-whitespace character */
  if (next_char < '0' || next_char > '9') {
#ifdef DEV_LOG
    Log("DEV LOG: invalid argument is found: %c", next_char);
#endif /*DEV_LOG*/
    return SI_STEP_INVALID;
  }

  /* Get the number of steps from the argument starting from next_pos
   * 1. Only digits are allowed to appear
   * 2. Maximum 20 characters considering maximum value of uint64_t, should be handled by strtoull()
   */
  uint64_t num_of_digits = 0;
  for (; (next_char = *(args + next_pos)) != ']' && next_char != '\0' && next_char != ' '; next_pos++, num_of_digits++);
  
  if (next_char == '\0') {
#ifdef DEV_LOG
    Log("DEV LOG: invalid argument is found: %c", next_char);
#endif /*DEV_LOG*/
    return SI_STEP_INVALID;
  }

  /* Copy the found number of characters supposed to contain an integer */
  char step_digits[num_of_digits + 1];
  strncpy(step_digits, args + next_pos - num_of_digits, num_of_digits);
  /* Append the null char */
  step_digits[num_of_digits] = '\0';

  /* Convert the string to the number */
  char *end;
  uint64_t step_val;
  /* Ignore all errors happening before */
  errno = 0;
  step_val = strtoull(step_digits, &end, 10);
  if (end == step_digits || *end != '\0' || ERANGE == errno || step_val == 0) {
#ifdef DEV_LOG
    Log("DEV LOG: invalid argument is found: %s", step_digits);
#endif /*DEV_LOG*/
    return SI_STEP_INVALID;
  }

  /* Given number of steps must be valid */
  *step = step_val;

  /* Read out all tailing whitespaces */
  for (; (next_char = *(args + next_pos)) == ' '; next_pos++);

  if ((next_char = *(args + next_pos)) != ']') {
#ifdef DEV_LOG
    Log("DEV LOG: invalid argument is found: %s", args);
#endif /*DEV_LOG*/
    return SI_STEP_INVALID;
  }

  /* Points to the next character */
  next_pos++;

  /* Read out all tailing whitespaces */
  for (; (next_char = *(args + next_pos)) == ' '; next_pos++);

  /* Argument is considered as invalid even the given digits are perfectly correct if there is trailing characters other than end char */
  if (next_char != '\0') {
#ifdef DEV_LOG
    Log("DEV LOG: invalid arguments are appending after ]: %c", next_char);
#endif
    return SI_STEP_INVALID;
  }

  return SI_STEP_VALID;
}



static void test_parse_si_arg() {
    char *arg = NULL;
    uint64_t step = 0;

    /* Test Case 1: No argument */
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_VALID);
    ASSERT_EQUAL(step, 1U);


    arg = " ";
    step = 0;
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_VALID);
    ASSERT_EQUAL(step, 1U);

    arg = "    ";
    step = 0;
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_VALID);
    ASSERT_EQUAL(step, 1U);

    arg = "123456";
    step = 0;
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_INVALID);
    ASSERT_EQUAL(step, 0U);

    arg = " [10]";
    step = 0;
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_VALID);
    ASSERT_EQUAL(step, 10U);

    arg = "  [10]  ";
    step = 0;
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_VALID);
    ASSERT_EQUAL(step, 10U);

    arg = " [ 10]";
    step = 0;
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_VALID);
    ASSERT_EQUAL(step, 10U);

    arg = " [ 10 ]";
    step = 0;
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_VALID);
    ASSERT_EQUAL(step, 10U);

    arg = " [ 10 ]    ";
    step = 0;
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_VALID);
    ASSERT_EQUAL(step, 10U);

    arg = " [ 10 ]1234";
    step = 0;
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_INVALID);
    ASSERT_EQUAL(step, 0U);

    arg = " [ 10 ] 1234";
    step = 0;
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_INVALID);
    ASSERT_EQUAL(step, 0U);

    arg = " [ 1 0 ] ";
    step = 0;
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_INVALID);
    ASSERT_EQUAL(step, 0U);

    arg = " [ 18446744073709551615 ]  ";
    step = 0;
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_VALID);
    ASSERT_EQUAL(step, 1844674407370955LU);

    arg = " [ 18446744073709551616 ]";
    step = 0;
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_INVALID);
    ASSERT_EQUAL(step, 0U);

    arg = " [[10]";
    step = 0;
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_INVALID);
    ASSERT_EQUAL(step, 0U);

    arg = " [10]] ";
    step = 0;
    ASSERT_EQUAL(parse_si_arg(arg, &step), SI_STEP_INVALID);
    ASSERT_EQUAL(step, 0U);
}



int main(int argc, char *argv[]) {
    test_parse_si_arg();
    return 0;
}