#define     FORWARD     1
#define     BACK        0

#define     max_dutycicle       320
#define     max_trigger_value   255
#define     max_joystick_value  32767

// -------------------- Direcciones de registros -------------------- //
#BYTE       CCPTMRS0        = 0xF49
#BYTE       CCPTMRS1        = 0xF48

#BYTE       CCP1CON         = 0xFBD
#BYTE       CCP2CON         = 0xF66
#BYTE       CCP3CON         = 0xF5D
#BYTE       CCP4CON         = 0xF57

#BYTE       T2CON           = 0xFBA
#BYTE       PR2             = 0xFBB

// -------------------------- Estructuras --------------------------- //
struct bth_conection_t
{
    short updated_data;
    short IsConnected;
};

struct motor_t
{
    int     *DutyCicle;
    long    IN1_pin;
    long    IN2_pin;
};

struct controller_t
{
    signed long     Joystick[2];    // Entero de -32768 a 32767 (0 eje x, 1 eje y)
    int             Triggers[2];    // Entero de 0 a 255 (0 izquierdo, 1 derecho)
};

// ---------------------------- Funciones --------------------------- //
void motors_init(struct motor_t tires[2][2])
{
    // Configuracion de Timers
    T2CON       = 0b00000100;
    PR2         = 79;
    
    // Configuracion de CCPXCON para PWM
    CCPTMRS0 = CCPTMRS0 = 0b00000000;       // Configuramos CCPX con timer 2
    
    // Configiracion de modulo CCPX para PWM
    CCP4CON = 0b00001100;
    CCP3CON = 0b00001100;
    CCP2CON = 0b00001100;
    CCP1CON = 0b00001100;

    for(int i = 0; i < 2 ; i++)
        for(int j = 0; j < 2; j++)
            *(tires[i][j].DutyCicle) = 0;
}

void motor_movement(struct motor_t *motor, float speed)   // Modo de movimiento normal
{
    short direction;
    
    if(speed >= 0)           // Movimiento hacia adelante
        direction = FORWARD;
    else if(speed < 0)      // Movimiento hacia atras
    {
        direction = BACK;
        speed = -speed;     // Para calculo de DutyCicle
    }
    
    // Establecemos direccion de giro
    output_bit(motor->IN1_pin, direction);
    output_bit(motor->IN2_pin, !direction);

    // Establecemos DC para velocidad
    long aux = max_dutycicle * speed / 100;      // Ver si eliminar
    *(motor->DutyCicle) = aux >> 2;     // Revisar
}

void motor_shortbrake(struct motor_t *motor)    // Modo de freno de corto (Solo para emergencias)
{
    output_high(motor->IN1_pin);
    output_high(motor->IN2_pin);
}

void drive_tires(struct controller_t *xbox_controller, struct motor_t tires[2][2])
{    
    // Si estan presionados los 2 al maximo, aplicamos freno de corto y salimos de funcion
    if((xbox_controller->Triggers[0] == 255) && (xbox_controller->Triggers[1] == 255))
    {
        // Activamos modo short brake
        motor_shortbrake(&tires[0][0]), motor_shortbrake(&tires[0][1]);
        motor_shortbrake(&tires[1][0]), motor_shortbrake(&tires[1][1]);
        
        return;
    }

    /*  Notas para comprender
    * Llanta contraria al giro, avanza con normalidad a la velocidad indicada (speed)
    * Llanta de apoyo, gira a menor velocidad en funcion de que tan desviado hacia la direccion esta el joystick 
        ∵ (-speed < turning_speed < speed)
    */

    // Si no se aplico freno de corto, calculamos velocidad y direccion de movimiento
    
    float speed[2];
    signed long trigger_diff = (signed long) xbox_controller->Triggers[1] - xbox_controller->Triggers[0];

    if(xbox_controller->Joystick[0] >= 0)        // El movimiento es a la derecha o al centro
    {
        // Llanta izquierda avanza normal
        speed[0] = trigger_diff * 100.0 / max_trigger_value;
        // Flotante de -100 a 100 que indica la velocidad y direccion de movimiento (+ adelante, - atras)

        // Llanta derecha avanza como punto de apoyo para curva, por lo que calculamos velocidad de giro, en funcion de que tan cercano esta al valor maximo o minimo del eje X
        speed[1] = speed[0] - (2 * speed[0] * xbox_controller->Joystick[0] / 32767.0);
        // Velocidad - (2 veces Velocidad * (Valor Joystick X / Valor maximo Joystick X))
    }
    else                                        // El movimiento es a la izquierda
    {
        // Llanta derecha avanza normal
        speed[1] = trigger_diff * 100.0 / max_trigger_value;
        // Flotante de -100 a 100 que indica la velocidad y direccion de movimiento (+ adelante, - atras)

        // Llanta izquierda avanza como punto de apoyo para curva, por lo que Calculamos velocidad de giro, en funcion de que tan cercano esta al valor maximo o minimo del eje X
        speed[0] = speed[1] - (2 * speed[1] * xbox_controller->Joystick[0] / 32768.0);
        // Velocidad - (2 veces Velocidad * (Valor Joystick X / Valor maximo Joystick X))
    }

    // Enviamos movimiento a las llantas //
    motor_movement(&tires[0][0], speed[0]);      // Llanta delantera izquierda
    motor_movement(&tires[1][0], speed[0]);      // Llanta trasera izquierda
    
    motor_movement(&tires[0][1], speed[1]);      // Llanta delantera derecha
    motor_movement(&tires[1][1], speed[1]);      // Llanta trasera derecha
}

/*
Los gatillos establecen la velocidad maxima de giro, y la posicion del joystick establece como se repartira esta velocidad en las llantas, para poder hacer diferentes maniobras, por ejemlo:

* Cuanto mas pegado al maximo o minimo del eje Y, mayor sera la diferencia de velocidad entre las llantas de la izquierda con las de la derecha
* Cuanto mas pegado al maximo o minimo del eje X, mayor sera la diferencia de velocidad entre las llantas de arriba y abajo

Por lo tanto: 
* La llanta mas lejana a la direccion de movimiento en X tendra mas velocidad que la mas pegada
* La llanta mas lejana a la direccion de movimiento en Y tendra mas velocidad que la mas pegada
*/ 