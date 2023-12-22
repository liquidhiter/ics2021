#include <isa.h>
#include <cpu/cpu.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "sdb.h"

#include <stdlib.h>
#include <errno.h>

#define DEV_LOG 1

static int is_batch_mode = false;

void init_regex();
void init_wp_pool();

/* We use the `readline' library to provide more flexibility to read from stdin. */
static char* rl_gets() {
  static char *line_read = NULL;

  if (line_read) {
    free(line_read);
    line_read = NULL;
  }

  line_read = readline("(nemu) ");

  if (line_read && *line_read) {
    add_history(line_read);
  }

  return line_read;
}

static int cmd_c(char *args) {
  cpu_exec(-1);
  return 0;
}


static int cmd_q(char *args) {
  exit(0);
}

static int cmd_help(char *args);

/* Single-step exeuction: step into
 * Implemnetation:
 * 1> read the command and arguments
 * 2> argument is 1 by default
 * 3> argument needs validation
 * 
 * returns state code
 */
static int cmd_si(char* args);

static struct {
  const char *name;
  const char *description;
  int (*handler) (char *);
} cmd_table [] = {
  { "help", "Display informations about all supported commands", cmd_help },
  { "c", "Continue the execution of the program", cmd_c },
  { "q", "Exit NEMU", cmd_q },
  { "si", "Execute given number of step(s), one by default", cmd_si},
  /* TODO: Add more commands */

};

#define NR_CMD ARRLEN(cmd_table)

static int cmd_help(char *args) {
  /* extract the first argument */
  char *arg = strtok(NULL, " ");
  int i;

  if (arg == NULL) {
    /* no argument given */
    for (i = 0; i < NR_CMD; i ++) {
      printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
    }
  }
  else {
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(arg, cmd_table[i].name) == 0) {
        printf("%s - %s\n", cmd_table[i].name, cmd_table[i].description);
        return 0;
      }
    }
    printf("Unknown command '%s'\n", arg);
  }
  return 0;
}

enum {SI_STEP_SINGLE = 0x001, SI_STEP_MULTIPLE = 0x010, SI_STEP_INVALID = 0x100, };
static int parse_si_arg(char* args, uint64_t* step) {

  /* CASE 1: no argument is found after parsing the command line argument by strtok */
  if (NULL == args) {
#ifdef DEV_LOG
    Log("DEV LOG: parse_si_arg: no argument is found after parsing the command line argument by strtok\n");
#endif /*DEV_LOG*/
    *step = 1U;
    return SI_STEP_SINGLE;
  }

  uint64_t next_pos;
  uint8_t next_char;

  /* Read out the leading whitespaces */
  for (next_pos = 0; (next_char = *(args + next_pos)) == ' '; next_pos++);

  /* Check the first non-whitespace character */
  if (next_char == '0') {
#ifdef DEV_LOG
    Log("DEV LOG: parse_si_arg: argument only contains whitespaces\n");
#endif /*DEV_LOG*/
    return SI_STEP_SINGLE;
  }

  /* CASE 3: non-whitespace argument(s) is found, which can be invalid though */
  if (next_char != '[') {
#ifdef DEV_LOG
    Log("DEV LOG: parse_si_arg: invalid argument is found: %c", next_char);
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
    Log("DEV LOG: parse_si_arg: invalid argument is found: %c", next_char);
#endif /*DEV_LOG*/
    return SI_STEP_INVALID;
  }

  /* Get the number of steps from the argument starting from next_pos
   * 1. Only digits are allowed to appear
   * 2. Maximum 20 characters considering maximum value of uint64_t, should be handled by strtoull()
   */
  uint64_t start_of_digit = next_pos;
  uint64_t num_of_digits = 0;
  for (; (next_char = *(args + next_pos)) != ']' && next_char != '\0'; next_pos++, num_of_digits++);
  
  if (next_char == '\0') {
#ifdef DEV_LOG
    Log("DEV LOG: parse_si_arg: invalid argument is found: %c", next_char);
#endif /*DEV_LOG*/
    return SI_STEP_INVALID;
  }

  /* Copy the found number of characters supposed to contain an integer */
  char step_digits[num_of_digits + 1];
  strncpy(step_digits, args + start_of_digit, num_of_digits);
  /* Append the null char */
  step_digits[num_of_digits + 1] = '\0';

  /* Convert the string to the number */
  char *end;
  uint64_t step_val;
  /* Ignore all errors happening before */
  errno = 0;
  step_val = strtoull(step_digits, &end, 10);
  if (end == step_digits || *end != '\0' || ERANGE == errno) {
#ifdef DEV_LOG
    Log("DEV LOG: parse_si_arg: invalid argument is found: %s, num_of_digits = %lu", step_digits, num_of_digits);
#endif /*DEV_LOG*/
    return SI_STEP_INVALID;
  }

  /* Given number of steps must be valid */
  *step = step_val;
  /* Points to the next character */
  next_pos++;

  /* Read out all tailing whitespaces appending after ']' if exists */
  for (; (next_char = *(args + next_pos)) == ' '; next_pos++);

  /* Argument is considered as invalid even the given digits are perfectly correct if there is trailing characters other than end char */
  if (next_char != '\0') {
#ifdef DEV_LOG
    Log("DEV LOG: parse_si_arg: invalid arguments are appending after ]: %c", next_char);
#endif
    return SI_STEP_INVALID;
  }

  return SI_STEP_MULTIPLE;
}

static int cmd_si(char *args) {
  Log("Given argument is: %s\n", args);
  
  /* Format of the argument:
   * 1. allow the leading trailing spaces
   * 2. allow spaces inside of the bracket [ 20 ]
   * 3. not allow the argument to contain more than one whitespace when there is no step number, e.x. si(\s)*
   *    this is to simplify argument parsing
   */
  uint64_t step_val = 0;
  int parse_result = parse_si_arg(args, &step_val);
  if (SI_STEP_INVALID == parse_result) {
    return 1;
  }

  /* Call cpu_exec to execute given number of steps */
  Log("Execute %lu number of steps\n", step_val);
  cpu_exec(step_val);

  return 0;
}

void sdb_set_batch_mode() {
  is_batch_mode = true;
}

void sdb_mainloop() {
  if (is_batch_mode) {
    cmd_c(NULL);
    return;
  }

  for (char *str; (str = rl_gets()) != NULL; ) {
    char *str_end = str + strlen(str);

    /* extract the first token as the command */
    char *cmd = strtok(str, " ");
    if (cmd == NULL) { continue; }

    /* treat the remaining string as the arguments,
     * which may need further parsing
     */
    char *args = cmd + strlen(cmd) + 1;
    if (args >= str_end) {
      args = NULL;
    }

#ifdef CONFIG_DEVICE
    extern void sdl_clear_event_queue();
    sdl_clear_event_queue();
#endif

    int i;
    for (i = 0; i < NR_CMD; i ++) {
      if (strcmp(cmd, cmd_table[i].name) == 0) {
        if (cmd_table[i].handler(args) < 0) { return; }
        break;
      }
    }

    if (i == NR_CMD) { printf("Unknown command '%s'\n", cmd); }
  }
}

void init_sdb() {
  /* Compile the regular expressions. */
  init_regex();

  /* Initialize the watchpoint pool. */
  init_wp_pool();
}
