#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <errno.h>

int main(int argc, char *argv[]) {
    // Открываем соединение с syslog
    openlog("writer", 0, LOG_USER);

    // Проверяем количество аргументов
    if (argc != 3) {
        syslog(LOG_ERR, "Invalid number of arguments. Expected 2, got %d", argc - 1);
        fprintf(stderr, "Usage: %s <file> <string>\n", argv[0]);
        closelog();
        return 1;
    }

    const char *writefile = argv[1];
    const char *writestr = argv[2];

    // Открываем файл для записи
    FILE *file = fopen(writefile, "w");
    if (file == NULL) {
        syslog(LOG_ERR, "Error opening file %s: %s", writefile, strerror(errno));
        closelog();
        return 1;
    }

    // Записываем строку и логируем успех
    syslog(LOG_DEBUG, "Writing %s to %s", writestr, writefile);
    if (fprintf(file, "%s", writestr) < 0) {
        syslog(LOG_ERR, "Error writing to file %s", writefile);
        fclose(file);
        closelog();
        return 1;
    }

    fclose(file);
    closelog();
    return 0;
}
