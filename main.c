#include <gtk/gtk.h>
#include <curl/curl.h>
#include "ui.h"

static void activate(GtkApplication *app, gpointer user_data) {
    (void)user_data;
    build_ui(app);
}

int main(int argc, char **argv) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    GtkApplication *app = gtk_application_new("com.example.alicloudspend",
                                              G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activate), NULL);
    int status = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);

    curl_global_cleanup();
    return status;
}