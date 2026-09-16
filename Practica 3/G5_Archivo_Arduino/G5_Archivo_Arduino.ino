// =====================================================
// CONTROL DE MOTOR + CONTADORES
// =====================================================
//
// PIN 4  -> Señal de inicio
// PIN 5  -> Motor A
// PIN 6  -> Motor B
// PIN 7  -> CLK contador ascendente (0 -> 15)
// PIN 9  -> CLK contador descendente (10 -> 0)
// PIN 10 -> SET contador descendente
// PIN 11 -> SET contador descendente
//
// SECUENCIA:
//
// PIN 4
//   |
//   +--> Motor dirección 1
//   |
//   +--> 15 pulsos PIN 7 (1 por segundo)
//   |
//   +--> Detener motor
//   |
//   +--> SET = 10 (PIN 10 y 11 una sola vez)
//   |
//   +--> Motor dirección 2
//   |
//   +--> 10 pulsos PIN 9 (1 por segundo)
//   |
//   +--> Detener motor
//   |
//   +--> Esperar nuevo inicio
//
// =====================================================


// -------------------------
// PINES
// -------------------------

const byte PIN_INICIO = 4;

const byte MOTOR_A = 5;
const byte MOTOR_B = 6;

const byte CLK_ASC = 7;
const byte CLK_DESC = 9;

const byte SET_A = 10;
const byte SET_B = 11;


// -------------------------
// TIEMPOS
// -------------------------

// Tiempo entre pulsos del contador
const unsigned long INTERVALO_PULSO = 1000;

// Duración del pulso HIGH
const unsigned long DURACION_PULSO = 50;

// Tiempo de debounce del botón/señal de inicio
const unsigned long DEBOUNCE = 50;


// -------------------------
// ESTADOS DEL SISTEMA
// -------------------------

enum Estado
{
  ESPERANDO,
  CONTANDO_ASCENDENTE,
  SETEANDO_10,
  CONTANDO_DESCENDENTE
};

Estado estado = ESPERANDO;


// -------------------------
// VARIABLES DE TIEMPO
// -------------------------

unsigned long ultimoPulso = 0;
unsigned long inicioSet = 0;


// -------------------------
// VARIABLES DE CONTADORES
// -------------------------

byte pulsosAsc = 0;
byte pulsosDesc = 0;


// -------------------------
// VARIABLES PARA PULSOS
// -------------------------

bool pulsoActivo = false;
byte pinPulsoActual = 255;
unsigned long inicioPulso = 0;


// -------------------------
// DEBOUNCE DE PIN 4
// -------------------------

bool lecturaInicioAnterior = LOW;
bool estadoInicioEstable = LOW;

unsigned long ultimoCambioInicio = 0;


// =====================================================
// MOTOR
// =====================================================

void motorDireccion1()
{
  digitalWrite(MOTOR_A, HIGH);
  digitalWrite(MOTOR_B, LOW);
}


void motorDireccion2()
{
  digitalWrite(MOTOR_A, LOW);
  digitalWrite(MOTOR_B, HIGH);
}


void detenerMotor()
{
  digitalWrite(MOTOR_A, LOW);
  digitalWrite(MOTOR_B, LOW);
}


// =====================================================
// INICIAR UN PULSO
// =====================================================

void iniciarPulso(byte pin)
{
  digitalWrite(pin, HIGH);

  pulsoActivo = true;
  pinPulsoActual = pin;
  inicioPulso = millis();
}


// =====================================================
// ACTUALIZAR PULSO
// =====================================================
//
// No utiliza delay().
// Arduino puede seguir ejecutando el programa mientras
// el pulso está activo.
// =====================================================

void actualizarPulso()
{
  if (pulsoActivo)
  {
    if (millis() - inicioPulso >= DURACION_PULSO)
    {
      digitalWrite(pinPulsoActual, LOW);

      pulsoActivo = false;
      pinPulsoActual = 255;
    }
  }
}


// =====================================================
// SETEAR CONTADOR EN 10
// =====================================================
//
// PIN 10 y PIN 11 se activan UNA SOLA VEZ.
// No se mandan 10 pulsos.
//
// Se asume que estos dos SET hacen que los bits
// correspondientes formen:
//
// 1010 = 10
// =====================================================

void iniciarSet10()
{
  digitalWrite(SET_A, HIGH);
  digitalWrite(SET_B, HIGH);

  inicioSet = millis();

  estado = SETEANDO_10;
}


void actualizarSet10()
{
  if (millis() - inicioSet >= DURACION_PULSO)
  {
    digitalWrite(SET_A, LOW);
    digitalWrite(SET_B, LOW);

    // Preparar contador descendente
    pulsosDesc = 0;

    // Comenzar inmediatamente el conteo
    motorDireccion2();

    ultimoPulso = millis();

    estado = CONTANDO_DESCENDENTE;
  }
}


// =====================================================
// DEBOUNCE + DETECCIÓN DE FLANCO
// =====================================================
//
// Devuelve true SOLO cuando PIN 4 pasa de LOW a HIGH
// y permanece estable.
// =====================================================

bool detectarInicio()
{
  bool lecturaActual = digitalRead(PIN_INICIO);

  if (lecturaActual != lecturaInicioAnterior)
  {
    ultimoCambioInicio = millis();
    lecturaInicioAnterior = lecturaActual;
  }

  if (millis() - ultimoCambioInicio >= DEBOUNCE)
  {
    if (lecturaActual != estadoInicioEstable)
    {
      estadoInicioEstable = lecturaActual;

      // Solo nos interesa LOW -> HIGH
      if (estadoInicioEstable == HIGH)
      {
        return true;
      }
    }
  }

  return false;
}


// =====================================================
// SETUP
// =====================================================

void setup()
{
  pinMode(PIN_INICIO, INPUT);

  pinMode(MOTOR_A, OUTPUT);
  pinMode(MOTOR_B, OUTPUT);

  pinMode(CLK_ASC, OUTPUT);
  pinMode(CLK_DESC, OUTPUT);

  pinMode(SET_A, OUTPUT);
  pinMode(SET_B, OUTPUT);


  // Estados iniciales

  detenerMotor();

  digitalWrite(CLK_ASC, LOW);
  digitalWrite(CLK_DESC, LOW);

  digitalWrite(SET_A, LOW);
  digitalWrite(SET_B, LOW);
}


// =====================================================
// LOOP
// =====================================================

void loop()
{
  // Actualizar señales temporizadas
  actualizarPulso();


  // ---------------------------------------------
  // ESPERANDO SEÑAL DE INICIO
  // ---------------------------------------------

  if (estado == ESPERANDO)
  {
    if (detectarInicio())
    {
      // Reiniciar contador ascendente
      pulsosAsc = 0;

      // Motor dirección 1
      motorDireccion1();

      // Preparar primer pulso
      ultimoPulso = millis();

      estado = CONTANDO_ASCENDENTE;
    }
  }


  // ---------------------------------------------
  // CONTADOR ASCENDENTE 0 -> 15
  // ---------------------------------------------

  else if (estado == CONTANDO_ASCENDENTE)
  {
    // No iniciar otro pulso mientras el anterior
    // todavía está HIGH.
    if (!pulsoActivo)
    {
      if (millis() - ultimoPulso >= INTERVALO_PULSO)
      {
        ultimoPulso = millis();

        iniciarPulso(CLK_ASC);

        pulsosAsc++;

        // Ya mandamos los 16 pulsos
        if (pulsosAsc >= 16)
        {
          estado = SETEANDO_10;

          // Detener motor antes de cambiar dirección
          detenerMotor();

          // Preparar SET
          inicioSet = millis();

          digitalWrite(SET_A, HIGH);
          digitalWrite(SET_B, HIGH);
        }
      }
    }
  }


  // ---------------------------------------------
  // SETEAR 10
  // ---------------------------------------------

  else if (estado == SETEANDO_10)
  {
    // El SET permanece HIGH durante 50 ms
    if (millis() - inicioSet >= DURACION_PULSO)
    {
      digitalWrite(SET_A, LOW);
      digitalWrite(SET_B, LOW);

      // El contador ya debe estar en:
      //
      // 1010 = 10
      //
      pulsosDesc = 0;

      // Cambiar motor de dirección
      motorDireccion2();

      // Esperar un segundo para el primer CLK
      ultimoPulso = millis();

      estado = CONTANDO_DESCENDENTE;
    }
  }


  // ---------------------------------------------
  // CONTADOR DESCENDENTE 10 -> 0
  // ---------------------------------------------

  else if (estado == CONTANDO_DESCENDENTE)
  {
    if (!pulsoActivo)
    {
      if (millis() - ultimoPulso >= INTERVALO_PULSO)
      {
        ultimoPulso = millis();

        iniciarPulso(CLK_DESC);

        pulsosDesc++;

        // 10 pulsos:
        //
        // 10 -> 9
        //  9 -> 8
        //  ...
        //  1 -> 0
        //
        if (pulsosDesc >= 10)
        {
          detenerMotor();

          estado = ESPERANDO;
        }
      }
    }
  }
}
