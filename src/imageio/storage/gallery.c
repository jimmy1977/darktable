/*
    This file is part of darktable,
    copyright (c) 2009--2011 johannes hanika.

    darktable is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    darktable is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with darktable.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "common/darktable.h"
#include "common/image.h"
#include "common/image_cache.h"
#include "common/imageio_module.h"
#include "common/imageio.h"
#include "common/variables.h"
#include "common/metadata.h"
#include "common/debug.h"
#include "control/control.h"
#include "control/conf.h"
#include "gui/gtk.h"
#include "dtgtk/button.h"
#include "dtgtk/paint.h"
#include <stdio.h>
#include <stdlib.h>
#include <glade/glade.h>

DT_MODULE(1)

// gui data
typedef struct gallery_t
{
  GtkEntry *entry;
}
gallery_t;

// saved params
typedef struct dt_imageio_gallery_t
{
  char filename[1024];
  dt_variables_params_t *vp;
  GList *l;
}
dt_imageio_gallery_t;

// sorted list of all images
typedef struct pair_t
{
  char line[4096];
  int pos;
}
pair_t;


const char*
name ()
{
  return _("website gallery");
}

static void
button_clicked (GtkWidget *widget, dt_imageio_module_storage_t *self)
{
  gallery_t *d = (gallery_t *)self->gui_data;
  GtkWidget *win = glade_xml_get_widget (darktable.gui->main_window, "main_window");
  GtkWidget *filechooser = gtk_file_chooser_dialog_new (_("select directory"),
                           GTK_WINDOW (win),
                           GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
                           GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                           GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                           (char *)NULL);

  gtk_file_chooser_set_select_multiple(GTK_FILE_CHOOSER(filechooser), FALSE);
  gchar *old = g_strdup(gtk_entry_get_text(d->entry));
  char *c = g_strstr_len(old, -1, "$");
  if(c) *c = '\0';
  gtk_file_chooser_set_current_folder(GTK_FILE_CHOOSER(filechooser), old);
  g_free(old);
  if (gtk_dialog_run (GTK_DIALOG (filechooser)) == GTK_RESPONSE_ACCEPT)
  {
    gchar *dir = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (filechooser));
    char composed[1024];
    snprintf(composed, 1024, "%s/$(FILE_NAME)", dir);
    gtk_entry_set_text(GTK_ENTRY(d->entry), composed);
    dt_conf_set_string("plugins/imageio/storage/gallery/file_directory", composed);
    g_free(dir);
  }
  gtk_widget_destroy (filechooser);
}

void
gui_init (dt_imageio_module_storage_t *self)
{
  gallery_t *d = (gallery_t *)malloc(sizeof(gallery_t));
  self->gui_data = (void *)d;
  self->widget = gtk_hbox_new(FALSE, 5);
  GtkWidget *widget;

  widget = gtk_entry_new();
  gtk_box_pack_start(GTK_BOX(self->widget), widget, TRUE, TRUE, 0);
  gchar *dir = dt_conf_get_string("plugins/imageio/storage/gallery/file_directory");
  if(dir)
  {
    gtk_entry_set_text(GTK_ENTRY(widget), dir);
    g_free(dir);
  }
  d->entry = GTK_ENTRY(widget);
  dt_gui_key_accel_block_on_focus (GTK_WIDGET (d->entry));
  g_object_set(G_OBJECT(widget), "tooltip-text", _("enter the path where to create the website gallery:\n"
                 "$(ROLL_NAME) - roll of the input image\n"
                 "$(FILE_DIRECTORY) - directory of the input image\n"
                 "$(FILE_NAME) - basename of the input image\n"
                 "$(FILE_EXTENSION) - extension of the input image\n"
                 "$(SEQUENCE) - sequence number\n"
                 "$(YEAR) - year\n"
                 "$(MONTH) - month\n"
                 "$(DAY) - day\n"
                 "$(HOUR) - hour\n"
                 "$(MINUTE) - minute\n"
                 "$(SECOND) - second\n"
                 "$(EXIF_YEAR) - exif year\n"
                 "$(EXIF_MONTH) - exif month\n"
                 "$(EXIF_DAY) - exif day\n"
                 "$(EXIF_HOUR) - exif hour\n"
                 "$(EXIF_MINUTE) - exif minute\n"
                 "$(EXIF_SECOND) - exif second\n"
                 "$(PICTURES_FOLDER) - pictures folder\n"
                 "$(HOME_FOLDER) - home folder\n"
                 "$(DESKTOP_FOLDER) - desktop folder"
                                                      ), (char *)NULL);
  widget = dtgtk_button_new(dtgtk_cairo_paint_directory, 0);
  gtk_widget_set_size_request(widget, 18, 18);
  g_object_set(G_OBJECT(widget), "tooltip-text", _("select directory"), (char *)NULL);
  gtk_box_pack_start(GTK_BOX(self->widget), widget, FALSE, FALSE, 0);
  g_signal_connect(G_OBJECT(widget), "clicked", G_CALLBACK(button_clicked), self);
}

void
gui_cleanup (dt_imageio_module_storage_t *self)
{
  free(self->gui_data);
}

void
gui_reset (dt_imageio_module_storage_t *self)
{
  gallery_t *d = (gallery_t *)self->gui_data;
  dt_conf_set_string("plugins/imageio/storage/gallery/file_directory", gtk_entry_get_text(d->entry));
}

static gint
sort_pos(pair_t *a, pair_t *b)
{
  return a->pos - b->pos;
}

int
store (dt_imageio_module_data_t *sdata, const int imgid, dt_imageio_module_format_t *format, dt_imageio_module_data_t *fdata, const int num, const int total)
{
  dt_image_t *img = dt_image_cache_get(imgid, 'r');
  if(!img) return 1;
  dt_imageio_gallery_t *d = (dt_imageio_gallery_t *)sdata;

  char filename[1024]= {0};
  char dirname[1024]= {0};
  dt_image_full_path(img->id, dirname, 1024);
  // we're potentially called in parallel. have sequence number synchronized:
  dt_pthread_mutex_lock(&darktable.plugin_threadsafe);
  {

    // if filenamepattern is a directory just let att ${FILE_NAME} as default..
    if ( g_file_test(d->filename, G_FILE_TEST_EXISTS | G_FILE_TEST_IS_DIR) || ((d->filename+strlen(d->filename))[0]=='/' || (d->filename+strlen(d->filename))[0]=='\\') )
      snprintf (d->filename+strlen(d->filename), 1024-strlen(d->filename), "$(FILE_NAME)");

    // avoid braindead export which is bound to overwrite at random:
    if(total > 1 && !g_strrstr(d->filename, "$"))
    {
      snprintf(d->filename+strlen(d->filename), 1024-strlen(d->filename), "_$(SEQUENCE)");
    }


    d->vp->filename = dirname;
    d->vp->jobcode = "export";
    d->vp->img = img;
    d->vp->sequence = num;
    dt_variables_expand(d->vp, d->filename, TRUE);
    strncpy(filename, dt_variables_get_result(d->vp), 1024);
    strncpy(dirname, filename, 1024);

    const char *ext = format->extension(fdata);
    char *c = dirname + strlen(dirname);
    for(; c>dirname && *c != '/'; c--);
    if(*c == '/') *c = '\0';
    if(g_mkdir_with_parents(dirname, 0755))
    {
      fprintf(stderr, "[imageio_storage_gallery] could not create directory: `%s'!\n", dirname);
      dt_control_log(_("could not create directory `%s'!"), dirname);
      dt_image_cache_release(img, 'r');
      dt_pthread_mutex_unlock(&darktable.plugin_threadsafe);
      return 1;
    }

    c = filename + strlen(filename);
    for(; c>filename && *c != '.' && *c != '/' ; c--);
    if(c <= filename || *c=='/') c = filename + strlen(filename);

    sprintf(c,".%s",ext);

    // save image to list, in order:
    pair_t *pair = malloc(sizeof(pair_t));
    
    char *title = NULL, *description = NULL;
    char query[1024];
    sqlite3_stmt *stmt;
    snprintf(query, 1024, "select value from meta_data where id = %d and key = %d", img->id, DT_METADATA_XMP_DC_TITLE);
    DT_DEBUG_SQLITE3_PREPARE_V2(darktable.db, query, -1, &stmt, NULL);
    if(sqlite3_step(stmt) == SQLITE_ROW) title = g_strdup((char *)sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);
    snprintf(query, 1024, "select value from meta_data where id = %d and key = %d", img->id, DT_METADATA_XMP_DC_DESCRIPTION);
    DT_DEBUG_SQLITE3_PREPARE_V2(darktable.db, query, -1, &stmt, NULL);
    if(sqlite3_step(stmt) == SQLITE_ROW) description = g_strdup((char *)sqlite3_column_text(stmt, 0));
    sqlite3_finalize(stmt);

    // TODO: tags, too!

    char relfilename[256], relthumbfilename[256];
    c = filename + strlen(filename);
    for(; c>filename && *c != '/' ; c--);
    if(*c == '/') c++;
    if(c <= filename) c = filename;
    snprintf(relfilename, 256, "%s", c);
    snprintf(relthumbfilename, 256, "%s", relfilename);
    c = relthumbfilename + strlen(relthumbfilename);
    for(; c>relthumbfilename && *c != '.'; c--);
    if(c <= relthumbfilename) c = relthumbfilename + strlen(relthumbfilename);
    sprintf(c, "-thumb.%s", ext);

    snprintf(pair->line, 4096,
        "\n"
        "      <div><a href=\"%s\"><span></span><img src=\"%s\" alt=\"img%d\" class=\"img\"/></a>\n"
        "      <h1>%s</h1>\n"
        "      %s</div>\n", relfilename, relthumbfilename, num, title?title:"&nbsp;", description?description:"&nbsp;");
    pair->pos = num;
    g_free(title);
    g_free(description);
    d->l = g_list_insert_sorted(d->l, pair, (GCompareFunc)sort_pos);
  } // end of critical block
  dt_pthread_mutex_unlock(&darktable.plugin_threadsafe);

  /* export image to file */
  dt_imageio_export(img, filename, format, fdata);
  /* also export thumbnail: */
  // write with reduced resolution:
  const int max_width  = fdata->max_width;
  const int max_height = fdata->max_height;
  fdata->max_width  = 200;
  fdata->max_height = 200;
  // alter filename with -thumb:
  char *c = filename + strlen(filename);
  for(; c>filename && *c != '.' && *c != '/' ; c--);
  if(c <= filename || *c=='/') c = filename + strlen(filename);
  const char *ext = format->extension(fdata);
  sprintf(c,"-thumb.%s",ext);
  dt_imageio_export(img, filename, format, fdata);
  // restore for next image:
  fdata->max_width = max_width;
  fdata->max_height = max_height;
  dt_image_cache_release(img, 'r');

  printf("[export_job] exported to `%s'\n", filename);
  char *trunc = filename + strlen(filename) - 32;
  if(trunc < filename) trunc = filename;
  dt_control_log(_("%d/%d exported to `%s%s'"), num, total, trunc != filename ? ".." : "", trunc);
  return 0;
}

static void
copy_res(const char *src, const char *dst)
{
  char share[1024];
  dt_get_datadir(share, 1024);
  gchar *sourcefile = g_build_filename(share, src, NULL);
  FILE *fin = fopen(sourcefile, "rb");
  FILE *fout = fopen(dst, "wb");
  if(fin && fout)
  {
    fseek(fin,0,SEEK_END);
    int end = ftell(fin);
    rewind(fin);
    char *content = (char*)g_malloc(sizeof(char)*end);
    if(content == NULL)
      goto END;
    if(fread(content,sizeof(char),end,fin) != end)
      goto END;
    if(fwrite(content,sizeof(char),end,fout) != end)
      goto END;
    g_free(content);
    fclose(fout);
    fclose(fin);
  }
END: 
  g_free(sourcefile);
}

void
finalize_store(dt_imageio_module_storage_t *self, void *dd)
{
  dt_imageio_gallery_t *d = (dt_imageio_gallery_t *)dd;
  char filename[1024];
  snprintf(filename, 1024, "%s", d->filename);
  char *c = filename + strlen(filename);
  for(; c>filename && *c != '/' ; c--);
  if(c <= filename) c = filename + strlen(filename);

  // also create style/ subdir:
  sprintf(c, "/style");
  g_mkdir_with_parents(filename, 0755);
  sprintf(c, "/style/style.css");
  copy_res("/style/style.css", filename);
  sprintf(c, "/style/favicon.ico");
  copy_res("/style/favicon.ico", filename);

  sprintf(c, "/index.html");

  // TODO: textbox for this!
  const char *title = "wellington";

  FILE *f = fopen(filename, "wb");
  if(!f) return;
  fprintf(f,
"<!DOCTYPE html PUBLIC \"-//W3C//DTD XHTML 1.0 Transitional//EN\" \"http://www.w3.org/TR/xhtml1/DTD/xhtml1-transitional.dtd\">\n"
"<html xmlns=\"http://www.w3.org/1999/xhtml\">\n"
"  <head>\n"
"    <meta http-equiv=\"Content-type\" content=\"text/html;charset=UTF-8\" />\n"
"    <link rel=\"shortcut icon\" href=\"style/favicon.ico\" />\n"
"    <link rel=\"stylesheet\" href=\"style/style.css\" type=\"text/css\" />\n"
"    <title>%s</title>\n"
"  </head>\n"
"  <body>\n"
"    <div class=\"title\">%s</div>\n"
"    <div class=\"page\">\n",
      title, title);

  while(d->l)
  {
    pair_t *p = (pair_t *)d->l->data;
    fprintf(f, "%s", p->line);
    free(p);
    d->l = g_list_delete_link(d->l, d->l);
  }
  
  fprintf(f,
"        <p style=\"clear:both;\"></p>\n"
"    </div>\n"
"    <div class=\"footer\">\n"
"    created with darktable "PACKAGE_VERSION"\n"
"    </div>\n"
"  </body>\n"
"</html>\n"
  );
  fclose(f);
}

void*
get_params(dt_imageio_module_storage_t *self, int* size)
{
  dt_imageio_gallery_t *d = (dt_imageio_gallery_t *)malloc(sizeof(dt_imageio_gallery_t));
  memset(d, 0, sizeof(dt_imageio_gallery_t));
  // have to return the size of the struct to store (i.e. without all the variable pointers at the end)
  *size = sizeof(dt_imageio_gallery_t) - 2*sizeof(void *);
  gallery_t *g = (gallery_t *)self->gui_data;
  d->vp = NULL;
  d->l = NULL;
  dt_variables_params_init(&d->vp);
  const char *text = gtk_entry_get_text(GTK_ENTRY(g->entry));
  strncpy(d->filename, text, 1024);
  dt_conf_set_string("plugins/imageio/storage/gallery/file_directory", d->filename);
  return d;
}

void
free_params(dt_imageio_module_storage_t *self, void *params)
{
  dt_imageio_gallery_t *d = (dt_imageio_gallery_t *)params;
  dt_variables_params_destroy(d->vp);
  free(params);
}

int
set_params(dt_imageio_module_storage_t *self, void *params, int size)
{
  if(size != sizeof(dt_imageio_gallery_t) - 2*sizeof(void *)) return 1;
  dt_imageio_gallery_t *d = (dt_imageio_gallery_t *)params;
  gallery_t *g = (gallery_t *)self->gui_data;
  gtk_entry_set_text(GTK_ENTRY(g->entry), d->filename);
  dt_conf_set_string("plugins/imageio/storage/gallery/file_directory", d->filename);
  return 0;
}
