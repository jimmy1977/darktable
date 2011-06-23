BEGIN {
  FS = ";"
  print "#include <stdlib.h>"
  print "#include <string.h>"
  print "#include <stdio.h>"
  print "#include <gtk/gtk.h>"
  print "#include <gdk/gdkkeysyms.h>"
  print "struct accel{ char *path; char *key; };"
  print "gint compare(gconstpointer a, gconstpointer b){"
  print "  const char *sa = ((struct accel*)a)->path;"
  print "  const char *sb = ((struct accel*)b)->path;"
  print "  return strcmp(sb, sa);"
  print "}"
  print "int main(int argc, char *argv[])"
  print "{"
  print "  GSList *accels = NULL;"
  print "  struct accel *acc;"
  print "  char buffer[1024];"
  print "  char *s, *d, *temp;"
}

/^"[^"]+";[^;]+;/{
  print "  acc = (struct accel*)malloc(sizeof(struct accel));"
  print "  s = temp = strdup(" $1 ");"
  print "  d = buffer;"
  print "  while(*s != '\\0')"
  print "  {"
  print "    if(*s == '<')"
  print "    {"
  print "      *d = '&';d++;*d = 'l';d++;*d = 't';d++;*d = ';';"
  print "    }"
  print "    else if(*s == '>')"
  print "    {"
  print "      *d = '&';d++;*d = 'g';d++;*d = 't';d++;*d = ';';"
  print "    }"
  print "    else"
  print "    {"
  print "      *d = *s;"
  print "    }"
  print "    d++; s++;"
  print "  }"
  print "  *d = '\\0';"
  print "  acc->path = strdup(buffer);"
  print "  free(temp);"
  print "  s = temp = strdup(gtk_accelerator_name(" $2 "," $3 "));"
  print "  d = buffer;"
  print "  while(*s != '\\0')"
  print "  {"
  print "    if(*s == '<')"
  print "    {"
  print "      *d = '&';d++;*d = 'l';d++;*d = 't';d++;*d = ';';"
  print "    }"
  print "    else if(*s == '>')"
  print "    {"
  print "      *d = '&';d++;*d = 'g';d++;*d = 't';d++;*d = ';';"
  print "    }"
  print "    else"
  print "    {"
  print "      *d = *s;"
  print "    }"
  print "    d++; s++;"
  print "  }"
  print "  *d = '\\0';"
  print "  acc->key = strdup(buffer);"
  print "  free(temp);"
  print "  accels = g_slist_insert_sorted(accels, acc, compare);"
}

END {
  print "  printf(\"<informaltable frame=\\\"none\\\"><tgroup cols=\\\"2\\\" colsep=\\\"0\\\" rowsep=\\\"0\\\">\\n\");"
  print "  printf(\"%s\\n\",\"<colspec colwidth=\\\"100%\\\"/>\");"
  print "  printf(\"<colspec colwidth=\\\"5cm\\\"/>\\n\");"
  print "  printf(\"<tbody>\\n\");"
  print "  while(accels){"
  print "    acc = (struct accel*)accels->data;"
  print "    printf(\"<row>\\n\");"
  print "    printf(\"<entry>\\n\");"
  print "    printf(\"%s\\n\", acc->path);"
  print "    printf(\"</entry>\\n\");"
  print "    printf(\"<entry>\\n\");"
  print "    printf(\"%s\\n\", acc->key);"
  print "    printf(\"</entry>\\n\");"
  print "    printf(\"</row>\\n\");"
  print "    accels = g_slist_next(accels);"
  print "  }"
  print "  printf(\"</tbody>\\n\");"
  print "  printf(\"</tgroup>\\n\");"
  print "  printf(\"</informaltable>\\n\");"
  print "  return 0;"
  print "}"
}