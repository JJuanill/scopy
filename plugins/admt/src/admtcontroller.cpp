#include "admtcontroller.h"

#include <iioutil/connectionprovider.h>

#include <string>
#include <cstdint>
#include <algorithm>
#include <cstring>
#include <list>

#include <vector>
#include <cstdlib>
#include <cmath>
#include <math.h>
#include <numeric>
#include <complex>
#include <iterator>
#include <iomanip>

static const size_t maxAttrSize = 512;

using namespace scopy::admt;
using namespace std;

ADMTController::ADMTController(QString uri, QObject *parent)
    :QObject(parent)
    , uri(uri)
{

}

ADMTController::~ADMTController() {}

void ADMTController::connectADMT()
{
	m_conn = ConnectionProvider::open(uri);
    m_iioCtx = m_conn->context();
}

void ADMTController::disconnectADMT()
{
	if(!m_conn || !m_iioCtx){
		return;
	}

	ConnectionProvider::close(uri);
	m_conn = nullptr;
	m_iioCtx = nullptr;

}

const char* ADMTController::getChannelId(Channel channel)
{
	if(channel >= 0 && channel < CHANNEL_COUNT){
		return ChannelIds[channel];
	}
	return "Unknown";
}

const char* ADMTController::getDeviceId(Device device)
{
	if(device >= 0 && device < DEVICE_COUNT){
		return DeviceIds[device];
	}
	return "Unknown";
}

const char* ADMTController::getMotorAttribute(MotorAttribute attribute)
{
	if(attribute >= 0 && attribute < MOTOR_ATTR_COUNT){
		return MotorAttributes[attribute];
	}
	return "Unknown";
}

const uint32_t ADMTController::getHarmonicRegister(HarmonicRegister registerID)
{
	if(registerID >= 0 && registerID < HARMONIC_REGISTER_COUNT){
		return HarmonicRegisters[registerID];
	}
	return 0x0;
}

const uint32_t ADMTController::getConfigurationRegister(ConfigurationRegister registerID)
{
	if(registerID >= 0 && registerID < CONFIGURATION_REGISTER_COUNT){
		return ConfigurationRegisters[registerID];
	}
	return UINT32_MAX;
}

const uint32_t ADMTController::getSensorRegister(SensorRegister registerID)
{
	if(registerID >= 0 && registerID < SENSOR_REGISTER_COUNT){
		return SensorRegisters[registerID];
	}
	return UINT32_MAX;
}

const uint32_t ADMTController::getSensorPage(SensorRegister registerID)
{
	if(registerID >= 0 && registerID < SENSOR_REGISTER_COUNT){
		return SensorPages[registerID];
	}
	return UINT32_MAX;
}

int ADMTController::getChannelIndex(const char *deviceName, const char *channelName)
{
	iio_device *admtDevice = iio_context_find_device(m_iioCtx, deviceName);
    if(admtDevice == NULL) { return -1; }
	int channelCount = iio_device_get_channels_count(admtDevice);
	iio_channel *channel;
	std::string message = "";
	for(int i = 0; i < channelCount; i++){
		channel = iio_device_get_channel(admtDevice, i);
		const char *deviceChannel = iio_channel_get_id(channel);
		
		std::string strDeviceChannel = std::string(deviceChannel);
		std::string strChannelName = std::string(channelName);

		if(deviceChannel != nullptr && strDeviceChannel == strChannelName){
			message = message + "[" + std::to_string(i) + "]" + std::string(deviceChannel) + ", ";
			return iio_channel_get_index(channel);
		}
		else {
			channel = NULL;
		}
	}
	return -1;
}

double ADMTController::getChannelValue(const char *deviceName, const char *channelName, int bufferSize)
{
	double value;
	char converted[bufferSize] = "";

	int deviceCount = iio_context_get_devices_count(m_iioCtx);
	//if(deviceCount < 1) return QString("No devices found");

	iio_device *admtDevice = iio_context_find_device(m_iioCtx, deviceName);
	//if(admtDevice == NULL) return QString("No ADMT4000 device");

	int channelCount = iio_device_get_channels_count(admtDevice);
	//if(channelCount < 1) return QString("No channels found.");

	iio_channel *channel;
	std::string message = "";
	for(int i = 0; i < channelCount; i++){
		channel = iio_device_get_channel(admtDevice, i);
		const char *deviceChannel = iio_channel_get_id(channel);
		
		if(deviceChannel != nullptr && std::string(deviceChannel) == std::string(channelName)){
			message = message + "[" + std::to_string(i) + "]" + std::string(deviceChannel) + ", ";
			break;
		}
		else {
			channel = NULL;
		}
	}
	if(channel == NULL) {
		//return QString::fromStdString(message);
	}
	iio_channel_enable(channel);

	double scale = 1.0;
	int offsetAttrVal = 0;
	const char *scaleAttrName = "scale";
	const char *offsetAttrName = "offset";
	const char *scaleAttr = iio_channel_find_attr(channel, scaleAttrName);
	//if(scaleAttr == NULL) return QString("No scale attribute");
	const char *offsetAttr = iio_channel_find_attr(channel, offsetAttrName);
	//if(offsetAttr == NULL) return QString("No offset attribute");

	double *scaleVal = new double(1);
	int scaleRet = iio_channel_attr_read_double(channel, scaleAttr, scaleVal);
	//if(scaleRet != 0) return QString("Cannot read scale attribute");
	scale = *scaleVal;

	char *offsetDst = new char[maxAttrSize];
	iio_channel_attr_read(channel, offsetAttr, offsetDst, maxAttrSize);
	offsetAttrVal = std::atoi(offsetDst);

	iio_buffer *iioBuffer = iio_device_create_buffer(admtDevice, bufferSize, false);
	//if(!iioBuffer) return QString("Cannot create buffer.");

	ssize_t numBytesRead;
	int8_t *pointerData, *pointerEnd;
	void *buffer;
	ptrdiff_t pointerIncrement;

	numBytesRead = iio_buffer_refill(iioBuffer);
	//if(numBytesRead < 1) return QString("Cannot refill buffer.");

	pointerIncrement = reinterpret_cast<ptrdiff_t>(iio_buffer_step(iioBuffer));
	pointerEnd = static_cast<int8_t*>(iio_buffer_end(iioBuffer));

	const struct iio_data_format *format = iio_channel_get_data_format(channel);
	const struct iio_data_format channelFormat = *format;
	unsigned int repeat = channelFormat.repeat;
	uint8_t bitLength = static_cast<uint8_t>(channelFormat.bits);
	size_t offset = static_cast<uint8_t>(channelFormat.shift);
	
	QString result;
	std::list<char> rawSamples;
	// std::list<uint16_t> unsignedSamples;
	std::list<int16_t> castSamples;

	size_t sample, bytes;

	size_t sampleSize = channelFormat.length / 8 * repeat;
	//if(sampleSize == 0) return QString("Sample size is zero.");

	buffer = malloc(sampleSize * bufferSize);
	//if(!buffer) return QString("Cannot allocate memory for buffer.");

	bytes = iio_channel_read(channel, iioBuffer, buffer, sampleSize * bufferSize);
	for(sample = 0; sample < bytes / sampleSize; ++sample)
	{
		for(int j = 0; j < repeat; ++j)
		{
			if(channelFormat.length / 8 == sizeof(int16_t))
			{
				rawSamples.push_back(*((int8_t*)buffer));
				int16_t rawValue = ((int16_t*)buffer)[sample+j];
				castSamples.push_back(rawValue);
				value = (rawValue - static_cast<int16_t>(offsetAttrVal)) * scale;
				result = QString::number(value);
			}
		}
	}

	message = message + result.toStdString();
 	iio_buffer_destroy(iioBuffer);
	return value; //QString::fromStdString(message);
}

/** @brief Get the attribute value of a device
 * @param deviceName A pointer to the device name
 * @param attributeName A NULL-terminated string corresponding to the name of the
 * attribute
 * @param returnValue A pointer to a double variable where the value should be stored
 * @return On success, 0 is returned.
 * @return On error, -1 is returned. */
int ADMTController::getDeviceAttributeValue(const char *deviceName, const char *attributeName, double *returnValue)
{
    int result = -1;
    int deviceCount = iio_context_get_devices_count(m_iioCtx);
    if(deviceCount == 0) { return result; }
	iio_device *iioDevice = iio_context_find_device(m_iioCtx, deviceName);
    if(iioDevice == NULL) { return result; }
    const char* hasAttr = iio_device_find_attr(iioDevice, attributeName);
    if(hasAttr == NULL) { return result; }
    result = iio_device_attr_read_double(iioDevice, attributeName, returnValue);

    return result;
}

/** @brief Set the attribute value of a device
 * @param deviceName A pointer to the device name
 * @param attributeName A NULL-terminated string corresponding to the name of the
 * attribute
 * @param writeValue A double variable of the value to be set
 * @return On success, 0 is returned.
 * @return On error, -1 is returned. */
int ADMTController::setDeviceAttributeValue(const char *deviceName, const char *attributeName, double writeValue)
{
    int result = -1;
    int deviceCount = iio_context_get_devices_count(m_iioCtx);
    if(deviceCount == 0) { return result; }
	iio_device *iioDevice = iio_context_find_device(m_iioCtx, deviceName);
    if(iioDevice == NULL) { return result; }
    const char* hasAttr = iio_device_find_attr(iioDevice, attributeName);
    if(hasAttr == NULL) { return result; }
    result = iio_device_attr_write_double(iioDevice, attributeName, writeValue);

    return result;
}

int ADMTController::writeDeviceRegistry(const char *deviceName, uint32_t address, uint32_t value)
{
    int result = -1;
    int deviceCount = iio_context_get_devices_count(m_iioCtx);
    if(deviceCount == 0) { return result; }
    iio_device *iioDevice = iio_context_find_device(m_iioCtx, deviceName);
    if(iioDevice == NULL) { return result; }
    result = iio_device_reg_write(iioDevice, address, value);

    return result;
}

int ADMTController::readDeviceRegistry(const char *deviceName, uint32_t address, uint32_t *returnValue)
{
    int result = -1;
    int deviceCount = iio_context_get_devices_count(m_iioCtx);
    if(deviceCount == 0) { return result; }
    iio_device *iioDevice = iio_context_find_device(m_iioCtx, deviceName);
    if(iioDevice == NULL) { return result; }
    result = iio_device_reg_read(iioDevice, address, returnValue);

    return result;
}

/* bit reversal from online example */
unsigned int ADMTController::bitReverse(unsigned int x, int log2n) {
    int n = 0;
    int mask = 0x1;
    for (int i = 0; i < log2n; i++) {
        n <<= 1;
        n |= (x & 1);
        x >>= 1;
    }
    return n;
}

template<class Iter_T>
/* fft from example codes online */
void ADMTController::fft(Iter_T a, Iter_T b, int log2n)
{
    typedef typename iterator_traits<Iter_T>::value_type complex;
    const complex J(0, 1);
    int n = 1 << log2n;
    for (unsigned int i = 0; i < n; ++i) {
        b[bitReverse(i, log2n)] = a[i];
    }
    for (int s = 1; s <= log2n; ++s) {
        int m = 1 << s;
        int m2 = m >> 1;
        complex w(1, 0);
        complex wm = exp(-J * (M_PI / m2));
        for (int j = 0; j < m2; ++j) {
            for (int k = j; k < n; k += m) {
                complex t = w * b[k + m2];
                complex u = b[k];
                b[k] = u + t;
                b[k + m2] = u - t;
            }
            w *= wm;
        }
    }
}

/* For linear fitting (hard-coded based on examples and formula for polynomial fitting) */
int ADMTController::linear_fit(vector<double> x, vector<double> y, double* slope, double* intercept)
{
    /* x, y, x^2, y^2, xy, xy^2 */
    double sum_x = 0, sum_y = 0, sum_x2 = 0, sum_y2 = 0, sum_xy = 0;
    int i;

    if (x.size() != y.size())
        return -22;

    for (i = 0; i < x.size(); i++) {
        sum_x += x[i];
        sum_y += y[i];
        sum_x2 += (x[i] * x[i]);
        sum_y2 += (y[i] * y[i]);
        sum_xy += (x[i] * y[i]);
    }

    *slope = (x.size() * sum_xy - sum_x * sum_y) / (x.size() * sum_x2 - sum_x * sum_x);

    *intercept = (sum_y * sum_x2 - sum_x * sum_xy) / (x.size() * sum_x2 - sum_x * sum_x);

    return 0;
}

/* Calculate angle error based on MATLAB and C# implementation */
int ADMTController::calculate_angle_error(vector<double> angle_meas, vector<double>& angle_error_ret, double* max_angle_err)
{
    // Ideal angles
    vector<double> expected_angles = { 
        0.00000, 15.46875, 30.93750, 46.40625, 61.87500, 77.34375, 92.81250, 108.28125, 123.75000, 139.21875, 154.68750, 170.15625, 185.62500, 201.09375, 216.56250, 232.03125, 247.50000, 262.96875, 278.43750, 293.90625, 309.37500, 324.84375, 340.31250, 355.78125,
        11.25000, 26.71875, 42.18750, 57.65625, 73.12500, 88.59375, 104.06250, 119.53125, 135.00000, 150.46875, 165.93750, 181.40625, 196.87500, 212.34375, 227.81250, 243.28125, 258.75000, 274.21875, 289.68750, 305.15625, 320.62500, 336.09375, 351.56250, 7.03125,
        22.50000, 37.96875, 53.43750, 68.90625, 84.37500, 99.84375, 115.31250, 130.78125, 146.25000, 161.71875, 177.18750, 192.65625, 208.12500, 223.59375, 239.06250, 254.53125, 270.00000, 285.46875, 300.93750, 316.40625, 331.87500, 347.34375, 2.81250, 18.28125,
        33.75000, 49.21875, 64.68750, 80.15625, 95.62500, 111.09375, 126.56250, 142.03125, 157.50000, 172.96875, 188.43750, 203.90625, 219.37500, 234.84375, 250.31250, 265.78125, 281.25000, 296.71875, 312.18750, 327.65625, 343.12500, 358.59375, 14.06250, 29.53125,
        45.00000, 60.46875, 75.93750, 91.40625, 106.87500, 122.34375, 137.81250, 153.28125, 168.75000, 184.21875, 199.68750, 215.15625, 230.62500, 246.09375, 261.56250, 277.03125, 292.50000, 307.96875, 323.43750, 338.90625, 354.37500, 9.84375, 25.31250, 40.78125,
        56.25000, 71.71875, 87.18750, 102.65625, 118.12500, 133.59375, 149.06250, 164.53125, 180.00000, 195.46875, 210.93750, 226.40625, 241.87500, 257.34375, 272.81250, 288.28125, 303.75000, 319.21875, 334.68750, 350.15625, 5.62500, 21.09375, 36.56250, 52.03125,
        67.50000, 82.96875, 98.43750, 113.90625, 129.37500, 144.84375, 160.31250, 175.78125, 191.25000, 206.71875, 222.18750, 237.65625, 253.12500, 268.59375, 284.06250, 299.53125, 315.00000, 330.46875, 345.93750, 1.40625, 16.87500, 32.34375, 47.81250, 63.28125,
        78.75000, 94.21875, 109.68750, 125.15625, 140.62500, 156.09375, 171.56250, 187.03125, 202.50000, 217.96875, 233.43750, 248.90625, 264.37500, 279.84375, 295.31250, 310.78125, 326.25000, 341.71875, 357.18750, 12.65625, 28.12500, 43.59375, 59.06250, 74.53125,
        90.00000, 105.46875, 120.93750, 136.40625, 151.87500, 167.34375, 182.81250, 198.28125, 213.75000, 229.21875, 244.68750, 260.15625, 275.62500, 291.09375, 306.56250, 322.03125, 337.50000, 352.96875, 8.43750, 23.90625, 39.37500, 54.84375, 70.31250, 85.78125,
        101.25000, 116.71875, 132.18750, 147.65625, 163.12500, 178.59375, 194.06250, 209.53125, 225.00000, 240.46875, 255.93750, 271.40625, 286.87500, 302.34375, 317.81250, 333.28125, 348.75000, 4.21875, 19.68750, 35.15625, 50.62500, 66.09375, 81.56250, 97.03125,
        112.50000, 127.96875, 143.43750, 158.90625, 174.37500, 189.84375, 205.31250, 220.78125, 236.25000, 251.71875, 267.18750, 282.65625, 298.12500, 313.59375, 329.06250, 344.53125
    };

    // Ensure that the angle_meas and expected_angles are of the same size
    if (angle_meas.size() != expected_angles.size()) {
        // Handle size mismatch error
        return -1;
    }

    vector<double> angle_meas_rad(angle_meas.size()); // Convert measured angles to radians
    vector<double> expected_angles_rad(expected_angles.size()); // Convert expected angles to radians

    // Convert measured and expected angles to radians
    for (int i = 0; i < angle_meas.size(); i++) {
        angle_meas_rad[i] = angle_meas[i] * M_PI / 180.0;
        expected_angles_rad[i] = expected_angles[i] * M_PI / 180.0;
    }

    // Unwrap the measured angles
    vector<double> angle_meas_rad_unwrap(angle_meas.size());
    double num = 0.0;
    angle_meas_rad_unwrap[0] = angle_meas_rad[0];
    for (int i = 1; i < angle_meas.size(); i++) {
        double diff = angle_meas_rad[i] - angle_meas_rad[i - 1];
        if (diff > M_PI) {
            num -= 2.0 * M_PI;
        } else if (diff < -M_PI) {
            num += 2.0 * M_PI;
        }
        angle_meas_rad_unwrap[i] = angle_meas_rad[i] + num;
    }

    // Set the initial point to zero
    double offset = angle_meas_rad_unwrap[0];
    for (int i = 0; i < angle_meas.size(); i++) {
        angle_meas_rad_unwrap[i] -= offset;
    }

    // Calculate the angle error using the expected angles (unwrap vs expected)
    angle_error_ret.resize(angle_meas.size());
    for (int i = 0; i < angle_meas.size(); i++) {
        angle_error_ret[i] = angle_meas_rad_unwrap[i] - expected_angles_rad[i];
    }

    // Find the min/max error for offset correction
    auto minmax = minmax_element(angle_error_ret.begin(), angle_error_ret.end());
    double angle_err_offset = (*minmax.first + *minmax.second) / 2;

    // Subtract the error offset
    for (int i = 0; i < angle_meas.size(); i++) {
        angle_error_ret[i] -= angle_err_offset;
    }

    // Convert angle errors back to degrees
    for (int i = 0; i < angle_meas.size(); i++) {
        angle_error_ret[i] *= (180.0 / M_PI);
    }

    // Find maximum absolute angle error
    *max_angle_err = max(fabs(*minmax.first), fabs(*minmax.second)) * (180.0 / M_PI);

    return 0;
}

QString ADMTController::calibrate(vector<double> PANG, int cycles, int samplesPerCycle) {
    int CCW = 0, circshiftData = 0;
    QString result = "";

    /* Check CCW flag to know if array is to be reversed */
    if (CCW)
        reverse(PANG.begin(), PANG.end());

    /* Randomize starting point of array */
    if (circshiftData) {
        int shift = rand() % PANG.size();
        rotate(PANG.begin(), PANG.begin() + shift, PANG.end());
    }

    // Declare vectors for pre-calibration FFT results
    vector<double> angle_errors_fft_pre(PANG.size() / 2);
    vector<double> angle_errors_fft_phase_pre(PANG.size() / 2);

    // Call the new function for pre-calibration FFT
    getPreCalibrationFFT(PANG, cycles, samplesPerCycle, angle_errors_fft_pre, angle_errors_fft_phase_pre);

    // Extract HMag parameters
    double H1Mag = angle_errors_fft_pre[cycles];
    double H2Mag = angle_errors_fft_pre[2 * cycles];
    double H3Mag = angle_errors_fft_pre[3 * cycles];
    double H8Mag = angle_errors_fft_pre[8 * cycles];

    /* Display HMAG values */
    result.append("H1Mag = " + QString::number(H1Mag) + "\n");
    result.append("H2Mag = " + QString::number(H2Mag) + "\n");
    result.append("H3Mag = " + QString::number(H3Mag) + "\n");
    result.append("H8Mag = " + QString::number(H8Mag) + "\n");

    // Extract HPhase parameters
    double H1Phase = (180 / M_PI) * (angle_errors_fft_phase_pre[cycles]);
    double H2Phase = (180 / M_PI) * (angle_errors_fft_phase_pre[2 * cycles]);
    double H3Phase = (180 / M_PI) * (angle_errors_fft_phase_pre[3 * cycles]);
    double H8Phase = (180 / M_PI) * (angle_errors_fft_phase_pre[8 * cycles]);

    /* Display HPHASE values */
    result.append("H1Phase = " + QString::number(H1Phase) + "\n");
    result.append("H2Phase = " + QString::number(H2Phase) + "\n");
    result.append("H3Phase = " + QString::number(H3Phase) + "\n");
    result.append("H8Phase = " + QString::number(H8Phase) + "\n");

    double H1 = H1Mag * cos(M_PI / 180 * (H1Phase));
    double H2 = H2Mag * cos(M_PI / 180 * (H2Phase));
    double H3 = H3Mag * cos(M_PI / 180 * (H3Phase));
    double H8 = H8Mag * cos(M_PI / 180 * (H8Phase));

    double init_err = H1 + H2 + H3 + H8;
    double init_angle = PANG[0] - init_err;
    double H1PHcor, H2PHcor, H3PHcor, H8PHcor;

    /* Counterclockwise, slope of error FIT is negative */
    if (CCW) {
        H1Phase *= -1;
        H2Phase *= -1;
        H3Phase *= -1;
        H8Phase *= -1;
    }

    /* Clockwise */
    H1PHcor = H1Phase - (1 * init_angle - 90);
    H2PHcor = H2Phase - (2 * init_angle - 90);
    H3PHcor = H3Phase - (3 * init_angle - 90);
    H8PHcor = H8Phase - (8 * init_angle - 90);

    /* Get modulo from 360 */
    H1PHcor = (int)H1PHcor % 360;
    H2PHcor = (int)H2PHcor % 360;
    H3PHcor = (int)H3PHcor % 360;
    H8PHcor = (int)H8PHcor % 360;

    /* Variables declaration for data correction */
    vector<double> H1c(PANG.size());
    vector<double> H2c(PANG.size());
    vector<double> H3c(PANG.size());
    vector<double> H8c(PANG.size());
    vector<double> HXcorrection(PANG.size());

    ///* Apply correction and check if working on original data */
    for (int i = 0; i < PANG.size(); i++) {
        H1c[i] = H1Mag * sin(M_PI / 180 * (PANG[i]) + M_PI / 180 * (H1PHcor));
        H2c[i] = H2Mag * sin(2 * M_PI / 180 * (PANG[i]) + M_PI / 180 * (H2PHcor));
        H3c[i] = H3Mag * sin(3 * M_PI / 180 * (PANG[i]) + M_PI / 180 * (H3PHcor));
        H8c[i] = H8Mag * sin(8 * M_PI / 180 * (PANG[i]) + M_PI / 180 * (H8PHcor));

        HXcorrection[i] = H1c[i] + H2c[i] + H3c[i] + H8c[i];
    }

    // HMag Scaling
    H1Mag = H1Mag * 0.6072;
    H2Mag = H2Mag * 0.6072;
    H3Mag = H3Mag * 0.6072;
    H8Mag = H8Mag * 0.6072;

    // Derive register compatible HMAG values
    double mag_scale_factor_11bit = 11.2455 / (1 << 11);
    double mag_scale_factor_8bit = 1.40076 / (1 << 8);
    HAR_MAG_1 = (int)(H1Mag / mag_scale_factor_11bit) & (0x7FF); // 11 bit 
    HAR_MAG_2 = (int)(H2Mag / mag_scale_factor_11bit) & (0x7FF); // 11 bit
    HAR_MAG_3 = (int)(H3Mag / mag_scale_factor_8bit) & (0xFF); // 8 bit
    HAR_MAG_8 = (int)(H8Mag / mag_scale_factor_8bit) & (0xFF);  // 8 bit

    // Derive register compatible HPHASE values
    double pha_scale_factor_12bit = 360.0 / (1 << 12); // in Deg
    HAR_PHASE_1 = (int)(H1PHcor / pha_scale_factor_12bit) & (0xFFF); // 12bit number
    HAR_PHASE_2 = (int)(H2PHcor / pha_scale_factor_12bit) & (0xFFF); // 12bit number
    HAR_PHASE_3 = (int)(H3PHcor / pha_scale_factor_12bit) & (0xFFF);// 12bit number
    HAR_PHASE_8 = (int)(H8PHcor / pha_scale_factor_12bit) & (0xFFF); // 12bit number

    result.append("HMAG1: " + QString::number(HAR_MAG_1) + "\n");
    result.append("HMAG2: " + QString::number(HAR_MAG_2) + "\n");
    result.append("HMAG3: " + QString::number(HAR_MAG_3) + "\n");
    result.append("HMAG8: " + QString::number(HAR_MAG_8) + "\n");

    result.append("HPHASE1: " + QString::number(HAR_PHASE_1) + "\n");
    result.append("HPHASE2: " + QString::number(HAR_PHASE_2) + "\n");
    result.append("HPHASE3: " + QString::number(HAR_PHASE_3) + "\n");
    result.append("HPHASE8: " + QString::number(HAR_PHASE_8) + "\n");

    return result;
}

void ADMTController::getPreCalibrationFFT(const vector<double>& PANG, int cycles, int samplesPerCycle, vector<double>& angle_errors_fft_pre, vector<double>& angle_errors_fft_phase_pre) {
    // Calculate the angle errors before calibration
    double max_err_pre = 0;
    vector<double> angle_errors_pre(PANG.size());

    // Calculate angle errors
    calculate_angle_error(PANG, angle_errors_pre, &max_err_pre);

    // Perform FFT on pre-calibration angle errors
    performFFT(angle_errors_pre, samplesPerCycle, cycles, angle_errors_fft_pre, angle_errors_fft_phase_pre);
}

QString ADMTController::postcalibrate(vector<double> PANG, int cycles, int samplesPerCycle){
    int CCW = 0, circshiftData = 0;
    QString result = "";

    /* Check CCW flag to know if array is to be reversed */
    if (CCW)
        reverse(PANG.begin(), PANG.end());

    /* Randomize starting point of array */
    if (circshiftData) {
        int shift = rand() % PANG.size();
        rotate(PANG.begin(), PANG.begin() + shift, PANG.end());
    }

    // Declare vectors for pre-calibration FFT results
    vector<double> angle_errors_fft_post(PANG.size() / 2);
    vector<double> angle_errors_fft_phase_post(PANG.size() / 2);

    // Call the new function for post-calibration FFT
    getPostCalibrationFFT(PANG, cycles, samplesPerCycle, angle_errors_fft_post, angle_errors_fft_phase_post);
}

void ADMTController::getPostCalibrationFFT(const vector<double>& updated_PANG, int cycles, int samplesPerCycle, vector<double>& angle_errors_fft_post, vector<double>& angle_errors_fft_phase_post) {
    // Calculate the angle errors after calibration
    double max_err_post = 0;
    vector<double> angle_errors_post(updated_PANG.size());

    // Calculate angle errors
    calculate_angle_error(updated_PANG, angle_errors_post, &max_err_post);

    // Perform FFT on post-calibration angle errors
    performFFT(angle_errors_post, samplesPerCycle, cycles, angle_errors_fft_post, angle_errors_fft_phase_post);
}

void performFFT(const vector<double>& angle_errors, vector<double>& angle_errors_fft, vector<double>& angle_errors_fft_phase) {
    typedef complex<double> cx;

    int size = angle_errors.size();
    cx fft_in[size];
    cx fft_out[size];

    // Format angle errors to match data type used in fft function
    for (int i = 0; i < size; i++) {
        fft_in[i] = cx(angle_errors[i], 0);
    }

    // Invoke FFT function
    fft(fft_in, fft_out, 8);

    // Extract magnitude and phase from complex fft_out array
    vector<double> angle_errors_fft_temp(size);
    vector<double> angle_errors_fft_phase_temp(size);

    for (int i = 0; i < size; i++) {
        angle_errors_fft_temp[i] = sqrt(pow(fft_out[i].real() / size, 2) + pow(fft_out[i].imag() / size, 2)) * 2;
        angle_errors_fft_phase_temp[i] = atan2(fft_out[i].imag(), fft_out[i].real());
    }

    // Get upper half only
    for (int i = 0; i < size / 2; i++) {
        angle_errors_fft[i] = angle_errors_fft_temp[i];
        angle_errors_fft_phase[i] = angle_errors_fft_phase_temp[i];
    }
}


void ADMTController::computeSineCosineOfAngles(const vector<double>& angles) {
    // Vectors to store sine and cosine values
    calibration_samples_sine = vector<double>(angles.size());
    calibration_samples_cosine = vector<double>(angles.size());
    calibration_samples_sine_scaled = vector<double>(angles.size());
    calibration_samples_cosine_scaled = vector<double>(angles.size());

    const double scaleMin = 0.0;
    const double scaleMax = 360.0;

    // Convert angles to radians and compute sine, cosine, and their scaled versions
    for (size_t i = 0; i < angles.size(); ++i) {
        double radians = angles[i] * M_PI / 180.0; // Convert degrees to radians
        calibration_samples_sine[i] = sin(radians);
        calibration_samples_cosine[i] = cos(radians);

        // Scale sine and cosine to the range 0 to 360
        calibration_samples_sine_scaled[i] = ((calibration_samples_sine[i] + 1) / 2) * (scaleMax - scaleMin) + scaleMin;
        calibration_samples_cosine_scaled[i] = ((calibration_samples_cosine[i] + 1) / 2) * (scaleMax - scaleMin) + scaleMin;
    }
}

// Function to calculate the scaled harmonic coefficient magnitude and return a 16-bit unsigned integer
uint16_t ADMTController::calculateHarmonicCoefficientMagnitude(double harmonicCoefficient, uint16_t originalValue, string key) {
    // CORDIC scaler
    const double cordicScaler = 0.6072;

    // LSB value (0.005493 degrees)
    const double LSB = 0.005493;

    // Multiply the harmonic coefficient by the CORDIC scaler
    double scaledValue = harmonicCoefficient * cordicScaler;
    
    uint16_t result = 0;

    // Switch case for different bitmapping based on the key
    if (key == "h1" || key == "h2") {
        // For h1 and h2: [15:11 reserved], [10:0 write]
        result = static_cast<uint16_t>(scaledValue / LSB) & 0x07FF;
        originalValue = (originalValue & 0xF800) | result;
    } 
    else if (key == "h3" || key == "h8") {
        // For h3 and h8: [15:8 reserved], [7:0 write]
        result = static_cast<uint16_t>(scaledValue / LSB) & 0x00FF;
        originalValue = (originalValue & 0xFF00) | result;
    } 
    else {
        // Handle invalid key, here we return the original value unchanged
        return originalValue;
    }
    return result;
}

// Function to calculate the scaled harmonic coefficient phase and return a 16-bit unsigned integer
uint16_t ADMTController::calculateHarmonicCoefficientPhase(double harmonicCoefficient, uint16_t originalValue) {
    // LSB value (0.087891 degrees)
    const double LSB = 0.087891;
    
    uint16_t result = 0;

    // Convert the result to an unsigned integer by dividing by LSB, fitting into 12 bits
    // Mask to keep only bits 11:0
    result = static_cast<uint16_t>(harmonicCoefficient / LSB) & 0x0FFF; 

    // Clear bits 11:0 of the original value, keeping bits 15:12 intact
    uint16_t preservedValue = (originalValue & 0xF000) | result;

    return preservedValue;
}

double ADMTController::getActualHarmonicRegisterValue(uint16_t registerValue, const string key) {
    double result = 0.0;
    const double cordicScaler = 0.6072;

    // Switch case for different bitmapping based on the key
    if (key == "h1mag" || key == "h2mag") {
        // For h1h2mag: value is in bits [10:0], bits [15:12] are reserved
        const double LSB = 0.005493;

        // Extract the value from bits [10:0]
        uint16_t extractedValue = registerValue & 0x07FF;

        // Convert the extracted value by applying CORDIC scaler and LSB
        result = extractedValue * LSB / cordicScaler;
    }
    else if (key == "h3mag" || key == "h8mag") {
        // For h3h8mag: value is in bits [7:0], bits [15:8] are reserved
        const double LSB = 0.005493;

        // Extract the value from bits [7:0]
        uint16_t extractedValue = registerValue & 0x00FF;

        // Convert the extracted value by applying CORDIC scaler and LSB
        result = extractedValue * LSB / cordicScaler;
    }
    else if (key == "h1phase" || key == "h2phase" || key == "h3phase" || key == "h8phase") {
        // For Phase: value is in bits [11:0], bits [15:12] are reserved
        const double LSB = 0.087891;

        // Extract the value from bits [11:0]
        uint16_t extractedValue = registerValue & 0x0FFF;

        // Convert the extracted value by applying the LSB
        result = extractedValue * LSB;
    } 
    else {
        // Indicating an error or invalid key
        result = -404.0;
    }

    return result;
}

map<string, bool> ADMTController::getFaultRegisterBitMapping(uint16_t registerValue) {
    map<string, bool> result;

    // Extract each bit and store the result in the map
    // Rain: Current returns it as <key,bool> value. 
    result["Sequencer Watchdog"]      = (registerValue >> 15) & 0x01;
    result["AMR Radius Check"]  = (registerValue >> 14) & 0x01;
    result["Turn Counter Cross Check"]     = (registerValue >> 13) & 0x01;
    result["MT Diagnostic"]      = (registerValue >> 12) & 0x01;
    result["Turn Count Sensor Levels"]    = (registerValue >> 11) & 0x01;
    result["Angle Cross Check"]          = (registerValue >> 10) & 0x01;
    result["Count Sensor False State"]        = (registerValue >> 9)  & 0x01;
    result["Oscillator Drift"]        = (registerValue >> 8)  & 0x01;
    result["ECC Double Bit Error"]        = (registerValue >> 7)  & 0x01;
    result["Reserved"]          = (registerValue >> 6)  & 0x01;
    result["NVM CRC Fault"]           = (registerValue >> 5)  & 0x01;
    result["AFE Diagnostic"]           = (registerValue >> 4)  & 0x01;
    result["VDRIVE Over Voltage"]       = (registerValue >> 3)  & 0x01;
    result["VDRIVE Under Voltage"]      = (registerValue >> 2)  & 0x01;
    result["VDD Over Voltage"]          = (registerValue >> 1)  & 0x01;
    result["VDD Under Voltage"]         = (registerValue >> 0)  & 0x01;

    return result;
}
// // How to read each value sample
// for (const auto& pair : result) {
//     std::cout << pair.first << ": " << (pair.second ? "Set" : "Not Set") << std::endl;
// }

map<string, string> ADMTController::getGeneralRegisterBitMapping(uint16_t registerValue) {
    map<string, string> result;

    // Bit 15: STORAGE[7]
    result["STORAGE[7]"] = ((registerValue >> 15) & 0x01) ? "Set" : "Not Set";

    // Bits 14:13: Convert Synchronization
    uint16_t convertSync = (registerValue >> 13) & 0x03;
    switch (convertSync) {
        case 0x00:
            result["Convert Synchronization"] = "Disabled";
            break;
        case 0x03:
            result["Convert Synchronization"] = "Enabled";
            break;
        default:
            result["Convert Synchronization"] = "Reserved";
            break;
    }

    // Bit 12: Angle Filter
    result["Angle Filter"] = ((registerValue >> 12) & 0x01) ? "Enabled" : "Disabled";

    // Bit 11: STORAGE[6]
    result["STORAGE[6]"] = ((registerValue >> 11) & 0x01) ? "Set" : "Not Set";

    // Bit 10: 8th Harmonic
    result["8th Harmonic"] = ((registerValue >> 10) & 0x01) ? "User-Supplied Values" : "ADI Factory Values";

    // // Bit 9: Reserved (skipped)
    // result["Reserved"] = "Reserved";

    // Bits 8:6: STORAGE[5:3]
    uint16_t storage_5_3 = (registerValue >> 6) & 0x07;
    result["STORAGE[5:3]"] = std::to_string(storage_5_3);

    // Bits 5:4: Sequence Type
    uint16_t sequenceType = (registerValue >> 4) & 0x03;
    switch (sequenceType) {
        case 0x00:
            result["Sequence Type"] = "Mode 2";
            break;
        case 0x03:
            result["Sequence Type"] = "Mode 1";
            break;
        default:
            result["Sequence Type"] = "Reserved";
            break;
    }

    // Bits 3:1: STORAGE[2:0]
    uint16_t storage_2_0 = (registerValue >> 1) & 0x07;
    result["STORAGE[2:0]"] = std::to_string(storage_2_0);

    // Bit 0: Conversion Type
    result["Conversion Type"] = (registerValue & 0x01) ? "One-shot conversion" : "Continuous conversions";

    return result;
}
// // How to read each value sample
// for (const auto& pair : result) {
//     std::cout << pair.first << ": " << pair.second << std::endl;
// }

map<string, bool> ADMTController::getDigioenRegisterBitMapping(uint16_t registerValue) {
    map<string, bool> result;

    // // Bits 15:14: Reserved (skipped)
    // result["Reserved (15:14)"] = "Reserved";

    // Bit 13: DIGIO5EN
    result["DIGIO5EN"] = ((registerValue >> 13) & 0x01) ? true : false; // ? "GPIO5 output enable" : "GPIO5 output disable";

    // Bit 12: DIGIO4EN
    result["DIGIO4EN"] = ((registerValue >> 12) & 0x01) ? true : false; // ? "GPIO4 output enable" : "GPIO4 output disable";

    // Bit 11: DIGIO3EN
    result["DIGIO3EN"] = ((registerValue >> 11) & 0x01) ? true : false; // ? "GPIO3 output enable" : "GPIO3 output disable";

    // Bit 10: DIGIO2EN
    result["DIGIO2EN"] = ((registerValue >> 10) & 0x01) ? true : false; // ? "GPIO2 output enable" : "GPIO2 output disable";

    // Bit 9: DIGIO1EN
    result["DIGIO1EN"] = ((registerValue >> 9) & 0x01) ? true : false; // ? "GPIO1 output enable" : "GPIO1 output disable";

    // Bit 8: DIGIO0EN
    result["DIGIO0EN"] = ((registerValue >> 8) & 0x01) ? true : false; // ? "GPIO0 output enable" : "GPIO0 output disable";

    // // Bits 7:6: Reserved (skipped)
    // result["Reserved (7:6)"] = "Reserved";

    // Bit 5: Bootload
    result["BOOTLOAD"] = ((registerValue >> 5) & 0x01) ? false : true; // ? "GPIO5" : "Bootload (Output only)";

    // Bit 4: Fault
    result["FAULT"] = ((registerValue >> 4) & 0x01) ? false : true; // ? "GPIO4" : "Fault (Output only)";

    // Bit 3: Acalc
    result["ACALC"] = ((registerValue >> 3) & 0x01) ? false : true; // ? "GPIO3" : "Acalc (Output only)";

    // Bit 2: Sent
    result["SENT"] = ((registerValue >> 2) & 0x01) ? false : true; // ? "GPIO2" : "Sent (Output only)";

    // Bit 1: Cnv
    result["CNV"] = ((registerValue >> 1) & 0x01) ? false : true; // ? "GPIO1" : "Cnv (Output only)";

    // Bit 0: Busy
    result["BUSY"] = (registerValue & 0x01) ? false : true; // ? "GPIO0" : "Busy (Output only)";

    return result;
}

map<string, bool> ADMTController::getDiag1RegisterBitMapping_Register(uint16_t registerValue) {
    map<string, bool> result;

    // Bits 15 to 8: R7 to R0 (Enabled or Disabled)
    result["R7"] = ((registerValue >> 15) & 0x01) ? true : false;
    result["R6"] = ((registerValue >> 14) & 0x01) ? true : false;
    result["R5"] = ((registerValue >> 13) & 0x01) ? true : false;
    result["R4"] = ((registerValue >> 12) & 0x01) ? true : false;
    result["R3"] = ((registerValue >> 11) & 0x01) ? true : false;
    result["R2"] = ((registerValue >> 10) & 0x01) ? true : false;
    result["R1"] = ((registerValue >> 9) & 0x01) ? true : false;
    result["R0"] = ((registerValue >> 8) & 0x01) ? true : false;

    return result;
}

map<string, double> ADMTController::getDiag1RegisterBitMapping_Afe(uint16_t registerValue) {
    map<string, double> result;

    // Bits 7:0: AFE Diagnostic 2 - Measurement of Fixed voltage (stored in 2's complement)
    int8_t afeDiagnostic = static_cast<int8_t>(registerValue & 0x00FF); // Interpret as signed 8-bit

    // Choose the correct resolution based on the voltage level (5V or 3.3V part)
    double resolution = 0.003222; // 0.0048828 if 5V

    // Convert the AFE Diagnostic value to a voltage
    double diagnosticVoltage = static_cast<double>(afeDiagnostic) * resolution;
    result["AFE Diagnostic 2"] = diagnosticVoltage;

    return result;
}

map<string, double> ADMTController::getDiag2RegisterBitMapping(uint16_t registerValue) {
    map<string, double> result;

    // Bits 15:8: AFE Diagnostic 1 - Measurement of AFE +57% diagnostic resistor
    uint16_t afeDiagnostic1 = (registerValue >> 8) & 0x00FF;
    
    // Convert AFE Diagnostic 1 value to double
    // Rain: adjust scaling factor as needed with actual method.
    double diagnostic1Voltage = static_cast<double>(afeDiagnostic1) * 0.01;  // what I found (to be confirmed)
    
    // Store the result with fixed precision
    result["AFE Diagnostic 1 (+57%)"] = diagnostic1Voltage;

    // Bits 7:0: AFE Diagnostic 0 - Measurement of AFE -57% diagnostic resistor
    uint16_t afeDiagnostic0 = registerValue & 0x00FF;
    
    // Convert AFE Diagnostic 0 value to double
    // Rain: adjust scaling factor as needed with actual method.
    double diagnostic0Voltage = static_cast<double>(afeDiagnostic0) * 0.01;  // what I found (to be confirmed)
    
    // Store the result with fixed precision
    result["AFE Diagnostic 0 (-57%)"] = diagnostic0Voltage;

    return result;
}