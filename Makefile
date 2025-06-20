# Makefile pour FCM Door Close Reminder

CC = gcc
CFLAGS = -Wall -Wextra -std=c99 -D_GNU_SOURCE
LIBS = -lcurl -ljson-c -lssl -lcrypto

# Noms des fichiers
TARGET = door_reminder
SOURCES = main.c fcm_token.c fcm_notification.c
OBJECTS = $(SOURCES:.c=.o)
HEADERS = fcm_token.h fcm_notification.h

# Règle par défaut
all: $(TARGET)

# Compilation du programme principal
$(TARGET): $(OBJECTS)
	$(CC) $(OBJECTS) -o $(TARGET) $(LIBS)

# Compilation des fichiers objets
%.o: %.c $(HEADERS)
	$(CC) $(CFLAGS) -c $< -o $@

# Nettoyage
clean:
	rm -f $(OBJECTS) $(TARGET)

# Installation des dépendances (Ubuntu/Debian)
install-deps:
	sudo apt-get update
	sudo apt-get install -y libcurl4-openssl-dev libjson-c-dev libssl-dev build-essential

# Test de compilation
test: $(TARGET)
	@echo "Compilation réussie!"
	@echo "Pour tester, assurez-vous d'avoir le fichier JSON du compte de service."

# Aide
help:
	@echo "Commandes disponibles:"
	@echo "  make                 - Compile le programme"
	@echo "  make clean          - Supprime les fichiers compilés"
	@echo "  make install-deps   - Installe les dépendances (Ubuntu/Debian)"
	@echo "  make install-deps-rpm - Installe les dépendances (CentOS/RHEL/Fedora)"
	@echo "  make test           - Teste la compilation"
	@echo "  make help           - Affiche cette aide"

.PHONY: all clean install-deps install-deps-rpm test help