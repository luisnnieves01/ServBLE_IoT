# src/makefile
#
# Compila únicamente el binario 'setup_msg1':
#   pre-enrollment en memoria + construcción del Mensaje 1
#
# Dependencias externas: dbus-1 (para BLE, fase siguiente)
# Ya NO se usa: sqlite3, servidor TCP, enrollment completo

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11  \
           -I.                          \
           -Iascon_hash                 \
           -Icifrador_ascon             \
           -Iregistro                   \
           -Ibch_puf                    \
           -Ipuf_ref                    \
           -Ible                        \
           -Idb                        \
           -I../lib/ascon               \
           -I../lib/bch_codec           \
           $(shell pkg-config --cflags dbus-1)

LDFLAGS = $(shell pkg-config --libs dbus-1)

# ── Fuentes comunes (sin main) ────────────────────────────────────
SRCS_COMUN = db/tabla_ls.c                     \
             registro/pre_enrollment.c      \
             ascon_hash/hash.c              \
             bch_puf/bch_puf.c              \
             cifrador_ascon/ascon_ccm.c     \
             puf_ref/referencia.c           \
             puf_ref/mascara.c              \
             ../lib/ascon/hash.c            \
             ../lib/bch_codec/bch_codec.c

OBJS_COMUN = $(SRCS_COMUN:.c=.o)

# ── Regla genérica ────────────────────────────────────────────────
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# ── Objetivos ─────────────────────────────────────────────────────
all: servidor_ble setup_msg1

# Binario principal: pre-enrollment + M1 + envío BLE
servidor_ble: ble/ble_server_setup.o $(OBJS_COMUN)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Binario auxiliar: solo pre-enrollment + M1 (sin BLE, para pruebas)
setup_msg1: main.o $(OBJS_COMUN)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	find . -name "*.o" -delete
	rm -f servidor_ble setup_msg1

.PHONY: all clean
