#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include "alarma.h"
#include "ui.h"
#include "controller/controller.h"

int main(int argc, char *argv[])
{
    iniciar_aplicacion();
    return 0;

    Alarma alarmas[MAX_ALARMAS];
    int num_alarmas = cargar_alarmas(alarmas, MAX_ALARMAS);
    printf("%d alarmas cargadas.\n", num_alarmas);

    while (1)
    {
        time_t t = time(NULL);
        struct tm *tm_info = localtime(&t);
        char hora_actual[MAX_HORA];
        snprintf(hora_actual, MAX_HORA, "%02d:%02d", tm_info->tm_hour, tm_info->tm_min);

        for (int i = 0; i < num_alarmas; i++)
        {
            if (alarmas[i].activa && strcmp(alarmas[i].hora, hora_actual) == 0)
            {
                printf("\aAlarma: %s (%s)\n", alarmas[i].nombre, alarmas[i].hora);
                alarmas[i].activa = 0; // Desactivar tras sonar
                guardar_alarmas(alarmas, num_alarmas);
            }
        }
        sleep(60); // Revisar cada minuto
    }
    return 0;
}
