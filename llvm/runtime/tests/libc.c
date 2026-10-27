#include "t.h"
#include <ctype.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* The newer runtime pieces: scanf, strtod, string helpers and FILE
 * streams (on fpga_cpu, through the file service). */
int main(void)
{
    int a = 0, b = 0, c = 0, n = 0;
    long l = 0;
    unsigned u = 0;
    char s1[40], s2[40];
    float f = 0;

    PI("sscanf1", sscanf("  0x1F 017 -42", " %x %i %d", &a, &b, &c));
    PI("a", a); PI("b", b); PI("c", c);
    PI("sscanf2", sscanf("key   some value\n", "%39s %39[^\n]", s1, s2));
    printf("[%s] [%s]\n", s1, s2);
    PI("sscanf3", sscanf("123456 7", "%ld %n%u", &l, &n, &u));
    PI("l", l); PI("n", n); PI("u", u);
    PI("sscanf4", sscanf("abc", "%d", &a));
    PI("sscanf5", sscanf("", "%d", &a));
    PI("sscanf6", sscanf("2.5e1", "%f", &f));
    PF("f", f);
    PF("strtod", strtod("  -3.25e-2xyz", NULL));
    PI("strcasecmp", strcasecmp("Hello", "hELLO"));
    PI("strncasecmp", strncasecmp("abcX", "ABCy", 3) == 0);
    char *d = strdup("duplicate");
    printf("%s %d\n", d, (int)strlen(d));
    free(d);
    PI("strspn", (long)strspn("aabbc", "ab"));
    PI("strcspn", (long)strcspn("hello world", " "));
    char tok[] = "a,b,,c";
    for (char *t = strtok(tok, ","); t; t = strtok(NULL, ","))
        printf("tok %s\n", t);

    /* files */
    FILE *fp = fopen("TIO.DAT", "wb");
    PI("open-w", fp != NULL);
    for (int i = 0; i < 100; i++)
        fprintf(fp, "%d,", i * 7);
    fputs("end\n", fp);
    PI("tell-w", ftell(fp));
    fclose(fp);
    fp = fopen("TIO.DAT", "rb");
    PI("open-r", fp != NULL);
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    PI("size", size);
    fseek(fp, 10, SEEK_SET);
    char buf[16] = { 0 };
    PI("read", (long)fread(buf, 1, 8, fp));
    printf("[%s]\n", buf);
    PI("tell-r", ftell(fp));
    fseek(fp, -4, SEEK_END);
    PI("getc", fgetc(fp));
    PI("ungetc", ungetc('Z', fp));
    PI("getc2", fgetc(fp));
    fseek(fp, 0, SEEK_SET);
    int v1 = 0, v2 = 0;
    PI("fscanf", fscanf(fp, "%d,%d,", &v1, &v2));
    PI("v1", v1); PI("v2", v2);
    rewind(fp);
    char line[600];
    PI("fgets", fgets(line, sizeof line, fp) != NULL);
    PI("linelen", (long)strlen(line));
    PI("eof-before", feof(fp));
    PI("fgetc-eof", fgetc(fp));
    PI("eof-after", feof(fp) != 0);
    fclose(fp);
    PI("remove", remove("TIO.DAT"));
    errno = 0;
    PI("open-missing", fopen("TIO.DAT", "rb") == NULL);
    PI("errno", errno == ENOENT);
    return 0;
}
