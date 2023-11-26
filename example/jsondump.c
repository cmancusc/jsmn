#include "../jsmn.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* NOTE: Function realloc_it() is a wrapper function for standard realloc()
 * with one difference - it frees old memory pointer in case of realloc
 * failure. Thus, DO NOT use old data pointer in anyway after call to
 * realloc_it(). If your code has some kind of fallback algorithm if
 * memory can't be re-allocated - use standard realloc() instead.
 */
static inline void *realloc_it(void *ptrmem, size_t size) {
  void *p = realloc(ptrmem, size);
  if (!p) {
    free(ptrmem);
    fprintf(stderr, "realloc(): errno=%d\n", errno);
  }
  return p;
}

/* NOTE:
 * An example of reading JSON from stdin and printing its content to stdout.
 * The output looks like YAML, but I'm not sure if it's really compatible.
 */

static int dump(const char *js, jsmntok_t *t, size_t count, int indent) {
  int i, j, k;
  jsmntok_t *key;
  if (count == 0) {
    return 0;
  }
  if (t->type == JSMN_PRIMITIVE) {
    printf("%.*s", t->end - t->start, js + t->start);
    return 1;
  } else if (t->type == JSMN_STRING) {
    printf("'%.*s'", t->end - t->start, js + t->start);
    return 1;
  } else if (t->type == JSMN_OBJECT) {
    printf("\n");
    j = 0;
    for (i = 0; i < t->size; i++) {
      for (k = 0; k < indent; k++) {
        printf("  ");
      }
      key = t + 1 + j;
      j += dump(js, key, count - j, indent + 1);
      if (key->size > 0) {
        printf(": ");
        j += dump(js, t + 1 + j, count - j, indent + 1);
      }
      printf("\n");
    }
    return j + 1;
  } else if (t->type == JSMN_ARRAY) {
    j = 0;
    printf("\n");
    for (i = 0; i < t->size; i++) {
      for (k = 0; k < indent - 1; k++) {
        printf("  ");
      }
      printf("   - ");
      j += dump(js, t + 1 + j, count - j, indent + 1);
      printf("\n");
    }
    return j + 1;
  }
  return 0;
}


int main(int argc, char ** argv) {
  if (argc != 2) {
    printf("you should provide a json file\n");
    return EXIT_FAILURE;
  }
  int rv;
  int eof_expected = 0;
  char * jsBuf = 0;
  size_t buflen = 0;

  jsmn_parser p;
  jsmntok_t *tok = 0;
  size_t tokcount = 2;

  /* Prepare parser */
  jsmn_init(&p);

  /* Allocate some tokens as a start */
  tok = (jsmntok_t*)malloc(sizeof(*tok) * tokcount);
  if (!tok) {
    fprintf(stderr, "malloc() tok: errno=%d\n", errno);
    return 3;
  }

  // NOTE: open json file provided by args
  FILE * in_file = fopen(argv[1], "rb");
  if (!in_file) {
    fprintf(stderr, "cannot open file %s: errno=%d\n", argv[1], errno);
    return EXIT_FAILURE;
  }
  fseek(in_file, 0, SEEK_END);
  long int lenght = ftell(in_file);
  if (lenght < 0) {
    fprintf(stderr, "ftell(): errno=%d\n", errno);
    return EXIT_FAILURE;
  }
  fseek(in_file, 0, SEEK_SET);

  buflen = (size_t)lenght;
  jsBuf = (char*)malloc(buflen);
  if (!jsBuf) {
    fprintf(stderr, "malloc() buff: errno=%d\n", errno);
    return 3;
  }

  /* Read data from file */
  rv = fread(jsBuf, 1, buflen, in_file);
  if (rv != buflen) {
    fprintf(stderr, "fread(): %d, errno=%d\n", rv, errno);
    return 1;
  }

again:
  rv = jsmn_parse(&p, jsBuf, buflen, tok, tokcount);
  if (rv < 0) {
    if (rv == JSMN_ERROR_NOMEM) {
      tokcount = tokcount * 2;
      tok = (jsmntok_t*)realloc_it(tok, sizeof(*tok) * tokcount);
      if (!tok) {
        return 3;
      }
      goto again;
    }
  } else {
    dump(jsBuf, tok, p.toknext, 0);
    eof_expected = 1;
  }

  fclose(in_file);
  free(jsBuf);
  free(tok);
  return EXIT_SUCCESS;
}
