//=====================================================
// Для обмена по UART с модулем MEGA на данный момент пока используется тот же Serial, который идет на USB.
// но у esp8266 есть еще один UART-Serial1 (фактически это просто вывод того же uart на другие ножки мк), вот только буфер у них общий, поэтому нужно переключаться с одного на другой 
// функцией Serial.swap() и при этом, на всякий случай, еще чистим буфер функцией Serial.flush() 
// подробнее тут: https://esp8266.ru/forum/threads/zachem-polzovatsja-kostylem-softserial-kogda-u-esp8266-dva-apparatnyx-uart.4749/
//


//включение отладки в модуле программы 
#define DEBUG_ENABLE_PS
#ifdef DEBUG_ENABLE_PS
#define DEBUG_PRINT_PS(x) (Serial.print(x))
#define DEBUG_PRINTLN_PS(x) (Serial.println(x))
#define DEBUGR_PRINTR_PS(x,r) (Serial.print(x,r))
#else
#define DEBUG_PRINT_PS(x) 
#define DEBUG_PRINTLN_PS(x) 
#define DEBUGR_PRINTR_PS(x,r) 
#endif // DEBUG_ENABLE_PS


//=============================== -- Обработка поступившей по UART информации от модуля MEGA -- ===========================
bool sFlag = true;   //флаг, информирует, что в данных, полученных через uart был найден перевод строки, а это значит конец отправленныой команды
String serialReq = "";
//проверяем поступили-ли данные в порт Serial
void checkUART() {
	while (Serial.available() > 0) {
		if (sFlag) {
			serialReq = "";
			sFlag = false;
		}
		char c = Serial.read();
		if (c == 10) { // '\n' LF //При получении символа перевода строки, считывание прерывааем и вызываем команду парсинга полученной строки
			sFlag = true;
			parseSerialStr();
		}
		else if (c == 13) { //'\r' CR// skip								
			}
			else {
				if (serialReq.length() < MAX_SERIAL_REQ) {
					serialReq += c;
				}
			}
	}
}

	//1 этап парсинга: выделение команды и параметра
void parseSerialStr() {
	if (serialReq[0] == '?') {
		parseSerialCmd();
	}
	else {
		///DEBUGLN("ESP[" + serialReq + "]");  //выводим, то, что пришло в порт 
	}
}

//********  
// 2 этап:  разборс поступившей команды и ее обработка
void parseSerialCmd() {
	String command, parameter;
	if (serialReq.indexOf(F("?")) >= 0) {
		int pBegin = serialReq.indexOf(F("?")) + 1;
		if (serialReq.indexOf(F("=")) >= 0) {
			int pParam = serialReq.indexOf(F("="));
			command = serialReq.substring(pBegin, pParam);
			parameter = serialReq.substring(pParam + 1);
		}
		else {
			command = serialReq.substring(pBegin);
			parameter = "";
		}

	//**********************************************************************************
	//============ Разбор поступивших команд и тут же они обрабатываются =============
	//**********************************************************************************
	//?test 
	if (command == F("test")) {//Команда для проверки работы функции обработки в ESP													// ?test
		Serial.println(F("Put command test"));
	}
	//**********
// ?mega
	else if (command == F("mega")) {//MEGA прислала подтверждение, что работает
			if (parameter == F("1")) {
				mega = MEGA_ON;
				megaTimer = millis(); //сбросим таймер выявление зависания модуля MEGA
				DEBUG_PRINTLN_PS(F("Received from MEGA - working!"));
			}
		}

//**********
// ?sendtempХ              ?sendtemp=1.23;5.78;33,33;7,77																										// ?sendtempХ  
		else if (command.substring(0, 8) == F("sendtemp")) {
			if (command.substring(8) == "A") {
				DEBUG_PRINTLN_PS("Get from MEGA string of temperatures " + parameter);
				//Обработка не сделана!!!!  Были проблемы стабильноси при отправки такой большой строки
			}
			else {
				int index = command.substring(8).toInt();
				temperatures[index] = parameter.toFloat();
				DEBUG_PRINTLN_PS("temp " + command.substring(8) + ": " + String(temperatures[index]) + "");
				// при получении последнего параметра из массива, установим флаг окончания первоначального полного получения температур
				if ((index == (NUMBER_OF_TEMPERATURE_SENSORS - 1)) && !allTemperaturesObtained) {
					allTemperaturesObtained = true;
					DEBUG_PRINTLN_PS("Все значения температур получены. Установлен флаг allTemperaturesObtained");
				}
			}
		}

//**********
// ?reqestrssi
		else if (command == F("reqestrssi")) {//Запрос уровня сигнала wi-fi
			Serial.print("?sendrssi=");
			Serial.println((long)WiFi.RSSI());
		}

//**********
// ?sendGTargetTemp
		else if (command == F("sendGTargetTemp")) {//Передача от MEGA значения глобальной целевой температуры системы, без учета расписания
			//DEBUG_PRINTLN_PS(F("The global target temperature is obtained: ") + parameter);
			MegaParameters.RoomSetPointTemperature = parameter.toFloat();
			//Отправляем на сервер MQTT в field3 (GTargetTemp)
			/*dataToPublish[2] = parameter.toFloat();
			fieldsToPublish[0] = 0; //field1 will be rec
			fieldsToPublish[1] = 0; //...
			fieldsToPublish[2] = 1;
			fieldsToPublish[3] = 0;
			fieldsToPublish[4] = 0;
			fieldsToPublish[5] = 0;
			fieldsToPublish[6] = 0;
			fieldsToPublish[7] = 0;*/

			//mqttPublish(writeChannelID, dataToPublish, fieldsToPublish);
		}
		//**********
		// ?sendSystemParameters=XYZK																																						// ?sendSystemParameters
		else if (command == F("sendSystemParameters")) {   
			//DEBUG_PRINTLN_PS(F("Получена команда от меги: ?sendSystemParameters с параметрами: ") + parameter);

			MegaParameters.TTKPumpMode = parameter.substring(0,1).toInt();
			MegaParameters.SystemPumpMode = parameter.substring(1,2).toInt();
			MegaParameters.SysTempControlMode = parameter.substring(2,3).toInt();
			MegaParameters.DoorAirMode = parameter.substring(3,4).toInt();

			MegaParameters.ReservParam1 = parameter.substring(4,8).toFloat();
			MegaParameters.ReservParam2 = parameter.substring(8,9).toInt();
		}

		//**********
		// ?sendPIDParam = Temperature,Cicle, kP, kI, kD																																						// ?sendSystemParameters
		else if (command == F("sendPIDParam")) {
			DEBUG_PRINTLN_PS(F("Получена команда от меги: ?sendPIDParam с параметрами: ") + parameter);
	
			int p1 = 0, p2=0;
			p2 = parameter.indexOf(",");
			MegaParameters.pid_set_value = parameter.substring(p1, p2-1).toFloat();
			p1 = parameter.indexOf(",",(p2+1));
			MegaParameters.pid_cycleS = parameter.substring(p2+1, p1-1).toFloat();
			p2 = parameter.indexOf(",", (p1 + 1));
			MegaParameters.pid_kP = parameter.substring(p1 + 1, p2 - 1).toFloat();
			p1 = parameter.indexOf(",",(p2+1));
			MegaParameters.pid_kI = parameter.substring(p2+1, p1-1).toFloat();
			p2 = parameter.indexOf(",", (p1 + 1));
			MegaParameters.pid_kD = parameter.substring(p1 + 1, p2 - 1).toFloat();

			Serial.println(F("Проверь разложение строки на параметры: "));
			Serial.println(MegaParameters.pid_set_value);
			Serial.println(MegaParameters.pid_cycleS);
			Serial.println(MegaParameters.pid_kP);
			Serial.println(MegaParameters.pid_kI);
			Serial.println(MegaParameters.pid_kD);

		}

	}
}




