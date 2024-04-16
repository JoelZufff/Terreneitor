// --------------- Preprocesadores de microcontrolador -------------- //
#include    <18F26K22.h>                                                // Libreria del Microcontrolador
#fuses      INTRC, NOWDT, NOPROTECT, NOLVP, CCP2C1, CCP3B5              // Fusibles (Multiplexado de P2B en C0)
#use        delay(internal = 16MHz)                                     // Configuracion de frecuencia y delay
#use        rs232(rcv = pin_c7, xmit = pin_c6, baud = 9600, bits = 8, parity = n) 

#include    <stdlib.h>
#include    "motor_control.c"

// --------------------- Direccion de registros --------------------- //
#BYTE       TRISA               = 0xF92
#BYTE       TRISB               = 0xF93
#BYTE       TRISC               = 0xF94
#BIT        bluetooth_state     = 0xF81.1   // PORTB
#BIT        status_LED          = 0xF89.0   // LATA

#BYTE       INTCON              = 0xFF2
#BIT        INT1IE              = 0xFF0.3   // INTCON3
#BIT        INTEDG1             = 0xFF1.5   // INTCON2
#BIT        RC1IE               = 0xF9D.5   // PIE1
#BIT        IPEN                = 0xFD0.7   // RCON
#BIT        RC1IP               = 0xF9F.5   // IPR1

// ----------------------- Variable Globales ------------------------ //
struct motor_t  tires[2][2]   = 
{
    {   // Llantas delanteras
        { 0xFBE, PIN_B3, PIN_B4 },  // Izquierda
        { 0xF67, PIN_B6, PIN_B7 }   // Derecha
    },
    {   // Llantas traseras
        { 0xF5E, PIN_A4, PIN_A5 },  // Izquierda
        { 0xF58, PIN_A6, PIN_A7 }   // Derecha
    }
};

const struct    controller_t resting_state = { {0, 0}, {0, 0} };
struct          controller_t xbox_controller;

struct          bth_conection_t PC = { FALSE, FALSE };


// -------------------------- Interrupciones ------------------------ //
#INT_RDA
void control_instructions()
{
    int data = getch();
    
    if(data == '*')     // Recibimos caracter de inicio de datos
    {           
        char XJoystick[8], YJoystick[8], LeftTrigger[5], RightTrigger[5];
        
        // Obtenemos cadenas de caracteres con los valores a recibir
        gets(XJoystick);
        gets(YJoystick);

        gets(LeftTrigger);
        gets(RightTrigger);

        // Realizamos la conversion y actualizamos los valores del control
        xbox_controller.Joystick[0] =  atol(XJoystick);
        xbox_controller.Joystick[1] =  atol(YJoystick);
        
        xbox_controller.Triggers[0] =  atol(LeftTrigger);
        xbox_controller.Triggers[1] =  atol(RightTrigger);

        // Actualizamos variables de conexion
        PC.updated_data = TRUE;
    }
}

/* Inforamcion de variables
* updated_data: Booleano para señalar que hubo una actualizacion de informacion y no ha sido aplicada.
    * updated_data = FALSE   : No hay nueva informacion
    * updated_data = TRUE    : Hay informacion pendiente de aplicar
* isConnected: Booleno para saber el estatus de la conexion de bluetooth
*/

#INT_EXT1
void bluetooth_connection()
{
    INTEDG1 ^= TRUE;                            // Cambiamos el flanco de deteccion
    PC.IsConnected = status_LED = !INTEDG1;     // Cambiamos el estatus de conexion
}

// ---------------------------- Funciones --------------------------- //
void log_init()
{
    // Configuracion de pines GPIO
    TRISA       = 0b00000000;
    TRISB       = 0b00000010;
    TRISC       = 0b11000000;

    INTCON = 0b11000000;

    // Configuracion de Interrupciones
    IPEN = RC1IE = RC1IP =  TRUE;       // Establecemos la interrupcion de recepcion serial como prioridad
    
    INT1IE  = TRUE;                 // Activamos INT_EXT1 
    INTEDG1 = !bluetooth_state;     // TRUE: flanco de subida, cuando el pin state esta en bajo. FALSE: flanco de bajada, cuando el pin state esta en alto
    PC.IsConnected = bluetooth_state;
}

// ------------------------ Codigo Principal ----------------------- //
void main()
{
    log_init();
    motors_init(tires);

    Connection_animation:    
    // Esperamos conexion bluetooth con una animacion del LED indicador
    for(int time = 0; !PC.IsConnected; time++, delay_ms(1))
    {
        if(time == 200)
        {
            status_LED ^= TRUE;
            time = 0;
        }
    }

    while (TRUE)
    {   
        //*/
        if(!PC.IsConnected)                 // Si no hay conexion ponemos modo reposo (Detenido)
        {
            // Aplicamos freno de corto cuando no hay conexion
            motor_movement(&tires[0][0]), motor_movement(&tires[0][1]);
            motor_movement(&tires[1][0]), motor_movement(&tires[1][1]);
            
            goto Connection_animation;
        }

        // Si hay conexion
        if(PC.updated_data)                // Si se actualizo la posicion
        {
            // Enviamos instrucciones a motores
            drive_tires(&xbox_controller, tires);
            PC.updated_data = FALSE;
        }
        //*/

        /*/ Revision de  almacenamiento de variables
        printf("Joystick X = %ld | Y = %ld\r\n", xbox_controller.Joystick[0], xbox_controller.Joystick[1]);
        printf("Triggers 1 =  %u | 2 = %u\r\n", xbox_controller.Triggers[0],  xbox_controller.Triggers[1]);
        delay_ms(100);
        //*/
    }
}

/*
El codigo esta compuesto de las siguientes partes

    * Interrupcion de recepcion de datos en puerto serial: Recibe los datos actualizados del control de XBOX desde el programa de computadora en C#

    * Interrupcion externa 1: Para detectar pin de state de modulo Bluetooth

    * Funcion principal: Inicializa los registros, y mueve las ruedas cuando hay nueva informacion
*/