import serial,sys

port = sys.argv[1] if len(sys.argv) > 1 else 'COM5'
outfile = sys.argv[2] if len(sys.argv) > 2 else 'jitter.txt'

ser = serial.Serial(port, 115200, timeout=1)
print(f"reading {port} -> {outfile}, Ctrl+C to stop")

with open(outfile, 'w') as f:
    try:
        while True:
            line = ser.readline().decode(errors='ignore')
            if line:
                f.write(line)
                print(line, end='')
    except KeyboardInterrupt:
        print("\nstopped")
ser.close()