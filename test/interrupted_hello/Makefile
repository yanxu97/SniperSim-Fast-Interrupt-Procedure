TARGET=hello
include ../shared/Makefile.shared

$(TARGET): $(TARGET).o
	$(CC) $(TARGET).o -lm $(SNIPER_LDFLAGS) -o $(TARGET)

run_$(TARGET):
	# ../../run-sniper -v -n 1 -c gainestown --roi --frontend --pin_frontend -- ./hello -p 1
	../../run-sniper -v -n 1 -c gainestown --roi -- ./hello -p 1

	# ../../run-sniper -v -n 1 -c gainestown -s periodic-stats:10000 --roi -- ./hello -p 1

CLEAN_EXTRA=viz