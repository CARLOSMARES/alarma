#include "ui.h"
#include "../model/alarma.h"
#include <gtk/gtk.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <ao/ao.h>
#include <sndfile.h>
#include <pthread.h>
#include <time.h>

// Variables globales para la UI
static Alarma alarmas[MAX_ALARMAS];
static int num_alarmas = 0;
static GtkWidget *lista_alarmas_box = NULL;
static GtkWidget *reloj_label = NULL;

// Prototipos de funciones auxiliares y callbacks
static void refrescar_lista_alarmas();
static void detener_alarma();
static void actualizar_reloj(gpointer data);
static void on_btn_agregar_clicked(GtkButton *btn, gpointer user_data);
static void on_btn_editar_clicked(GtkButton *btn, gpointer user_data);
static void on_btn_eliminar_clicked(GtkButton *btn, gpointer user_data);
static void mostrar_dialogo_alarma(GtkWindow *parent, Alarma *alarma, int editar);
gboolean on_switch_activada(GtkSwitch *sw, gboolean state, gpointer user_data);
void on_editar_clicked(GtkButton *btn, gpointer user_data);
void on_eliminar_clicked(GtkButton *btn, gpointer user_data);

// Función para reproducir sonido de alarma
// Reproducción de sonido con libao/libsndfile
static ao_device *ao_dev = NULL;
static short *audio_buffer = NULL;
static sf_count_t audio_frames = 0;
static int audio_channels = 0;
static int audio_samplerate = 0;

static pthread_t hilo_alarma;
static int alarma_sonando = 0;

void *hilo_sonar_alarma(void *arg)
{
    SF_INFO sfinfo;
    SNDFILE *sndfile = sf_open("alarma.wav", SFM_READ, &sfinfo);
    if (!sndfile)
    {
        fprintf(stderr, "[ERROR] No se pudo abrir alarma.wav\n");
        GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "No se pudo abrir alarma.wav");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        alarma_sonando = 0;
        return NULL;
    }
    sf_count_t frames = sfinfo.frames;
    int channels = sfinfo.channels;
    int samplerate = sfinfo.samplerate;
    short *buffer = malloc(frames * channels * sizeof(short));
    if (!buffer)
    {
        sf_close(sndfile);
        fprintf(stderr, "[ERROR] No hay memoria para el buffer de audio\n");
        GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "No hay memoria para el buffer de audio");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        alarma_sonando = 0;
        return NULL;
    }
    sf_count_t leidos = sf_read_short(sndfile, buffer, frames * channels);
    sf_close(sndfile);
    if (leidos != frames * channels)
    {
        fprintf(stderr, "[ERROR] No se leyeron todos los datos de audio\n");
        GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "No se leyeron todos los datos de audio");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
        free(buffer);
        alarma_sonando = 0;
        return NULL;
    }

    ao_sample_format format;
    ao_initialize();
    int driver = ao_default_driver_id();
    format.bits = 16;
    format.channels = channels;
    format.rate = samplerate;
    format.byte_format = AO_FMT_NATIVE;
    format.matrix = 0;
    ao_device *dev = ao_open_live(driver, &format, NULL);
    if (dev)
    {
        // Reproducir en bucle hasta que se apague la alarma
        while (alarma_sonando)
        {
            int ok = ao_play(dev, (char *)buffer, frames * channels * sizeof(short));
            if (!ok)
            {
                fprintf(stderr, "[ERROR] Fallo la reproducción de audio con libao\n");
                GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "Fallo la reproducción de audio con libao");
                gtk_dialog_run(GTK_DIALOG(dialog));
                gtk_widget_destroy(dialog);
                break;
            }
        }
        ao_close(dev);
    }
    else
    {
        fprintf(stderr, "[ERROR] No se pudo abrir el dispositivo de audio (libao)\n");
        GtkWidget *dialog = gtk_message_dialog_new(NULL, GTK_DIALOG_MODAL, GTK_MESSAGE_ERROR, GTK_BUTTONS_OK, "No se pudo abrir el dispositivo de audio (libao)");
        gtk_dialog_run(GTK_DIALOG(dialog));
        gtk_widget_destroy(dialog);
    }
    ao_shutdown();
    free(buffer);
    alarma_sonando = 0;
    return NULL;
}

void sonar_alarma()
{
    if (alarma_sonando)
        return;
    alarma_sonando = 1;
    pthread_create(&hilo_alarma, NULL, hilo_sonar_alarma, NULL);

    // Buscar la alarma activa (la que está sonando)
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char hora_actual[6];
    snprintf(hora_actual, sizeof(hora_actual), "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
    int dia_semana = tm_info->tm_wday;
    for (int i = 0; i < num_alarmas; i++)
    {
        int dias_marcados = 0;
        for (int d = 0; d < 7; d++)
            if (alarmas[i].dias[d])
                dias_marcados = 1;
        int suena_hoy = (dias_marcados == 0) || alarmas[i].dias[dia_semana];
        if (alarmas[i].activa && strcmp(alarmas[i].hora, hora_actual) == 0 && suena_hoy)
        {
            // Mostrar diálogo modal para detener la alarma
            GtkWidget *dialog = gtk_dialog_new_with_buttons(
                "¡Alarma!",
                NULL,
                GTK_DIALOG_MODAL,
                "Detener alarma", GTK_RESPONSE_ACCEPT,
                NULL);
            GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));
            GtkWidget *label = gtk_label_new("\n¡La alarma está sonando!\n");
            gtk_box_pack_start(GTK_BOX(content), label, TRUE, TRUE, 0);
            gtk_widget_show_all(dialog);
            gtk_dialog_run(GTK_DIALOG(dialog));
            detener_alarma();
            gtk_widget_destroy(dialog);
            // Si no tiene repeticiones, desactivar y guardar
            if (!dias_marcados)
            {
                alarmas[i].activa = 0;
                guardar_alarmas(alarmas, num_alarmas);
                refrescar_lista_alarmas();
            }
            break; // Solo una alarma a la vez
        }
    }
}

static void detener_alarma()
{
    if (alarma_sonando)
    {
        pthread_cancel(hilo_alarma);
        pthread_join(hilo_alarma, NULL);
        alarma_sonando = 0;
    }
}

// Prototipos de funciones auxiliares y callbacks
static void refrescar_lista_alarmas();
static void detener_alarma();
static void actualizar_reloj(gpointer data);
static void on_btn_agregar_clicked(GtkButton *btn, gpointer user_data);
static void on_btn_editar_clicked(GtkButton *btn, gpointer user_data);
static void on_btn_eliminar_clicked(GtkButton *btn, gpointer user_data);
static void mostrar_dialogo_alarma(GtkWindow *parent, Alarma *alarma, int editar);
gboolean on_switch_activada(GtkSwitch *sw, gboolean state, gpointer user_data);
void on_editar_clicked(GtkButton *btn, gpointer user_data);
void on_eliminar_clicked(GtkButton *btn, gpointer user_data);

// --- Actualización del reloj digital ---
static gboolean timer_reloj(gpointer data)
{
    actualizar_reloj(data);
    return TRUE;
}

static void actualizar_reloj(gpointer data)
{
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    char hora[16];
    snprintf(hora, sizeof(hora), "%02d:%02d:%02d", tm_info->tm_hour, tm_info->tm_min, tm_info->tm_sec);
    gtk_label_set_text(GTK_LABEL(reloj_label), hora);

    // Revisar alarmas activas
    char hora_actual[6];
    snprintf(hora_actual, sizeof(hora_actual), "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);
    int dia_semana = tm_info->tm_wday;
    for (int i = 0; i < num_alarmas; i++)
    {
        int dias_marcados = 0;
        for (int d = 0; d < 7; d++)
            if (alarmas[i].dias[d])
                dias_marcados = 1;
        int suena_hoy = (dias_marcados == 0) || alarmas[i].dias[dia_semana];
        if (alarmas[i].activa && strcmp(alarmas[i].hora, hora_actual) == 0 && suena_hoy)
        {
            sonar_alarma();
        }
    }
}

// --- Refrescar lista de alarmas en la UI ---
static void refrescar_lista_alarmas()
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(lista_alarmas_box));
    for (GList *l = children; l != NULL; l = l->next)
        gtk_widget_destroy(GTK_WIDGET(l->data));
    g_list_free(children);

    for (int i = 0; i < num_alarmas; i++)
    {
        char texto[128];
        snprintf(texto, sizeof(texto), "%s  |  %s%s", alarmas[i].hora, alarmas[i].nombre, alarmas[i].activa ? " [Activa]" : "");
        GtkWidget *hbox = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
        gtk_widget_set_margin_top(hbox, 4);
        gtk_widget_set_margin_bottom(hbox, 4);
        gtk_widget_set_margin_start(hbox, 8);
        gtk_widget_set_margin_end(hbox, 8);

        GtkWidget *lbl = gtk_label_new(texto);
        PangoAttrList *attrs = pango_attr_list_new();
        pango_attr_list_insert(attrs, pango_attr_size_new(13 * PANGO_SCALE));
        gtk_label_set_attributes(GTK_LABEL(lbl), attrs);
        pango_attr_list_unref(attrs);
        gtk_box_pack_start(GTK_BOX(hbox), lbl, TRUE, TRUE, 0);

        GtkWidget *btn_editar = gtk_button_new();
        GtkWidget *img_edit = gtk_image_new_from_icon_name("document-edit", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(btn_editar), img_edit);
        gtk_widget_set_tooltip_text(btn_editar, "Editar alarma");
        g_object_set_data(G_OBJECT(btn_editar), "alarma_id", GINT_TO_POINTER(alarmas[i].id));
        g_signal_connect(btn_editar, "clicked", G_CALLBACK(on_btn_editar_clicked), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), btn_editar, FALSE, FALSE, 0);

        GtkWidget *btn_eliminar = gtk_button_new();
        GtkWidget *img_del = gtk_image_new_from_icon_name("edit-delete", GTK_ICON_SIZE_BUTTON);
        gtk_button_set_image(GTK_BUTTON(btn_eliminar), img_del);
        gtk_widget_set_tooltip_text(btn_eliminar, "Eliminar alarma");
        g_object_set_data(G_OBJECT(btn_eliminar), "alarma_id", GINT_TO_POINTER(alarmas[i].id));
        g_signal_connect(btn_eliminar, "clicked", G_CALLBACK(on_btn_eliminar_clicked), NULL);
        gtk_box_pack_start(GTK_BOX(hbox), btn_eliminar, FALSE, FALSE, 0);

        gtk_box_pack_start(GTK_BOX(lista_alarmas_box), hbox, FALSE, FALSE, 0);
    }
    gtk_widget_show_all(lista_alarmas_box);
}

// --- Diálogo para agregar/editar alarma ---
static void mostrar_dialogo_alarma(GtkWindow *parent, Alarma *alarma, int editar)
{
    GtkWidget *dialog = gtk_dialog_new_with_buttons(
        editar ? "Editar alarma" : "Agregar alarma",
        parent,
        GTK_DIALOG_MODAL,
        (editar ? "Guardar" : "Agregar"), GTK_RESPONSE_ACCEPT,
        "Cancelar", GTK_RESPONSE_REJECT,
        NULL);
    GtkWidget *content = gtk_dialog_get_content_area(GTK_DIALOG(dialog));

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 6);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 6);
    gtk_container_add(GTK_CONTAINER(content), grid);

    GtkWidget *lbl_nombre = gtk_label_new("Nombre:");
    GtkWidget *entry_nombre = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry_nombre), MAX_NOMBRE - 1);
    if (editar)
        gtk_entry_set_text(GTK_ENTRY(entry_nombre), alarma->nombre);
    gtk_grid_attach(GTK_GRID(grid), lbl_nombre, 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_nombre, 1, 0, 2, 1);

    GtkWidget *lbl_hora = gtk_label_new("Hora (HH:MM):");
    GtkWidget *entry_hora = gtk_entry_new();
    gtk_entry_set_max_length(GTK_ENTRY(entry_hora), 5);
    if (editar)
        gtk_entry_set_text(GTK_ENTRY(entry_hora), alarma->hora);
    gtk_grid_attach(GTK_GRID(grid), lbl_hora, 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), entry_hora, 1, 1, 2, 1);

    GtkWidget *lbl_activa = gtk_label_new("Activa:");
    GtkWidget *switch_activa = gtk_switch_new();
    gtk_switch_set_active(GTK_SWITCH(switch_activa), editar ? alarma->activa : 1);
    GtkWidget *hbox_activa = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_box_pack_start(GTK_BOX(hbox_activa), switch_activa, FALSE, FALSE, 0);
    gtk_grid_attach(GTK_GRID(grid), lbl_activa, 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), hbox_activa, 1, 2, 2, 1);

    GtkWidget *lbl_dias = gtk_label_new("Días:");
    gtk_grid_attach(GTK_GRID(grid), lbl_dias, 0, 3, 1, 1);
    GtkWidget *dias_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 2);
    const char *dias_txt[7] = {"D", "L", "M", "M", "J", "V", "S"};
    GtkWidget *dias_chk[7];
    for (int d = 0; d < 7; d++)
    {
        dias_chk[d] = gtk_check_button_new_with_label(dias_txt[d]);
        if (editar && alarma->dias[d])
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dias_chk[d]), TRUE);
        else if (!editar && d != 0)
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(dias_chk[d]), TRUE); // por defecto activa L-V
        gtk_box_pack_start(GTK_BOX(dias_box), dias_chk[d], FALSE, FALSE, 0);
    }
    gtk_grid_attach(GTK_GRID(grid), dias_box, 1, 3, 2, 1);

    gtk_widget_show_all(dialog);
    int resp = gtk_dialog_run(GTK_DIALOG(dialog));
    if (resp == GTK_RESPONSE_ACCEPT)
    {
        const char *nombre = gtk_entry_get_text(GTK_ENTRY(entry_nombre));
        const char *hora = gtk_entry_get_text(GTK_ENTRY(entry_hora));
        int activa = gtk_switch_get_active(GTK_SWITCH(switch_activa));
        int dias[7];
        for (int d = 0; d < 7; d++)
            dias[d] = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(dias_chk[d]));
        if (editar)
        {
            actualizar_alarma(alarmas, num_alarmas, alarma->id, nombre, hora, activa, dias);
        }
        else
        {
            agregar_alarma(alarmas, &num_alarmas, nombre, hora, dias);
        }
        guardar_alarmas(alarmas, num_alarmas);
        refrescar_lista_alarmas();
    }
    gtk_widget_destroy(dialog);
}

// --- Callbacks de botones ---
static void on_btn_agregar_clicked(GtkButton *btn, gpointer user_data)
{
    Alarma nueva = {0};
    mostrar_dialogo_alarma(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(btn))), &nueva, 0);
}

static void on_btn_editar_clicked(GtkButton *btn, gpointer user_data)
{
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "alarma_id"));
    Alarma *a = buscar_alarma_por_id(alarmas, num_alarmas, id);
    if (a)
        mostrar_dialogo_alarma(GTK_WINDOW(gtk_widget_get_toplevel(GTK_WIDGET(btn))), a, 1);
}

static void on_btn_eliminar_clicked(GtkButton *btn, gpointer user_data)
{
    int id = GPOINTER_TO_INT(g_object_get_data(G_OBJECT(btn), "alarma_id"));
    eliminar_alarma(alarmas, &num_alarmas, id);
    guardar_alarmas(alarmas, num_alarmas);
    refrescar_lista_alarmas();
}

// --- Implementación de la función principal de la UI ---
void iniciar_ui()
{
    gtk_init(NULL, NULL);
    num_alarmas = cargar_alarmas(alarmas, MAX_ALARMAS);

    GtkWidget *ventana = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(ventana), "AlarmaCIOM");
    gtk_window_set_default_size(GTK_WINDOW(ventana), 480, 520);
    gtk_window_set_position(GTK_WINDOW(ventana), GTK_WIN_POS_CENTER);
    gtk_window_set_icon_from_file(GTK_WINDOW(ventana), "icono.png", NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(ventana), vbox);

    // Encabezado
    GtkWidget *header = gtk_label_new("<span size='xx-large' weight='bold' foreground='#2E86C1'>⏰ AlarmaCIOM</span>");
    gtk_label_set_use_markup(GTK_LABEL(header), TRUE);
    gtk_widget_set_margin_top(header, 18);
    gtk_widget_set_margin_bottom(header, 8);
    gtk_box_pack_start(GTK_BOX(vbox), header, FALSE, FALSE, 0);

    // Reloj digital grande y centrado
    reloj_label = gtk_label_new("");
    gtk_widget_set_halign(reloj_label, GTK_ALIGN_CENTER);
    gtk_widget_set_margin_top(reloj_label, 10);
    gtk_widget_set_margin_bottom(reloj_label, 18);
    PangoAttrList *attrs = pango_attr_list_new();
    pango_attr_list_insert(attrs, pango_attr_size_new(32 * PANGO_SCALE));
    pango_attr_list_insert(attrs, pango_attr_weight_new(PANGO_WEIGHT_BOLD));
    gtk_label_set_attributes(GTK_LABEL(reloj_label), attrs);
    pango_attr_list_unref(attrs);
    gtk_box_pack_start(GTK_BOX(vbox), reloj_label, FALSE, FALSE, 0);

    // Separador visual
    GtkWidget *sep = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_box_pack_start(GTK_BOX(vbox), sep, FALSE, FALSE, 0);

    // Lista de alarmas
    lista_alarmas_box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    gtk_widget_set_margin_top(lista_alarmas_box, 10);
    gtk_widget_set_margin_bottom(lista_alarmas_box, 10);
    gtk_box_pack_start(GTK_BOX(vbox), lista_alarmas_box, TRUE, TRUE, 0);

    // Botón agregar con icono y color
    GtkWidget *btn_agregar = gtk_button_new();
    GtkWidget *img_add = gtk_image_new_from_icon_name("list-add", GTK_ICON_SIZE_BUTTON);
    gtk_button_set_image(GTK_BUTTON(btn_agregar), img_add);
    gtk_button_set_label(GTK_BUTTON(btn_agregar), "Agregar alarma");
    gtk_widget_set_name(btn_agregar, "btn_agregar");
    gtk_box_pack_start(GTK_BOX(vbox), btn_agregar, FALSE, FALSE, 0);
    g_signal_connect(btn_agregar, "clicked", G_CALLBACK(on_btn_agregar_clicked), NULL);

    // CSS para colores y estilos
    GtkCssProvider *css = gtk_css_provider_new();
    gtk_css_provider_load_from_data(css,
                                    "#btn_agregar { background: #27ae60; color: #fff; font-weight: bold; border-radius: 8px; padding: 8px 16px; }\n"
                                    "#btn_agregar:hover { background: #229954; }\n"
                                    "button { border-radius: 6px; }\n"
                                    "button image { margin-right: 4px; }\n",
                                    -1, NULL);
    GtkStyleContext *context = gtk_widget_get_style_context(btn_agregar);
    gtk_style_context_add_provider(context, GTK_STYLE_PROVIDER(css), GTK_STYLE_PROVIDER_PRIORITY_USER);

    g_signal_connect(ventana, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    refrescar_lista_alarmas();

    g_timeout_add(1000, timer_reloj, NULL);

    gtk_widget_show_all(ventana);
    gtk_main();
}
