# Переменные компилятора и флагов
CC ?= $(CROSS_COMPILE)gcc
CFLAGS ?= -Wall -Werror -g
TARGET = aesdsocket
OBJS = aesdsocket.o

# Цели по умолчанию
all: $(TARGET)
default: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJS)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Очистка артефактов сборки
clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all default clean
