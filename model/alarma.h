#ifndef ALARMA_H
#define ALARMA_H

#define MAX_ALARMAS 100
#define MAX_NOMBRE 64
#define MAX_HORA 6

typedef struct
{
    int id;
    char nombre[MAX_NOMBRE];
    char hora[MAX_HORA]; // formato HH:MM
    int activa;          // 1 = activa, 0 = inactiva
    int dias[7];         // 0=domingo ... 6=sábado, 1=activo, 0=no
} Alarma;

// CRUD
int cargar_alarmas(Alarma alarmas[], int max_alarmas);
int guardar_alarmas(Alarma alarmas[], int num_alarmas);
int agregar_alarma(Alarma alarmas[], int *num_alarmas, const char *nombre, const char *hora, const int dias[7]);
int actualizar_alarma(Alarma alarmas[], int num_alarmas, int id, const char *nuevo_nombre, const char *nueva_hora, int activa, const int dias[7]);
int eliminar_alarma(Alarma alarmas[], int *num_alarmas, int id);
Alarma *buscar_alarma_por_id(Alarma alarmas[], int num_alarmas, int id);

#endif // ALARMA_H
