#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <assert.h>
#include <string.h>

// this should be enough
static char buf[65536] = {};
static char code_buf[65536 + 128] = {}; // a little larger than `buf`
static char *code_format =
"#include <stdio.h>\n"
"int main() { "
"  unsigned result = %s; "
"  printf(\"%%u\", result); "
"  return 0; "
"}";

/* Index of the buffer used to track the available space in buffer */
static uint32_t buf_idx = 0;

/**
 * @brief Return a random uint32_t integer smaller than n
 * 
 * @param n
 * @source https://www.geeksforgeeks.org/generating-random-number-range-c/, partially refer to this page
 * @return uint32_t 
 */
static uint32_t choose(uint32_t n) {
  /* Saturate at the number 0 */
  if (n == 0) return 0;

  return rand() % n;
}

/**
 * @brief 
 * 
 */
static void gen_num() {
  while (1) {
    uint32_t rand_num = choose(UINT32_MAX);

    /* Copy the random integer byte by byte to find minimum number of bytes needed to store it */
    uint8_t num_buf[sizeof(uint32_t)];
    uint8_t idx = 0;
    while (rand_num > 0) {
      num_buf[idx++] = rand_num % 256;
      rand_num >>= 8;
    }

    /* Check available space in the buf >= buf_idx */
    if ((65536 - buf_idx) < idx) {
      continue;
    }

    /* Copy random number into the buf */
    memcpy(buf + buf_idx, num_buf, idx);

    break;
}


static void gen_rand_expr() {
  switch(choose(3)) {
    case 0: gen_num(); break;
    case 1: gen('('); gen_rand_expr(); gen(')'); break;
    default: gen_rand_expr(); gen_rand_op(); gen_rand_expr(); break;
  }
}

int main(int argc, char *argv[]) {
  int seed = time(0);
  srand(seed);
  int loop = 1;
  if (argc > 1) {
    sscanf(argv[1], "%d", &loop);
  }
  int i;
  for (i = 0; i < loop; i ++) {
    gen_rand_expr();

    sprintf(code_buf, code_format, buf);

    FILE *fp = fopen("/tmp/.code.c", "w");
    assert(fp != NULL);
    fputs(code_buf, fp);
    fclose(fp);

    int ret = system("gcc /tmp/.code.c -o /tmp/.expr");
    if (ret != 0) continue;

    fp = popen("/tmp/.expr", "r");
    assert(fp != NULL);

    int result;
    fscanf(fp, "%d", &result);
    pclose(fp);

    printf("%u %s\n", result, buf);
  }
  return 0;
}
