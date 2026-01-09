//Restrictions for the free version of ThingSpeak:
//	1. Number of messages - 3 million / year(~8 200 / day)
//	2. Message update interval limit - Every 15 seconds
//	3. Number of channels - 4
//	4. Private channel sharing - Limited to 3 shares


//включение лога отладки в данном модуле программы/* Оформление отладки как у Алекса Гайвера*/
//#define DEBUG_THSP
#ifdef DEBUG_THSP
#define DEBUG_PRINT_THSP(x) (Serial.print(x))
#define DEBUG_PRINTLN_THSP(x) (Serial.println(x))
#define DEBUGR_PRINTR_THSP(x,r) (Serial.print(x,r))
#else
#define DEBUG_PRINT_THSP(x) 
#define DEBUG_PRINTLN_THSP(x) 
#define DEBUGR_PRINTR_THSP(x,r) 
#endif // DEBUG_THSP

//EspParameters

// Основной канал Tonshaevo_HCS на сайте ThingSpeak
	unsigned long ThingSpeakChannelNumber = 71287359;					
	const char * ThingSpeakWriteAPIKey = "8JCE0XOM858Q9P7O";	

// Дополнительный канал Tonshaevo_HCS на сайте ThingSpeak
	unsigned long ThingSpeakChannelNumber_2 = 1627034;					//ThingSpeak test cannel
	const char * ThingSpeakWriteAPIKey_2 = "39CPJQ9CZM9QEHUH";	

// always include thingspeak header file after other header files and custom macros
#include <ThingSpeak.h>	 // В этом файле определяется объект ThingSpeak

WiFiClient clientThingSpeak;

void initThingSpeak() {
	DEBUG_PRINTLN_THSP("ThingSpeak initialization");
	ThingSpeak.begin(clientThingSpeak);
}

// Write one [value] to Field [item] of a ThingSpeak Channel
/* На данный момент вызов не используется.*/
int ThingSpeakWriteField(int item, float value, int channelNumber, char* writeAPIKey, int optionParametr) {
	int httpCode = ThingSpeak.writeField(channelNumber, item, value, writeAPIKey);

	if (httpCode == 200) {
		DEBUG_PRINTLN_THSP("Channel " + String(item) + " write successful.");
	}
	else {
		DEBUG_PRINTLN_THSP("Problem writing to channel " + String(item) + ", HTTP error " + String(httpCode));
	}
	return 0;
}

//Запись нескольких значений  о температурах на сайт ThingSpeak одновременно
void ThingSpeakWriteItems() {
	//{ 0x28, 0x18, 0x07, 0x40, 0x04, 0x00, 0x00, 0xCB },  // [0]- t1, подача cистемы
	//{ 0x28, 0x4D, 0xF7, 0xBF, 0x04, 0x00, 0x00, 0x43 },  // [1]- t2, обратка системы
	//{ 0x28, 0x18, 0x07, 0x40, 0x04, 0x00, 0x00, 0xCB },  // [2]- t3, Температура в Большой спальне
	//{ 0x28, 0x20, 0x0F, 0x40, 0x04, 0x00, 0x00, 0xB9 },  // [3]- t4, подача ТТК
	//{ 0x28, 0x07, 0xBB, 0x3F, 0x04, 0x00, 0x00, 0xE8 },  // [4]- t5, обратка ТТК
	//{ 0x28, 0x18, 0x07, 0x40, 0x04, 0x00, 0x00, 0xCB },  // [5]- t6, верх ТА подача от ТТК
	//{ 0x28, 0x18, 0x07, 0x40, 0x04, 0x00, 0x00, 0xCB },  // [6]- t7, низ ТА обратка ТТК
	//{ 0x28, 0x18, 0x07, 0x40, 0x04, 0x00, 0x00, 0xCB },  // [7]- t8, верх ТА подача в систему
	//{ 0x28, 0x18, 0x07, 0x40, 0x04, 0x00, 0x00, 0xCB },  // [8]- t9, отвод трехходового крана системы
	//{ 0x28, 0x18, 0x07, 0x40, 0x04, 0x00, 0x00, 0xCB },  // [9]- t10, низ ТА, обратка системы
	//{ 0x28, 0xFF, 0x75, 0x61, 0x68, 0x18, 0x01, 0xAC },  // [10]-t11, верх ТА
	//{ 0x28, 0xFF, 0xF6, 0xDD, 0x67, 0x18, 0x01, 0xF5 },  // [11]-t12, низ ТА
	//{ 0x28, 0xFF, 0x04, 0x60, 0x68, 0x18, 0x01, 0x83 },  // [12]-t13, температура в Зале
	//{ 0x28, 0xD9, 0xEA, 0x16, 0xA8, 0x01, 0x3C, 0x10 },  // [13]-t14, температура на улице
	//{ 0x28, 0x18, 0x07, 0x40, 0x04, 0x00, 0x00, 0xCB },  // [14]-t15, подача ЭК 
	//{ 0x28, 0x97, 0x55, 0xC0, 0x04, 0x00, 0x00, 0xCA },  // [15]-t16, обратка ЭК
	static unsigned long lastRequestTime = 0;					//время последней отправки в основной канал
	if (EspParameters.ThingspeakMainChannel && allTemperaturesObtained && ((millis() - lastRequestTime) > EspParameters.DelayBetweenSendingTemperatureToThingSpeakMainChannel)) {
		DEBUG_PRINTLN_THSP("ThingSpeak. Запись всех полей в основной канал " + String(ThingSpeakChannelNumber));		
		// set the fields with the values
		ThingSpeak.setField(1, temperatures[3]);		// Подача ТТК
		ThingSpeak.setField(2, temperatures[0]);		// Подача системы
		ThingSpeak.setField(3, temperatures[10]);		// Верх ТА
		ThingSpeak.setField(4, temperatures[11]);		// Низ ТА
		ThingSpeak.setField(5, temperatures[12]);		// Температура в Зале
		ThingSpeak.setField(6, temperatures[13]);		// Температура на Улице
		ThingSpeak.setField(7, temperatures[16]);		// Температура Дымовых газов
		ThingSpeak.setField(8, temperatures[17]);		// Текущая целевая температура с учетом расписания
		// set the status
		String myStatus = F("Data ") + ntp.timeString() + F(" ") + ntp.dateString() + F("Wi-Fi RSSI ") + String(WiFi.RSSI());
		ThingSpeak.setStatus(myStatus);
		// write to the ThingSpeak channel
		int httpCode = ThingSpeak.writeFields(ThingSpeakChannelNumber, ThingSpeakWriteAPIKey);
		if (httpCode == 200) {
			DEBUG_PRINTLN_THSP("ThingSpeak. Channel main update all Fields successful.");
		}
		else {
			DEBUG_PRINTLN_THSP("ThingSpeak. Problem update main channel all Fields. HTTP error. Code " + String(httpCode) + "");
		}
		lastRequestTime = millis();
	}

	static unsigned long lastRequestTime_ch2 = 0;			//время последней отправки в дополнительный канал
	if (EspParameters.ThingspeakSecondaryChannel && allTemperaturesObtained && ((millis() - lastRequestTime_ch2) > EspParameters.DelayBetweenSendingTemperatureToThingSpeakSecondaryChannel)) {
		DEBUG_PRINTLN_THSP("ThingSpeak. Запись всех полей в дополнительный канал " + String(ThingSpeakChannelNumber_2));
		// set the fields with the values
		ThingSpeak.setField(1, temperatures[10]);		// Верх ТА
		ThingSpeak.setField(2, temperatures[11]);		// Низ ТА
		ThingSpeak.setField(3, temperatures[0]);		// Подача системы
		ThingSpeak.setField(4, temperatures[1]);		// Обратка системы
		ThingSpeak.setField(5, temperatures[12]);		// Температура в Зале
		ThingSpeak.setField(6, temperatures[2]);		// Температура в Большой спальне
		ThingSpeak.setField(7, temperatures[16]);		// Температура Дымовых газов
		ThingSpeak.setField(8, MegaParameters.RoomSetPointTemperature);		// /*Текущая целевая температура с учетом расписания*/
																								// set the status
		String myStatus = F("Data ") + ntp.timeString() + F(" ") + ntp.dateString() + F("Wi-Fi RSSI ") + String(WiFi.RSSI());
		ThingSpeak.setStatus(myStatus);
		// write to the ThingSpeak channel
		int httpCode = ThingSpeak.writeFields(ThingSpeakChannelNumber_2, ThingSpeakWriteAPIKey_2);
		if (httpCode == 200) {
			DEBUG_PRINTLN_THSP("ThingSpeak. Channel secondary update all Fields successful.");
		}
		else {
			DEBUG_PRINTLN_THSP("ThingSpeak. Problem secondary channel update all Fields. HTTP error. Code " + String(httpCode) + "");
		}
		lastRequestTime_ch2 = millis();
	}




}