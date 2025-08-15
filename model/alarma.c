#include "alarma.h"
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>

#define CONFIG_DIR "/.config/alarmaciom/"
#define CONFIG_FILE_NAME ".alarma.config"

static void get_config_path(char *path, size_t size)
{
    const char *home = getenv("HOME");
    snprintf(path, size, "%s%s%s", home, CONFIG_DIR, CONFIG_FILE_NAME);
}

static void ensure_config_dir()
{
    char dir[512];
    const char *home = getenv("HOME");
    snprintf(dir, sizeof(dir), "%s%s", home, CONFIG_DIR);
    struct stat st = {0};
    if (stat(dir, &st) == -1)
    {
        mkdir(dir, 0700);
    }
}

int cargar_alarmas(Alarma alarmas[], int max_alarmas)
{
    char path[512];
    get_config_path(path, sizeof(path));
    FILE *f = fopen(path, "r");
    if (!f)
        return 0;
    int count = 0;
    while (count < max_alarmas)
    {
        int dias[7];
        int r = fscanf(f, "%d,%63[^,],%5[^,],%d,%d,%d,%d,%d,%d,%d,%d\n",
                       &alarmas[count].id, alarmas[count].nombre, alarmas[count].hora, &alarmas[count].activa,
                       &dias[0], &dias[1], &dias[2], &dias[3], &dias[4], &dias[5], &dias[6]);
        if (r != 11)
            break;
        for (int d = 0; d < 7; d++)
            alarmas[count].dias[d] = dias[d];
        count++;
    }
    fclose(f);
    return count;
}

int guardar_alarmas(Alarma alarmas[], int num_alarmas)
{
    ensure_config_dir();
    char path[512];
    get_config_path(path, sizeof(path));
    FILE *f = fopen(path, "w");
    if (!f)
        return -1;
    for (int i = 0; i < num_alarmas; i++)
    {
        fprintf(f, "%d,%s,%s,%d,%d,%d,%d,%d,%d,%d,%d\n",
                alarmas[i].id, alarmas[i].nombre, alarmas[i].hora, alarmas[i].activa,
                alarmas[i].dias[0], alarmas[i].dias[1], alarmas[i].dias[2], alarmas[i].dias[3],
                alarmas[i].dias[4], alarmas[i].dias[5], alarmas[i].dias[6]);
    }
    fclose(f);
    return 0;
}

int agregar_alarma(Alarma alarmas[], int *num_alarmas, const char *nombre, const char *hora, const int dias[7])
{
    if (*num_alarmas >= MAX_ALARMAS)
        return -1;
    int id = *num_alarmas > 0 ? alarmas[*num_alarmas - 1].id + 1 : 1;
    alarmas[*num_alarmas].id = id;
    strncpy(alarmas[*num_alarmas].nombre, nombre, MAX_NOMBRE);
    strncpy(alarmas[*num_alarmas].hora, hora, MAX_HORA);
    alarmas[*num_alarmas].activa = 1;
    for (int d = 0; d < 7; d++)
        alarmas[*num_alarmas].dias[d] = dias[d];
    (*num_alarmas)++;
    return id;
}

int actualizar_alarma(Alarma alarmas[], int num_alarmas, int id, const char *nuevo_nombre, const char *nueva_hora, int activa, const int dias[7])
{
    for (int i = 0; i < num_alarmas; i++)
    {
        if (alarmas[i].id == id)
        {
            if (nuevo_nombre)
                strncpy(alarmas[i].nombre, nuevo_nombre, MAX_NOMBRE);
            if (nueva_hora)
                strncpy(alarmas[i].hora, nueva_hora, MAX_HORA);
            alarmas[i].activa = activa;
            if (dias)
                for (int d = 0; d < 7; d++)
                    alarmas[i].dias[d] = dias[d];
            return 0;
        }
    }
    return -1;
}

int eliminar_alarma(Alarma alarmas[], int *num_alarmas, int id)
{
    for (int i = 0; i < *num_alarmas; i++)
    {
        if (alarmas[i].id == id)
        {
            for (int j = i; j < *num_alarmas - 1; j++)
            {
                alarmas[j] = alarmas[j + 1];
            }
            (*num_alarmas)--;
            return 0;
        }
    }
    return -1;
}

Alarma *buscar_alarma_por_id(Alarma alarmas[], int num_alarmas, int id)
{
    for (int i = 0; i < num_alarmas; i++)
    {
        if (alarmas[i].id == id)
            return &alarmas[i];
    }
    return NULL;
}
