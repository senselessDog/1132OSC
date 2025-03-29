import struct
import serial
import time

def send_kernel(kernel_path, serial_port):
    with open(kernel_path, "rb") as f:
        kernel_data = f.read()

    checksum = sum(kernel_data)
    header = struct.pack('<III', 0x544F4F42, len(kernel_data), checksum)

    ser= serial.Serial(serial_port, 115200)
    print("start send the header")
    ser.write(header)
    print("finish send the header")
    time.sleep(3)
    print("start send the kernel")
    ser.write(kernel_data)
    print("finish send the kernel")
    ser.flush()
    # ser.close()

if __name__ == "__main__":
    send_kernel("/home/kuan/lab/lab3/script/kernel8.img", "/dev/pts/2") # Replace /dev/pts/X with the actual path