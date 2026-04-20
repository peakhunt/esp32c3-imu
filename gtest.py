from machine import SoftI2C, Pin
import ustruct
import time

# BluePill Pins
i2c = SoftI2C(scl=Pin('B6'), sda=Pin('B7'), freq=400000)
MPU_ADDR = 0x68

# Configuration mapping
# Key: (Register Value, Official Divisor)
ranges = [
    ("2G",  0x00, 16384.0),
    ("4G",  0x08, 8192.0),
    ("8G",  0x10, 4096.0),
    ("16G", 0x18, 2048.0)
]

def get_z_raw():
    # Read 2 bytes from Z-axis MSB/LSB
    data = i2c.readfrom_mem(MPU_ADDR, 0x3F, 2)
    # unpack returns a tuple, we take the first element [0]
    return ustruct.unpack('>h', data)[0]

# 1. Wake up the MPU
i2c.writeto_mem(MPU_ADDR, 0x6B, b'\x00')
time.sleep(0.2)

print("-" * 45)
print("Range    | Raw Z      | Calculated G")
print("-" * 45)

for name, reg_val, divisor in ranges:
    # Set range in Register 0x1C
    i2c.writeto_mem(MPU_ADDR, 0x1C, bytes([reg_val]))
    time.sleep(0.1)
    
    # Average 5 readings manually to avoid sum() errors
    total = 0
    for _ in range(5):
        total += get_z_raw()
        time.sleep(0.01)
    avg_raw = total / 5
    
    calc_g = avg_raw / divisor
    
    print("{:8} | {:10.1f} | {:15.4f}".format(name, avg_raw, calc_g))

print("-" * 45)
