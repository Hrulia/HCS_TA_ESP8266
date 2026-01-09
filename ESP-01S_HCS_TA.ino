/*
Name:		ESP_01S_HCS_TA.ino
Created:	02.01.2021 11:23:52
Author:	Serge
*/

/*//!!!! как нужно добавлять файлы cpp и h в проект  https://arduinoprosto.ru/q/61634/neskolko-faylov-ino-v-odnom-eskize

в *.h - файле:
extern byte Test;

в *.cpp - файле:
byte Test = 0;

Всё, теперь переменная Test доступна везде, где подключен соответствующий *.h - файл.*/

#include <ESP8266WebServer.h>

//подключаем мои файлы .h и .cpp
#include "myCycle.h"

//включение отладки в основном модуле программы/* Оформление отладки как у Алекса Гайвера*/
//#define DEBUG_ENABLE_MAIN
#ifdef DEBUG_ENABLE_MAIN
#define DEBUG_PRINT_MAIN(x) (Serial.print(x))
#define DEBUG_PRINTLN_MAIN(x) (Serial.println(x))
#define DEBUGR_PRINTR_MAIN(x,r) (Serial.print(x,r))
#else
#define DEBUG_PRINT_MAIN(x) 
#define DEBUG_PRINTLN_MAIN(x) 
#define DEBUGR_PRINTR_MAIN(x,r) 
#endif // DEBUG_ENABLE_MAIN

//Подключенные модули программы
//#define MOD_MQTT

#define MAX_SERIAL_REQ  50			// Максимальное количество символов которое будем читать целиком из входного буфера UART
#define NUMBER_OF_TEMPERATURE_SENSORS 18		// Общее количество источников значений температуры: 16 - DS18B20 + 1 датчик температуры дыма + 1 текущая целевая температура с учетом расписания
#define SERIAL_TO_MEGA Serial		//Serial port for communication with MEGA (Serial, Serial1)
#define PIN_RESET_MEGA 12				//вывод ESP для подачи сигнала сброса на МЕГУ
#define TIME_OUT_WATCH_DOG_MEGA 300000 //5 минут. Две минуты на открытие, две минуты на закрытие при тесте .


//Enumeration - параметры работы системы
enum sysParam { ERROR = -1, EMPTY, ON, OFF, AUTO, OPEN, CLOSE, MYALG, PID };
String sysParamString[8] = { "Empty","On", "Off", "Auto", "Open", "Close", "myAlg", "PID" }; //отображает имена элементов sysParam


float temperatures[(NUMBER_OF_TEMPERATURE_SENSORS)];	//Массив температур всех источников температуры
bool allTemperaturesObtained = false;									//сигнализирует, что уже все значения температуры получены от меги, минимум один раз
bool pidParamChange = true; //флаг, указывающий, что в мегу был отправлен запрос на изменение праметров PID регулятора и нужно считать новые параметры

// Структура с параметры ESP
struct EspParametersStruct {
	// Получение данных по температурам
	unsigned long DelayBetweenSendingTemperatureRequests = 500;											//задержка между отправками запросов о температуре
	
	//Параметры запуска модуля ThingSpeak
	bool ThingspeakMainChannel = true;																							//Включить передачу информации на соответствующий канал
	bool ThingspeakSecondaryChannel = true;
	unsigned long DelayBetweenSendingTemperatureToThingSpeakMainChannel = 300000;			//задержка между отправкой данных по температуре на основной канал ThingSpeak (не 
	unsigned long DelayBetweenSendingTemperatureToThingSpeakSecondaryChannel = 60000;	//задержка между отправкой данных по температуре на дополнительный канал ThingSpeak 

} EspParameters;  //в данном объекте храним параметры системы, полученные из МЕГИ

// Структура с параметры системы из МЕГИ
struct MegaParametersStruct {
	// режимы работы насосов ТТк и Системы
	int TTKPumpMode = EMPTY;			//1 - on, 2 - off, 3 - auto
	int SystemPumpMode = EMPTY;			//1 - on, 2 - off, 3 - auto
	
	// алгоритм регулирования температуры в системе
	int SysTempControlMode = EMPTY;	//6 – мой алгоритм регулирования, 7 - PID регулятор
	
	// Алгоритм управления дверкой поддувала
	int DoorAirMode = EMPTY;				//4 - open, 5 - close, 3 - auto		
	float ReservParam1 = EMPTY;
	int ReservParam2 = EMPTY;

	// Параметры температурного режима
	float RoomSetPointTemperature;	// Целевое значение температуры помещения. Задаем системе как параметр, а уже от него расчитываем текущую целевую.

	//PID регулятор
	float pid_set_value; //Целевая температура для ПИд регулятора
	float pid_cycleS; //Цикл регулятора
	float pid_kP;
	float pid_kI;
	float pid_kD;

} MegaParameters;  //в данном объекте храним параметры системы, полученные из МЕГИ


/* Watch dog for Mega */
//Флаги состояния подключенного модуля Mega
#define MEGA_OFF 0
#define MEGA_ON  1
byte mega = MEGA_OFF; //начальная установка флага присутствия МЕГИ
unsigned long megaTimer = millis(); //Таймер проверки состояния подключенного модуля Mega
																		/* Параметры MQTT сервера */
#ifdef MOD_MQTT
extern long writeChannelID;				//ID канала ThingSpeak для записи через MQTT сервер
//extern int fieldsToPublish[8];    // Change to allow multiple fields.
// float dataToPublish[8];    // Holds your field data.
#endif 




/* Инициализируем таймеры */
myCycle cycleRequestSystemParameters(MS_10S, true);			// 3м, 30c, запрос параметров работы системы.
//myCycle cycleRequestTargetTemperature(120000, true);	// 2м запрос текущей целевой температуры с учетом суточного расписания
myCycle cycleCheckMegaAndESP(MS_01M, true);							//3мин цикл отправки в mega через serial команды своего присутствия: ?esp=1
myCycle cycleMegaTimeSynchronization(MS_30M, true, true);			// 1час цикл отправки команды синхронизации времени в мегу.


/********************* SETUP ********************************/
void setup() {
	//Serial.setRxBufferSize(500); // по умолчанию в ESP 256 Байт
	//Serial.swap(); // GPIO15/D8 (TX) и GPIO13/D7 (RX)
	//Serial.setTimeout(250);
	Serial.begin(115200);
	Serial.println();
	DEBUG_PRINTLN_MAIN(F("*******   Start setup()   *******"));

	// Вывод информации о контроллере
	DEBUG_PRINTLN_MAIN("");
	DEBUG_PRINTLN_MAIN("ESP8266 board info:");
	DEBUG_PRINT_MAIN("\tChip ID: ");
	DEBUG_PRINTLN_MAIN(ESP.getFlashChipId());
	DEBUG_PRINT_MAIN("\tCore Version: ");
	DEBUG_PRINTLN_MAIN(ESP.getCoreVersion());
	DEBUG_PRINT_MAIN("\tChip Real Size: ");
	DEBUG_PRINTLN_MAIN(ESP.getFlashChipRealSize());
	DEBUG_PRINT_MAIN("\tChip Flash Size: ");
	DEBUG_PRINTLN_MAIN(ESP.getFlashChipSize());
	DEBUG_PRINT_MAIN("\tChip Flash Speed: ");
	DEBUG_PRINTLN_MAIN(ESP.getFlashChipSpeed());
	DEBUG_PRINT_MAIN("\tChip Speed: ");
	DEBUG_PRINTLN_MAIN(ESP.getCpuFreqMHz());
	DEBUG_PRINT_MAIN("\tChip Mode: ");
	DEBUG_PRINTLN_MAIN(ESP.getFlashChipMode());
	DEBUG_PRINT_MAIN("\tSketch Size: ");
	DEBUG_PRINTLN_MAIN(ESP.getSketchSize());
	DEBUG_PRINT_MAIN("\tSketch Free Space: ");
	DEBUG_PRINTLN_MAIN(ESP.getFreeSketchSpace());
	//Вывод напряжения питания ESP
	//ADC_MODE (ADC_VCC); //перенастроить АЦП при запуске
	//ESP.getVcc() //может использоваться для измерения напряжения питания
	////В этом режиме вывод TOUT должен быть отключен.
	////по умолчанию АЦП настроен на чтение с помощью TOUT pin analogRead(A0)и ESP.getVCC()недоступен.

	//Pins configuration
	//настраиваем выход для сброса модуля MEGA
	pinMode(PIN_RESET_MEGA, OUTPUT_OPEN_DRAIN);
	digitalWrite(PIN_RESET_MEGA, HIGH);

	//Пин встроенного светодиода на плате ESP8266
	pinMode(LED_BUILTIN, OUTPUT);


	//Инициализация модулей
	DEBUG_PRINTLN_MAIN(F("call connectWifi()"));
	connectWifi();

	DEBUG_PRINTLN_MAIN(F("call initThingSpeak()"));
	initThingSpeak();

#ifdef MOD_MQTT
	DEBUG_PRINTLN_MAIN(F("call initMQTT()"));
	initMQTT();
#endif

	DEBUG_PRINTLN_MAIN(F("call initWebServer()"));
	initWebServer();

	DEBUG_PRINTLN_MAIN(F("call initNTP()"));
	initNTP();

} //end setup

/********************* LOOP ********************************/
void loop() {
	DEBUG_PRINTLN_MAIN(F("*******   Start loop()   *******"));

	// check wifi connection
	DEBUG_PRINTLN_MAIN(F("call checkWiFiConnect()"));
	checkWiFiConnect();

#ifdef MOD_MQTT
	//Проверка и поддержание связи с сервером mqtt и подписки на топики, проверка поступления новых сообщений от MQTT брокера
	DEBUG_PRINTLN_MAIN(F("call mqttloop()"));
	mqttloop();
#endif // MOD_MQTT

	// Web-server listen for HTTP requests from clients
	DEBUG_PRINTLN_MAIN(F("call checkWebClient()"));
	checkWebClient();

	// работа NTP модуля. Обновление времени c сервера с периодичностью по своему внутреннему таймеру.
	DEBUG_PRINTLN_MAIN(F("call ntpClockWork()"));
	ntpClockWork();

	//проверка поступления данных на порт Serial от модуля Mega
	DEBUG_PRINTLN_MAIN(F("call checkUART()"));
	checkUART(); //проверяем как можно чаще

/*************************************************/
/*   процедуры вызываемые по сработке таймеров   */
/*************************************************/

//Отправка в Mega2560 точного времени
	DEBUG_PRINTLN_MAIN(F("call SendActualTime()"));
	SendActualTime();

	//Запрос данных о всех температурах из модуля MEGA
	DEBUG_PRINTLN_MAIN(F("call RequestTemperatures()"));
	RequestTemperatures();

	////Запрос данных о текущей целевой температуре с учетом расписания
	//if (cycleRequestTargetTemperature.check()) {
	//	RequestTargetTemperature();
	//	// перезапуск таймера вызова функции.
	//	cycleRequestTargetTemperature.clear();
	//	cycleRequestTargetTemperature.reStart();
	//}

	//отправки данных о температуре на сайт ThingSpeak (не чаще раза в 15 секунд)
		DEBUG_PRINTLN_MAIN(F("call ThingSpeakWriteItems()"));
		ThingSpeakWriteItems();

	// отпарвки команды своего присутствия на Mega через Serial и проверка не зависла ли мега
	if (cycleCheckMegaAndESP.check()) {
		DEBUG_PRINTLN_MAIN(F("call checkMegaAndESP()"));
		checkMegaAndESP();  		//Проверка нормального функционирования модуля MEGA.
		cycleCheckMegaAndESP.reStart();
	}

	//запрос параметров работы системы отопления
	if (cycleRequestSystemParameters.check()) {
		DEBUG_PRINTLN_MAIN(F("call RequestSystemParameters()"));
		RequestSystemParameters(); //запро параметров работы системы
		cycleRequestSystemParameters.reStart();
	}



	//мигнем встроенным светодиодом, просигнализируем об окончании цикла loop и видеть, что плата не зависла
	//digitalWrite(LED_BUILTIN, !digitalRead(LED_BUILTIN)); 
	DEBUG_PRINTLN_MAIN(F("call blinkBuiltInLed()"));
	blinkBuiltInLed();


	//тестируем mqtt
	//2//mqttTest();

	DEBUG_PRINTLN_MAIN(F("*******   End loop()"));
} //end loop()


	/*******************************************************************************************/
	/*******************************  Общие функции модуля ESP   ***********************************************/
	/*******************************************************************************************/

	//Запрос информации о температуре с модуля MEGA
	/**/
void RequestTemperatures() {
	/* Появлялась ошибка, видимо при переполнении буфера на Меге от такого запроса (а скорее от ответа)
	//запрос к меге на передачу всех значений температуры
	SERIAL_TO_MEGA.println(F("?reqesttemp=A"));
	*/
	//номера датчиков температуры в массиве теммператур []: 
	/*
	0 - подача cистемы
	1 - обратка системы
	2 - отвод трехходового крана ТТК
	3 - подача ТТК
	4 - обратка ТТК
	5 - верх ТА подача от ТТК
	6 - низ ТА обратка ТТК
	7 - верх ТА подача в систему
	8 - отвод трехходового крана системы
	9 - низ ТА, обратка системы
	10 -верх ТА
	11 -низ ТА
	12 -температура в помещении
	13 -температура на улице
	14 -подача ЭК
	15 -температура в спальне //обратка ЭК 0x28, 0x97, 0x55, 0xC0, 0x04, 0x00, 0x00, 0xCA  // 0x28, 0xAA, 0x18, 0x40, 0x04, 0x00, 0x00, 0xB1 - датчик в кочегарке
	16 -Температура дымовых газов
	17 -Текущая целевая температура в соответствии с расписанием
	*/
	static int count=0;												// номер запрашиваемого параметра температуры
	static unsigned long lastRequestTime = 0;					//время отправки последнего запроса
	if ((millis() - lastRequestTime) > EspParameters.DelayBetweenSendingTemperatureRequests) {
		SERIAL_TO_MEGA.println(F("?reqesttemp=") + String(count));
		lastRequestTime = millis();
		count == 17 ? count = 0 : count++;
	}
}

////Запрос информации о текущей целевой температуре с учетом расписания с модуля MEGA
//void RequestTargetTemperature() {
//	Serial.println("?reqestTargetTemp");
//}

//Контроль работы модуля mega и отправка сигнала своего ESP присутствия 
void checkMegaAndESP() {
	//Отправка модулю MEGA информации о своем нормальном функционировании 
	SERIAL_TO_MEGA.println(F("?esp=1"));
	//Проверяем как долго от модуля mega не поступала информации о его присутствии. за 5 минуты должен поступить сигнал присутствия
	if ((millis() - megaTimer) > TIME_OUT_WATCH_DOG_MEGA) {//180000UL
		mega = MEGA_OFF;
		megaTimer = millis();
		//делаем reset MEGA
		DEBUG_PRINTLN_MAIN(F("Module MEGA reseting"));
		digitalWrite(PIN_RESET_MEGA, LOW);
		delay(100);
		digitalWrite(PIN_RESET_MEGA, HIGH);
	}
}	// cheсkMegaAndESP() 


	// ******  запрос к меге на передачу значений внутренних параметров 
void RequestSystemParameters() {
	SERIAL_TO_MEGA.println(F("?getSystemParameters")); //параметры насосов и кранов и режимов их работы
	SERIAL_TO_MEGA.println(F("?GetGTargetTemp")); //значение целевой глобальной температуры: systemParameters.RoomSetPointTemperature
	//если флаг выполненого изменения параметров пид регулятора выставлен, то запросим параметры
	if (pidParamChange) {
		SERIAL_TO_MEGA.println(F("?getPIDParam")); //запросим параметры pid регулятора
		pidParamChange = false;
	}																							
}

/*******************************************************************************************/
/*******************************************************************************************/
/*******************************************************************************************/

void blinkBuiltInLed() {
	static bool flag;
	digitalWrite(LED_BUILTIN, flag);
	flag = !flag;
	delay(300);
}
