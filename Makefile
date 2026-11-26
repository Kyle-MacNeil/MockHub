# Variables
CC = gcc
CFLAGS = -Iheaders
TARGET = HUB-UserApp
SRCDIR = src
HEADDIR = headers
SRCFILES = $(wildcard $(SRCDIR)/*.c)
OBJFILES = $(SRCFILES:.c=.o)

# Rules
all: $(TARGET)

$(TARGET): $(OBJFILES)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJFILES) -lssl -lcrypto -lpam

$(SRCDIR)/%.o: $(SRCDIR)/%.c
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -f $(OBJFILES) $(TARGET)

.PHONY: clean
