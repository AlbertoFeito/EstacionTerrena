#ifndef ESTRUCTURAS_H
#define ESTRUCTURAS_H
#pragma pack(push, 1)  // Alineación de 1 byte

struct sPUNTOXYZ{
    char modo = 2; //0,1,2,3,4
    float x = 0;//lat
    float y = 0;//long
    float z = 0;//altura
    float parametro_00 = 0;
    float parametro_01 = 0;
    float parametro_02 = 0;
    float velocidad = 0;//velocidad
};

struct sTRAMA1
{
    // Entradas dinámicas
    float Realimentacion_Cabeceo = 0;
    float Realimentacion_Banqueo= 0;
    float Realimentacion_Velocidad= 0;
    float Realimentacion_Altura= 0;
    float Realimentacion_Curso= 0;
    float Realimentacion_Latitud= 0;
    float Realimentacion_Longitud= 0;
    float Realimentacion_CursoIMO = 0;
};

struct sTRAMA2
{
    float Control_Estabilizadores = 0;
    float Control_Alerones= 0;
    float Control_Motor= 0;
    float Control_Rumbo= 0;
    float Convergencia_Cabeceo= 0;
    float Convergencia_Banqueo= 0;
    float Distancia_Chequeo = 0;
    float Curso_Deseado= 0;
    float Ganancia_Cabeceo[2] = {0,0};
    float Ganancia_Banqueo[2] = {0,0};
    float Ganancia_Velocidad[2] = {0,0};
    float Ganancia_Altura[2] = {0,0};
    float Ganancia_Curso = 0;
    float Ganancia_Rumbo[2] = {0,0};
    char Realimentacion_Regimen = 0;
    char Realimentacion_ComendoMision = 0;
    float Cumplir_Punto[3] = {0,0,0};
    char Cantidad_Satelites = 0;

};
struct sMANDOS {
    unsigned char Sintonizacion_Cabeceo = 0;//0,1
    unsigned char Sintonizacion_Banqueo = 0;//0,1

    float Ganancias_Cabeceo_Manual1 = 0;
    float Ganancias_Cabeceo_Manual2 = 0;
    unsigned char Actualizar_Ganancias_Cabeceo = 0;//0,1

    float Ganancias_Banqueo_Manual1 = 0;
    float Ganancias_Banqueo_Manual2 = 0;
    unsigned char Actualizar_Ganancias_Banqueo = 0;//0,1

    float Ganancias_Altura_Manual1 = 0;
    float Ganancias_Altura_Manual2 = 0;
    unsigned char Actualizar_Ganancias_Altura = 0;//0,1

    float Ganancias_Velocidad_Manual1 = 0;
    float Ganancias_Velocidad_Manual2 = 0;
    unsigned char Actualizar_Ganancias_Velocidad = 0;//0,1

    float Ganancias_Curso_Manual = 0;
    unsigned char Actualizar_Ganancias_Curso = 0;//0,1

    float Ganancias_Rumbo_Manual1 = 0;
    float Ganancias_Rumbo_Manual2 = 0;
    unsigned char Actualizar_Ganancias_Rumbo = 0;//0,1

    unsigned char Regimen = 0;//0,1,2
    unsigned char Comando_Mision = 0;//0,1,2,3

    float Radio_Acercamiento = 20;
    float Distancia_Encuentro = 25;
    float Home[3] = {0,0,0};
    float Velocidad_RTH = 20;

    unsigned char Paracaidas = 0;//0,1
};

struct sCUADROPTEROS
{
    char Identificadores[3] = {'a','b','c'};
    float Realimentacion_Cabeceo = 0.0;
    float Realimentacion_Banqueo= 0.0;
    float Realimentacion_Velocidad= 0.0;
    float Realimentacion_Altura= 0.0;
    float Realimentacion_Curso= 0.0;
    float Realimentacion_Latitud= 0.0;
    float Realimentacion_Longitud= 0.0;
    float Realimentacion_CursoIMU = 0.0;
    float ControlM1 = 0.0;
    float ControlM2 = 0.0;
    float ControlM3 = 0.0;
    float ControlM4 = 0.0;

    float Convergencia_Cabeceo= 0.0;
    float Convergencia_Banqueo= 0.0;
    float Distancia_Chequeo = 0.0;
    float Curso_Deseado = 0.0;
    float Ganancias_Cabeceo[2] = {0.0, 0.0};

    float Ganancias_Banqueo[2] = {0.0, 0.0};

    float Ganancias_Altura[3] = {0.0, 0.0, 0.0};

    float Ganancias_Curso = 0.0;
    float Ganancias_Rumbo[3] = {0.0, 0.0, 0.0};

    char Realimentacion_Regimen = 0;
    char Realimentacion_ComandoMision = 0;

    float Cumplir_Punto[3] = {0.0, 0.0, 0.0};

    char Cantidad_Satelites = 17;

};
#pragma pack(pop)

#endif // ESTRUCTURAS_H
