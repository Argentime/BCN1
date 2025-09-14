#define _CRT_SECURE_NO_WARNINGS
#include "main.h"

HANDLE hComm1;
HANDLE hComm2;

#define COM_PORT1 L"COM2"
#define COM_PORT2 L"COM4"

int main() {
    setlocale(LC_ALL, "Russian");
    SetConsoleOutputCP(CP_UTF8);
    SetConsoleCP(CP_UTF8);

    DWORD baudRate = CBR_9600;

    hComm1 = open_com_port(COM_PORT1);
    hComm2 = open_com_port(COM_PORT2);
    if (hComm1 == NULL || hComm2 == NULL) {
        if (hComm1 != NULL) CloseHandle(hComm1);
        if (hComm2 != NULL) CloseHandle(hComm2);
        std::cout << "Не удалось открыть один или оба COM-порта. Проверьте их доступность." << std::endl;
        return 1;
    }

    configure_com_port(hComm1, baudRate);
    configure_com_port(hComm2, baudRate);
    std::cout << "Порты успешно настроены. Скорость: " << baudRate << std::endl;

    std::string message;
    int choice = -1;

    while (true) {
        std::cout << "\nВыберите действие:\n";
        std::cout << "1 - Отправить сообщение с " << WCharToString(COM_PORT1) << " на " << WCharToString(COM_PORT2) << "\n";
        std::cout << "2 - Прочитать сообщение с " << WCharToString(COM_PORT2) << "\n";
        std::cout << "3 - Установить другую скорость передачи\n";
        std::cout << "0 - Выход\n";
        std::cout << "Ваш выбор: ";
        std::cin >> choice;

        while (getchar() != '\n');

        switch (choice) {
        case 1:
            std::cout << "Введите сообщение для отправки: ";
            std::getline(std::cin, message);
            send_string(hComm1, message);
            break;
        case 2:
            receive_and_print_string(hComm2);
            break;
        case 3:
            baudRate = select_baud_rate();
            configure_com_port(hComm1, baudRate);
            configure_com_port(hComm2, baudRate);
            break;
        case 0:
            std::cout << "Выход из программы.\n";
            CloseHandle(hComm1);
            CloseHandle(hComm2);
            return 0;
        default:
            std::cout << "Неверный выбор. Пожалуйста, попробуйте еще раз.\n";
            break;
        }
    }
}

void print_last_error() {
    DWORD dwError = GetLastError();
    std::cout << "Ошибка: " << dwError << std::endl;
}

HANDLE open_com_port(LPCWSTR COM_PORT) {
    HANDLE hComm = CreateFile(
        COM_PORT,
        GENERIC_READ | GENERIC_WRITE,
        0,
        NULL,
        OPEN_EXISTING,
        0,
        NULL
    );

    if (hComm == INVALID_HANDLE_VALUE) {
        std::cout << "Не удалось открыть порт " << WCharToString(COM_PORT) << "\n";
        print_last_error();
        return NULL;
    }

    return hComm;
}

void configure_com_port(HANDLE hComm, DWORD baudRate) {
    DCB dcbSerialParams = { 0 };
    dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

    if (!GetCommState(hComm, &dcbSerialParams)) {
        std::cout << "Ошибка получения текущих параметров порта." << std::endl;
        print_last_error();
        return;
    }

    dcbSerialParams.BaudRate = baudRate;
    dcbSerialParams.ByteSize = 8;
    dcbSerialParams.StopBits = ONESTOPBIT;
    dcbSerialParams.Parity = NOPARITY;

    if (!SetCommState(hComm, &dcbSerialParams)) {
        std::cout << "Ошибка настройки параметров порта." << std::endl;
        print_last_error();
        return;
    }

    COMMTIMEOUTS timeouts = { 0 };
    timeouts.ReadIntervalTimeout = 50;
    timeouts.ReadTotalTimeoutConstant = 50;
    timeouts.ReadTotalTimeoutMultiplier = 10;
    timeouts.WriteTotalTimeoutConstant = 50;
    timeouts.WriteTotalTimeoutMultiplier = 10;

    if (!SetCommTimeouts(hComm, &timeouts)) {
        std::cout << "Ошибка настройки тайм-аутов порта." << std::endl;
        print_last_error();
        return;
    }
}

void send_string(HANDLE hComm, const std::string& data) {
    DWORD bytesWritten;
    DWORD dataSize = data.length();

    if (WriteFile(hComm, data.c_str(), dataSize, &bytesWritten, NULL) && bytesWritten == dataSize) {
    }
    else {
        std::cout << "Ошибка отправки сообщения." << std::endl;
        print_last_error();
    }
}

void receive_and_print_string(HANDLE hComm) {
    DWORD bytesRead;
    char buffer[256] = { 0 };

    COMSTAT com_stat;
    DWORD dw_error;
    if (!ClearCommError(hComm, &dw_error, &com_stat)) {
        std::cout << "Ошибка ClearCommError" << std::endl;
        print_last_error();
        return;
    }

    if (com_stat.cbInQue == 0) {
        std::cout << "Нет данных для чтения." << std::endl;
        return;
    }

    if (ReadFile(hComm, buffer, com_stat.cbInQue, &bytesRead, NULL)) {
        // Конвертация из UTF-8 в системную кодировку для вывода
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, buffer, bytesRead, NULL, 0);
        std::vector<wchar_t> wstr(size_needed);
        MultiByteToWideChar(CP_UTF8, 0, buffer, bytesRead, &wstr[0], size_needed);

        int output_size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], size_needed, NULL, 0, NULL, NULL);
        std::string received_message(output_size_needed, 0);
        WideCharToMultiByte(CP_UTF8, 0, &wstr[0], size_needed, &received_message[0], output_size_needed, NULL, NULL);

        std::cout << "Получено сообщение: " << received_message << std::endl;
    }
    else {
        std::cout << "Ошибка чтения сообщения." << std::endl;
        print_last_error();
    }
}

DWORD select_baud_rate() {
    int choice;
    DWORD baudRates[] = {
        CBR_110, CBR_300, CBR_600, CBR_1200, CBR_2400, CBR_4800,
        CBR_9600, CBR_14400, CBR_19200, CBR_38400, CBR_57600,
        CBR_115200, CBR_128000, CBR_256000
    };
    int numRates = sizeof(baudRates) / sizeof(baudRates[0]);

    std::cout << "Выберите скорость передачи (Baud Rate):\n";
    for (int i = 0; i < numRates; i++) {
        std::cout << i + 1 << " - " << baudRates[i] << std::endl;
    }
    std::cout << "Введите номер: ";
    std::cin >> choice;

    if (choice >= 1 && choice <= numRates) {
        std::cout << "Выбрана скорость: " << baudRates[choice - 1] << std::endl;
        return baudRates[choice - 1];
    }
    else {
        std::cout << "Неверный выбор. Устанавливается скорость по умолчанию: 9600\n";
        return CBR_9600;
    }
}

std::string WCharToString(LPCWSTR wstr) {
    if (!wstr) {
        return "";
    }
    int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
    std::string str_to(size_needed, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr, -1, &str_to[0], size_needed, NULL, NULL);
    return str_to;
}