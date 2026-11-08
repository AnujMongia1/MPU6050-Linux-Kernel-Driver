#include <stdio.h>
#include <fcntl.h>
#include <stdint.h>
#include <unistd.h>

// MPU-6000 and MPU-6050 Product Specification Revision 3.1 (https://cdn.sparkfun.com/datasheets/Components/General%20IC/PS-MPU-6000A.pdf)
#define ACCELEROMETER_LSB_PER_G 16384.0
#define ACCELERATION_IN_G(raw) ((raw)/ACCELEROMETER_LSB_PER_G)
#define GYROSCOPE_LSB_PER_DEGREE_PER_SECOND 131.0
#define ANGULAR_VELOCITY_IN_DEGREE_PER_SECOND(raw) ((raw)/GYROSCOPE_LSB_PER_DEGREE_PER_SECOND)
#define TEMP_IN_C(raw) (((raw)/340.0) + 36.53)



#define DEVICE_FILE_PATH "/dev/mpu6050"
#define MPU6050_SENSOR_READING_BYTES_COUNT 14

// Coverting two bytes (high byte first) into 16-bit integer value
static int16_t combine_bytes_big_endian_16(uint8_t* value)
{
	return (int16_t)(((uint16_t)value[0] << 8) | value[1]);
}

int main(){

	int fd = open(DEVICE_FILE_PATH, O_RDONLY);
	if (fd < 0){
		printf("Error opening device file.\n");
		return -1;
	}

	uint8_t data[MPU6050_SENSOR_READING_BYTES_COUNT];

	while(1){
		ssize_t bytes_read = read(fd, data, MPU6050_SENSOR_READING_BYTES_COUNT);
		if (bytes_read < 0){
			printf("Error reading 14 bytes sensor reading from device file %s\n", DEVICE_FILE_PATH);
			close(fd);
			return -1;
		}
		
		if (bytes_read != MPU6050_SENSOR_READING_BYTES_COUNT){
			printf("Full 14 bytes could not be read from device file.\n");
			close(fd);
			return -1;
		}

		int16_t ax = combine_bytes_big_endian_16(&data[0]);
		int16_t ay = combine_bytes_big_endian_16(&data[2]);
		int16_t az = combine_bytes_big_endian_16(&data[4]);
		int16_t tmp = combine_bytes_big_endian_16(&data[6]);
		int16_t gx = combine_bytes_big_endian_16(&data[8]);
		int16_t gy = combine_bytes_big_endian_16(&data[10]);
		int16_t gz = combine_bytes_big_endian_16(&data[12]);

		printf("Accelerometer: %6.2f %6.2f %6.2f | Gyroscope: %8.2f %8.2f %8.2f | Temperature: %6.2f\n", ACCELERATION_IN_G(ax), ACCELERATION_IN_G(ay), ACCELERATION_IN_G(az), ANGULAR_VELOCITY_IN_DEGREE_PER_SECOND(gx), ANGULAR_VELOCITY_IN_DEGREE_PER_SECOND(gy), ANGULAR_VELOCITY_IN_DEGREE_PER_SECOND(gz), TEMP_IN_C(tmp));

		usleep(100000); // 100000 microseconds = 0.1 second
	}

	close(fd);
	return 0;
}
